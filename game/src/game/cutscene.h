/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_CUTSCENE_H
#define FELIX_CUTSCENE_H

#include <stdint.h>
#include "bank.h"

extern uint8_t crash_pending;

void cut_setup(void) BANKED;
void cut_tick(void) BANKED;
void cut_render(void) BANKED;
uint8_t cut_done(void) BANKED;
void cut_free(void) BANKED;
void cut_draw(void) BANKED;
void cut_anim_tick(void) BANKED;
void lvlend_init(void) BANKED;
void lvlend_tick(void) BANKED;
void lvlend_render(void) BANKED;
uint8_t lvlend_done(void) BANKED;
void lvlend_free(void) BANKED;
void lvlend_draw(void) BANKED;
void lvlend_anim_tick(void) BANKED;
#endif
