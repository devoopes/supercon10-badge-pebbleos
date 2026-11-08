/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

// Host services of an ESP-IDF build (CONFIG_POSIX_ESPIDF), beyond what
// posix_host.h offers. Implemented by the ESP-IDF application in
// soc/posix/espidf/app, which sees none of the firmware headers.

//! Size of the flash partition that holds the board's partitions.
uint32_t espidf_flash_size(void);

//! Reads @p len bytes at @p addr in that partition. 0 or a negative errno.
int espidf_flash_read(uint32_t addr, void *buf, size_t len);

//! Programs @p len bytes at @p addr: like NOR flash, only clears bits.
int espidf_flash_write(uint32_t addr, const void *buf, size_t len);

//! Erases @p len bytes at @p addr, both multiples of 4 KiB.
int espidf_flash_erase(uint32_t addr, uint32_t len);
