import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
)
from esphome.cpp_generator import RawExpression  # <-- add this

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["epaper_spi", "spi", "split_buffer"]

CONF_TEMPERATURE_C = "temperature_c"

e2271_ns = cg.esphome_ns.namespace("e2271ks0c1")
epaper_spi_ns = cg.esphome_ns.namespace("epaper_spi")

EPaperBase = epaper_spi_ns.class_(
    "EPaperBase", cg.PollingComponent, spi.SPIDevice, display.Display
)
E2271KS0C1 = e2271_ns.class_("E2271KS0C1", EPaperBase)

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        spi.spi_device_schema(
            default_mode="MODE0",
            default_data_rate=4_000_000,
        )
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(E2271KS0C1),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=30): cv.int_range(1, 255),
            cv.Optional(CONF_TEMPERATURE_C, default=25.0): cv.float_,
        }
    )
)

async def to_code(config):
    # ctor signature: (const char* name, int width, int height, const uint8_t* init, size_t init_len)
    var = cg.new_Pvariable(
        config[CONF_ID],
        "E2271KS0C1",
        264,  # swap: panel is 264 wide in native orientation
        176,  # swap: panel is 176 tall in native orientation
        RawExpression("nullptr"),
        0,
    )

    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_dc_pin(await cg.gpio_pin_expression(config[CONF_DC_PIN])))
    cg.add(var.set_reset_pin(await cg.gpio_pin_expression(config[CONF_RESET_PIN])))
    cg.add(var.set_busy_pin(await cg.gpio_pin_expression(config[CONF_BUSY_PIN])))

    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    cg.add(var.set_temperature_c(config[CONF_TEMPERATURE_C]))

    # Explicitly handle the display lambda
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
