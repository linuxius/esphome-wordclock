// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.

#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/core/color.h"
#include "esphome/core/time.h"
#include <cstdint>

namespace wordclock {

constexpr uint16_t kInvalidIndex = 0xFFFF;

enum Language : uint8_t {
  EN = 0,
  DE = 1,
  CH = 2,
};

struct WordRange {
  uint16_t start;
  uint16_t end;

  constexpr bool valid() const {
    return start != kInvalidIndex && end != kInvalidIndex && start <= end;
  }
};

struct WordMap {
  WordRange it;
  WordRange is;
  WordRange oclock;
  WordRange past;
  WordRange to;
  WordRange minutes;
  WordRange half;
  WordRange five;
  WordRange ten;
  WordRange fifteen;
  WordRange twenty;
  WordRange twentyfive;
  WordRange thirty;
  WordRange hour_0;
  WordRange hours[12];
  WordRange dots[4];
};

struct RenderConfig {
  bool random_colors = false;
  esphome::Color fixed_color = esphome::Color(255, 180, 40);
};

class Renderer {
 public:
  bool render(esphome::light::AddressableLight &it, const esphome::ESPTime &now,
              const RenderConfig &cfg, Language language);

  void reset() {
    last_minute_ = -1;
    last_hour_ = -1;
  }

 private:
  int last_minute_ = -1;
  int last_hour_ = -1;
};

}  // namespace wordclock
