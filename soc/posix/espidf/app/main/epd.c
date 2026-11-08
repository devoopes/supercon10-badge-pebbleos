/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// E-paper panel driver. The command sequences are the badge firmware's own
// (Hack-a-Day/2026-Billboard_Badge, firmware/badge/lib/epd.py and
// epd_grayscale.py), as is the short waveform (lut_b.h). This adds a
// "strong" variant of that waveform: its run length is set by the red table,
// which drives in groups 1-9 while black and white only drive in 1-4, so
// repeating their drive in the idle groups gives solid black in one pull at
// no cost in time.

#include "epd.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lut_b.h"

static const char *TAG = "epd";

#define PIN_SCK    21
#define PIN_MOSI   47
#define PIN_CS1    13 // left half
#define PIN_CS2    3  // right half
#define PIN_BUSY1  12
#define PIN_BUSY2  8
#define PIN_DC     10
#define PIN_RST    11
#define PIN_PWR_EN 48 // held low, as the badge firmware does

#define HALF_W     (EPD_WIDTH / 2)
#define ROW_BYTES  (HALF_W / 8)
#define HALF_BYTES (ROW_BYTES * EPD_HEIGHT)
#define SPI_CHUNK  4092

enum { LEFT = 1, RIGHT = 2, BOTH = 3 };

static spi_device_handle_t s_spi;
static uint8_t *s_bw[2];
static uint8_t *s_red[2];
static uint8_t s_lut[400];
static bool s_powered;

static inline int64_t prv_ms(void) {
  return esp_timer_get_time() / 1000;
}

static inline void prv_sleep(int ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static void prv_spi_write(const uint8_t *data, size_t len) {
  while (len > 0) {
    size_t n = len > SPI_CHUNK ? SPI_CHUNK : len;
    spi_transaction_t t = {.length = n * 8, .tx_buffer = data};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
    data += n;
    len -= n;
  }
}

static void prv_cs(int mask, bool active) {
  if (mask & LEFT) {
    gpio_set_level(PIN_CS1, !active);
  }
  if (mask & RIGHT) {
    gpio_set_level(PIN_CS2, !active);
  }
}

static void prv_send(int mask, uint8_t index, const uint8_t *data, size_t len) {
  gpio_set_level(PIN_DC, 0);
  prv_cs(mask, true);
  prv_spi_write(&index, 1);
  prv_cs(BOTH, false);
  gpio_set_level(PIN_DC, 1);
  prv_cs(mask, true);
  prv_spi_write(data, len);
  prv_cs(BOTH, false);
}

#define SEND(mask, index, ...)                  \
  do {                                          \
    static const uint8_t d_[] = {__VA_ARGS__};  \
    prv_send((mask), (index), d_, sizeof(d_));  \
  } while (0)

// BUSY is low while a controller works.
static bool prv_ready(void) {
  return gpio_get_level(PIN_BUSY1) && gpio_get_level(PIN_BUSY2);
}

static bool prv_wait_ready(int timeout_ms) {
  int64_t start = prv_ms();
  while (!prv_ready()) {
    if (prv_ms() - start > timeout_ms) {
      ESP_LOGE(TAG, "BUSY timeout (%d ms)", timeout_ms);
      return false;
    }
    prv_sleep(1);
  }
  return true;
}

static void prv_reset(void) {
  prv_sleep(200);
  gpio_set_level(PIN_RST, 1);
  prv_sleep(20);
  gpio_set_level(PIN_RST, 0);
  prv_sleep(200);
  gpio_set_level(PIN_RST, 1);
  prv_sleep(200);
  prv_cs(BOTH, false);
  prv_sleep(5);
  SEND(BOTH, 0x70, 0x71, 0x36); // B1 booster config (itc97 errata 9)
}

void epd_send(void) {
  SEND(BOTH, 0x13, 0x00, 0x3B, 0x00, 0x00, 0x9F, 0x02); // DUW
  SEND(BOTH, 0x90, 0x00, 0x3B, 0x00, 0xA9);             // DRFW
  for (int h = 0; h < 2; h++) {
    int mask = h == 0 ? LEFT : RIGHT;
    SEND(mask, 0x12, 0x3B, 0x00, 0x14); // RAM_RW
    prv_send(mask, 0x10, s_bw[h], HALF_BYTES);
    SEND(mask, 0x12, 0x3B, 0x00, 0x14);
    prv_send(mask, 0x11, s_red[h], HALF_BYTES);
  }
}

static void prv_cog_initial(void) {
  SEND(BOTH, 0x05, 0x7D);
  prv_sleep(200);
  SEND(BOTH, 0x05, 0x00);
  prv_sleep(10);
  SEND(BOTH, 0xD8, 0x80); // MS_SYNC
  SEND(BOTH, 0xD6, 0x00); // BVSS
  SEND(BOTH, 0xA7, 0x10);
  prv_sleep(100);
  SEND(BOTH, 0xA7, 0x00);
  prv_sleep(100);
  SEND(BOTH, 0x03, 0x00, 0x11); // OSC
  for (int h = 0; h < 2; h++) {
    int mask = h == 0 ? LEFT : RIGHT;
    SEND(mask, 0x44, 0x00);
    SEND(mask, 0x45, 0x80);
    SEND(mask, 0xA7, 0x10);
    prv_sleep(100);
    SEND(mask, 0xA7, 0x00);
    prv_sleep(100);
    SEND(mask, 0x44, 0x06);
    SEND(mask, 0x45, 0x82); // temperature 0x82 = 25 C
    SEND(mask, 0xA7, 0x10);
    prv_sleep(100);
    SEND(mask, 0xA7, 0x00);
    prv_sleep(100);
  }
  SEND(BOTH, 0x60, 0x25); // TCON
  SEND(LEFT, 0x61, 0x01); // STV_DIR, left half only
  SEND(BOTH, 0x02, 0x00); // VCOM
}

static void prv_dcdc_soft_start(void) {
  uint8_t d[2];
  SEND(BOTH, 0x51, 0x50, 0x01);
  for (int v = 1; v < 5; v++) {
    SEND(BOTH, 0x09, 0x1F);
    d[0] = 0x50;
    d[1] = v;
    prv_send(BOTH, 0x51, d, 2);
    SEND(BOTH, 0x09, 0x9F);
    prv_sleep(2);
  }
  for (int v = 1; v < 11; v++) {
    SEND(BOTH, 0x09, 0x1F);
    d[0] = 0x0A;
    d[1] = v;
    prv_send(BOTH, 0x51, d, 2);
    SEND(BOTH, 0x09, 0x9F);
    prv_sleep(2);
  }
  for (int v = 3; v < 11; v++) {
    SEND(BOTH, 0x09, 0x7F);
    d[0] = 0x0A;
    d[1] = v;
    prv_send(BOTH, 0x51, d, 2);
    SEND(BOTH, 0x09, 0xFF);
    prv_sleep(2);
  }
  for (int v = 9; v > 1; v--) {
    SEND(BOTH, 0x09, 0x7F);
    d[0] = v;
    d[1] = 0x0A;
    prv_send(BOTH, 0x51, d, 2);
    SEND(BOTH, 0x09, 0xFF);
    prv_sleep(2);
  }
  SEND(BOTH, 0x09, 0xFF);
  prv_sleep(10);
}

void epd_power_on(void) {
  prv_reset();
  epd_send();
  prv_cog_initial();
  prv_send(BOTH, 0xB0, s_lut, sizeof(s_lut));
  prv_dcdc_soft_start();
  s_powered = true;
}

int epd_pull(void) {
  if (!prv_wait_ready(20000)) {
    return -1;
  }
  int64_t t0 = prv_ms();
  SEND(BOTH, 0x15, 0x3C);
  bool left = false;
  bool right = false;
  while (!(left && right) && prv_ms() - t0 < 100) {
    left |= !gpio_get_level(PIN_BUSY1);
    right |= !gpio_get_level(PIN_BUSY2);
  }
  if (!prv_wait_ready(20000)) {
    return -1;
  }
  return (int)(prv_ms() - t0);
}

void epd_power_off(void) {
  prv_wait_ready(20000);
  SEND(BOTH, 0x09, 0x7F);
  SEND(BOTH, 0x05, 0x7D);
  SEND(BOTH, 0x09, 0x00);
  prv_sleep(200);
  prv_wait_ready(15000);
  gpio_set_level(PIN_DC, 0);
  gpio_set_level(PIN_CS1, 0);
  gpio_set_level(PIN_RST, 0);
  gpio_set_level(PIN_CS1, 1);
  s_powered = false;
}

bool epd_is_powered(void) {
  return s_powered;
}

// B1 panels take the leftmost pixel in the least significant bit.
void epd_set(int x, int y, enum epd_sel sel) {
  int h = x >= HALF_W;
  int xb = x - h * HALF_W;
  int i = y * ROW_BYTES + xb / 8;
  uint8_t m = 1U << (xb & 7);
  if (sel & 1) {
    s_bw[h][i] |= m;
  } else {
    s_bw[h][i] &= ~m;
  }
  if (sel & 2) {
    s_red[h][i] |= m;
  } else {
    s_red[h][i] &= ~m;
  }
}

void epd_fill(enum epd_sel sel) {
  for (int h = 0; h < 2; h++) {
    memset(s_bw[h], (sel & 1) ? 0xFF : 0x00, HALF_BYTES);
    memset(s_red[h], (sel & 2) ? 0xFF : 0x00, HALF_BYTES);
  }
}

static void prv_out(int pin, int level) {
  gpio_config_t c = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_OUTPUT};
  ESP_ERROR_CHECK(gpio_config(&c));
  gpio_set_level(pin, level);
}

static void prv_in(int pin) {
  gpio_config_t c = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_INPUT};
  ESP_ERROR_CHECK(gpio_config(&c));
}

void epd_init(void) {
  prv_out(PIN_PWR_EN, 0);
  prv_out(PIN_DC, 1);
  prv_out(PIN_RST, 1);
  prv_out(PIN_CS1, 1);
  prv_out(PIN_CS2, 1);
  prv_in(PIN_BUSY1);
  prv_in(PIN_BUSY2);

  spi_bus_config_t bus = {
      .mosi_io_num = PIN_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = PIN_SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = SPI_CHUNK,
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
  spi_device_interface_config_t dev = {
      .clock_speed_hz = 20 * 1000 * 1000,
      .mode = 0,
      .spics_io_num = -1,
      .queue_size = 1,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));

  // 160 KiB of image planes in PSRAM: internal RAM is for the firmware's
  // thread stacks. The SPI driver bounces each chunk through internal RAM.
  for (int h = 0; h < 2; h++) {
    s_bw[h] = heap_caps_malloc(HALF_BYTES, MALLOC_CAP_SPIRAM);
    s_red[h] = heap_caps_malloc(HALF_BYTES, MALLOC_CAP_SPIRAM);
    assert(s_bw[h] != nullptr && s_red[h] != nullptr);
  }

  // The strong waveform: paper and ink drive (tables 0, 1) of group 1
  // repeated in groups 5-9, and hold (table 3) given the same frame counts.
  memcpy(s_lut, LUT_B_BASE, sizeof(s_lut));
  for (int g = 5; g < 10; g++) {
    for (int t = 0; t < 2; t++) {
      memcpy(s_lut + t * 80 + g * 8, LUT_B_BASE + t * 80 + 1 * 8, 8);
    }
    memcpy(s_lut + 3 * 80 + g * 8, LUT_B_BASE + 3 * 80 + 1 * 8, 8);
  }
}
