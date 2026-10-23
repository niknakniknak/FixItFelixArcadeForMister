/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rng.h"

#define RNG_ADD 2531011UL

static uint32_t state;
static uint32_t entropy;

void rng_seed(uint32_t seed)
{
    state = seed;
}

uint16_t rng(void)
{
    uint32_t x = state;
    state = (x << 17) + (x << 16) + (x << 14) + (x << 10) - (x << 1) - x + RNG_ADD;
    entropy += state;
    return (uint16_t)(state >> 16) & 0x7FFF;
}

uint32_t rng_entropy(void)
{
    return entropy;
}
