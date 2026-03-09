// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.
// Portions of this file were adapted from ESPHome's
// esp32_rmt_led_strip component.

#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "wordclock.h"

#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <esp_err.h>

namespace wordclock {

enum RGBOrder : uint8_t {
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

struct LedParams {
  rmt_symbol_word_t bit0;
  rmt_symbol_word_t bit1;
  rmt_symbol_word_t reset;
};

class WordClockLight : public esphome::light::AddressableLight {
 public:
  void set_pin(esphome::InternalGPIOPin *pin) { pin_ = pin; }
  void set_num_leds(uint16_t count) { num_leds_ = count; }
  void set_time(esphome::time::RealTimeClock *time) { time_ = time; }
  void set_order(RGBOrder order) { order_ = order; }
  void set_rgbw(bool rgbw) { rgbw_ = rgbw; }
  void set_wrgb(bool wrgb) { wrgb_ = wrgb; }
  void set_sk6812(bool sk6812) { sk6812_ = sk6812; }
  void set_color(esphome::Color color) {
    color_ = color;
    trigger_refresh_();
  }
  void set_random_colors(bool random) {
    random_colors_ = random;
    trigger_refresh_();
  }
  void set_five_minute_splash_enabled(bool enabled) {
    five_minute_splash_enabled_ = enabled;
  }
  void set_splash_interval_minutes(uint8_t minutes) {
    switch (minutes) {
      case 1:
      case 5:
      case 15:
      case 60:
        splash_interval_minutes_ = minutes;
        break;
      default:
        splash_interval_minutes_ = 5;
        break;
    }
  }
  void trigger_five_minute_splash() { five_minute_splash_requested_ = true; }
  void set_rainbow_mode(bool rainbow_mode) {
    rainbow_mode_ = rainbow_mode;
    trigger_refresh_();
  }
  void set_language(wordclock::Language language) {
    language_ = language;
    trigger_refresh_();
  }
  void set_render_brightness(float brightness) {
    if (brightness < 0.0f) {
      brightness = 0.0f;
    } else if (brightness > 1.0f) {
      brightness = 1.0f;
    }
    render_brightness_ = brightness;
    trigger_refresh_();
  }
  void start_led_test();
  void stop_led_test();

  void setup() override;
  void dump_config() override;
  void write_state(esphome::light::LightState *state) override;
  float get_setup_priority() const override;
  int32_t size() const override { return this->num_leds_; }

  esphome::light::LightTraits get_traits() override {
    auto traits = esphome::light::LightTraits();
    if (this->rgbw_ || this->wrgb_) {
      traits.set_supported_color_modes(
          {esphome::light::ColorMode::RGB_WHITE,
           esphome::light::ColorMode::WHITE});
    } else {
      traits.set_supported_color_modes({esphome::light::ColorMode::RGB});
    }
    return traits;
  }

  void clear_effect_data() override {
    if (this->effect_data_ == nullptr) {
      return;
    }
    for (int i = 0; i < this->size(); i++) {
      this->effect_data_[i] = 0;
    }
  }

 protected:
  esphome::light::ESPColorView get_view_internal(int32_t index) const override;
  size_t get_buffer_size_() const {
    return this->num_leds_ * (this->rgbw_ || this->wrgb_ ? 4 : 3);
  }
  void clear_frame_();
  bool render_current_frame_(bool force);
  void apply_rainbow_to_lit_pixels_();
  esphome::Color rainbow_color_for_index_(uint16_t index) const;
  void run_five_minute_splash_();
  void show_five_minute_splash_frame_(uint8_t phase, uint8_t corner);
  bool index_to_matrix_xy_(uint16_t index, uint8_t *x, uint8_t *y) const;
  uint8_t corner_metric_(uint8_t x, uint8_t y, uint8_t corner) const;
  void show_led_test_step_();
  esphome::Color led_test_color_() const;
  void update_();
  void trigger_refresh_();
  void set_led_params_(uint32_t bit0_high, uint32_t bit0_low,
                       uint32_t bit1_high, uint32_t bit1_low,
                       uint32_t reset_time_high, uint32_t reset_time_low);

  esphome::InternalGPIOPin *pin_{nullptr};
  uint8_t pin_num_{0};
  uint16_t num_leds_{0};

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  LedParams params_{};
  rmt_channel_handle_t channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
  rmt_symbol_word_t *rmt_buf_{nullptr};
  uint32_t rmt_symbols_{256};

  RGBOrder order_{ORDER_GRB};
  bool rgbw_{false};
  bool wrgb_{false};
  bool sk6812_{false};
  uint32_t max_refresh_rate_us_{0};
  uint32_t last_refresh_{0};

  esphome::time::RealTimeClock *time_{nullptr};
  wordclock::Renderer renderer_{};
  wordclock::Language language_{wordclock::EN};
  esphome::Color color_{255, 180, 40};
  float render_brightness_{1.0f};
  bool random_colors_{false};
  bool rainbow_mode_{false};
  bool is_on_{true};
  bool led_test_active_{false};
  uint16_t led_test_index_{0};
  uint8_t led_test_color_phase_{0};
  uint32_t last_clock_render_ms_{0};
  uint32_t last_led_test_step_ms_{0};
  uint32_t last_render_request_ms_{0};
  bool five_minute_splash_enabled_{false};
  bool five_minute_splash_requested_{false};
  uint8_t splash_interval_minutes_{5};
  uint8_t five_minute_splash_corner_{0};
  bool five_minute_splash_active_{false};
  uint8_t five_minute_splash_active_corner_{0};
  uint8_t five_minute_splash_phase_{0};
  uint8_t five_minute_splash_final_phase_{0};
  uint32_t five_minute_splash_next_frame_ms_{0};
  int32_t last_five_minute_splash_key_{-1};
};

}  // namespace wordclock
