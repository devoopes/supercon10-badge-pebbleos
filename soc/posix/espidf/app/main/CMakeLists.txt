# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(pbl_base ${CMAKE_CURRENT_LIST_DIR}/../../../../..)

idf_component_register(
  SRCS
    host.c
    uart_bottom.c
    flash_bottom.c
    display_bottom.c
    input_bottom.c
    epd.c
    bt_bottom.c
  INCLUDE_DIRS "."
  PRIV_INCLUDE_DIRS
    ${pbl_base}/soc/posix/include
    ${pbl_base}/drivers/display/sdl
    ${pbl_base}/drivers/input/sdl
    ${pbl_base}/drivers/uart/posix
  PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_usb_serial_jtag esp_partition esp_psram
                esp_timer pthread bt nvs_flash
  LDFRAGMENTS pebbleos.lf
  # The firmware calls the bottoms, and comes later on the link line.
  WHOLE_ARCHIVE
)

# The panel's waveform comes from the badge firmware, a submodule; lut_b.h is
# generated from it rather than kept here.
set(badge_luts
    ${pbl_base}/third_party/billboard_badge/2026-Billboard_Badge/firmware/badge/data/rendered_luts.py)
if(NOT EXISTS ${badge_luts})
  message(FATAL_ERROR "${badge_luts} is missing: git submodule update --init "
                      "third_party/billboard_badge/2026-Billboard_Badge")
endif()
idf_build_get_property(python PYTHON)
set(lut_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_custom_command(
  OUTPUT ${lut_dir}/lut_b.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${lut_dir}
  COMMAND ${python} ${CMAKE_CURRENT_LIST_DIR}/gen_lut.py ${badge_luts} ${lut_dir}/lut_b.h
  DEPENDS ${CMAKE_CURRENT_LIST_DIR}/gen_lut.py ${badge_luts}
  VERBATIM
)
add_custom_target(billboard_lut DEPENDS ${lut_dir}/lut_b.h)
add_dependencies(${COMPONENT_LIB} billboard_lut)
target_include_directories(${COMPONENT_LIB} PRIVATE ${lut_dir})

# Every object, for the constructors and the sections nothing refers to by
# name; --gc-sections then drops what is unused. One argument, so that CMake
# cannot merge its flags with ESP-IDF's own; and a link option, which comes
# before the libraries, so that what the firmware brings (mbedTLS) is defined
# before ESP-IDF's libraries would pull their own copies in.
target_link_options(${COMPONENT_LIB} INTERFACE
  "-Wl,--whole-archive,${PBL_BUILD_DIR}/libpebbleos.a,--no-whole-archive")
# Also as a library ESP-IDF knows, so that pebbleos.lf applies to it; by
# then everything in it is linked, and this adds nothing more.
add_prebuilt_library(pebbleos ${PBL_BUILD_DIR}/libpebbleos.a)
target_link_libraries(${COMPONENT_LIB} INTERFACE pebbleos)
set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_LINK_DEPENDS
             ${PBL_BUILD_DIR}/libpebbleos.a)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu23)

# The registries' bounds under the names the firmware expects (pebbleos.lf).
foreach(reg pbl_devices pbl_input_cb pbl_log_be pbl_shroot pbl_shset pbl_shsub pbl_sys_off)
  target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--defsym=__start_${reg}=_${reg}_start" "-Wl,--defsym=__stop_${reg}=_${reg}_end")
endforeach()
# Cleanup handlers, which ESP-IDF's pthreads lack (see host.c).
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pthread_exit")
