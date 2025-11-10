import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@radiacode_ble"]

radiacode_ble_ns = cg.esphome_ns.namespace("radiacode_ble")
RadiaCodeBLEComponent = radiacode_ble_ns.class_("RadiaCodeBLEComponent", ble_client.BLEClientNode, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RadiaCodeBLEComponent),
    }
).extend(ble_client.BLE_CLIENT_SCHEMA).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
