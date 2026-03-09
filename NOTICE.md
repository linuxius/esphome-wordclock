## Third-Party Attribution

This project includes code adapted from:

- ESPHome `esp32_rmt_led_strip` component
  - Project: https://github.com/esphome/esphome
  - License: GPL-3.0-or-later for C++ runtime components

The adapted code is primarily in:

- `components/wordclock/wordclock_light.cpp`
- `components/wordclock/wordclock_light.h`

Modifications include integration with WordClock rendering, color/language
controls, LED mapping test, and splash animation behavior.

At the time of preparing this publish copy, no direct source copy from WLED was
identified in this repository snapshot. If WLED-derived code is added later,
add corresponding attribution and license compliance notes here.
