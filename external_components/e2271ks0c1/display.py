import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_ID,
    CONF_RESET_PIN,
    CONF_UPDATE_INTERVAL,
)

e2271_ns = cg.esphome_ns.namespace("e2271ks0c1")
E2271KS0C1 = e2271_ns.class_("E2271KS0C1", display.DisplayBuffer)

CONF_CLK_PIN = "clk_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_SPI_HOST = "spi_host"
CONF_OTP_CLOCK_HZ = "otp_clock_hz"
CONF_WRITE_CLOCK_HZ = "write_clock_hz"
CONF_FULL_UPDATE_EVERY = "full_update_every"
CONF_TEMPERATURE_C = "temperature_c"

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(E2271KS0C1),
        cv.Required(CONF_CLK_PIN): cv.int_,
        cv.Required(CONF_MOSI_PIN): cv.int_,
        cv.Required(CONF_CS_PIN): cv.int_,
        cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_SPI_HOST, default="SPI2_HOST"): cv.one_of("SPI2_HOST", "SPI3_HOST", upper=True),
        cv.Optional(CONF_OTP_CLOCK_HZ, default=4_000_000): cv.int_range(min=100_000, max=10_000_000),
        cv.Optional(CONF_WRITE_CLOCK_HZ, default=10_000_000): cv.int_range(min=100_000, max=20_000_000),
        cv.Optional(CONF_FULL_UPDATE_EVERY, default=120): cv.int_range(min=1, max=10000),
        cv.Optional(CONF_TEMPERATURE_C, default=25.0): cv.float_,
        cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # IMPORTANT: register_display already registers the component
    await display.register_display(var, config)

    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_mosi_pin(config[CONF_MOSI_PIN]))
    cg.add(var.set_cs_pin(config[CONF_CS_PIN]))
    cg.add(var.set_dc_pin(await cg.gpio_pin_expression(config[CONF_DC_PIN])))
    cg.add(var.set_reset_pin(await cg.gpio_pin_expression(config[CONF_RESET_PIN])))
    cg.add(var.set_busy_pin(await cg.gpio_pin_expression(config[CONF_BUSY_PIN])))

    cg.add(var.set_spi_host(config[CONF_SPI_HOST]))
    cg.add(var.set_otp_clock_hz(config[CONF_OTP_CLOCK_HZ]))
    cg.add(var.set_write_clock_hz(config[CONF_WRITE_CLOCK_HZ]))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    cg.add(var.set_temperature_c(config[CONF_TEMPERATURE_C]))
