/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/board/partitions.h>
#include <pbl/drivers/flash/espidf.h>
#include <pbl/logging/logging.h>

#include <espidf_host.h>

// The external flash of an ESP-IDF build is a partition of the chip's own
// flash, read and written through the host (espidf_host.h). The system
// resources are written to their bank when the application is flashed.

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

static struct pbl_flash_geometry s_geometry = {
  .size = 0U,
  .page_size = PAGE_SIZE_BYTES,
  .sector_size = SECTOR_SIZE_BYTES,
  .subsector_size = SUBSECTOR_SIZE_BYTES,
  .sector_erase_ms = 50U,
  .subsector_erase_ms = 50U,
};

static int prv_init(const struct pbl_flash *flash) {
  s_geometry.size = espidf_flash_size();
  if (s_geometry.size == 0U) {
    PBL_LOG_ERR("no flash partition for the firmware");
    return -ENODEV;
  }
  PBL_LOG_INFO("flash: %lu KiB", (unsigned long)(s_geometry.size / 1024U));
  return 0;
}

static int prv_read(const struct pbl_flash *flash, uint32_t addr, void *buf, size_t len) {
  return espidf_flash_read(addr, buf, len);
}

static int prv_write(const struct pbl_flash *flash, uint32_t addr, const void *buf, size_t len) {
  return espidf_flash_write(addr, buf, len);
}

static int prv_erase(const struct pbl_flash *flash, uint32_t addr, uint32_t size) {
  return espidf_flash_erase(addr, size);
}

static const struct pbl_flash_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
  .write = prv_write,
  .erase = prv_erase,
};

PBL_FLASH_STATE_DEFINE(pbl_flash_espidf);
const struct pbl_flash pbl_flash_espidf = PBL_FLASH_INIT(
    pbl_flash_espidf, "flash", nullptr, nullptr, &s_ops, &s_geometry, nullptr, nullptr);
PBL_DEVICE_REGISTER(pbl_flash_espidf, &pbl_flash_espidf.dev);
