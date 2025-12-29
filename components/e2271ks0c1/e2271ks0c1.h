#pragma once

#include "esphome/components/epaper_spi/epaper_spi.h"

namespace esphome {
namespace e2271ks0c1 {

class E2271KS0C1 : public epaper_spi::EPaperBase {
 public:
  E2271KS0C1(const char *name, int width, int height, const uint8_t *init_seq, size_t init_len)
      : EPaperBase(name, width, height, init_seq, init_len) {
        // 1bpp => 8 pixels per byte
        this->buffer_length_ = (size_t) width * height / 8;
  }

  void set_temperature_c(float t) { temperature_c_ = t; }

  // Force next update to be a full refresh (useful when changing dark mode, etc.)
  void force_full_update() { this->update_count_ = 0; }

  // EPaperBase hooks
  bool reset() override;  // Override to control reset timing
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override;

 protected:
  void draw_pixel_at(int x, int y, Color color) override {
    // use EPaperBase packing, we’ll invert on TX to match panel polarity
    epaper_spi::EPaperBase::draw_pixel_at(x, y, color);
  }

 private:
  static constexpr uint8_t ADDR_INPUT_TEMP  = 0xE5;
  static constexpr uint8_t ADDR_ACTIVE_TEMP = 0xE0;
  static constexpr uint8_t ADDR_PSR         = 0x00;
  static constexpr uint8_t ADDR_FRAME1      = 0x10;
  static constexpr uint8_t ADDR_FRAME2      = 0x13;
  static constexpr uint8_t ADDR_PWR_ON      = 0x04;
  static constexpr uint8_t ADDR_REFRESH     = 0x12;
  static constexpr uint8_t ADDR_PWR_OFF     = 0x02;
  static constexpr uint8_t ADDR_VCOM_CDI    = 0x50;

  static constexpr uint8_t VCOM_CDI[1]    = {0x07};
  static constexpr uint8_t VCOM_BORDER[1] = {0x27};

  // If you don’t want OTP yet, hardcode what you already read: cf 8d
  uint8_t psr_[2] = {0xCF, 0x8D};

  float temperature_c_{25.0f};
  std::vector<uint8_t> tx_;   // inverted buffer for panel (1=black)
  std::vector<uint8_t> prev_; // for fast updates later
  bool initialized_{false};   // track if hardware reset has been done
};

}  // namespace e2271ks0c1
}  // namespace esphome
