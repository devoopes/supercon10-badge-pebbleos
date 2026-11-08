# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# ESP-IDF's Xtensa toolchain, for boards whose host is ESP-IDF on an ESP32-S3
# (CONFIG_POSIX_ESPIDF). The firmware is compiled against the toolchain's
# newlib with ESP-IDF's additions to it (pthread.h, sys/lock.h, ...), the
# libc the ESP-IDF application links it with. IDF_PATH comes from ESP-IDF's
# export.sh, or is passed with -DIDF_PATH=.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR xtensa)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT IDF_PATH)
  set(IDF_PATH $ENV{IDF_PATH})
endif()
if(NOT IDF_PATH OR NOT EXISTS ${IDF_PATH}/components/newlib/platform_include)
  message(FATAL_ERROR "ESP-IDF not found: source its export.sh, or pass -DIDF_PATH=")
endif()
set(IDF_PATH ${IDF_PATH} CACHE PATH "ESP-IDF" FORCE)

set(espidf_tools $ENV{IDF_TOOLS_PATH})
if(NOT espidf_tools)
  set(espidf_tools $ENV{HOME}/.espressif)
endif()
file(GLOB espidf_gcc_hints ${espidf_tools}/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin)
find_program(CMAKE_C_COMPILER xtensa-esp32s3-elf-gcc HINTS ${espidf_gcc_hints} REQUIRED)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
find_program(CMAKE_AR xtensa-esp32s3-elf-ar HINTS ${espidf_gcc_hints} REQUIRED)
find_program(CMAKE_RANLIB xtensa-esp32s3-elf-ranlib HINTS ${espidf_gcc_hints} REQUIRED)

# What ESP-IDF compiles its own components with: calls that reach across the
# address space, and volatile bitfields accessed at their declared width.
set(CMAKE_C_FLAGS_INIT "-mlongcalls -fstrict-volatile-bitfields")
set(CMAKE_ASM_FLAGS_INIT "-mlongcalls")

# ESP-IDF's libc headers, and a stand-in for the sdkconfig.h they include.
include_directories(SYSTEM
  ${CMAKE_CURRENT_LIST_DIR}/../../soc/posix/espidf/include
  ${IDF_PATH}/components/newlib/platform_include
)
