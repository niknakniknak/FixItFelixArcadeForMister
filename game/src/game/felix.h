/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_FELIX_H
#define FELIX_FELIX_H

#include <stdint.h>
#include "bank.h"
#include "exe.h"

#define LEFT_LIMIT X16(felix_left_limit)
#define RIGHT_LIMIT X16(felix_right_limit)

void felix_create(void) BANKED;
void felix_level_start(void);
void felix_reset_flags(void);
void felix_start_walk_in(void);
void felix_set_state(uint8_t s);
void felix_kill(void);
void felix_update(void);
void felix_place_after_scroll(void);
void felix_draw(void);
int16_t felix_x(void);
int16_t felix_y(void);
extern fix_t box_x, box_y;
extern uint8_t box_w, box_h, box_felix_w;
uint8_t felix_hit(void);
uint8_t game_over_route(void);

#endif
