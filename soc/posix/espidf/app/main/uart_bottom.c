/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The UART bottoms: the console is the ESP32-S3's USB serial port. The other
// channels (the emulator's phone link, Bluetooth HCI) are not connected yet,
// so what is written to them is dropped.

#include <stdbool.h>
#include <stdint.h>

#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <uart_posix_bottom.h>

static bool s_console_started;

static void prv_console_rx_task(void *arg) {
  uint8_t buf[64];
  for (;;) {
    int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), portMAX_DELAY);
    for (int i = 0; i < n; i++) {
      // The firmware holds bytes back while its receive interrupt is off.
      while (!uart_posix_rx_enabled(UART_POSIX_CONSOLE)) {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
      uart_posix_rx(UART_POSIX_CONSOLE, buf[i]);
    }
  }
}

void uart_posix_bottom_start(enum uart_posix_channel channel) {
  if (channel != UART_POSIX_CONSOLE || s_console_started) {
    return;
  }
  s_console_started = true;

  usb_serial_jtag_driver_config_t config = {
      .tx_buffer_size = 4096,
      .rx_buffer_size = 1024,
  };
  if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
    return;
  }
  xTaskCreate(prv_console_rx_task, "console_rx", 4096, nullptr, 5, nullptr);
}

void uart_posix_bottom_write(enum uart_posix_channel channel, uint8_t c) {
  if (channel != UART_POSIX_CONSOLE || !s_console_started) {
    return;
  }
  // Don't wait when nobody is listening on the USB port.
  usb_serial_jtag_write_bytes(&c, 1, 0);
}
