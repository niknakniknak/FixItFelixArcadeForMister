/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_BG_H
#define FELIX_BG_H

#include <stdint.h>

#define BG_PICS 9
#define BG_PIC_GROUND 8
#define HUD_HEIGHT X8(bg_hud_height)
#define FG_PICS 5

typedef struct {
    uint8_t tm;
    uint8_t on;
    int8_t col;
    int16_t row;
} bg_pic_t;

extern bg_pic_t bg_pics[BG_PICS];
extern uint8_t ground_on;

void bg_reset(void);
void bg_invalidate(void);
void bg_rows_dirty(int16_t top, uint8_t count);
void bg_special(int16_t top, uint8_t count, uint8_t tm, uint8_t old, uint8_t col);
void bg_pic_set(uint8_t i, uint8_t tm, int8_t col, int16_t row);
void bg_pic_off(uint8_t i);
void bg_sync(void);
void fg_pic_set(uint8_t i, uint8_t tm, int8_t col, int8_t row);
void fg_pic_off(uint8_t i);
void fg_pics_forget(void);
void fg_pics_sync(void);
void bg_shown(uint8_t sx, uint16_t sy);

extern int8_t bg_dy;

#endif
