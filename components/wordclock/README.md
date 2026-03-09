# WordClock External Component

ESPHome addressable light platform for a 132-LED word clock face with language-aware time rendering.

## Highlights

- Renders word-based time phrases in `en`, `de`, and `ch` (Swiss German layout).
- Supports RGB strips (`WS2812`, `WS2812B`, `WS2812X`, `WS2813`) and RGBW strips (`SK6812`).
- Supports configurable color order for RGB and RGBW/WRGB wiring.
- Includes optional random-color mode and rainbow overlay mode.
- Includes LED mapping test and periodic/manual splash animation helpers.

## Status

Only the Swiss German layout (`ch`) has been tested on hardware so far.
The English (`en`) and German (`de`) layouts are included, but may not work
correctly yet.

## Requirements

- ESP32 target only.
- LED count must be between `132` and `65535` (map requires at least 132 LEDs).
- A valid ESPHome `time` source (`time_id`) is required.

## Tested With

- ESPHome `2026.2.4`
- Board profile: `esp32dev`
- Framework: ESP-IDF (with built-in `esp_driver_rmt` component enabled)

## Install

Use ESPHome `external_components` and point to the folder that contains this component:

```yaml
external_components:
  - source:
      type: local
      path: ./external_components
```

## Minimal Configuration

```yaml
light:
  - platform: wordclock
    name: "Word Clock"
    id: wordclock_light
    output_id: wordclock_light_output
    pin: GPIO19
    num_leds: 132
    time_id: my_time
    language: ch
    variant: WS2812
    rgbw: false
    order: GRB
    color: FFB428
    random_colors: false
    default_transition_length: 0s
```

## Configuration Options

- `output_id` (Optional): ID for accessing the output object in lambdas.
  Required only if you want to call runtime methods from lambdas.
- `pin` (Required): Data pin for the LED strip.
- `num_leds` (Required): Total strip LEDs. Must be `>=132`.
- `time_id` (Required): ID of a `time:` component.
- `language` (Optional, default `en`): `en`, `de`, or `ch`.
- `variant` (Optional): `WS2812`, `WS2812B`, `WS2812X`, `WS2813`, `SK6812`.
- `rgbw` (Optional): Set `true` for RGBW strips (normally with `SK6812`).
- `order` (Optional): RGB order (`RGB`, `RBG`, `GRB`, `GBR`, `BRG`, `BGR`).
  For RGBW, use forms with `W` at start or end (example: `GRBW`, `WGRB`).
- `color` (Optional, default `FFB428`): Fixed color as 6-digit RGB hex.
- `random_colors` (Optional, default `false`): Random per-word colors.

### Auto Defaults

- If `rgbw` is omitted:
  - `true` when `variant: SK6812`
  - `false` otherwise
- If `variant` is omitted:
  - `SK6812` when `rgbw: true`
  - `WS2812` when `rgbw: false`
- If `order` is omitted:
  - `GRBW` for RGBW
  - `GRB` for RGB

## Runtime API (Lambda)

Use `output_id` to call runtime controls:

```cpp
id(wordclock_light_output).set_render_brightness(0.40f);          // 0.0 to 1.0
id(wordclock_light_output).set_color(esphome::Color(255,180,40)); // fixed color
id(wordclock_light_output).set_random_colors(false);
id(wordclock_light_output).set_rainbow_mode(false);
id(wordclock_light_output).set_language(wordclock::CH);            // EN, DE, CH
id(wordclock_light_output).set_five_minute_splash_enabled(true);
id(wordclock_light_output).set_splash_interval_minutes(5);        // 1, 5, 15, 60
id(wordclock_light_output).trigger_five_minute_splash();          // manual splash
id(wordclock_light_output).start_led_test();                      // mapping test
id(wordclock_light_output).stop_led_test();
```

## Notes

- The text matrix is mapped to 120 LEDs starting at index `12` (12x10 layout).
- Swiss map uses minute dots for `+1..+4` minutes.
- If your board/framework needs explicit RMT component inclusion, ensure your ESP-IDF setup includes `esp_driver_rmt`.

## License and Provenance

- SPDX license for this component: `GPL-3.0-or-later`.
- Parts of the low-level LED output path in `wordclock_light.cpp/.h` were adapted
  from ESPHome's `esp32_rmt_led_strip` implementation.
- Repository-level attribution details: `../../NOTICE.md`.
