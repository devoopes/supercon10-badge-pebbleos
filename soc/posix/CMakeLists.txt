# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(include)
pbl_library_sources(
  core_dump.c
  memory.c
  soc.c
)

pbl_host_include_directories(include)

# On ESP-IDF the application in espidf/app is the host: it has its own main
# and driver bottoms, and the SDL drivers keep only their firmware halves.
if(CONFIG_POSIX_ESPIDF)
  return()
endif()

pbl_host_sources(host/main.c)

if(CONFIG_SDL)
  add_subdirectory(sdl)
endif()
