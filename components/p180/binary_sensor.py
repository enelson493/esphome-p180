import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import P180Component

CONF_P180_ID = "p180_id"

# key -> (device_class, cpp_setter)
BINARY_SENSORS = {
    "connected": ("connectivity", "set_connected_binary_sensor"),
    # This is the one you actually want for outage detection - confirmed by
    # direct test (AC input frequency register reads 0 with no grid power)
    "grid_power": ("power", "set_grid_power_binary_sensor"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_P180_ID): cv.use_id(P180Component),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(device_class=dclass)
            for key, (dclass, _setter) in BINARY_SENSORS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_P180_ID])
    for key, (_dclass, setter) in BINARY_SENSORS.items():
        if key in config:
            bsens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(parent, setter)(bsens))
