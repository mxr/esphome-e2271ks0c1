#include "e2271ks0c1.h"

#include "esp_heap_caps.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace e2271ks0c1 {

static const char *const TAG = "e2271ks0c1";

// Panel + your known-good firmware:
// 1 = black, 0 = white
static constexpr uint8_t WHITE_FILL = 0x00;
static constexpr uint8_t BLACK_FILL = 0xFF;

E2271KS0C1::~E2271KS0C1() {
  if (spi_ != nullptr) {
    spi_remove_device_();
  }
  if (cur_ != nullptr) {
    heap_caps_free(cur_);
    cur_ = nullptr;
  }
  if (prev_ != nullptr) {
    heap_caps_free(prev_);
    prev_ = nullptr;
  }
  if (zero_ != nullptr) {
    heap_caps_free(zero_);
    zero_ = nullptr;
  }
  if (buf_ != nullptr) {
    delete[] buf_;
    buf_ = nullptr;
  }
}

void E2271KS0C1::setup() {
  ESP_LOGI(TAG, "setup() entered");

  // --- Allocate ESPHome DisplayBuffer backing store ---
  buf_ = new uint8_t[BYTES];
  if (buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buf_ (BYTES=%d)", BYTES);
    return;
  }
  std::memset(buf_, WHITE_FILL, BYTES);
  this->buffer_ = buf_;

  // --- DMA buffers for SPI transfers ---
  cur_ = (uint8_t *) heap_caps_malloc(BYTES, MALLOC_CAP_DMA);
  prev_ = (uint8_t *) heap_caps_malloc(BYTES, MALLOC_CAP_DMA);
  zero_ = (uint8_t *) heap_caps_malloc(BYTES, MALLOC_CAP_DMA);
  if (cur_ == nullptr || prev_ == nullptr || zero_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate DMA buffers (BYTES=%d)", BYTES);
    return;
  }
  std::memset(cur_, WHITE_FILL, BYTES);
  std::memset(prev_, WHITE_FILL, BYTES);
  std::memset(zero_, 0x00, BYTES);

  if (dc_pin_ == nullptr || rst_pin_ == nullptr || busy_pin_ == nullptr) {
    ESP_LOGE(TAG, "Pins not set (dc/rst/busy null)");
    return;
  }

  dc_pin_->setup();
  rst_pin_->setup();
  busy_pin_->setup();

  // Match your working driver: start low
  dc_pin_->digital_write(false);
  rst_pin_->digital_write(false);

  // SPI host selection
  host_ = (spi_host_ == "SPI3_HOST") ? SPI3_HOST : SPI2_HOST;

  if (clk_gpio_ < 0 || mosi_gpio_ < 0 || cs_gpio_ < 0) {
    ESP_LOGE(TAG, "SPI GPIOs not set (clk=%d mosi=%d cs=%d)", clk_gpio_, mosi_gpio_, cs_gpio_);
    return;
  }

  spi_init_bus_();

  // --- OTP read at low speed (matches working code) ---
  spi_add_device_(otp_clock_hz_);
  power_on_();   // reset + (0x00, 0x0e)
  read_otp_();   // reads PSR[0..1]
  spi_remove_device_();

  // --- Main device at write speed ---
  spi_add_device_(write_clock_hz_);

  ESP_LOGI(TAG, "EPD ready. PSR[0]=0x%02x PSR[1]=0x%02x", psr_data_[0], psr_data_[1]);

  frame_count_ = 0;
  sync_needed_ = true;
}

void E2271KS0C1::update() {
  if (spi_ == nullptr || cur_ == nullptr || prev_ == nullptr || this->buffer_ == nullptr) {
    ESP_LOGW(TAG, "update() skipped (not ready)");
    return;
  }

  // Clear UI buffer to white (0 = white)
  std::memset(this->buffer_, WHITE_FILL, BYTES);

  // Let ESPHome render; it will call draw_absolute_pixel_internal()
  this->do_update_();

  // IMPORTANT: No inversion. ESPHome buffer already matches panel (1=black, 0=white).
  std::memcpy(cur_, this->buffer_, BYTES);

  ESP_LOGD(TAG, "buf_[0]=%02x buf_[1]=%02x  cur_[0]=%02x cur_[1]=%02x",
           this->buffer_[0], this->buffer_[1], cur_[0], cur_[1]);

  const bool full = (frame_count_ == 0) || ((frame_count_ % full_update_every_) == 0);
  const bool fast = !full;

  // While debugging you can force:
  // sync_needed_ = true;
  if (sync_needed_) {
    do_update_cycle_(fast);
    sync_needed_ = false;
  }

  frame_count_++;
}

void E2271KS0C1::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (this->buffer_ == nullptr) return;

  // UI space is 264x176 (landscape)
  if (x < 0 || y < 0 || x >= 264 || y >= 176) return;

  const bool black = (color.red == 0 && color.green == 0 && color.blue == 0);

  // Match your known-good firmware mapping for DISPLAY_ROTATION == -90:
  // disp_x = DISPLAY_WIDTH - (1 + y)
  // disp_y = x
  const int disp_x = PANEL_W - 1 - y;  // 0..175
  const int disp_y = x;               // 0..263

  if (disp_x < 0 || disp_y < 0 || disp_x >= PANEL_W || disp_y >= PANEL_H) return;

  const uint32_t index = (uint32_t) disp_y * (PANEL_W / 8u) + (uint32_t) disp_x / 8u;
  const uint8_t mask = (uint8_t) (1u << (7u - ((uint32_t) disp_x & 7u)));

  // 1 = black, 0 = white  (same as your working driver)
  if (black) this->buffer_[index] |= mask;
  else       this->buffer_[index] &= (uint8_t) ~mask;

  sync_needed_ = true;
}

/* ---------------- SPI helpers ---------------- */

void E2271KS0C1::spi_init_bus_() {
  spi_bus_config_t buscfg{};
  buscfg.mosi_io_num = mosi_gpio_;
  buscfg.miso_io_num = -1;
  buscfg.sclk_io_num = clk_gpio_;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = BYTES;

  esp_err_t err = spi_bus_initialize(host_, &buscfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %d", (int) err);
  }
}

void E2271KS0C1::spi_add_device_(uint32_t hz) {
  spi_device_interface_config_t devcfg{};
  devcfg.clock_speed_hz = (int) hz;
  devcfg.mode = 0;
  devcfg.flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE;
  devcfg.spics_io_num = cs_gpio_;
  devcfg.queue_size = 1;

  esp_err_t err = spi_bus_add_device(host_, &devcfg, &spi_);
  if (err != ESP_OK) ESP_LOGE(TAG, "spi_bus_add_device failed: %d", (int) err);
}

void E2271KS0C1::spi_remove_device_() {
  if (spi_ == nullptr) return;
  esp_err_t err = spi_bus_remove_device(spi_);
  if (err != ESP_OK) ESP_LOGE(TAG, "spi_bus_remove_device failed: %d", (int) err);
  spi_ = nullptr;
}

void E2271KS0C1::send_cmd_(uint8_t addr, const uint8_t *data, int len) {
  if (spi_ == nullptr) return;

  static spi_transaction_t t{};
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
  t.length = 8;
  t.user = (void *) 1;

  t.tx_data[0] = addr;

  dc_pin_->digital_write(false);
  esp_err_t ret = spi_device_polling_transmit(spi_, &t);
  dc_pin_->digital_write(true);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI cmd transmit failed: %d", (int) ret);
    return;
  }

  if (data == nullptr || len <= 0) return;

  for (int i = 0; i < len; i++) {
    t.tx_data[0] = data[i];
    ret = spi_device_polling_transmit(spi_, &t);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI data transmit failed: %d", (int) ret);
      return;
    }
  }
}

void E2271KS0C1::send_data_byte_(uint8_t b) {
  if (spi_ == nullptr) return;

  // DC must be HIGH for data
  dc_pin_->digital_write(true);

  static spi_transaction_t t{};
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
  t.length = 8;
  t.user = (void *) 1;
  t.tx_data[0] = b;

  esp_err_t ret = spi_device_polling_transmit(spi_, &t);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI data byte failed: %d", (int) ret);
  }
}

void E2271KS0C1::send_frame_(uint8_t *buf) {
  if (spi_ == nullptr || buf == nullptr) return;

  // DC must be HIGH for data
  dc_pin_->digital_write(true);

  static spi_transaction_t t{};
  t.flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL;
  t.length = BYTES * 8;
  t.user = (void *) 1;
  t.tx_buffer = buf;

  esp_err_t ret = spi_device_polling_transmit(spi_, &t);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI frame failed: %d", (int) ret);
  }
}

/* ---------------- EPD protocol (matches working firmware) ---------------- */

void E2271KS0C1::wait_busy_() {
  // BUSY: 1 = ready, 0 = busy
  const uint32_t start = millis();
  const uint32_t timeout_ms = 15000;

  ESP_LOGD(TAG, "Waiting for BUSY...");
  while (!busy_pin_->digital_read()) {
    delay(10);
    if (millis() - start > timeout_ms) {
      ESP_LOGE(TAG, "BUSY timeout!");
      break;
    }
  }
  ESP_LOGD(TAG, "BUSY released");
}

void E2271KS0C1::power_on_() {
  // Match your working reset timing
  rst_pin_->digital_write(false); delay(50);
  rst_pin_->digital_write(true);  delay(50);
  rst_pin_->digital_write(false); delay(50);
  rst_pin_->digital_write(true);  delay(50);

  wait_busy_();

  // Panel settings soft reset: (0x00, 0x0e)
  send_cmd_(ADDR_PSR, PSR_SOFT_RESET, 1);
  delay(50);
}

void E2271KS0C1::read_otp_() {
  if (spi_ == nullptr) return;

  // Matches your working eink_read_otp()
  send_cmd_(ADDR_OTP, nullptr, 0);

  spi_transaction_t t_rx{};
  t_rx.flags = SPI_TRANS_DMA_BUFFER_ALIGN_MANUAL | SPI_TRANS_USE_RXDATA;
  t_rx.rxlength = 8;
  t_rx.user = (void *) 1;

  ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &t_rx));  // dummy
  ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &t_rx));  // address 0
  uint8_t addr0 = t_rx.rx_data[0];

  uint16_t bank_addr = (addr0 == 0xA5) ? OTP_BANK0_ADDR : OTP_BANK1_ADDR;

  for (uint16_t i = 1; i < bank_addr; i++) {
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &t_rx));
  }

  ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &t_rx));
  psr_data_[0] = t_rx.rx_data[0];

  ESP_ERROR_CHECK(spi_device_polling_transmit(spi_, &t_rx));
  psr_data_[1] = t_rx.rx_data[0];
}

static inline uint8_t encode_temp_tsset(float temp_c, bool fast) {
  // Two's complement in 8-bit, rounded.
  int t = (int) lroundf(temp_c);
  uint8_t ts = (uint8_t) (t & 0xFF);
  if (fast) ts |= 0x40;
  return ts;
}

void E2271KS0C1::send_config_(bool fast) {
  uint8_t ts = encode_temp_tsset(temperature_c_, fast);
  send_cmd_(ADDR_INPUT_TEMP, &ts, 1);

  uint8_t at = 0x02;
  send_cmd_(ADDR_ACTIVE_TEMP, &at, 1);

  if (fast) {
    uint8_t psr[2];
    psr[0] = psr_data_[0] | 0x10;
    psr[1] = psr_data_[1] | 0x02;
    send_cmd_(ADDR_PSR, psr, 2);

    send_cmd_(ADDR_VCOM_CDI, VCOM_CDI, 1);
  } else {
    send_cmd_(ADDR_PSR, psr_data_, 2);
  }
}

void E2271KS0C1::send_image_full_() {
  // Frame 1: pixels (1==black)
  send_cmd_(ADDR_FRAME1, nullptr, 0);
  send_frame_(cur_);

  // Frame 2: zeros (use pre-allocated zero buffer for speed)
  send_cmd_(ADDR_FRAME2, nullptr, 0);
  send_frame_(zero_);

  std::memcpy(prev_, cur_, BYTES);
}

void E2271KS0C1::send_image_fast_() {
  // 2x border setting
  send_cmd_(ADDR_VCOM_CDI, VCOM_BORDER, 1);
  send_cmd_(ADDR_VCOM_CDI, VCOM_BORDER, 1);

  // Frame 1: previous
  send_cmd_(ADDR_FRAME1, nullptr, 0);
  send_frame_(prev_);

  // Frame 2: next
  send_cmd_(ADDR_FRAME2, nullptr, 0);
  send_frame_(cur_);

  // 2x CDI
  send_cmd_(ADDR_VCOM_CDI, VCOM_CDI, 1);
  send_cmd_(ADDR_VCOM_CDI, VCOM_CDI, 1);

  std::memcpy(prev_, cur_, BYTES);
}

void E2271KS0C1::do_update_cycle_(bool fast) {
  // Mirrors working loop:
  // send_config -> send_image -> POWER_ON (2x) -> REFRESH (2x) -> POWER_OFF (2x)

  wait_busy_();

  send_config_(fast);

  if (fast) send_image_fast_();
  else      send_image_full_();

  wait_busy_();

  send_cmd_(ADDR_PWR_ON, nullptr, 0);
  send_cmd_(ADDR_PWR_ON, nullptr, 0);

  wait_busy_();

  send_cmd_(ADDR_REFRESH, nullptr, 0);
  send_cmd_(ADDR_REFRESH, nullptr, 0);

  wait_busy_();

  send_cmd_(ADDR_PWR_OFF, nullptr, 0);
  send_cmd_(ADDR_PWR_OFF, nullptr, 0);

  wait_busy_();
}

void E2271KS0C1::power_off_() {
  send_cmd_(ADDR_PWR_OFF, nullptr, 0);
  send_cmd_(ADDR_PWR_OFF, nullptr, 0);
  wait_busy_();
}

}  // namespace e2271ks0c1
}  // namespace esphome
