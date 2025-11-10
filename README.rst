RadiaCode BLE Component
=======================

.. seo::
    :description: Instructions for setting up RadiaCode radiation detectors via BLE
    :image: radiacode.jpg
    :keywords: RadiaCode, radiation, BLE, Geiger counter

The ``radiacode_ble`` component allows you to connect to RadiaCode radiation detectors (such as RadiaCode-110)
via Bluetooth Low Energy (BLE) and read radiation measurements in real-time.

.. code-block:: yaml

    # Example configuration entry
    esp32_ble_tracker:

    ble_client:
      - mac_address: "52:43:06:E0:05:C5"
        id: radiacode_ble_client

    radiacode_ble:
      id: radiacode_device
      ble_client_id: radiacode_ble_client

    sensor:
      - platform: radiacode_ble
        radiacode_ble_id: radiacode_device
        dose_rate:
          name: "Dose Rate"
        count_rate:
          name: "Count Rate"
        count_rate_cpm:
          name: "Count Rate CPM"
        dose_accumulated:
          name: "Dose Accumulated"
        temperature:
          name: "Temperature"

Configuration variables:
------------------------

- **id** (*Optional*, :ref:`config-id`): Manually specify the ID used for code generation.
- **ble_client_id** (**Required**, :ref:`config-id`): ID of the associated BLE client.

Sensor
------

The ``radiacode_ble`` sensor platform exposes radiation measurements from RadiaCode devices.

Configuration variables:
~~~~~~~~~~~~~~~~~~~~~~~~

- **radiacode_ble_id** (**Required**, :ref:`config-id`): The ID of the RadiaCode BLE component.
- **dose_rate** (*Optional*): Radiation dose rate in nSv/h (nanosieverts per hour).

  - All options from :ref:`Sensor <config-sensor>`.

- **count_rate** (*Optional*): Count rate in CPS (counts per second).

  - All options from :ref:`Sensor <config-sensor>`.

- **count_rate_cpm** (*Optional*): Count rate in CPM (counts per minute), reported as integer.

  - All options from :ref:`Sensor <config-sensor>`.

- **dose_accumulated** (*Optional*): Accumulated radiation dose in µSv (microsieverts), calculated by integrating dose_rate over time. Updated every 60 seconds.

  - All options from :ref:`Sensor <config-sensor>`.

- **temperature** (*Optional*): Internal temperature of the device in °C. Updated every 30 seconds.

  - All options from :ref:`Sensor <config-sensor>`.

Hardware Support
----------------

This component has been tested with:

- **RadiaCode-110** (firmware >= 4.8)

The component uses the RadiaCode BLE protocol to communicate with the device. The device must be
in BLE mode (not connected via USB to another application).

Protocol Details
----------------

The component implements the RadiaCode BLE protocol with the following specifications:

- **Service UUID**: ``e63215e5-7003-49d8-96b0-b024798fb901``
- **Write characteristic**: ``e63215e6-7003-49d8-96b0-b024798fb901``
- **Notify characteristic**: ``e63215e7-7003-49d8-96b0-b024798fb901``
- **Fragmentation**: 18-byte chunks with 5ms delay between writes
- **Update intervals**:
  - Radiation data (dose rate, count rate): every 5 seconds
  - Temperature: every 30 seconds via VSFR register read
  - Accumulated dose: reported every 60 seconds
- **Timeout handling**: 30-second response timeout with automatic recovery

Data Sources
~~~~~~~~~~~~

The component reads data from the device using two methods:

- **DATA_BUF** (Virtual String 256): Contains real-time radiation data (dose rate, count rate)
  - Parsed from RealTimeData records (eid=0, gid=0)
  - Updated every 5 seconds
- **VSFR** (Virtual Special Function Registers): Direct register reads
  - TEMP_degC (0x8024): Temperature sensor reading
  - Updated every 30 seconds
- **Local Integration**: Accumulated dose calculated by integrating dose_rate over time

Units and Conversions
~~~~~~~~~~~~~~~~~~~~~

- **count_rate**: Raw value in CPS (counts per second)
- **count_rate_cpm**: CPS × 60, reported as integer
- **dose_rate**: Raw value × 10,000,000 = nSv/h
- **dose_accumulated**: Integrated from dose_rate, reported in µSv (microsieverts)
- **temperature**: Read as float from VSFR register in °C

Example Configuration
---------------------

Complete example with OLED display support for ESP32-C3:

.. code-block:: yaml

    esphome:
      name: radiacode-bridge
      friendly_name: RadiaCode BLE Bridge
      on_boot:
        priority: -100
        then:
          - lambda: |-
              id(radiacode_device).set_accumulated_dose(id(stored_dose_nsv));

    esp32:
      board: esp32-c3-devkitm-1
      framework:
        type: esp-idf

    # I2C for OLED display (optional)
    i2c:
      sda: GPIO5
      scl: GPIO6
      scan: true

    logger:
      level: INFO
      logs:
        radiacode_ble: INFO
        ble_client: WARN

    api:
      encryption:
        key: !secret radiacode_api_key

    ota:
      - platform: esphome

    wifi:
      ssid: !secret wifi_ssid
      password: !secret wifi_password

    # Global variable to persist accumulated dose across reboots
    globals:
      - id: stored_dose_nsv
        type: float
        restore_value: true
        initial_value: '0.0'

    esp32_ble_tracker:
      scan_parameters:
        interval: 1100ms
        window: 1100ms
        active: true

    ble_client:
      - mac_address: !secret radiacode_mac_address
        id: radiacode_ble_client

    radiacode_ble:
      id: radiacode_device
      ble_client_id: radiacode_ble_client
      on_connect:
        - lambda: |-
            id(radiacode_device).set_accumulated_dose(id(stored_dose_nsv));

    sensor:
      - platform: radiacode_ble
        radiacode_ble_id: radiacode_device
        dose_rate:
          name: "RadiaCode Dose Rate"
          icon: mdi:radioactive
        count_rate:
          name: "RadiaCode Count Rate"
          icon: mdi:counter
        count_rate_cpm:
          name: "RadiaCode Count Rate CPM"
          icon: mdi:counter
        dose_accumulated:
          name: "RadiaCode Dose Accumulated"
          icon: mdi:radioactive
          on_value:
            - lambda: |-
                id(stored_dose_nsv) = id(radiacode_device).get_accumulated_dose();
        temperature:
          name: "RadiaCode Temperature"
          icon: mdi:thermometer

    # Optional: OLED display (SH1106 72x40)
    font:
      - file: "gfonts://Roboto"
        id: font_medium
        size: 12

    display:
      - platform: ssd1306_i2c
        model: "SH1106_128X64"
        address: 0x3C
        update_interval: 1s
        lambda: |-
          it.fill(COLOR_OFF);
          if (id(count_rate_cpm).has_state()) {
            float cpm = id(count_rate_cpm).state;
            it.printf(30, 22, id(font_medium), "%.0f CPM", cpm);
          }
          if (id(dose_rate).has_state()) {
            float dose = id(dose_rate).state;
            it.printf(30, 35, id(font_medium), "%.0f nSv/h", dose);
          }
          if (id(dose_accumulated).has_state()) {
            it.printf(30, 47, id(font_medium), "%.2fuSv", id(dose_accumulated).state);
          }

    binary_sensor:
      - platform: ble_presence
        mac_address: !secret radiacode_mac_address
        name: "RadiaCode Present"

    button:
      - platform: restart
        name: "Restart Bridge"

      - platform: template
        name: "Connect RadiaCode"
        icon: mdi:bluetooth-connect
        on_press:
          - lambda: |-
              id(radiacode_ble_client)->set_enabled(true);
              id(radiacode_ble_client)->connect();

      - platform: template
        name: "Disconnect RadiaCode"
        icon: mdi:bluetooth-off
        on_press:
          - lambda: |-
              id(radiacode_ble_client)->set_enabled(false);
              id(radiacode_ble_client)->disconnect();

    number:
      - platform: template
        name: "Set Accumulated Dose"
        icon: mdi:radioactive
        unit_of_measurement: "µSv"
        min_value: 0
        max_value: 10000
        step: 0.001
        mode: box
        optimistic: true
        set_action:
          - lambda: |-
              float dose_nsv = x * 1000.0f;
              id(radiacode_device).set_accumulated_dose(dose_nsv);
              id(stored_dose_nsv) = dose_nsv;

    web_server:
      port: 80
      version: 3
      local: true

Troubleshooting
---------------

Connection Issues
~~~~~~~~~~~~~~~~~

- Ensure RadiaCode is powered on and in range
- Check BLE is enabled (device not connected via USB to another application)
- Verify MAC address matches your device (check device label or use BLE scanner)
- Check WiFi signal strength if using wireless logging

No Data
~~~~~~~

- Check logs for "Response timeout" messages
- Verify initialization command succeeded
- Ensure firmware version >= 4.8
- Try restarting both ESP32 and RadiaCode device

Compilation Errors
~~~~~~~~~~~~~~~~~~

- Use esp-idf framework (not Arduino) for better BLE performance
- Ensure ESPHome version >= 2024.6.0
- Check custom_components directory structure is correct

Finding Device MAC Address
~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can find your RadiaCode device MAC address by:

1. Using a BLE scanner app on your phone
2. Looking at the device label (some models)
3. Checking ESPHome logs with ``esp32_ble_tracker`` scan enabled

Performance Notes
~~~~~~~~~~~~~~~~~

- The component uses independent timers for different data types to optimize BLE traffic
- Radiation data updates every 5 seconds for responsive monitoring
- Temperature updates every 30 seconds (less critical, reduces BLE overhead)
- Accumulated dose is calculated locally and reported every 60 seconds
- BLE range is typically 10-30 meters depending on environment
- ESP32-C3 with esp-idf framework provides best BLE stability

Accumulated Dose
~~~~~~~~~~~~~~~~

The accumulated dose sensor integrates the dose_rate over time to calculate total radiation exposure:

- Integration formula: ``accumulated_dose += dose_rate × elapsed_time``
- Persists across reboots using ESPHome's ``globals`` component
- Automatically saved to flash memory every 60 seconds when updated
- Restored on boot/reconnect from saved value
- Can be manually set to any value using the "Set Accumulated Dose" number input
- Reported in µSv (microsieverts)
- Updated every 60 seconds to reduce sensor update frequency

To enable persistence, add a global variable in your configuration:

.. code-block:: yaml

    globals:
      - id: stored_dose_nsv
        type: float
        restore_value: true
        initial_value: '0.0'

    radiacode_ble:
      id: radiacode_device
      ble_client_id: radiacode_ble_client
      on_connect:
        - lambda: |-
            id(radiacode_device).set_accumulated_dose(id(stored_dose_nsv));

    sensor:
      - platform: radiacode_ble
        radiacode_ble_id: radiacode_device
        dose_accumulated:
          name: "Dose Accumulated"
          on_value:
            - lambda: |-
                id(stored_dose_nsv) = id(radiacode_device).get_accumulated_dose();

To manually set the accumulated dose (e.g., to match a known exposure or reset to zero), add a number input (see complete example above).

BLE Connection Control
~~~~~~~~~~~~~~~~~~~~~~~

The component supports manual BLE connection control via buttons:

- **Connect button**: Manually connects and enables auto-reconnect
- **Disconnect button**: Disconnects and disables auto-reconnect until manually reconnected
- **Auto-reconnect**: If connection drops unexpectedly (device out of range, battery low, etc.), it will automatically reconnect when possible
- **Manual disconnect**: When you press the Disconnect button, it stays disconnected until you press Connect

This behavior is achieved using ``set_enabled()`` to control the auto-reconnect feature:

.. code-block:: yaml

    button:
      - platform: template
        name: "Connect RadiaCode"
        on_press:
          - lambda: |-
              id(radiacode_ble_client)->set_enabled(true);
              id(radiacode_ble_client)->connect();

      - platform: template
        name: "Disconnect RadiaCode"
        on_press:
          - lambda: |-
              id(radiacode_ble_client)->set_enabled(false);
              id(radiacode_ble_client)->disconnect();

See Also
--------

- :doc:`/components/ble_client`
- :doc:`/components/sensor/index`
- `RadiaCode Python Library <https://github.com/cdump/radiacode>`__
- `RadiaCode Arduino Library <https://github.com/mkgeiger/RadiaCode>`__
- :apiref:`radiacode_ble/radiacode_component.h`
- :ghedit:`Edit`
