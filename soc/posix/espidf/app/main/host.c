/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The host side of PebbleOS on ESP-IDF: what soc/posix/host/main.c does for a
// native build. app_main is the "main thread" the driver bottoms' hooks run
// on; the firmware's kernel threads are pthreads, which ESP-IDF runs as
// FreeRTOS tasks.

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <posix_host.h>

#define MAX_HOOKS 16
#define POLL_MS   10

static const char *TAG = "pebbleos";

static const struct posix_host_hook *s_hooks[MAX_HOOKS];
static size_t s_num_hooks;
static TaskHandle_t s_main_task;

// Survives esp_restart(), as retained registers survive a watch's reset.
RTC_NOINIT_ATTR static uint32_t s_retained[POSIX_HOST_RETAINED_WORDS];
RTC_NOINIT_ATTR static uint32_t s_retained_magic;
#define RETAINED_MAGIC 0x50424c52U // "PBLR"

// Bottoms that need the main thread (input_bottom.c) register their hooks here.
void posix_host_option_register(const struct posix_host_option *option) {
  (void)option; // No command line on a badge.
}

void posix_host_hook_register(const struct posix_host_hook *hook) {
  if (s_num_hooks < MAX_HOOKS) {
    s_hooks[s_num_hooks++] = hook;
  }
}

void posix_host_wake(void) {
  if (s_main_task != nullptr) {
    xTaskNotifyGive(s_main_task);
  }
}

void posix_host_quit(void) {
  ESP_LOGW(TAG, "quit requested; restarting");
  esp_restart();
}

void posix_host_reboot(void) {
  fprintf(stderr, "\n*** reboot ***\n\n");
  fflush(stderr);
  s_retained_magic = RETAINED_MAGIC;
  esp_restart();
  for (;;) {
  }
}

uint32_t *posix_host_retained(void) {
  return s_retained;
}

uint64_t posix_host_monotonic_us(void) {
  return (uint64_t)esp_timer_get_time();
}

const char *posix_host_exe_dir(void) {
  return "/";
}

// Samsa's .pbw bottom (subsys/samsa/samsa_pbw_bottom.c on other hosts):
// a badge has no command line to install apps from, so the firmware only
// ever gets them from the phone, and never has a host buffer to give back.
void samsa_pbw_release(void *pbw) {
  free(pbw);
}

// Odds and ends of POSIX that newlib on ESP-IDF lacks.

int pause(void) {
  vTaskSuspend(nullptr);
  return -1;
}

int pthread_setname_np(pthread_t thread, const char *name) {
  (void)thread;
  (void)name;
  return 0;
}

// Cleanup handlers. newlib's pthread_cleanup_push() and _pop() call these,
// which ESP-IDF leaves unimplemented, and the posix kernel backend relies on
// one to hand the CPU back when a kernel thread ends. pthread_exit() is
// wrapped (-Wl,--wrap=pthread_exit) to run the pending ones.

static __thread struct _pthread_cleanup_context *t_cleanup;

void _pthread_cleanup_push(struct _pthread_cleanup_context *ctx, void (*routine)(void *),
                           void *arg) {
  ctx->_routine = routine;
  ctx->_arg = arg;
  ctx->_previous = t_cleanup;
  t_cleanup = ctx;
}

void _pthread_cleanup_pop(struct _pthread_cleanup_context *ctx, int execute) {
  t_cleanup = ctx->_previous;
  if (execute) {
    ctx->_routine(ctx->_arg);
  }
}

void __real_pthread_exit(void *value) __attribute__((noreturn));

void __wrap_pthread_exit(void *value) {
  while (t_cleanup != nullptr) {
    struct _pthread_cleanup_context *ctx = t_cleanup;
    t_cleanup = ctx->_previous;
    ctx->_routine(ctx->_arg);
  }
  __real_pthread_exit(value);
}

void app_main(void) {
  s_main_task = xTaskGetCurrentTaskHandle();

  // Retained words are only meaningful after our own reboot.
  if (s_retained_magic != RETAINED_MAGIC || esp_reset_reason() == ESP_RST_POWERON) {
    memset(s_retained, 0, sizeof(s_retained));
  }
  s_retained_magic = 0;

  ESP_LOGI(TAG, "starting PebbleOS (%u hooks)", (unsigned)s_num_hooks);

  for (size_t i = 0; i < s_num_hooks; i++) {
    if (s_hooks[i]->init != nullptr) {
      s_hooks[i]->init();
    }
  }

  posix_fw_start();

  for (;;) {
    for (size_t i = 0; i < s_num_hooks; i++) {
      if (s_hooks[i]->poll != nullptr) {
        s_hooks[i]->poll();
      }
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_MS));
  }
}
