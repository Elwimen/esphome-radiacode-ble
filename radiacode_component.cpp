/**
 * RadiaCode BLE Component for ESPHome - Implementation
 */

#include "radiacode_component.h"

#ifdef USE_ESP32

namespace esphome {
namespace radiacode_ble {

static const char *TAG = "radiacode_ble";

void RadiaCodeBLEComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RadiaCode BLE component");
}

void RadiaCodeBLEComponent::loop() {
  // Check for response timeout
  if (bytes_received_ > 0 && is_response_timeout()) {
    ESP_LOGW(TAG, "Response timeout - resetting buffer");
    reset_response_buffer();
  }

  // Request data periodically if initialized
  if (device_initialized_) {
    uint32_t now = millis();

    // Request radiation data every 5 seconds
    if (now - last_update_time_ >= UPDATE_INTERVAL_MS) {
      request_data();
      last_update_time_ = now;
    }

    // Request temperature every 30 seconds
    if (now - last_temperature_time_ >= 30000) {
      request_temperature();
      last_temperature_time_ = now;
    }
  }
}

void RadiaCodeBLEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RadiaCode Component:");
  LOG_SENSOR("  ", "Dose Rate", dose_rate_sensor_);
  LOG_SENSOR("  ", "Count Rate", count_rate_sensor_);
  LOG_SENSOR("  ", "Count Rate CPM", count_rate_cpm_sensor_);
  LOG_SENSOR("  ", "Dose Accumulated", dose_accumulated_sensor_);
  LOG_SENSOR("  ", "Temperature", temperature_sensor_);
}

void RadiaCodeBLEComponent::gattc_event_handler(esp_gattc_cb_event_t event,
                                            esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully");
      }
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGW(TAG, "Disconnected");
      services_discovered_ = false;
      device_initialized_ = false;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
      discover_services();
      break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Notifications registered successfully");
        initialize_device();
      }
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == notify_handle_) {
        handle_notification(param->notify.value, param->notify.value_len);
      }
      break;

    default:
      break;
  }
}

void RadiaCodeBLEComponent::discover_services() {
  // Get RadiaCode service
  auto *service = this->parent_->get_service(esp32_ble::ESPBTUUID::from_raw(SERVICE_UUID));
  if (!service) {
    ESP_LOGE(TAG, "Service not found");
    return;
  }

  // Get write characteristic
  auto *write_char = service->get_characteristic(esp32_ble::ESPBTUUID::from_raw(WRITE_CHAR_UUID));
  if (!write_char) {
    ESP_LOGE(TAG, "Write characteristic not found");
    return;
  }
  write_handle_ = write_char->handle;

  // Get notify characteristic
  auto *notify_char = service->get_characteristic(esp32_ble::ESPBTUUID::from_raw(NOTIFY_CHAR_UUID));
  if (!notify_char) {
    ESP_LOGE(TAG, "Notify characteristic not found");
    return;
  }
  notify_handle_ = notify_char->handle;

  // Register for notifications
  esp_err_t status = esp_ble_gattc_register_for_notify(
      this->parent_->get_gattc_if(),
      this->parent_->get_remote_bda(),
      notify_char->handle);

  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Failed to register for notifications: %d", status);
    return;
  }

  services_discovered_ = true;
  ESP_LOGI(TAG, "Services discovered successfully");
}

void RadiaCodeBLEComponent::initialize_device() {
  ESP_LOGI(TAG, "Initializing device with SET_EXCHANGE command");
  std::vector<uint8_t> init_data = {0x01, 0xFF, 0x12, 0xFF};
  send_command(Command::SET_EXCHANGE, init_data);
  device_initialized_ = true;
}

void RadiaCodeBLEComponent::request_data() {
  ESP_LOGD(TAG, "Requesting data buffer");

  // Build DATA_BUF request (VS ID as 4-byte little-endian)
  uint32_t vs_id = static_cast<uint32_t>(VirtualString::DATA_BUF);
  std::vector<uint8_t> payload = {
      static_cast<uint8_t>(vs_id & 0xFF),
      static_cast<uint8_t>((vs_id >> 8) & 0xFF),
      static_cast<uint8_t>((vs_id >> 16) & 0xFF),
      static_cast<uint8_t>((vs_id >> 24) & 0xFF)
  };

  send_command(Command::RD_VIRT_STRING, payload);
}

void RadiaCodeBLEComponent::request_temperature() {
  uint32_t vsfr_id = static_cast<uint32_t>(VSFR::TEMP_degC);
  ESP_LOGD(TAG, "Requesting temperature (VSFR 0x%08X)", vsfr_id);

  // Build VSFR request for TEMP_degC (4-byte little-endian)
  std::vector<uint8_t> payload = {
      static_cast<uint8_t>(vsfr_id & 0xFF),
      static_cast<uint8_t>((vsfr_id >> 8) & 0xFF),
      static_cast<uint8_t>((vsfr_id >> 16) & 0xFF),
      static_cast<uint8_t>((vsfr_id >> 24) & 0xFF)
  };

  send_command(Command::RD_VIRT_SFR, payload);
}

void RadiaCodeBLEComponent::send_command(Command cmd, const std::vector<uint8_t> &payload) {
  if (!services_discovered_) {
    ESP_LOGW(TAG, "Services not discovered yet");
    return;
  }

  uint16_t cmd_code = static_cast<uint16_t>(cmd);

  // Build packet: [length: 4B] [cmd: 2B] [reserved: 1B] [seq: 1B] [payload]
  std::vector<uint8_t> packet;

  // Calculate total packet size (header + payload)
  uint32_t packet_size = 4 + payload.size();  // 4 bytes for header (cmd + reserved + seq)

  // Add length prefix (little-endian)
  packet.push_back(packet_size & 0xFF);
  packet.push_back((packet_size >> 8) & 0xFF);
  packet.push_back((packet_size >> 16) & 0xFF);
  packet.push_back((packet_size >> 24) & 0xFF);

  // Add command (little-endian)
  packet.push_back(cmd_code & 0xFF);
  packet.push_back((cmd_code >> 8) & 0xFF);

  // Add reserved byte and sequence number (sequence number needs 0x80 added!)
  packet.push_back(0x00);
  uint8_t seq = 0x80 + (sequence_number_++ % 32);
  packet.push_back(seq);

  // Add payload
  packet.insert(packet.end(), payload.begin(), payload.end());

  ESP_LOGD(TAG, "Sending command 0x%04X (%d bytes)", cmd_code, packet.size());

  // Send in chunks
  for (size_t i = 0; i < packet.size(); i += BLE_CHUNK_SIZE) {
    size_t chunk_len = std::min(static_cast<size_t>(BLE_CHUNK_SIZE), packet.size() - i);

    esp_err_t status = esp_ble_gattc_write_char(
        this->parent_->get_gattc_if(),
        this->parent_->get_conn_id(),
        write_handle_,
        chunk_len,
        const_cast<uint8_t *>(&packet[i]),
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE);

    if (status != ESP_OK) {
      ESP_LOGW(TAG, "Write failed at offset %d: %d", i, status);
    }

    delay(5);  // Small delay between chunks
  }

  reset_response_buffer();
  response_start_time_ = millis();
}

void RadiaCodeBLEComponent::handle_notification(const uint8_t *data, uint16_t length) {
  if (length == 0) return;

  // First packet contains 4-byte length header
  if (bytes_received_ == 0) {
    if (length < 4) {
      ESP_LOGW(TAG, "First packet too short: %d bytes", length);
      return;
    }

    // Parse expected response size (little-endian)
    expected_response_size_ =
        static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);

    ESP_LOGD(TAG, "New response, expected size: %d bytes", expected_response_size_);

    // Store data after length header
    response_buffer_.reserve(expected_response_size_);
    response_buffer_.insert(response_buffer_.end(), data + 4, data + length);
    bytes_received_ = length - 4;
  } else {
    // Subsequent packets - append data
    response_buffer_.insert(response_buffer_.end(), data, data + length);
    bytes_received_ += length;
  }

  ESP_LOGD(TAG, "Received %d/%d bytes", bytes_received_, expected_response_size_);

  // Check if complete
  if (bytes_received_ >= expected_response_size_) {
    process_complete_response();
  }
}

void RadiaCodeBLEComponent::process_complete_response() {
  ESP_LOGD(TAG, "Processing complete response (%d bytes)", response_buffer_.size());

  if (response_buffer_.size() < 4) {
    ESP_LOGW(TAG, "Response too short");
    reset_response_buffer();
    return;
  }

  const uint8_t *data = response_buffer_.data();
  size_t length = response_buffer_.size();

  // Parse response header: [cmd_echo: 2B] [reserved: 1B] [seq: 1B] [retcode: 4B] [payload...]
  uint16_t cmd_echo = data[0] | (data[1] << 8);
  uint8_t reserved = data[2];
  uint8_t seq = data[3];

  // Skip to retcode after 4-byte header
  if (length < 8) {
    ESP_LOGW(TAG, "Response too short for retcode");
    reset_response_buffer();
    return;
  }

  uint32_t retcode = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);

  ESP_LOGD(TAG, "Response: cmd=0x%04X seq=0x%02X retcode=%d payload_len=%d",
           cmd_echo, seq, retcode, length - 8);

  // Check retcode - VIRT_STRING expects 1, VIRT_SFR might use 0
  bool success = false;
  if (cmd_echo == static_cast<uint16_t>(Command::RD_VIRT_STRING)) {
    success = (retcode == 1);
  } else if (cmd_echo == static_cast<uint16_t>(Command::RD_VIRT_SFR)) {
    success = (retcode == 0 || retcode == 1);  // Accept both
  }

  if (!success) {
    ESP_LOGW(TAG, "Command 0x%04X failed with code %d", cmd_echo, retcode);
    reset_response_buffer();
    return;
  }

  // Process payload based on command (skip 8-byte header now)
  if (cmd_echo == static_cast<uint16_t>(Command::RD_VIRT_STRING) && length > 8) {
    parse_data_buffer(data + 8, length - 8);
  }
  else if (cmd_echo == static_cast<uint16_t>(Command::RD_VIRT_SFR) && length > 8) {
    // VSFR response for temperature
    if (length >= 12) {  // 8 byte header + 4 byte value
      float temperature;
      memcpy(&temperature, data + 8, 4);

      if (temperature > 0 && temperature < 100) {
        ESP_LOGI(TAG, "Temperature: %.1f°C", temperature);
        if (temperature_sensor_) {
          temperature_sensor_->publish_state(temperature);
        }
      }
    }
  }

  reset_response_buffer();
}

void RadiaCodeBLEComponent::log_hex_dump(const char *prefix, const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i += 16) {
    char hex_line[80];
    char ascii_line[20];
    int hex_pos = 0;
    int ascii_pos = 0;

    // Offset
    hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%08x: ", i);

    // Hex bytes in pairs
    for (size_t j = 0; j < 16; j++) {
      if (i + j < length) {
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "%02x", data[i + j]);
        // ASCII representation
        char c = data[i + j];
        ascii_line[ascii_pos++] = (c >= 32 && c <= 126) ? c : '.';
      } else {
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, "  ");
        ascii_line[ascii_pos++] = ' ';
      }
      // Space after every 2 bytes
      if (j % 2 == 1) {
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, " ");
      }
      // Extra space in the middle
      if (j == 7) {
        hex_pos += snprintf(hex_line + hex_pos, sizeof(hex_line) - hex_pos, " ");
      }
    }
    ascii_line[ascii_pos] = '\0';

    ESP_LOGV(TAG, "%s%s  %s", prefix, hex_line, ascii_line);
  }
}

void RadiaCodeBLEComponent::parse_data_buffer(const uint8_t *data, size_t length) {
  ESP_LOGVV(TAG, "Parsing data buffer (%d bytes)", length);
  if (esp_log_level_get(TAG) >= ESP_LOG_VERBOSE) {
    log_hex_dump("", data, length);
  }

  size_t offset = 0;
  while (offset + 7 <= length) {
    // Each record: [seq: 1B] [eid: 1B] [gid: 1B] [ts_offset: 4B] [data...]
    uint8_t eid = data[offset + 1];
    uint8_t gid = data[offset + 2];
    offset += 7;  // Skip header

    ESP_LOGVV(TAG, "Record at offset %d: eid=%d gid=%d", offset - 7, eid, gid);

    // eid=0, gid=0 is RealTimeData (19 bytes)
    if (eid == 0 && gid == 0) {
      if (offset + 19 <= length) {
        parse_realtime_record(data + offset, length - offset);
      }
      offset += 19;
    }
    // eid=0, gid=1 is RawData (8 bytes)
    else if (eid == 0 && gid == 1) {
      offset += 8;
    }
    // eid=0, gid=2 is DoseRateDB (16 bytes)
    else if (eid == 0 && gid == 2) {
      offset += 16;
    }
    // eid=0, gid=7 is Event (4 bytes)
    else if (eid == 0 && gid == 7) {
      offset += 4;
    }
    // eid=1 is extended/spectrum data - skip to end
    else if (eid == 1) {
      break;
    }
    else {
      // Unknown record type - stop parsing
      ESP_LOGW(TAG, "Unknown record type eid=%d gid=%d", eid, gid);
      break;
    }
  }
}

void RadiaCodeBLEComponent::parse_realtime_record(const uint8_t *data, size_t length) {
  if (length < 19) {
    return;
  }

  // Skip first 4 bytes, then read count_rate and dose_rate
  const uint8_t *record_data = data + 4;

  float count_rate, dose_rate;
  memcpy(&count_rate, record_data, 4);
  memcpy(&dose_rate, record_data + 4, 4);

  // Convert to proper units
  float count_rate_cps = count_rate;
  int count_rate_cpm = static_cast<int>(count_rate * 60.0f);
  float dose_rate_nsv = dose_rate * 10000000.0f;

  // Integrate dose rate to calculate accumulated dose
  uint32_t now = millis();
  if (last_dose_integration_time_ > 0) {
    float elapsed_hours = (now - last_dose_integration_time_) / 3600000.0f;
    accumulated_dose_nsv_ += dose_rate_nsv * elapsed_hours;
  }
  last_dose_integration_time_ = now;

  ESP_LOGI(TAG, "Radiation: %.2f CPS, %d CPM, %.1f nSv/h", count_rate_cps, count_rate_cpm, dose_rate_nsv);

  // Publish to sensors
  if (count_rate_sensor_) {
    count_rate_sensor_->publish_state(count_rate_cps);
  }

  if (count_rate_cpm_sensor_) {
    count_rate_cpm_sensor_->publish_state(count_rate_cpm);
  }

  if (dose_rate_sensor_) {
    dose_rate_sensor_->publish_state(dose_rate_nsv);
  }

  // Report accumulated dose every 60 seconds
  if (dose_accumulated_sensor_ && (now - last_dose_report_time_ >= 60000)) {
    float accumulated_usv = accumulated_dose_nsv_ / 1000.0f;
    dose_accumulated_sensor_->publish_state(accumulated_usv);
    last_dose_report_time_ = now;
    ESP_LOGI(TAG, "Accumulated dose: %.3f µSv", accumulated_usv);
  }
}

void RadiaCodeBLEComponent::reset_response_buffer() {
  response_buffer_.clear();
  expected_response_size_ = 0;
  bytes_received_ = 0;
  response_start_time_ = 0;
}

bool RadiaCodeBLEComponent::is_response_timeout() const {
  if (response_start_time_ == 0) return false;
  return (millis() - response_start_time_) > RESPONSE_TIMEOUT_MS;
}

}  // namespace radiacode_ble
}  // namespace esphome

#endif  // USE_ESP32
