# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(syscalls.c)
# ESP-IDF's newlib has its own, which its panic handler reports.
if(NOT CONFIG_POSIX_ESPIDF)
  pbl_library_sources(assert.c)
endif()
if(CONFIG_LIBC_NEWLIB_NANO)
  add_subdirectory(newlib)
endif()
