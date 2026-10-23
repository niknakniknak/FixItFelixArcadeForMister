/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_BLDG_H
#define FELIX_BLDG_H

#include <stdint.h>
#include "exe.h"

#define ROW_GROUND 0
#define ROW_GROUND2 1
#define ROW_UPPER_A 2
#define ROW_UPPER_B 3
#define ROW_ROOF 4

#define GROUND_Y X16(bldg_ground_y)
#define BUILDING_BASE_TILE X8(bldg_base_tile)

extern uint8_t rows_shown;
extern uint8_t building_on;

void level_load(void);
void bldg_create(void);
uint8_t row_type(uint8_t row);
uint8_t row_height(uint8_t row);
int16_t row_top_y(uint8_t row);
int16_t row_bottom_y(uint8_t row);
uint8_t slot_kind(uint8_t row, uint8_t slot);
uint8_t slot_tx(uint8_t row, uint8_t slot);
int16_t win_left(uint8_t row, uint8_t slot);
int16_t win_right(uint8_t row, uint8_t slot);
int16_t win_center(uint8_t row, uint8_t slot);
int16_t win_sill(uint8_t row, uint8_t slot);
int16_t pane_x(uint8_t row, uint8_t slot);
int16_t pane_y(uint8_t row, uint8_t lower);
uint8_t win_active(uint8_t row);
uint8_t row_current(uint8_t row);
uint8_t win_lower(uint8_t row, uint8_t slot);
uint8_t win_state(uint8_t row, uint8_t slot);
uint8_t win_repairing(uint8_t row, uint8_t slot);

void window_damage(uint8_t row, uint8_t slot, uint8_t heavy, uint8_t sound);
void window_fix(uint8_t row, uint8_t slot);
void windows_update(void);
void floor_damage_top(void);

void shards_clear(void);
void shards_update(void);
void shards_draw(void);
void popups_draw(void);
extern uint8_t statics_dirty;
void statics_row(uint8_t row);
void statics_update(uint8_t on);
void panes_init(void);
void doors_refresh(void);
uint8_t special_tile(uint8_t row, uint8_t slot);

#endif
