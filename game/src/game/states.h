/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_STATES_H
#define FELIX_STATES_H

#include <stdint.h>
#include "bank.h"

void attract_enter(uint8_t s) BANKED;
void attract_tick(void) BANKED;
void attract_draw(void) BANKED;
void attract_render(void) BANKED;
void attract_anim_tick(void) BANKED;
void game_init(void);
void game_reset(void);
void game_frame(void);
uint8_t vblank_count(void);
uint8_t present_busy(void);

#endif
