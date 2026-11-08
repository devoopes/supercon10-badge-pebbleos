/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The Bluetooth HCI bottom: the firmware's NimBLE host talks H4 over its
// BT_HCI_UART channel (third_party/nimble/transport/hci_uart.c), and this
// carries it to the ESP32-S3's own BLE controller through ESP-IDF's VHCI,
// which takes and gives whole H4 packets.
//
// Host to controller: bytes come one at a time; a packet goes out once its
// header says it is complete. Controller to host: packets are queued as they
// come, and fed to the firmware byte by byte while it takes them.

#include <stdbool.h>
#include <stdint.h>

#include "esp_bt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <posix_host.h>
#include <uart_posix_bottom.h>

#include "bt_bottom.h"

static const char *TAG = "bt";

#define H4_CMD 0x01
#define H4_ACL 0x02
#define H4_SCO 0x03

#define TX_MAX      (5 + 1024)
#define RX_BUF_SIZE 8192

static bool s_ready;
static uint8_t s_tx[TX_MAX];
static size_t s_tx_len;
static size_t s_tx_need;
static StreamBufferHandle_t s_rx;

// --- Host to controller ------------------------------------------------------

static void prv_send(void) {
  for (int i = 0; !esp_vhci_host_check_send_available(); i++) {
    if (i > 1000) {
      ESP_LOGE(TAG, "controller not taking packets; dropped one");
      return;
    }
    vTaskDelay(1);
  }
  esp_vhci_host_send_packet(s_tx, s_tx_len);
}

void bt_bottom_write(uint8_t c) {
  if (!s_ready) {
    return;
  }
  if (s_tx_len >= sizeof(s_tx)) {
    ESP_LOGE(TAG, "H4 packet too long; resynchronising");
    s_tx_len = 0;
    s_tx_need = 0;
  }
  s_tx[s_tx_len++] = c;

  if (s_tx_len == 1) {
    if (c != H4_CMD && c != H4_ACL && c != H4_SCO) {
      ESP_LOGE(TAG, "unknown H4 type 0x%02x", c);
      s_tx_len = 0;
    }
    return;
  }
  // The header gives the length: command 3 bytes, ACL 4, SCO 3.
  if (s_tx_need == 0) {
    switch (s_tx[0]) {
      case H4_CMD:
      case H4_SCO:
        if (s_tx_len == 4) {
          s_tx_need = 4 + s_tx[3];
        }
        break;
      case H4_ACL:
        if (s_tx_len == 5) {
          s_tx_need = 5 + (s_tx[3] | (s_tx[4] << 8));
        }
        break;
    }
  }
  if (s_tx_need != 0 && s_tx_len == s_tx_need) {
    prv_send();
    s_tx_len = 0;
    s_tx_need = 0;
  }
}

// --- Controller to host ------------------------------------------------------

static void prv_host_send_available(void) {
}

// From the controller's task: queue, and let prv_rx_task hand it over.
static int prv_host_recv(uint8_t *data, uint16_t len) {
  if (xStreamBufferSend(s_rx, data, len, pdMS_TO_TICKS(200)) != len) {
    ESP_LOGE(TAG, "receive queue full; dropped %u bytes", len);
  }
  return 0;
}

static const esp_vhci_host_callback_t s_vhci_cb = {
  .notify_host_send_available = prv_host_send_available,
  .notify_host_recv = prv_host_recv,
};

static void prv_rx_task(void *arg) {
  uint8_t buf[128];
  for (;;) {
    size_t n = xStreamBufferReceive(s_rx, buf, sizeof(buf), portMAX_DELAY);
    for (size_t i = 0; i < n; i++) {
      // The firmware holds bytes back while its ring is full, and until its
      // transport has started.
      while (!uart_posix_rx_enabled(UART_POSIX_BT_HCI)) {
        vTaskDelay(pdMS_TO_TICKS(2));
      }
      uart_posix_rx(UART_POSIX_BT_HCI, buf[i]);
    }
  }
}

// --- Start -------------------------------------------------------------------

// On the main thread, before the firmware starts.
static void prv_init(void) {
  // The PHY keeps its calibration in NVS.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "NVS: %s", esp_err_to_name(err));
  }

  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  if ((err = esp_bt_controller_init(&cfg)) != ESP_OK ||
      (err = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
    ESP_LOGE(TAG, "controller: %s", esp_err_to_name(err));
    return;
  }

  s_rx = xStreamBufferCreate(RX_BUF_SIZE, 1);
  xTaskCreate(prv_rx_task, "bt_rx", 4096, nullptr, 6, nullptr);
  esp_vhci_host_register_callback(&s_vhci_cb);
  s_ready = true;
  ESP_LOGI(TAG, "BLE controller up");
}

POSIX_HOST_HOOK(.init = prv_init)
