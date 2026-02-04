"""Custom ESPHome component for Victron VE.Direct Charge Current Limit (Register 0x2015)."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, number
from esphome.const import (
    CONF_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    UNIT_AMPERE,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["number"]

CONF_UART_ID = "uart_id"

victron_charge_limit_ns = cg.esphome_ns.namespace("victron_charge_limit")
VictronChargeLimit = victron_charge_limit_ns.class_(
    "VictronChargeLimit", number.Number, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    number.number_schema(VictronChargeLimit)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(VictronChargeLimit),
            cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=100.0): cv.float_,
            cv.Optional(CONF_STEP, default=0.1): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
