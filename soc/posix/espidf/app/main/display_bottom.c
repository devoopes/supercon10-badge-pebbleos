/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The display bottom: the firmware's 144 x 168 Flint frames, 4x on the
// 960 x 672 e-paper panel (576 x 672, its full height), centred.
//
// The panel takes about 1.2 s per update, far longer than the firmware takes
// to draw, so frames are coalesced: a task of its own shows the latest one,
// and whatever came in meanwhile is skipped. Only pixels that changed are
// driven, the rest held. The panel stays powered while frames keep coming,
// and powers down after a quiet spell; e-paper keeps its image unpowered.
// Pixels driven white keep a light ghost of what they showed, so every so
// often the whole screen is cleaned.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <display_sdl_bottom.h>
#include <posix_host.h>

#include "epd.h"

static const char *TAG = "display";

#define FB_W        144
#define FB_H        168
#define SCALE       4
#define X0          ((EPD_WIDTH - FB_W * SCALE) / 2)
#define Y0          ((EPD_HEIGHT - FB_H * SCALE) / 2)
#define IDLE_OFF_MS 4000
#define CLEAN_EVERY 40
#define CLEAN_PULLS 4

static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
EXT_RAM_BSS_ATTR static uint8_t s_pending[FB_H][FB_W]; // 1 = black, under s_lock
static bool s_has_pending;
EXT_RAM_BSS_ATTR static uint8_t s_shown[FB_H][FB_W];
EXT_RAM_BSS_ATTR static uint8_t s_next[FB_H][FB_W];
static bool s_panel_known; // s_shown is what the panel shows
static int s_updates;

// ARGB2222; a Flint draws black and white only.
static inline uint8_t prv_is_black(uint8_t argb) {
  int sum = ((argb >> 4) & 3) + ((argb >> 2) & 3) + (argb & 3);
  return sum < 5;
}

void display_sdl_bottom_update(const uint8_t *fb, int width, int height) {
  if (width != FB_W || height != FB_H || s_lock == nullptr) {
    return;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  for (int y = 0; y < FB_H; y++) {
    for (int x = 0; x < FB_W; x++) {
      s_pending[y][x] = prv_is_black(fb[y * FB_W + x]);
    }
  }
  s_has_pending = true;
  xSemaphoreGive(s_lock);
  xTaskNotifyGive(s_task);
}

void display_sdl_bottom_set_sunlight(bool sunlight) {
  (void)sunlight;
}

bool display_sdl_bottom_sunlight(void) {
  return false;
}

static void prv_block(int x, int y, enum epd_sel sel) {
  for (int dy = 0; dy < SCALE; dy++) {
    for (int dx = 0; dx < SCALE; dx++) {
      epd_set(X0 + x * SCALE + dx, Y0 + y * SCALE + dy, sel);
    }
  }
}

static void prv_power_on(void) {
  if (!epd_is_powered()) {
    epd_fill(EPD_HOLD);
    epd_power_on();
  }
}

// Whole screen to white a few times, then everything drawn afresh.
static void prv_clean(void) {
  ESP_LOGI(TAG, "cleaning");
  epd_fill(EPD_WHITE);
  if (epd_is_powered()) {
    epd_send();
  } else {
    epd_power_on();
  }
  for (int i = 0; i < CLEAN_PULLS; i++) {
    epd_pull();
  }
  memset(s_shown, 0, sizeof(s_shown));
  s_panel_known = true;
}

static void prv_show(void) {
  bool full = !s_panel_known || ++s_updates % CLEAN_EVERY == 0;
  if (full) {
    prv_clean();
  } else {
    prv_power_on();
  }

  int changed = 0;
  epd_fill(EPD_HOLD);
  for (int y = 0; y < FB_H; y++) {
    for (int x = 0; x < FB_W; x++) {
      if (s_next[y][x] != s_shown[y][x]) {
        prv_block(x, y, s_next[y][x] ? EPD_BLACK : EPD_WHITE);
        changed++;
      }
    }
  }
  if (changed == 0) {
    return;
  }
  epd_send();
  int ms = epd_pull();
  memcpy(s_shown, s_next, sizeof(s_shown));
  ESP_LOGI(TAG, "update %d: %d pixels, %d ms", s_updates, changed, ms);
}

static void prv_task(void *arg) {
  epd_init();
  for (;;) {
    uint32_t woken = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IDLE_OFF_MS));
    if (woken == 0) {
      if (epd_is_powered()) {
        epd_power_off();
        ESP_LOGI(TAG, "panel off");
      }
      continue;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool has = s_has_pending;
    memcpy(s_next, s_pending, sizeof(s_next));
    s_has_pending = false;
    xSemaphoreGive(s_lock);
    if (has) {
      prv_show();
    }
  }
}

// Started before the firmware, which draws its first frame early.
static void prv_start(void) {
  s_lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(prv_task, "epd", 6144, nullptr, 4, &s_task, 1);
}

POSIX_HOST_HOOK(.init = prv_start)
