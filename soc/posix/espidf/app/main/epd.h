/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// The Billboard badge's 9.7" Pervasive Displays E2969JS0B1 (B1, "driver B"):
// 960 x 672, two chip-on-glass controllers for the left and right halves.
// Measured on a badge: a pull with the short waveform takes 1.16 s, and
// powering up another 2.2 s.

#define EPD_WIDTH  960
#define EPD_HEIGHT 672

// Pixel selectors, the waveform table index: (red << 1) | black.
enum epd_sel {
  EPD_WHITE = 0,
  EPD_BLACK = 1,
  EPD_RED = 2,
  EPD_HOLD = 3, // ground throughout: the pixel keeps what it shows
};

void epd_init(void);

//! Sets every pixel of the next image.
void epd_fill(enum epd_sel sel);

//! Sets one pixel of the next image.
void epd_set(int x, int y, enum epd_sel sel);

//! Powers the panel up with the next image loaded and the short waveform.
void epd_power_on(void);

//! Sends the next image to a powered panel.
void epd_send(void);

//! Runs the waveform once, moving every pixel toward its selector. Returns
//! the time it took in ms, negative on a timeout.
int epd_pull(void);

//! Powers the panel down; it keeps showing what it shows.
void epd_power_off(void);

bool epd_is_powered(void);
