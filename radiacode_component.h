/**
 * RadiaCode BLE Component for ESPHome
 *
 * Clean implementation for reading radiation data from RadiaCode-110 devices
 * Based on reverse-engineered protocol from open source implementations
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <vector>

namespace esphome {
namespace radiacode_ble {

// BLE UUIDs for RadiaCode service
static const char *SERVICE_UUID = "e63215e5-7003-49d8-96b0-b024798fb901";
static const char *WRITE_CHAR_UUID = "e63215e6-7003-49d8-96b0-b024798fb901";
static const char *NOTIFY_CHAR_UUID = "e63215e7-7003-49d8-96b0-b024798fb901";

// Protocol constants
static constexpr uint16_t MAX_RESPONSE_SIZE = 4096;
static constexpr uint8_t BLE_CHUNK_SIZE = 18;
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 30000;
static constexpr uint32_t UPDATE_INTERVAL_MS = 5000;

// Command codes (from radiacode.types)
enum class Command : uint16_t {
  SET_EXCHANGE = 0x0017,
  RD_VIRT_SFR = 0x0824,
  RD_VIRT_STRING = 0x0826,
};

// Virtual string IDs
enum class VirtualString : uint32_t {
  DATA_BUF = 256,
};

// Virtual SFR (Special Function Register) IDs
enum class VSFR : uint32_t {
  DS_uR = 0x8022,        // Accumulated dose in microroentgen
  TEMP_degC = 0x8024,    // Temperature in Celsius
};

// Data record types
enum class RecordType : uint8_t {
  DOSE_RATE_DB = 1,
  RARE_DATA = 2,
  REAL_TIME_DATA = 3,
  RAW_DATA = 4,
  EVENT = 5,
};

/**
 * RadiaCode BLE component - handles BLE communication and data parsing
 */
class RadiaCodeBLEComponent : public ble_client::BLEClientNode, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                          esp_ble_gattc_cb_param_t *param) override;

  void set_dose_rate_sensor(sensor::Sensor *sensor) { dose_rate_sensor_ = sensor; }
  void set_count_rate_sensor(sensor::Sensor *sensor) { count_rate_sensor_ = sensor; }
  void set_count_rate_cpm_sensor(sensor::Sensor *sensor) { count_rate_cpm_sensor_ = sensor; }
  void set_dose_accumulated_sensor(sensor::Sensor *sensor) { dose_accumulated_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { temperature_sensor_ = sensor; }

  void reset_accumulated_dose() { accumulated_dose_nsv_ = 0.0f; }
  float get_accumulated_dose() { return accumulated_dose_nsv_; }
  void set_accumulated_dose(float dose_nsv) { accumulated_dose_nsv_ = dose_nsv; }

 private:
  // Sensors
  sensor::Sensor *dose_rate_sensor_{nullptr};
  sensor::Sensor *count_rate_sensor_{nullptr};
  sensor::Sensor *count_rate_cpm_sensor_{nullptr};
  sensor::Sensor *dose_accumulated_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};

  // BLE handles
  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};
  bool services_discovered_{false};
  bool device_initialized_{false};

  // Response handling
  std::vector<uint8_t> response_buffer_;
  uint32_t expected_response_size_{0};
  uint32_t bytes_received_{0};
  uint32_t response_start_time_{0};

  // State
  uint8_t sequence_number_{0};
  uint32_t last_update_time_{0};
  uint32_t last_temperature_time_{0};
  uint32_t last_dose_report_time_{0};
  float accumulated_dose_nsv_{0.0f};  // Accumulated dose in nSv
  uint32_t last_dose_integration_time_{0};

  // Protocol methods
  void discover_services();
  void initialize_device();
  void request_data();
  void request_temperature();
  void send_command(Command cmd, const std::vector<uint8_t> &payload);
  void handle_notification(const uint8_t *data, uint16_t length);
  void process_complete_response();
  void parse_data_buffer(const uint8_t *data, size_t length);
  void parse_realtime_record(const uint8_t *data, size_t length);

  // Utility
  void reset_response_buffer();
  bool is_response_timeout() const;
  void log_hex_dump(const char *prefix, const uint8_t *data, size_t length);
};

}  // namespace radiacode_ble
}  // namespace esphome

#endif  // USE_ESP32
