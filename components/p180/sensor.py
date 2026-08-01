import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from . import P180Component

CONF_P180_ID = "p180_id"

# Register map confirmed by diffing an AC-connected dump against an on-battery
# dump on a real P180 (Aug 2026). key -> (unit, accuracy_decimals, device_class, cpp_setter)
SENSORS = {
    "ac_in_voltage": ("V", 1, "voltage", "set_ac_in_voltage_sensor"),
    "ac_in_frequency": ("Hz", 2, "frequency", "set_ac_in_frequency_sensor"),
    "ac_out_voltage": ("V", 1, "voltage", "set_ac_out_voltage_sensor"),
    "ac_out_frequency": ("Hz", 1, "frequency", "set_ac_out_frequency_sensor"),
    "output_power": ("W", 0, "power", "set_output_power_sensor"),
    "battery_discharge_power": ("W", 0, "power", "set_battery_discharge_power_sensor"),
    "battery_percent": ("%", 0, "battery", "set_battery_percent_sensor"),
    "remaining_time": ("min", 0, "duration", "set_remaining_time_sensor"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_P180_ID): cv.use_id(P180Component),
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=unit,
                accuracy_decimals=accuracy,
                device_class=dclass,
                state_class="measurement",
            )
            for key, (unit, accuracy, dclass, _setter) in SENSORS.items()
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_P180_ID])
    for key, (_unit, _accuracy, _dclass, setter) in SENSORS.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(parent, setter)(sens))
