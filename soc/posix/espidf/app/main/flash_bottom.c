/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The flash bottom: PebbleOS's external flash is the "pebble" partition.

#include <errno.h>

#include "esp_partition.h"

#include <espidf_host.h>

static const esp_partition_t *prv_partition(void) {
  static const esp_partition_t *s_part;
  if (s_part == nullptr) {
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                      "pebble");
  }
  return s_part;
}

uint32_t espidf_flash_size(void) {
  const esp_partition_t *part = prv_partition();
  return part != nullptr ? part->size : 0U;
}

int espidf_flash_read(uint32_t addr, void *buf, size_t len) {
  return esp_partition_read(prv_partition(), addr, buf, len) == ESP_OK ? 0 : -EIO;
}

int espidf_flash_write(uint32_t addr, const void *buf, size_t len) {
  return esp_partition_write(prv_partition(), addr, buf, len) == ESP_OK ? 0 : -EIO;
}

int espidf_flash_erase(uint32_t addr, uint32_t len) {
  return esp_partition_erase_range(prv_partition(), addr, len) == ESP_OK ? 0 : -EIO;
}
