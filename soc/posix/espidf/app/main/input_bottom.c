/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The input bottom: the badge's four buttons as the watch's four. Read on the
// main thread's 10 ms turn, and debounced over two of them.
//
//   Menu +  (GPIO 40) -> Up
//   Menu -  (GPIO 15) -> Down
//   A       (GPIO 39) -> Select
//   B       (GPIO 16) -> Back

#include <stdbool.h>

#include "driver/gpio.h"

#include <input_sdl_bottom.h>
#include <posix_host.h>

static const struct {
  int pin;
  int key;
} s_buttons[] = {
  {40, INPUT_SDL_KEY_UP},
  {15, INPUT_SDL_KEY_DOWN},
  {39, INPUT_SDL_KEY_SELECT},
  {16, INPUT_SDL_KEY_BACK},
};

#define NUM_BUTTONS (sizeof(s_buttons) / sizeof(s_buttons[0]))

static bool s_pressed[NUM_BUTTONS];
static bool s_last_raw[NUM_BUTTONS];

static void prv_init(void) {
  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << s_buttons[i].pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&c);
  }
}

static void prv_poll(void) {
  for (size_t i = 0; i < NUM_BUTTONS; i++) {
    bool raw = gpio_get_level(s_buttons[i].pin) == 0;
    if (raw == s_last_raw[i] && raw != s_pressed[i]) {
      s_pressed[i] = raw;
      input_sdl_key_changed(s_buttons[i].key, raw);
    }
    s_last_raw[i] = raw;
  }
}

POSIX_HOST_HOOK(.init = prv_init, .poll = prv_poll)
