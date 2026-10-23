/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_VIDEO_H
#define FELIX_VIDEO_H

#include <stdint.h>
#include "hw.h"
#include "gfx.h"

void map_fill(volatile uint16_t *map, uint16_t entry);
void map_draw(volatile uint16_t *map, uint8_t tilemap, uint8_t col, uint8_t row);

void text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s);
void text_hex8(uint8_t col, uint8_t row, uint8_t colour, uint8_t value);
void text_hex16(uint8_t col, uint8_t row, uint8_t colour, uint16_t value);

void spr_begin(uint8_t first, uint8_t count);
void spr_rect(uint8_t rect, int16_t x, int16_t y, uint8_t felix, uint8_t hammer);
void spr_frame(const anim_frame_t *frame, int16_t x, int16_t y, uint8_t mirrored,
               uint8_t felix, uint8_t hammer);
uint8_t spr_end(void);
void spr_big(uint8_t rect, int16_t x, int16_t y);

extern uint8_t spr_big_flags;

#endif
