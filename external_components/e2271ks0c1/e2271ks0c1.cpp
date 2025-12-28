#include "e2271ks0c1.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace e2271ks0c1 {

static const char *const TAG = "e2271ks0c1";

static inline uint8_t encode_temp_tsset(float temp_c, bool fast) {
  int t = (int) lroundf(temp_c);
  uint8_t ts = (uint8_t) (t & 0xFF);   // 2's complement via cast
  if (fast) ts |= 0x40;
  return ts;
}

bool E2271KS0C1::transfer_data() {
  ESP_LOGD(TAG, "transfer_data() starting");

  // Clear to black, then let the writer (lambda) draw
  this->fill(display::COLOR_OFF);

  // Try to call the display writer/lambda directly
  if (this->writer_.has_value()) {
    ESP_LOGD(TAG, "Calling display writer lambda");
    (*this->writer_)(*this);
  } else {
    ESP_LOGW(TAG, "No display writer configured");
  }

  // Count what's in the buffer
  size_t non_zero = 0;
  for (size_t i = 0; i < this->buffer_length_; i++) {
    if (this->buffer_[i] != 0x00) non_zero++;
  }
  ESP_LOGI(TAG, "Buffer has %zu non-zero bytes (shapes drawn)", non_zero);

  // Determine if this is a fast (partial) update or full update
  // Full update on first frame and every full_update_every frames
  const bool fast = (this->update_count_ != 0);
  ESP_LOGD(TAG, "Update mode: %s (count=%u)", fast ? "FAST" : "FULL", this->update_count_);

  // Hardware reset sequence on first call only
  if (!this->initialized_) {
    if (this->reset_pin_ != nullptr) {
      ESP_LOGD(TAG, "Hardware reset sequence");
      this->reset_pin_->digital_write(false);
      delay(50);
      this->reset_pin_->digital_write(true);
      delay(50);
      this->reset_pin_->digital_write(false);
      delay(50);
      this->reset_pin_->digital_write(true);
      delay(50);
    }
    this->wait_for_idle_(true);
    this->initialized_ = true;
  }

  // Soft reset with data (critical for this panel)
  uint8_t reset_data = 0x0E;
  this->cmd_data(0x00, &reset_data, 1);
  delay(50);

  // Build TX buffer with 90-degree clockwise rotation
  // Source buffer: 264 wide x 176 tall (as reported to ESPHome)
  // Panel expects: 176 wide x 264 tall
  // Rotation: source(x,y) -> dest(src_height-1-y, x)
  const int src_width = 264;
  const int src_height = 176;
  const int dst_width = 176;
  const int dst_height = 264;
  const size_t n = (size_t)(dst_width * dst_height / 8);  // 5808 bytes

  if (this->tx_.size() != n) {
    this->tx_.assign(n, 0x00);
    this->prev_.assign(n, 0x00);
  }

  // Clear tx buffer to white (0 = white on panel)
  std::fill(this->tx_.begin(), this->tx_.end(), 0x00);

  // Rotate and copy pixels
  // COLOR_ON (1 in buffer) = black on screen
  // COLOR_OFF (0 in buffer) = white on screen
  for (int sy = 0; sy < src_height; sy++) {
    for (int sx = 0; sx < src_width; sx++) {
      // Get source pixel
      int src_byte = sy * (src_width / 8) + sx / 8;
      int src_bit = 7 - (sx % 8);
      bool pixel_on = (this->buffer_[src_byte] >> src_bit) & 1;

      // Calculate destination position (90 deg clockwise)
      int dx = src_height - 1 - sy;
      int dy = sx;

      // Set destination pixel: 1 = black on panel
      int dst_byte = dy * (dst_width / 8) + dx / 8;
      int dst_bit = 7 - (dx % 8);
      if (pixel_on) {
        this->tx_[dst_byte] |= (1 << dst_bit);  // Set bit = black on panel
      }
    }
  }

  // Temperature and panel configuration
  uint8_t ts = encode_temp_tsset(this->temperature_c_, fast);
  this->cmd_data(ADDR_INPUT_TEMP, &ts, 1);

  uint8_t at = 0x02;
  this->cmd_data(ADDR_ACTIVE_TEMP, &at, 1);

  // PSR (Panel Settings Register)
  if (fast) {
    uint8_t fast_psr[2] = {static_cast<uint8_t>(psr_[0] | 0x10), static_cast<uint8_t>(psr_[1] | 0x02)};
    this->cmd_data(ADDR_PSR, fast_psr, 2);
    uint8_t vcom_setting = 0x07;
    this->cmd_data(ADDR_VCOM_CDI, &vcom_setting, 1);
  } else {
    this->cmd_data(ADDR_PSR, psr_, 2);
  }

  // Send image data - Frame 1 (new image)
  this->command(ADDR_FRAME1);
  this->start_data_();
  this->write_array(this->tx_.data(), n);
  this->end_data_();

  // Frame 2: previous frame for fast update, zeros for full update
  this->command(ADDR_FRAME2);
  this->start_data_();
  if (fast && this->prev_.size() == n) {
    this->write_array(this->prev_.data(), n);
  } else {
    for (size_t i = 0; i < n; i++) this->write_byte(0x00);
  }
  this->end_data_();

  // Power on, refresh, power off sequence
  this->wait_for_idle_(true);

  this->command(ADDR_PWR_ON);
  this->command(ADDR_PWR_ON);
  this->wait_for_idle_(true);

  this->command(ADDR_REFRESH);
  this->command(ADDR_REFRESH);
  this->wait_for_idle_(true);

  this->command(ADDR_PWR_OFF);
  this->command(ADDR_PWR_OFF);
  this->wait_for_idle_(true);

  // Store for potential fast mode later
  this->prev_ = this->tx_;
  ESP_LOGD(TAG, "Display update complete");
  return true;
}

bool E2271KS0C1::reset() {
  // We handle hardware reset in transfer_data() on first call only
  return true;
}

void E2271KS0C1::power_on() {
  // handled in transfer_data
}

void E2271KS0C1::refresh_screen(bool /*partial*/) {
  // handled in transfer_data
}

void E2271KS0C1::power_off() {
  // handled in transfer_data
}

void E2271KS0C1::deep_sleep() {
  // no deep sleep command in your working flow; just leave idle
  ESP_LOGD(TAG, "Update done");
}

}  // namespace e2271ks0c1
}  // namespace esphome
