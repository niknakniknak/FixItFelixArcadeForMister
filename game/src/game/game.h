/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_GAME_H
#define FELIX_GAME_H

#include <stdint.h>
#include <string.h>
#include "board.h"
#include "bank.h"

void game_text_print(uint8_t col, uint8_t row, uint8_t colour, const char *s);
#define text_print(col, row, colour, s) game_text_print(col, row, colour, s)
#include "fixed.h"
#include "actor.h"

enum {
    ST_TITLE, ST_CUTSCENE, ST_HISCORES, ST_PLAY, ST_SWITCH, ST_SCROLL,
    ST_LEVEL_END, ST_GAME_OVER, ST_ENTRY, ST_BOOT, ST_HOWTO, ST_SERVICE
};

#define MAX_ROWS 17
#define SLOTS 5
#define FLOOR_ROWS 4
#define FLOOR_WINDOWS (FLOOR_ROWS * SLOTS)
#define BIRDS 4
#define CHUNKS 3
#define NO_ROW 0xFF

#define WIN_OBSTACLE 0x04
#define WIN_BROKEN 0x03

enum { SLOT_NORMAL, SLOT_DOOR, SLOT_ARCH, SLOT_UPPER, SLOT_UPPER_MID, SLOT_ROOF };

enum {
    FELIX_IDLE, FELIX_WALK_IN, FELIX_UNUSED, FELIX_WALK, FELIX_JUMP, FELIX_JUMP_UP,
    FELIX_DROP, FELIX_STEP, FELIX_HAMMER, FELIX_LAND, FELIX_DEAD
};

enum {
    RALPH_WALK, RALPH_SMASH, RALPH_NONE, RALPH_FLOOR_DONE, RALPH_CLIMB,
    RALPH_NONE5, RALPH_LEVEL_DONE, RALPH_RAGE
};

typedef struct {
    uint8_t dmg;
    uint8_t st;
} window_t;

typedef struct {
    actor_t act;
    uint8_t row;
    uint8_t hit;
    int16_t x;
} bird_t;

typedef struct {
    actor_t act;
    uint8_t active;
    uint8_t hit;
    uint8_t col;
    uint8_t row;
    uint8_t blinks;
    uint8_t blink_timer;
    int16_t x;
    fix_t y;
    int16_t target_y;
} chunk_t;

typedef struct {
    actor_t act;
    fix_t x, y, prev_y, vx, vy;
    uint8_t row, slot;
    uint8_t fix_row, fix_slot;
    uint8_t state;
    uint8_t die_phase;
    uint8_t die_timer;
    uint8_t super_on;
    uint16_t super_timer;
    uint8_t can_jump, can_updown, can_fix;
    uint8_t up_clamped, airborne, stopped, scroll_jump;
    uint8_t cycle_f, cycle_h;
    uint8_t hits, hit_row, hit_slot;
} felix_t;

typedef struct {
    actor_t act;
    uint8_t row;
    uint8_t state;
    uint8_t phase;
    uint8_t at_center;
    uint8_t frozen;
    uint8_t climbing;
    uint8_t alt;
    uint8_t smashes;
    uint8_t breaks;
    uint8_t delay;
    int8_t dx;
    int16_t x, y, target;
    int16_t timer;
    uint8_t chunk_timer;
} ralph_t;

typedef struct {
    actor_t nicelander;
    actor_t pie;
    uint8_t active;
    uint8_t ready;
    uint8_t row, slot;
    int16_t x, y;
    uint16_t timer;
} pie_t;

typedef struct {
    uint32_t score;
    uint8_t lives;
    uint8_t level;
    uint8_t out;
    uint8_t needs_intro;

    uint8_t rows;
    const uint8_t *row_types;
    int8_t row_top[MAX_ROWS];
    window_t win[MAX_ROWS][SLOTS];
    uint8_t floor_row;
    uint8_t win_row;
    uint8_t floors_left;
    uint8_t broken;
    uint8_t floor_done;
    uint8_t rage_request;
    uint8_t pie_flag;
    uint8_t bird_pairs;
    uint8_t obstacles;
    uint8_t chunk_max;
    uint8_t chunks_spawned;
    uint8_t last_fix_row, last_fix_slot;
    int16_t scroll;
    int16_t scroll_target;

    uint8_t repair[FLOOR_WINDOWS];
    uint8_t popup_age[FLOOR_WINDOWS];
    uint8_t popup_on[FLOOR_WINDOWS];
    uint8_t points500[FLOOR_WINDOWS];

    felix_t felix;
    ralph_t ralph;
    chunk_t chunk[CHUNKS];
    bird_t bird[BIRDS];
    uint8_t bird_count;
    pie_t pie;
} player_t;

extern player_t pl;
extern player_t saved[2];

extern uint8_t game_state;
extern uint8_t next_state;
extern uint8_t state_pending;
extern uint16_t ticks;
extern uint8_t step;
extern int16_t timer;
extern uint16_t idle;
extern uint8_t demo;
extern uint8_t next_level;
extern uint8_t after_game;
extern uint8_t players;
extern uint8_t cur;
extern int16_t scroll_y;
extern int16_t scroll_x;
extern uint8_t level_end_pending;
extern uint8_t service_pressed;
extern uint8_t view_player;
extern uint8_t felix_f, felix_h;

void state_request(uint8_t s);

#endif
