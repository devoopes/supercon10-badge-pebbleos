/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>

#include <pbl/drivers/rng.h>

#include <sys/random.h>

bool rng_rand(uint32_t *rand_out) {
#ifdef CONFIG_POSIX_ESPIDF
  // ESP-IDF has getrandom(), from the chip's hardware RNG, but no getentropy().
  return getrandom(rand_out, sizeof(*rand_out), 0) == (ssize_t)sizeof(*rand_out);
#else
  return getentropy(rand_out, sizeof(*rand_out)) == 0;
#endif
}
