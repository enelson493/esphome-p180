import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import ble_client

CODEOWNERS = ["@you"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor"]
MULTI_CONF = True

p180_ns = cg.esphome_ns.namespace("p180")
P180Component = p180_ns.class_("P180Component", cg.Component, ble_client.BLEClientNode)

CONF_POLLING_INTERVAL = "polling_interval"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(P180Component),
            cv.Optional(CONF_POLLING_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_polling_interval(config[CONF_POLLING_INTERVAL]))
