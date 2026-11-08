/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_runtime.h"

#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>

#include <process_management/pebble_process_md.h>

//! This comes from the generated pebble.auto.c with all the exported functions in it.
extern const void *const g_pbl_system_tbl[];

PBL_WEAK uint32_t process_runtime_address(PebbleTask task, const void *address) {
  return (uint32_t)(uintptr_t)address;
}

PBL_WEAK uint32_t process_runtime_sdk_table(PebbleTask task) {
  return (uint32_t)(uintptr_t)&g_pbl_system_tbl;
}

PBL_WEAK void process_runtime_run(const PebbleProcessMd *app_md, PebbleTask task, void *entry,
                                  void *stack_top) {
#ifndef __arm__
  // Installed apps are Thumb-2 code, which only an interpreter (Samsa) can
  // run on another CPU: jumping into them would execute data.
  if (app_md->process_storage != ProcessStorageBuiltin) {
    PBL_LOG_ERR("can't run an installed app without an ARM interpreter");
    return;
  }
#endif
  const PebbleMain main_func = entry;
  main_func();
}
