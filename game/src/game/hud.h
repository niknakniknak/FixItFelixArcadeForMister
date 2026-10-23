/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_HUD_H
#define FELIX_HUD_H

#include <stdint.h>
#include "bank.h"

#define C_DARK_BLUE 0
#define C_BLUE 1
#define C_GREEN 2
#define C_CYAN 3
#define C_YELLOW 4
#define C_RED 5
#define C_WHITE 6

#define TEXT_LAST_ROW 39

extern uint32_t hiscore;

void fg_clear(void) BANKED;
void fg_clear_rows(uint8_t first, uint8_t last) BANKED;
void fg_band(void) BANKED;
void fg_picture(uint8_t tilemap, uint8_t col, uint8_t row) BANKED;
void garbage_fill(uint16_t offset);
uint8_t rom_sum_slice(uint8_t slice);
void print_num(uint8_t col, uint8_t row, uint8_t colour, uint32_t value, uint8_t digits) BANKED;
void opaque_print(uint8_t col, uint8_t row, uint8_t colour, const char *s) BANKED;
void print_blank(uint8_t col, uint8_t row, uint8_t count) BANKED;
void hud_reset(void) BANKED;
void hud_hiscore(void) BANKED;
void hud_score(uint8_t always) BANKED;
void hud_lives(void) BANKED;
void hud_credits(void) BANKED;
uint8_t pause_update(void) BANKED;
void hud_switch_text(uint8_t out_other) BANKED;

#endif
