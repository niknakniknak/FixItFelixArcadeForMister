/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_TMAP_H
#define FELIX_TMAP_H

#include <stdint.h>

void tm_row(uint8_t tm, uint8_t src_row, volatile uint16_t *dst, int16_t col, int16_t lo, int16_t hi,
            uint8_t opaque);
void tm_clear(volatile uint16_t *dst, int16_t lo, int16_t hi);
void tm_draw(volatile uint16_t *map, uint8_t tm, uint8_t col, uint8_t row);

#endif
