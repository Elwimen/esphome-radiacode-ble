import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
)

from . import RadiaCodeBLEComponent, radiacode_ble_ns

CONF_RADIACODE_BLE_ID = "radiacode_ble_id"
CONF_DOSE_RATE = "dose_rate"
CONF_COUNT_RATE = "count_rate"
CONF_COUNT_RATE_CPM = "count_rate_cpm"
CONF_DOSE_ACCUMULATED = "dose_accumulated"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RADIACODE_BLE_ID): cv.use_id(RadiaCodeBLEComponent),
        cv.Optional(CONF_DOSE_RATE): sensor.sensor_schema(
            unit_of_measurement="nSv/h",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COUNT_RATE): sensor.sensor_schema(
            unit_of_measurement="cps",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COUNT_RATE_CPM): sensor.sensor_schema(
            unit_of_measurement="cpm",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DOSE_ACCUMULATED): sensor.sensor_schema(
            unit_of_measurement="µSv",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RADIACODE_BLE_ID])

    if CONF_DOSE_RATE in config:
        sens = await sensor.new_sensor(config[CONF_DOSE_RATE])
        cg.add(parent.set_dose_rate_sensor(sens))

    if CONF_COUNT_RATE in config:
        sens = await sensor.new_sensor(config[CONF_COUNT_RATE])
        cg.add(parent.set_count_rate_sensor(sens))

    if CONF_COUNT_RATE_CPM in config:
        sens = await sensor.new_sensor(config[CONF_COUNT_RATE_CPM])
        cg.add(parent.set_count_rate_cpm_sensor(sens))

    if CONF_DOSE_ACCUMULATED in config:
        sens = await sensor.new_sensor(config[CONF_DOSE_ACCUMULATED])
        cg.add(parent.set_dose_accumulated_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(parent.set_temperature_sensor(sens))
