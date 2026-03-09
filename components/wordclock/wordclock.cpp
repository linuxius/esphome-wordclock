// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.

#include "wordclock.h"
#include "wordclock_maps_ch.h"
#include "wordclock_maps_de.h"
#include "wordclock_maps_en.h"
#include "esphome/core/log.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace wordclock {
namespace {

static const char *const TAG = "wordclock";

struct DebugTrace {
  char words[512];
  int minute_dots;
};

int normalize_hour(int hour);

const WordMap &map_for(Language language) {
  switch (language) {
    case DE:
      return kMapDe;
    case CH:
      return kMapCh;
    case EN:
    default:
      return kMapEn;
  }
}

const char *language_name(Language language) {
  switch (language) {
    case DE:
      return "de";
    case CH:
      return "ch";
    case EN:
    default:
      return "en";
  }
}

void init_debug_trace(DebugTrace *trace) {
  if (trace == nullptr) {
    return;
  }
  trace->words[0] = '\0';
  trace->minute_dots = 0;
}

bool has_debug_token(const DebugTrace *trace, const char *token) {
  if (trace == nullptr || token == nullptr || token[0] == '\0') {
    return false;
  }

  const size_t token_len = strlen(token);
  const char *cursor = trace->words;
  while (cursor != nullptr && cursor[0] != '\0') {
    const char *separator = strstr(cursor, ", ");
    const size_t current_len = (separator != nullptr)
                                   ? static_cast<size_t>(separator - cursor)
                                   : strlen(cursor);
    if (current_len == token_len && strncmp(cursor, token, token_len) == 0) {
      return true;
    }
    if (separator == nullptr) {
      break;
    }
    cursor = separator + 2;
  }

  return false;
}

void append_debug_token(DebugTrace *trace, const char *token) {
  if (trace == nullptr || token == nullptr || token[0] == '\0') {
    return;
  }

  if (has_debug_token(trace, token)) {
    return;
  }

  const size_t len = strlen(trace->words);
  if (len >= sizeof(trace->words) - 1) {
    return;
  }

  const char *separator = (len == 0) ? "" : ", ";
  snprintf(trace->words + len, sizeof(trace->words) - len, "%s%s", separator,
           token);
}

void append_debug_word(DebugTrace *trace, const char *label,
                       const WordRange &range) {
  if (trace == nullptr || label == nullptr || !range.valid()) {
    return;
  }

  char token[64];
  snprintf(token, sizeof(token), "%s[%u-%u]", label,
           static_cast<unsigned>(range.start), static_cast<unsigned>(range.end));
  append_debug_token(trace, token);
}

void append_debug_hour(DebugTrace *trace, int hour, const WordRange &range) {
  if (trace == nullptr || !range.valid()) {
    return;
  }

  char label[16];
  snprintf(label, sizeof(label), "HOUR_%d", normalize_hour(hour));
  append_debug_word(trace, label, range);
}

void append_debug_dot(DebugTrace *trace, int dot_index, const WordRange &range) {
  if (trace == nullptr || !range.valid()) {
    return;
  }

  char label[16];
  snprintf(label, sizeof(label), "DOT_%d", dot_index);
  append_debug_word(trace, label, range);
}

void log_debug_trace(const esphome::ESPTime &now, Language language,
                     const DebugTrace &trace) {
  ESP_LOGVV(
      TAG,
      "render %04d-%02d-%02d %02d:%02d:%02d lang=%s words=%s minute_dots=%d",
      now.year, now.month, now.day_of_month, now.hour, now.minute, now.second,
      language_name(language), trace.words[0] != '\0' ? trace.words : "<none>",
      trace.minute_dots);
}

esphome::Color pick_color(const RenderConfig &cfg) {
  if (!cfg.random_colors) {
    return cfg.fixed_color;
  }

  static const esphome::Color kPalette[] = {
      esphome::Color(255, 0, 0),      esphome::Color(0, 255, 0),
      esphome::Color(0, 0, 255),      esphome::Color(255, 255, 0),
      esphome::Color(255, 0, 255),    esphome::Color(0, 255, 255),
      esphome::Color(255, 255, 255),  esphome::Color(165, 42, 42),
  };

  const uint32_t count = sizeof(kPalette) / sizeof(kPalette[0]);
  const uint32_t index = static_cast<uint32_t>(rand()) % count;
  return kPalette[index];
}

void apply_range(esphome::light::AddressableLight &it, const WordRange &range,
                 const esphome::Color &color) {
  if (!range.valid()) {
    return;
  }

  const int size = it.size();
  if (size <= 0 || range.start >= static_cast<uint16_t>(size)) {
    return;
  }

  uint16_t end = range.end;
  if (end >= static_cast<uint16_t>(size)) {
    end = static_cast<uint16_t>(size - 1);
  }

  for (uint16_t i = range.start; i <= end; ++i) {
    it[i] = color;
  }
}

int normalize_hour(int hour) {
  int h = hour % 12;
  if (h == 0) {
    h = 12;
  }
  return h;
}

const WordRange &minutes_word(const WordMap &map, int minute) {
  if (minute < 5) {
    return map.oclock;
  } else if (minute < 10) {
    return map.five;
  } else if (minute < 15) {
    return map.ten;
  } else if (minute < 20) {
    return map.fifteen;
  } else if (minute < 25) {
    return map.twenty;
  } else if (minute < 30) {
    return map.twentyfive;
  } else if (minute < 35) {
    return map.thirty;
  } else if (minute < 40) {
    return map.twentyfive;
  } else if (minute < 45) {
    return map.twenty;
  } else if (minute < 50) {
    return map.fifteen;
  } else if (minute < 55) {
    return map.ten;
  } else {
    return map.five;
  }
}

const char *minute_word_label(int minute) {
  if (minute < 5) {
    return "OCLOCK";
  } else if (minute < 10) {
    return "FIVE";
  } else if (minute < 15) {
    return "TEN";
  } else if (minute < 20) {
    return "FIFTEEN";
  } else if (minute < 25) {
    return "TWENTY";
  } else if (minute < 30) {
    return "TWENTYFIVE";
  } else if (minute < 35) {
    return "THIRTY";
  } else if (minute < 40) {
    return "TWENTYFIVE";
  } else if (minute < 45) {
    return "TWENTY";
  } else if (minute < 50) {
    return "FIFTEEN";
  } else if (minute < 55) {
    return "TEN";
  } else {
    return "FIVE";
  }
}

const WordRange &hour_word(const WordMap &map, int hour) {
  const int h = normalize_hour(hour);
  return map.hours[h - 1];
}

void apply_minute_dots(esphome::light::AddressableLight &it, const WordMap &map,
                       int minute, const RenderConfig &cfg,
                       DebugTrace *trace) {
  const int remainder = minute % 5;
  if (trace != nullptr) {
    trace->minute_dots = remainder;
  }

  if (remainder <= 0) {
    return;
  }

  for (int i = 0; i < remainder && i < 4; ++i) {
    const WordRange &dot = map.dots[i];
    apply_range(it, dot, pick_color(cfg));
    append_debug_dot(trace, i + 1, dot);
  }
}

void render_english(esphome::light::AddressableLight &it, const WordMap &map,
                    int hour, int minute, const RenderConfig &cfg,
                    DebugTrace *trace) {
  int display_hour = normalize_hour(hour);

  if (minute < 5) {
    apply_range(it, map.oclock, pick_color(cfg));
    append_debug_word(trace, "OCLOCK", map.oclock);
  } else if (minute < 35) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  } else {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
    display_hour = normalize_hour(hour + 1);
  }

  const WordRange &minute_range = minutes_word(map, minute);
  const WordRange &hour_range = hour_word(map, display_hour);
  apply_range(it, minute_range, pick_color(cfg));
  apply_range(it, hour_range, pick_color(cfg));

  append_debug_word(trace, minute_word_label(minute), minute_range);
  append_debug_hour(trace, display_hour, hour_range);
}

void render_swiss(esphome::light::AddressableLight &it, const WordMap &map,
                  int hour, int minute, const RenderConfig &cfg,
                  DebugTrace *trace) {
  int display_hour = normalize_hour(hour);

  if (minute < 5) {
    apply_range(it, map.oclock, pick_color(cfg));
    append_debug_word(trace, "OCLOCK", map.oclock);
  } else if (minute < 25) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
  } else if (minute < 30) {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
    apply_range(it, map.five, pick_color(cfg));
    append_debug_word(trace, "FIVE", map.five);
  } else if (minute < 35) {
    // no extra words
  } else if (minute < 40) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
    apply_range(it, map.five, pick_color(cfg));
    append_debug_word(trace, "FIVE", map.five);
  } else {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  }

  if (minute >= 25) {
    display_hour = normalize_hour(hour + 1);
  }

  const WordRange &minute_range = minutes_word(map, minute);
  const WordRange &hour_range = hour_word(map, display_hour);
  apply_range(it, minute_range, pick_color(cfg));
  apply_range(it, hour_range, pick_color(cfg));

  append_debug_word(trace, minute_word_label(minute), minute_range);
  append_debug_hour(trace, display_hour, hour_range);

  apply_minute_dots(it, map, minute, cfg, trace);
}

void render_german(esphome::light::AddressableLight &it, const WordMap &map,
                   int hour, int minute, const RenderConfig &cfg,
                   DebugTrace *trace) {
  int display_hour = normalize_hour(hour);

  if (minute < 5) {
    apply_range(it, map.oclock, pick_color(cfg));
    append_debug_word(trace, "OCLOCK", map.oclock);
  } else if (minute < 15) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  } else if (minute < 20) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
  } else if (minute < 25) {
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  } else if (minute < 30) {
    apply_range(it, map.five, pick_color(cfg));
    append_debug_word(trace, "FIVE", map.five);
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
  } else if (minute < 35) {
    apply_range(it, map.half, pick_color(cfg));
    append_debug_word(trace, "HALF", map.half);
  } else if (minute < 40) {
    apply_range(it, map.five, pick_color(cfg));
    append_debug_word(trace, "FIVE", map.five);
    apply_range(it, map.past, pick_color(cfg));
    append_debug_word(trace, "PAST", map.past);
  } else if (minute < 45) {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  } else if (minute < 50) {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
  } else {
    apply_range(it, map.to, pick_color(cfg));
    append_debug_word(trace, "TO", map.to);
    apply_range(it, map.minutes, pick_color(cfg));
    append_debug_word(trace, "MINUTES", map.minutes);
  }

  if (minute >= 25) {
    display_hour = normalize_hour(hour + 1);
  }

  if (display_hour == 1 && minute < 5 && map.hour_0.valid()) {
    apply_range(it, map.hour_0, pick_color(cfg));
    append_debug_word(trace, "HOUR_0", map.hour_0);
  } else {
    const WordRange &hour_range = hour_word(map, display_hour);
    apply_range(it, hour_range, pick_color(cfg));
    append_debug_hour(trace, display_hour, hour_range);
  }

  const WordRange &minute_range = minutes_word(map, minute);
  apply_range(it, minute_range, pick_color(cfg));
  append_debug_word(trace, minute_word_label(minute), minute_range);
}

}  // namespace

bool Renderer::render(esphome::light::AddressableLight &it, const esphome::ESPTime &now,
                      const RenderConfig &cfg, Language language) {
  if (!now.is_valid()) {
    return false;
  }

  const int hour = now.hour;
  const int minute = now.minute;

  if (minute == last_minute_ && hour == last_hour_) {
    return false;
  }

  last_minute_ = minute;
  last_hour_ = hour;

  it.all() = esphome::Color(0, 0, 0);

  const WordMap &map = map_for(language);
  DebugTrace trace;
  init_debug_trace(&trace);
  apply_range(it, map.it, pick_color(cfg));
  append_debug_word(&trace, "IT", map.it);
  apply_range(it, map.is, pick_color(cfg));
  append_debug_word(&trace, "IS", map.is);

  switch (language) {
    case DE:
      render_german(it, map, hour, minute, cfg, &trace);
      break;
    case CH:
      render_swiss(it, map, hour, minute, cfg, &trace);
      break;
    case EN:
    default:
      render_english(it, map, hour, minute, cfg, &trace);
      break;
  }

  log_debug_trace(now, language, trace);

  return true;
}

}  // namespace wordclock
