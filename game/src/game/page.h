/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_PAGE_H
#define FELIX_PAGE_H

#include <stdint.h>

#define PG_FG_SHOW 0x01
#define PG_FG_CPU 0x02
#define PG_FG_BOTH 0x04
#define PG_BG_SHOW 0x08
#define PG_BG_CPU 0x10
#define PG_BG_BOTH 0x20

#define PG_FG 0
#define PG_BG 1

extern uint8_t pg_show;

void pg_init(void);
void pg_hidden(void);
void pg_bg_both(void);
void fg_touch(uint8_t row);
void fg_touch_rows(uint8_t first, uint8_t n);
void bg_touch_row(uint8_t row, uint8_t hidden);
void bg_touch_part(uint8_t row);
void pg_catch_up(uint8_t budget);
uint8_t pg_flips(void);
uint8_t pg_cpu_bits(uint8_t show);

#endif
