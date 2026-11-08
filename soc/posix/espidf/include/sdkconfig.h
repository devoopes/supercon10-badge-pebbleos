/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Stand-in for ESP-IDF's generated sdkconfig.h, for the firmware side of an
// ESP-IDF build. Only ESP-IDF's libc headers include it, for these. The
// application's own sdkconfig.h must agree (newlib, the default).
#define CONFIG_LIBC_NEWLIB    1
#define CONFIG_VFS_SUPPORT_IO 1
