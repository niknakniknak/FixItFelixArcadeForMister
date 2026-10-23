/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_RALPH_H
#define FELIX_RALPH_H

#include <stdint.h>
#include "bank.h"
#include "exe.h"

#define RALPH_CENTER X8(ralph_center)

void ralph_place(uint8_t row) BANKED;
void ralph_set_state(uint8_t s);
void ralph_update(void);
void ralph_freeze(void);
void ralph_reset(void);
void ralph_floor_done(void);
void ralph_level_done(void);
void ralph_draw(void);
void bricks_clear(void) BANKED;
void bricks_draw(void);
void bricks_tick(void);

#endif
