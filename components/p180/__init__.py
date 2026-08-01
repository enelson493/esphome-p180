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
CONF_BATTERY_CAPACITY_WH = "battery_capacity_wh"
CONF_BATTERY_EFFICIENCY = "battery_efficiency"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(P180Component),
            cv.Optional(CONF_POLLING_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_BATTERY_CAPACITY_WH, default=1024.0): cv.float_range(min=1.0),
            cv.Optional(CONF_BATTERY_EFFICIENCY, default=0.85): cv.percentage,
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
    cg.add(var.set_battery_capacity_wh(config[CONF_BATTERY_CAPACITY_WH]))
    cg.add(var.set_battery_efficiency(config[CONF_BATTERY_EFFICIENCY]))
