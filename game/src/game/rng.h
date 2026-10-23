/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_RNG_H
#define FELIX_RNG_H

#include <stdint.h>
#include "exe.h"

#define DEMO_SEED XL(rng_demo_seed)

void rng_seed(uint32_t seed);
uint16_t rng(void);
uint32_t rng_entropy(void);

#endif
