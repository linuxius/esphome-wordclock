// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Peter Visser
//
// Part of the WordClock ESPHome component.
// Portions of this file were adapted from ESPHome's
// esp32_rmt_led_strip component.

#include "wordclock_light.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace wordclock {

static const char *const TAG = "wordclock";

#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_RESOLUTION_HZ = 32000000;
#else
static const uint32_t RMT_RESOLUTION_HZ = 40000000;
#endif

static const size_t RMT_SYMBOLS_PER_BYTE = 8;
static const uint32_t WS2812_RESET_NS = 300000;
static const uint32_t SK6812_RESET_NS = 120000;
static const uint32_t WS2812_INTER_FRAME_GAP_US = 300;
static const uint32_t SK6812_INTER_FRAME_GAP_US = 120;
static const uint16_t TEXT_MATRIX_START_INDEX = 12;
static const uint8_t TEXT_MATRIX_COLS = 12;
static const uint8_t TEXT_MATRIX_ROWS = 10;
// Physical wiring is serpentine and the first text row (after the 4-dot row)
// runs right-to-left.
static const bool TEXT_MATRIX_FIRST_ROW_REVERSED = true;
static const uint16_t TEXT_MATRIX_LED_COUNT =
    TEXT_MATRIX_COLS * TEXT_MATRIX_ROWS;
static const uint32_t SPLASH_FRAME_INTERVAL_MS = 70;
static const uint32_t LED_TEST_STEP_INTERVAL_MS = 500;
static const uint8_t SPLASH_TRAIL_LENGTH = 3;

void WordClockLight::set_led_params_(uint32_t bit0_high, uint32_t bit0_low,
                                     uint32_t bit1_high, uint32_t bit1_low,
                                     uint32_t reset_time_high,
                                     uint32_t reset_time_low) {
  const uint32_t ns_per_tick = 1000000000UL / RMT_RESOLUTION_HZ;
  this->params_.bit0.level0 = 1;
  this->params_.bit0.duration0 =
      uint16_t(bit0_high / ns_per_tick);
  this->params_.bit0.level1 = 0;
  this->params_.bit0.duration1 =
      uint16_t(bit0_low / ns_per_tick);

  this->params_.bit1.level0 = 1;
  this->params_.bit1.duration0 =
      uint16_t(bit1_high / ns_per_tick);
  this->params_.bit1.level1 = 0;
  this->params_.bit1.duration1 =
      uint16_t(bit1_low / ns_per_tick);

  this->params_.reset.level0 = 0;
  this->params_.reset.duration0 =
      uint16_t(reset_time_high / ns_per_tick);
  this->params_.reset.level1 = 0;
  this->params_.reset.duration1 =
      uint16_t(reset_time_low / ns_per_tick);
}

void WordClockLight::setup() {
  if (this->pin_ == nullptr) {
    ESP_LOGE(TAG, "Pin not set");
    this->mark_failed();
    return;
  }
  if (this->num_leds_ == 0) {
    ESP_LOGE(TAG, "num_leds must be > 0");
    this->mark_failed();
    return;
  }

  this->pin_->setup();
  this->pin_num_ = this->pin_->get_pin();

  esphome::RAMAllocator<uint8_t> allocator(
      esphome::RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  this->buf_ = allocator.allocate(this->get_buffer_size_());
  this->effect_data_ = allocator.allocate(this->num_leds_);
  size_t rmt_buffer_symbols = this->get_buffer_size_() * RMT_SYMBOLS_PER_BYTE + 1;
  this->rmt_buf_ = reinterpret_cast<decltype(this->rmt_buf_)>(
      allocator.allocate(rmt_buffer_symbols * sizeof(*this->rmt_buf_)));
  if (this->buf_ == nullptr || this->effect_data_ == nullptr ||
      this->rmt_buf_ == nullptr) {
    ESP_LOGE(TAG,
             "Could not allocate buffers (pixel=%u, effect=%u, rmt_symbols=%u)",
             static_cast<unsigned>(this->get_buffer_size_()),
             static_cast<unsigned>(this->num_leds_),
             static_cast<unsigned>(rmt_buffer_symbols));
    this->mark_failed();
    return;
  }
  memset(this->buf_, 0, this->get_buffer_size_());
  memset(this->effect_data_, 0, this->num_leds_);
  memset(this->rmt_buf_, 0, rmt_buffer_symbols * sizeof(*this->rmt_buf_));

  if (this->sk6812_) {
    this->set_led_params_(300, 900, 600, 600, 0, SK6812_RESET_NS);
  } else {
    this->set_led_params_(400, 850, 800, 450, 0, WS2812_RESET_NS);
  }

  rmt_tx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = RMT_RESOLUTION_HZ;
  channel.gpio_num = gpio_num_t(this->pin_num_);
  channel.trans_queue_depth = 1;
  channel.flags.io_loop_back = 0;
  channel.flags.io_od_mode = 0;
  channel.flags.invert_out = 0;
  channel.flags.with_dma = false;
  channel.intr_priority = 0;
  esp_err_t err = ESP_FAIL;
  const uint32_t symbol_candidates[] = {256, 192, 128, 64};
  bool channel_ready = false;
  for (size_t i = 0; i < sizeof(symbol_candidates) / sizeof(symbol_candidates[0]);
       i++) {
    channel.mem_block_symbols = symbol_candidates[i];
    err = rmt_new_tx_channel(&channel, &this->channel_);
    if (err == ESP_OK) {
      this->rmt_symbols_ = symbol_candidates[i];
      channel_ready = true;
      break;
    }
  }
  if (!channel_ready) {
    ESP_LOGE(TAG,
             "Cannot initialize RMT channel (tried 256/192/128/64 symbols): %s (%d)",
             esp_err_to_name(err), static_cast<int>(err));
    this->mark_failed();
    return;
  }

  rmt_copy_encoder_config_t encoder{};
  err = rmt_new_copy_encoder(&encoder, &this->encoder_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot initialize RMT encoder: %s (%d)",
             esp_err_to_name(err), static_cast<int>(err));
    this->mark_failed();
    return;
  }

  err = rmt_enable(this->channel_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Cannot enable RMT channel: %s (%d)",
             esp_err_to_name(err), static_cast<int>(err));
    this->mark_failed();
    return;
  }

  this->set_interval(50, [this]() { this->update_(); });
}

void WordClockLight::dump_config() {
  ESP_LOGCONFIG(TAG, "WordClock:");
  ESP_LOGCONFIG(TAG, "  Pin: GPIO%" PRIu8, this->pin_num_);
  ESP_LOGCONFIG(TAG, "  LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  RGBW: %s", this->rgbw_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  White first (WRGB): %s", this->wrgb_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Variant: %s", this->sk6812_ ? "SK6812" : "WS2812");
  ESP_LOGCONFIG(TAG, "  RMT mem symbols: %" PRIu32, this->rmt_symbols_);
  ESP_LOGCONFIG(TAG, "  Random colors: %s", this->random_colors_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Rainbow mode: %s", this->rainbow_mode_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Splash auto: %s",
                this->five_minute_splash_enabled_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Splash interval: %u min",
                static_cast<unsigned>(this->splash_interval_minutes_));
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Initialization failed");
  }
}

esphome::light::ESPColorView WordClockLight::get_view_internal(int32_t index) const {
  uint8_t multiplier = this->rgbw_ || this->wrgb_ ? 4 : 3;

  uint8_t red;
  uint8_t green;
  uint8_t blue;
  switch (this->order_) {
    case ORDER_RGB:
      red = 0;
      green = 1;
      blue = 2;
      break;
    case ORDER_RBG:
      red = 0;
      green = 2;
      blue = 1;
      break;
    case ORDER_GRB:
      red = 1;
      green = 0;
      blue = 2;
      break;
    case ORDER_GBR:
      red = 2;
      green = 0;
      blue = 1;
      break;
    case ORDER_BGR:
      red = 2;
      green = 1;
      blue = 0;
      break;
    case ORDER_BRG:
    default:
      red = 1;
      green = 2;
      blue = 0;
      break;
  }

  uint8_t white = this->wrgb_ ? 0 : 3;
  return {this->buf_ + (index * multiplier) + red + this->wrgb_,
          this->buf_ + (index * multiplier) + green + this->wrgb_,
          this->buf_ + (index * multiplier) + blue + this->wrgb_,
          this->rgbw_ || this->wrgb_
              ? this->buf_ + (index * multiplier) + white
              : nullptr,
          &this->effect_data_[index], &this->correction_};
}

void WordClockLight::clear_frame_() {
  if (this->buf_ == nullptr) {
    return;
  }
  memset(this->buf_, 0, this->get_buffer_size_());
}

esphome::Color WordClockLight::rainbow_color_for_index_(uint16_t index) const {
  if (this->num_leds_ == 0) {
    return esphome::Color(0, 0, 0);
  }

  const uint16_t wheel = static_cast<uint16_t>(
      (static_cast<uint32_t>(index % this->num_leds_) * 1536U) /
      this->num_leds_);
  const uint8_t section = wheel / 256U;
  const uint8_t offset = wheel % 256U;

  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  switch (section) {
    case 0:
      red = 255;
      green = offset;
      break;
    case 1:
      red = 255 - offset;
      green = 255;
      break;
    case 2:
      green = 255;
      blue = offset;
      break;
    case 3:
      green = 255 - offset;
      blue = 255;
      break;
    case 4:
      red = offset;
      blue = 255;
      break;
    case 5:
    default:
      red = 255;
      blue = 255 - offset;
      break;
  }

  const uint8_t brightness_scale =
      static_cast<uint8_t>(this->render_brightness_ * 255.0f + 0.5f);
  return esphome::Color(red, green, blue) * brightness_scale;
}

void WordClockLight::apply_rainbow_to_lit_pixels_() {
  if (this->buf_ == nullptr || this->num_leds_ == 0) {
    return;
  }

  uint8_t red;
  uint8_t green;
  uint8_t blue;
  switch (this->order_) {
    case ORDER_RGB:
      red = 0;
      green = 1;
      blue = 2;
      break;
    case ORDER_RBG:
      red = 0;
      green = 2;
      blue = 1;
      break;
    case ORDER_GRB:
      red = 1;
      green = 0;
      blue = 2;
      break;
    case ORDER_GBR:
      red = 2;
      green = 0;
      blue = 1;
      break;
    case ORDER_BGR:
      red = 2;
      green = 1;
      blue = 0;
      break;
    case ORDER_BRG:
    default:
      red = 1;
      green = 2;
      blue = 0;
      break;
  }

  const uint8_t multiplier = this->rgbw_ || this->wrgb_ ? 4 : 3;
  const uint8_t red_offset = red + this->wrgb_;
  const uint8_t green_offset = green + this->wrgb_;
  const uint8_t blue_offset = blue + this->wrgb_;
  const bool has_white = this->rgbw_ || this->wrgb_;
  const uint8_t white_offset = this->wrgb_ ? 0 : 3;

  for (uint16_t i = 0; i < this->num_leds_; i++) {
    const uint8_t *pixel = this->buf_ + (i * multiplier);
    bool lit = pixel[red_offset] != 0 || pixel[green_offset] != 0 ||
               pixel[blue_offset] != 0;
    if (has_white) {
      lit = lit || pixel[white_offset] != 0;
    }
    if (!lit) {
      continue;
    }

    (*this)[i] = this->rainbow_color_for_index_(i);
  }
}

bool WordClockLight::render_current_frame_(bool force) {
  if (this->buf_ == nullptr || this->time_ == nullptr || !this->is_on_ ||
      this->led_test_active_) {
    return false;
  }
  // Prevent near-simultaneous duplicate renders from multiple call paths.
  const uint32_t now_ms = esphome::millis();
  if ((now_ms - this->last_render_request_ms_) < 150) {
    return false;
  }
  this->last_render_request_ms_ = now_ms;

  wordclock::RenderConfig cfg;
  cfg.random_colors = this->random_colors_ && !this->rainbow_mode_;
  const uint8_t brightness_scale =
      static_cast<uint8_t>(this->render_brightness_ * 255.0f + 0.5f);
  cfg.fixed_color = this->color_ * brightness_scale;

  // Start from a clean frame every pass so partial/stale pixels cannot survive.
  this->clear_frame_();
  if (force) {
    this->renderer_.reset();
  }
  const bool rendered =
      this->renderer_.render(*this, this->time_->now(), cfg, this->language_);
  if (rendered && this->rainbow_mode_) {
    this->apply_rainbow_to_lit_pixels_();
  }
  return rendered;
}

esphome::Color WordClockLight::led_test_color_() const {
  switch (this->led_test_color_phase_) {
    case 0:
      return esphome::Color(255, 0, 0);
    case 1:
      return esphome::Color(0, 255, 0);
    case 2:
    default:
      return esphome::Color(0, 0, 255);
  }
}

bool WordClockLight::index_to_matrix_xy_(uint16_t index, uint8_t *x,
                                         uint8_t *y) const {
  if (index < TEXT_MATRIX_START_INDEX ||
      index >= TEXT_MATRIX_START_INDEX + TEXT_MATRIX_LED_COUNT) {
    return false;
  }

  const uint16_t relative = index - TEXT_MATRIX_START_INDEX;
  const uint8_t row_from_bottom = relative / TEXT_MATRIX_COLS;
  const uint8_t col_in_row = relative % TEXT_MATRIX_COLS;

  const bool row_reversed =
      ((row_from_bottom % 2) == 0) ? TEXT_MATRIX_FIRST_ROW_REVERSED
                                   : !TEXT_MATRIX_FIRST_ROW_REVERSED;
  const uint8_t mapped_x =
      row_reversed ? (TEXT_MATRIX_COLS - 1 - col_in_row) : col_in_row;
  const uint8_t mapped_y = TEXT_MATRIX_ROWS - 1 - row_from_bottom;

  if (x != nullptr) {
    *x = mapped_x;
  }
  if (y != nullptr) {
    *y = mapped_y;
  }
  return true;
}

uint8_t WordClockLight::corner_metric_(uint8_t x, uint8_t y, uint8_t corner) const {
  switch (corner & 0x03) {
    case 0:  // top-left -> bottom-right
      return x + y;
    case 1:  // top-right -> bottom-left
      return (TEXT_MATRIX_COLS - 1 - x) + y;
    case 2:  // bottom-right -> top-left
      return (TEXT_MATRIX_COLS - 1 - x) + (TEXT_MATRIX_ROWS - 1 - y);
    case 3:  // bottom-left -> top-right
    default:
      return x + (TEXT_MATRIX_ROWS - 1 - y);
  }
}

void WordClockLight::run_five_minute_splash_() {
  if (!this->is_on_ || this->buf_ == nullptr || this->num_leds_ == 0) {
    return;
  }

  this->five_minute_splash_active_corner_ = this->five_minute_splash_corner_ & 0x03;
  this->five_minute_splash_corner_ = (this->five_minute_splash_active_corner_ + 1) & 0x03;
  this->five_minute_splash_phase_ = 0;
  this->five_minute_splash_final_phase_ =
      static_cast<uint8_t>((TEXT_MATRIX_COLS - 1) + (TEXT_MATRIX_ROWS - 1) +
                           SPLASH_TRAIL_LENGTH);
  this->five_minute_splash_next_frame_ms_ = 0;
  this->five_minute_splash_active_ = true;
  ESP_LOGD(TAG, "5-minute splash corner=%u",
           static_cast<unsigned>(this->five_minute_splash_active_corner_));
}

void WordClockLight::show_five_minute_splash_frame_(uint8_t phase, uint8_t corner) {
  this->clear_frame_();
  const uint16_t end_index = static_cast<uint16_t>(
      std::min<uint32_t>(this->num_leds_, TEXT_MATRIX_START_INDEX + TEXT_MATRIX_LED_COUNT));
  for (uint16_t index = TEXT_MATRIX_START_INDEX; index < end_index; index++) {
    uint8_t x = 0;
    uint8_t y = 0;
    if (!this->index_to_matrix_xy_(index, &x, &y)) {
      continue;
    }
    const uint8_t metric = this->corner_metric_(x, y, corner);
    const int trail_delta = static_cast<int>(phase) - static_cast<int>(metric);
    if (trail_delta < 0 || trail_delta > SPLASH_TRAIL_LENGTH) {
      continue;
    }

    const uint16_t rainbow_index = static_cast<uint16_t>(
        (index + static_cast<uint16_t>(phase * 13U) +
         static_cast<uint16_t>(corner * 29U)) %
        this->num_leds_);
    const esphome::Color rainbow = this->rainbow_color_for_index_(rainbow_index);
    const uint8_t fade =
        static_cast<uint8_t>(255 - static_cast<uint8_t>(trail_delta * 70U));
    (*this)[index] = rainbow * fade;
  }
}

void WordClockLight::show_led_test_step_() {
  if (this->buf_ == nullptr || this->num_leds_ == 0) {
    return;
  }

  this->clear_frame_();
  if (this->led_test_index_ < this->num_leds_) {
    (*this)[this->led_test_index_] = this->led_test_color_();
  }
  this->schedule_show();
}

void WordClockLight::start_led_test() {
  if (this->num_leds_ == 0) {
    ESP_LOGW(TAG, "Cannot start LED test: num_leds is 0");
    return;
  }

  this->five_minute_splash_active_ = false;
  this->five_minute_splash_requested_ = false;
  this->led_test_active_ = true;
  this->led_test_index_ = 0;
  this->led_test_color_phase_ = 0;
  this->last_led_test_step_ms_ = esphome::millis();
  this->renderer_.reset();
  ESP_LOGI(TAG, "LED mapping test started");

  this->show_led_test_step_();
  this->led_test_index_++;
  if (this->led_test_index_ >= this->num_leds_) {
    this->led_test_index_ = 0;
    this->led_test_color_phase_++;
    if (this->led_test_color_phase_ >= 3) {
      this->led_test_active_ = false;
      ESP_LOGI(TAG, "LED mapping test completed");
    }
  }
}

void WordClockLight::stop_led_test() {
  if (!this->led_test_active_) {
    return;
  }

  this->led_test_active_ = false;
  this->led_test_index_ = 0;
  this->led_test_color_phase_ = 0;
  ESP_LOGI(TAG, "LED mapping test stopped");

  if (this->buf_ == nullptr) {
    return;
  }

  if (!this->is_on_) {
    this->clear_frame_();
    this->schedule_show();
    return;
  }

  this->renderer_.reset();
  this->update_();
}

void WordClockLight::write_state(esphome::light::LightState *state) {
  bool new_is_on = this->is_on_;
  if (state != nullptr) {
    new_is_on = state->current_values.is_on();
  }
  const bool on_state_changed = new_is_on != this->is_on_;
  this->is_on_ = new_is_on;

  if (!this->is_on_) {
    this->led_test_active_ = false;
    this->five_minute_splash_active_ = false;
    this->five_minute_splash_requested_ = false;
    this->renderer_.reset();
    this->clear_frame_();
  } else if (on_state_changed && !this->led_test_active_) {
    // Rebuild immediately when toggled on; ongoing frame updates are handled in update_().
    this->render_current_frame_(true);
  }

  if (this->is_failed() || this->buf_ == nullptr || this->effect_data_ == nullptr ||
      this->channel_ == nullptr || this->encoder_ == nullptr ||
      this->rmt_buf_ == nullptr) {
    return;
  }

  uint32_t now = esphome::micros();
  if (this->is_on_ && this->max_refresh_rate_us_ != 0 &&
      (now - this->last_refresh_) < this->max_refresh_rate_us_) {
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  if (rmt_tx_wait_all_done(this->channel_, 1000) != ESP_OK) {
    ESP_LOGW(TAG, "RMT timeout while waiting for previous frame");
  }
  esphome::delayMicroseconds(this->sk6812_ ? SK6812_INTER_FRAME_GAP_US
                                           : WS2812_INTER_FRAME_GAP_US);

  uint32_t total = this->get_buffer_size_() * RMT_SYMBOLS_PER_BYTE;
  for (size_t i = 0; i < this->get_buffer_size_(); i++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      this->rmt_buf_[i * 8 + bit] =
          (this->buf_[i] & (1 << (7 - bit))) ? this->params_.bit1
                                             : this->params_.bit0;
    }
  }
  this->rmt_buf_[total] = this->params_.reset;

  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.loop_count = 0;
  config.flags.eot_level = 0;
  if (rmt_transmit(this->channel_, this->encoder_, this->rmt_buf_,
                   (this->get_buffer_size_() * RMT_SYMBOLS_PER_BYTE + 1) *
                       sizeof(*this->rmt_buf_),
                   &config) != ESP_OK) {
    ESP_LOGW(TAG, "RMT transmission error");
  }
}

float WordClockLight::get_setup_priority() const {
  return esphome::setup_priority::HARDWARE;
}

void WordClockLight::update_() {
  const uint32_t now_ms = esphome::millis();

  if (this->led_test_active_) {
    if ((now_ms - this->last_led_test_step_ms_) < LED_TEST_STEP_INTERVAL_MS) {
      return;
    }
    this->last_led_test_step_ms_ = now_ms;
    this->show_led_test_step_();

    this->led_test_index_++;
    if (this->led_test_index_ >= this->num_leds_) {
      this->led_test_index_ = 0;
      this->led_test_color_phase_++;
      if (this->led_test_color_phase_ >= 3) {
        this->led_test_active_ = false;
        ESP_LOGI(TAG, "LED mapping test completed");

        if (!this->is_on_) {
          this->clear_frame_();
          this->schedule_show();
        } else {
          this->renderer_.reset();
        }
      }
    }

    return;
  }

  if (this->time_ == nullptr || !this->is_on_) {
    return;
  }
  const esphome::ESPTime now = this->time_->now();
  if (!now.is_valid()) {
    return;
  }

  const int32_t now_minute_key = now.timestamp / 60;
  const uint8_t interval_minutes =
      (this->splash_interval_minutes_ == 0) ? 5 : this->splash_interval_minutes_;
  bool on_five_minute_boundary = false;
  if (interval_minutes == 60) {
    on_five_minute_boundary = (now.minute == 0) && (now.second <= 1);
  } else {
    on_five_minute_boundary =
        ((now.minute % interval_minutes) == 0) && (now.second <= 1);
  }
  if (this->five_minute_splash_requested_ ||
      (this->five_minute_splash_enabled_ && on_five_minute_boundary &&
       now_minute_key != this->last_five_minute_splash_key_)) {
    this->five_minute_splash_requested_ = false;
    this->last_five_minute_splash_key_ = now_minute_key;
    this->run_five_minute_splash_();
  }

  if (this->five_minute_splash_active_) {
    if (now_ms < this->five_minute_splash_next_frame_ms_) {
      return;
    }

    this->show_five_minute_splash_frame_(this->five_minute_splash_phase_,
                                         this->five_minute_splash_active_corner_);
    this->schedule_show();

    if (this->five_minute_splash_phase_ >= this->five_minute_splash_final_phase_) {
      this->five_minute_splash_active_ = false;
      this->renderer_.reset();
      this->last_render_request_ms_ = 0;
      this->last_clock_render_ms_ = 0;
    } else {
      this->five_minute_splash_phase_++;
      this->five_minute_splash_next_frame_ms_ = now_ms + SPLASH_FRAME_INTERVAL_MS;
    }

    return;
  }

  // Keep word-clock refresh at 1 Hz; splash/test run with their own cadence.
  if ((now_ms - this->last_clock_render_ms_) < 1000) {
    return;
  }
  this->last_clock_render_ms_ = now_ms;

  // Rebuild and send the frame every cycle so stale/corrupted buffer content
  // cannot persist until the next minute boundary.
  this->render_current_frame_(true);
  this->schedule_show();
}

void WordClockLight::trigger_refresh_() {
  this->renderer_.reset();
  this->last_clock_render_ms_ = esphome::millis() - 1000;
  if (this->buf_ == nullptr) {
    return;
  }
  if (this->led_test_active_) {
    return;
  }
  if (!this->is_on_) {
    this->clear_frame_();
    this->schedule_show();
    return;
  }
  if (this->time_ == nullptr) {
    return;
  }
  this->update_();
}

}  // namespace wordclock
