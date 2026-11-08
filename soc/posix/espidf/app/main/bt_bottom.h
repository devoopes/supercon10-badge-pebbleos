/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! A byte of H4 from the firmware's NimBLE host, for the BLE controller.
void bt_bottom_write(uint8_t c);
