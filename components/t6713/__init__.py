# import esphome.codegen as cg
# import esphome.config_validation as cv
# from esphome.components import i2c
# from esphome.const import CONF_ID, CONF_SLEEP_DURATION

# DEPENDENCIES = ['i2c']

# CONF_I2C_ADDR = 0x51

# t6713 = cg.esphome_ns.namespace('t6713')
# T6713 = t6713.class_('T6713', cg.Component, i2c.I2CDevice)

# CONFIG_SCHEMA = cv.Schema({
#     cv.GenerateID(): cv.declare_id(T6713),
#     cv.Optional(CONF_SLEEP_DURATION): cv.positive_time_period_seconds,
# }).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(CONF_I2C_ADDR))

# def to_code(config):
#     var = cg.new_Pvariable(config[CONF_ID])
#     if CONF_SLEEP_DURATION in config:
#         cg.add(var.set_sleep_duration(config[CONF_SLEEP_DURATION]))
#     yield cg.register_component(var, config)
#     yield i2c.register_i2c_device(var, config)
    

  
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@skierz"]
DEPENDENCIES = ["uart"]

t6713_ns = cg.esphome_ns.namespace("t6713")
T6713Component = t6713_ns.class_("T6713Component", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(T6713Component),
            cv.Required(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "t6713", baud_rate=19200, require_rx=True, require_tx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))