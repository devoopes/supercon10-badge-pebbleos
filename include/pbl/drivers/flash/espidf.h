/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_flash_espidf ESP-IDF
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for an ESP-IDF build: a partition of the chip's own flash.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash pbl_flash_espidf;
/** @endcond */

/** @brief The external flash of an ESP-IDF build. */
#define ESPIDF_FLASH (&pbl_flash_espidf)

/** @} */
