/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_FIXED_H
#define FELIX_FIXED_H

#include <stdint.h>
#include "exe.h"

typedef int32_t fix_t;

#define FIX_ONE 65536L
#define FIX(i) ((fix_t)(i) * FIX_ONE)

#define FIX_0_1 XL(fixed_fix_0_1)
#define FIX_0_2 XL(fixed_fix_0_2)
#define FIX_0_33 XL(fixed_fix_0_33)
#define FIX_0_4 XL(fixed_fix_0_4)
#define FIX_0_5 XL(fixed_fix_0_5)
#define FIX_1_3 XL(fixed_fix_1_3)
#define FIX_1_9 XL(fixed_fix_1_9)

int16_t fix_trunc(fix_t v);
int16_t fix_round(fix_t v);

#endif
