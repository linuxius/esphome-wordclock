# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Peter Visser
#
# Part of the WordClock ESPHome component.

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, time
from esphome.core import CORE
from esphome.const import (
    CONF_COLOR,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_TIME_ID,
    CONF_VARIANT,
)

CONF_LANGUAGE = "language"
CONF_ORDER = "order"
CONF_RANDOM_COLORS = "random_colors"
CONF_RGBW = "rgbw"

DEPENDENCIES = ["light", "time"]

wordclock_ns = cg.esphome_ns.namespace("wordclock")
WordClockLight = wordclock_ns.class_(
    "WordClockLight", light.AddressableLight, cg.Component
)
Language = wordclock_ns.enum("Language")
RGBOrder = wordclock_ns.enum("RGBOrder")

VARIANT_RGB = ["WS2812", "WS2812B", "WS2812X", "WS2813"]
VARIANT_RGBW = ["SK6812"]

ORDER_OPTIONS_RGB = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BRG": RGBOrder.ORDER_BRG,
    "BGR": RGBOrder.ORDER_BGR,
}

LANGUAGE_OPTIONS = {
    "en": Language.EN,
    "de": Language.DE,
    "ch": Language.CH,
}

MIN_NUM_LEDS = 132
MAX_NUM_LEDS = 65535


def _validate_hex_color(value):
    value = cv.string_strict(value).strip()
    if value.startswith("#"):
        value = value[1:]

    if len(value) != 6:
        raise cv.Invalid("color must be a 6-digit RGB hex value like FFB428")

    try:
        int(value, 16)
    except ValueError as err:
        raise cv.Invalid("color must be a valid hex value like FFB428") from err

    return value.upper()


def _hex_to_color_expression(value):
    red = int(value[0:2], 16)
    green = int(value[2:4], 16)
    blue = int(value[4:6], 16)
    return cg.RawExpression(f"esphome::Color({red}, {green}, {blue})")


def _resolve_rgb_order(order, rgbw):
    order = cv.string_strict(order).strip().upper()
    if not rgbw:
        if order not in ORDER_OPTIONS_RGB:
            allowed = ", ".join(ORDER_OPTIONS_RGB.keys())
            raise cv.Invalid(f"order '{order}' is invalid. Allowed: {allowed}")
        return order, False

    if len(order) != 4:
        raise cv.Invalid(
            "RGBW order must be one of *RGBW (like GRBW) or WRGB-style (like WGRB)"
        )

    if order.startswith("W"):
        rgb_order = order[1:]
        is_wrgb = True
    elif order.endswith("W"):
        rgb_order = order[:3]
        is_wrgb = False
    else:
        raise cv.Invalid(
            "RGBW order must place W at the start or end (for example GRBW or WGRB)"
        )

    if rgb_order not in ORDER_OPTIONS_RGB:
        allowed_rgb = ", ".join(ORDER_OPTIONS_RGB.keys())
        raise cv.Invalid(f"RGB order '{rgb_order}' is invalid. Allowed RGB cores: {allowed_rgb}")

    return rgb_order, is_wrgb


def _validate_platform(config):
    if not CORE.is_esp32:
        raise cv.Invalid("wordclock requires ESP32 (uses the ESP32 RMT TX driver).")
    return config


CONFIG_SCHEMA_BASE = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WordClockLight),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_NUM_LEDS): cv.int_range(
            min=MIN_NUM_LEDS,
            max=MAX_NUM_LEDS,
        ),
        cv.Optional(CONF_VARIANT): cv.one_of(*(VARIANT_RGB + VARIANT_RGBW), upper=True),
        cv.Optional(CONF_ORDER): cv.string,
        cv.Optional(CONF_RGBW): cv.boolean,
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_LANGUAGE, default="en"): cv.one_of(
            *LANGUAGE_OPTIONS.keys(), lower=True
        ),
        cv.Optional(CONF_COLOR, default="FFB428"): _validate_hex_color,
        cv.Optional(CONF_RANDOM_COLORS, default=False): cv.boolean,
    }
)


def _validate_config(config):
    variant = config.get(CONF_VARIANT)
    rgbw = config.get(CONF_RGBW)

    if rgbw is None:
        if variant in VARIANT_RGBW:
            rgbw = True
        else:
            rgbw = False
        config[CONF_RGBW] = rgbw

    if rgbw:
        if variant is None:
            variant = VARIANT_RGBW[0]
        if variant not in VARIANT_RGBW:
            raise cv.Invalid(
                f"variant '{variant}' is RGB only. Use rgbw: false or a RGBW variant."
            )
    else:
        if variant is None:
            variant = VARIANT_RGB[0]
        if variant not in VARIANT_RGB:
            raise cv.Invalid(
                f"variant '{variant}' is RGBW only. Use rgbw: true or a RGB variant."
            )

    config[CONF_VARIANT] = variant

    order = config.get(CONF_ORDER)
    if order is None:
        order = "GRBW" if rgbw else "GRB"
    rgb_order, is_wrgb = _resolve_rgb_order(order, rgbw)

    config[CONF_ORDER] = order.upper()
    config["_rgb_order"] = rgb_order
    config["_is_wrgb"] = is_wrgb
    config["_is_sk6812"] = variant == "SK6812"
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA_BASE, _validate_config, _validate_platform)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_order(ORDER_OPTIONS_RGB[config["_rgb_order"]]))
    cg.add(var.set_rgbw(config[CONF_RGBW]))
    cg.add(var.set_wrgb(config["_is_wrgb"]))
    cg.add(var.set_sk6812(config["_is_sk6812"]))
    cg.add(var.set_language(LANGUAGE_OPTIONS[config[CONF_LANGUAGE]]))
    cg.add(var.set_color(_hex_to_color_expression(config[CONF_COLOR])))
    cg.add(var.set_random_colors(config[CONF_RANDOM_COLORS]))

    time_id = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_id))
