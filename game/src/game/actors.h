/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_ACTORS_H
#define FELIX_ACTORS_H

#include <stdint.h>
#include "bank.h"

void birds_create(void);
void birds_update(void);
void birds_reset(void);
void birds_free_current(void);
void birds_draw(void);
void birds_tick(void);

void obstacles_create(void);
void obstacles_clear(void) BANKED;
void obstacles_update(void);
void obstacle_bonk(uint8_t row, uint8_t slot);
uint8_t obstacle_wobble(uint8_t row, uint8_t slot);

void chunk_spawn(uint8_t col);
void chunks_update(void);
void chunks_clear(void) BANKED;
void chunks_draw(void);

void pie_init(void) BANKED;
uint16_t pie_delay(void);
void pie_place(void);
void pie_hide(void) BANKED;
void pie_update(void);
void pie_draw(void);
void pie_tick(void);

#endif
