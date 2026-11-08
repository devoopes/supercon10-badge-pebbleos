# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The ESP-IDF application that runs PebbleOS on the Billboard badge. It links
# the firmware built by `pbl -b <dir> build` for the billboard board
# (libpebbleos.a), and provides the host side: main, and the driver bottoms.
#
#   . $IDF_PATH/export.sh
#   idf.py -DPBL_BUILD_DIR=<pebbleos build dir> build flash monitor

cmake_minimum_required(VERSION 3.16)

if(NOT PBL_BUILD_DIR)
  set(PBL_BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../build-badge)
endif()
get_filename_component(PBL_BUILD_DIR ${PBL_BUILD_DIR} ABSOLUTE)
set(PBL_BUILD_DIR ${PBL_BUILD_DIR} CACHE PATH "PebbleOS build directory (billboard board)")

# Only what main needs: the firmware brings its own mbedTLS and Bluetooth.
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pebbleos_billboard)

# The system resources go to their bank in the "pebble" partition
# (PART_SYSTEM_RESOURCES_BANK_0_OFFSET in partitions_billboard.h).
partition_table_get_partition_info(pebble_offset "--partition-name pebble" "offset")
math(EXPR resources_offset "${pebble_offset} + 0x10000" OUTPUT_FORMAT HEXADECIMAL)
esptool_py_flash_target_image(flash resources ${resources_offset}
                              ${PBL_BUILD_DIR}/system_resources.pbpack)
