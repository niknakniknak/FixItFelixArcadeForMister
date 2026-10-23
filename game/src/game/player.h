/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_PLAYER_H
#define FELIX_PLAYER_H

#include <stdint.h>

void player_select(uint8_t n);
uint8_t player_out_other(void);
void player_new(void);
void game_start(uint8_t n);
void demo_setup(void);
void level_advance(void);
void level_build(void);
void player_free(void);

#endif
