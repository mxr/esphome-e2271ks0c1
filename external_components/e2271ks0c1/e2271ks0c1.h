#pragma once

#include <cstdint>
#include <string>

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/hal.h"

#include "driver/spi_master.h"

namespace esphome {
namespace e2271ks0c1 {

class E2271KS0C1 : public display::DisplayBuffer {
 public:
  void setup() override;
  void update() override;

  void set_clk_pin(int pin) { clk_gpio_ = pin; }
  void set_mosi_pin(int pin) { mosi_gpio_ = pin; }
  void set_cs_pin(int pin) { cs_gpio_ = pin; }
  void set_dc_pin(GPIOPin *p) { dc_pin_ = p; }
  void set_reset_pin(GPIOPin *p) { rst_pin_ = p; }
  void set_busy_pin(GPIOPin *p) { busy_pin_ = p; }

  void set_spi_host(const std::string &h) { spi_host_ = h; }
  void set_otp_clock_hz(uint32_t hz) { otp_clock_hz_ = hz; }
  void set_write_clock_hz(uint32_t hz) { write_clock_hz_ = hz; }
  void set_full_update_every(uint32_t n) { full_update_every_ = n; }
  void set_temperature_c(float t) { temperature_c_ = t; }

  ~E2271KS0C1();  // defined in .cpp

 protected:
  display::DisplayType get_display_type() override {
    return display::DisplayType::DISPLAY_TYPE_BINARY;
  }
  int get_width_internal() override { return 264; }   // UI width
  int get_height_internal() override { return 176; }  // UI height
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

 private:
  // pins
  int clk_gpio_{-1};
  int mosi_gpio_{-1};
  int cs_gpio_{-1};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  // spi
  std::string spi_host_{"SPI2_HOST"};
  spi_device_handle_t spi_{nullptr};
  spi_host_device_t host_{SPI2_HOST};
  uint32_t otp_clock_hz_{4 * 1000 * 1000};
  uint32_t write_clock_hz_{10 * 1000 * 1000};

  // panel geometry (physical)
  static constexpr int PANEL_W = 176;
  static constexpr int PANEL_H = 264;
  static constexpr int ROW_LEN = PANEL_W / 8;
  static constexpr int BYTES = ROW_LEN * PANEL_H;  // 5808
  static_assert(BYTES == 5808, "BYTES must be 5808 for 176x264");

  uint8_t *buf_{nullptr};   // ESPHome DisplayBuffer backing store

  uint8_t psr_data_[2]{0, 0};

  uint8_t *cur_{nullptr};
  uint8_t *prev_{nullptr};
  uint8_t *zero_{nullptr};  // Zero buffer for Frame 2 in full updates

  bool sync_needed_{true};
  uint32_t frame_count_{0};
  uint32_t full_update_every_{120};
  float temperature_c_{25.0f};

  // protocol constants
  static constexpr uint8_t ADDR_INPUT_TEMP = 0xe5;
  static constexpr uint8_t ADDR_ACTIVE_TEMP = 0xe0;
  static constexpr uint8_t ADDR_PSR = 0x00;
  static constexpr uint8_t ADDR_OTP = 0xa2;
  static constexpr uint8_t ADDR_FRAME1 = 0x10;
  static constexpr uint8_t ADDR_FRAME2 = 0x13;
  static constexpr uint8_t ADDR_PWR_ON = 0x04;
  static constexpr uint8_t ADDR_REFRESH = 0x12;
  static constexpr uint8_t ADDR_PWR_OFF = 0x02;
  static constexpr uint8_t ADDR_VCOM_CDI = 0x50;

  static constexpr uint16_t OTP_BANK0_ADDR = 0x0fb4;
  static constexpr uint16_t OTP_BANK1_ADDR = 0x1fb4;

  // sequences from your code
  static constexpr uint8_t PSR_SOFT_RESET[1] = {0x0e};
  static constexpr uint8_t VCOM_CDI[1] = {0x07};
  static constexpr uint8_t VCOM_BORDER[1] = {0x27};

  // helpers
  void spi_init_bus_();
  void spi_add_device_(uint32_t hz);
  void spi_remove_device_();

  void send_cmd_(uint8_t addr, const uint8_t *data, int len);
  void send_data_byte_(uint8_t b);
  void send_frame_(uint8_t *buf);

  void wait_busy_();
  void power_on_();
  void power_off_();
  void read_otp_();

  void send_config_(bool fast);
  void send_image_full_();
  void send_image_fast_();
  void do_update_cycle_(bool fast);
};

}  // namespace e2271ks0c1
}  // namespace esphome
