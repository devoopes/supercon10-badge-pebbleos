/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Hackaday Supercon 2026 Billboard badge, as a Flint. The firmware halves of
// the native drivers, with ESP-IDF bottoms (soc/posix/espidf/app): the console
// is the ESP32-S3's USB serial port, the display is the 9.7" e-paper panel,
// the buttons are the badge's four, and the flash is a partition of the
// chip's. The rest is simulated as on native_flint.

#include <pbl/board.h>
#include <pbl/drivers/ambient/posix.h>
#include <pbl/drivers/audio/qemu.h>
#include <pbl/drivers/backlight/posix.h>
#include <pbl/drivers/display/sdl.h>
#include <pbl/drivers/flash/espidf.h>
#include <pbl/drivers/imu/posix.h>
#include <pbl/drivers/rtc/posix.h>
#include <pbl/drivers/temperature/posix.h>
#include <pbl/drivers/uart/posix.h>
#include <pbl/drivers/vibe/posix.h>

PBL_UART_POSIX_DEFINE(s_dbg_uart, "console", PBL_UART_POSIX_CONSOLE, 115200);
const struct pbl_uart *const CONSOLE_UART = &s_dbg_uart.uart;

// The phone link of the emulator; not connected yet.
PBL_UART_POSIX_DEFINE(s_qemu_uart, "qemu", PBL_UART_POSIX_QEMU, 115200);
const struct pbl_uart *const QEMU_UART = &s_qemu_uart.uart;

PBL_DISPLAY_SDL_DEFINE(s_display, "display", CONFIG_DISPLAY_SDL_WIDTH, CONFIG_DISPLAY_SDL_HEIGHT,
                       PBL_DISPLAY_FORMAT_MONO, .round_mask = nullptr);

const struct pbl_display *const DISPLAY = &s_display;

// Silent: there is no microphone.
PBL_AUDIO_HOST_MIC_DEFINE(s_mic, "mic");

const struct pbl_audio *const MIC = &s_mic.audio;

const struct pbl_accel *const ACCEL = POSIX_ACCEL;

const struct pbl_als *const ALS = POSIX_ALS;

const struct pbl_backlight *const BACKLIGHT = POSIX_BACKLIGHT;

const struct pbl_vibe *const VIBE = POSIX_VIBE;

const struct pbl_temp *const DIE_TEMP = POSIX_TEMP;

const struct pbl_rtc *const RTC = POSIX_RTC;

const struct pbl_flash *const FLASH = ESPIDF_FLASH;
