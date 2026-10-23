/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fixed.h"

int16_t fix_trunc(fix_t v)
{
    if (v >= 0)
        return (int16_t)(v >> 16);
    return -(int16_t)((-v) >> 16);
}

int16_t fix_round(fix_t v)
{
    return fix_trunc(v + 32768L);
}
