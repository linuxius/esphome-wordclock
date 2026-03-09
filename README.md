# WordClock ESPHome External Component

External component for ESPHome that renders a 132-LED word clock in:

- `en` (English)
- `de` (German)
- `ch` (Swiss German)

## Status

Only the Swiss German layout (`ch`) has been tested on hardware so far.
The English (`en`) and German (`de`) layouts are included, but may not work
correctly yet.

Only RGB `WS2812B` has been tested on hardware so far.
RGBW (for example `SK6812`) is supported in configuration, but has not been
validated on hardware yet.

## Repository Layout

- `components/wordclock/` component source

## License

This repository is published as `GPL-3.0-or-later`.

Reason: parts of the C++ LED driver path were adapted from ESPHome's
`esp32_rmt_led_strip` component. See `NOTICE.md` for attribution.

Additional metadata:

- License text: `LICENSE`
- Copyright/summary: `COPYRIGHT`
- Third-party attribution: `NOTICE.md`

## Development Note

Parts of this repository were developed with AI assistance and then reviewed,
tested, and curated by a human maintainer before publication.

## Install (GitHub Source)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/<your-user>/<your-repo>.git
      ref: main
    components: [wordclock]
```

## Minimal Usage

```yaml
light:
  - platform: wordclock
    name: "Word Clock"
    output_id: wordclock_light_output
    pin: GPIO19
    num_leds: 132
    time_id: ha_time
    language: ch
```

For full configuration and runtime API methods, see:
`components/wordclock/README.md`
