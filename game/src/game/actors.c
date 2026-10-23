/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "actors.h"
#include "bldg.h"
#include "felix.h"
#include "rng.h"
#include "sound.h"
#include "input.h"
#include "service.h"
#include "exe.h"

#define ANIM_BIRD X8(actors_anim_bird)
#define ANIM_PIE X8(actors_anim_pie)
#define ANIM_CHUNK X8(actors_anim_chunk)
#define FIRST_NICELANDER X8(actors_first_nicelander)
#define BIRD_RIGHT_EDGE X16(actors_bird_right_edge)
#define BIRD_LEFT_EDGE X16(actors_bird_left_edge)
#define WOBBLES 2
#define WOBBLE_STEP X8(actors_wobble_step)
#define CHUNK_BLINK X8(actors_chunk_blink)

typedef struct {
    uint8_t row, slot;
    uint8_t idx;
    int8_t timer;
    uint8_t active;
} wobble_t;

static wobble_t wobbles[WOBBLES];
static const uint16_t pie_delays[4] = { 0, 1000, 2000, 2500 };

uint16_t pie_delay(void)
{
    uint8_t i = (dsw1 >> 4) & 3;
    return i ? pie_delays[i] : XU16(actors_pie_delay0);
}

static void bird_add(uint8_t row, uint8_t left)
{
    bird_t *b = &pl.bird[pl.bird_count++];
    b->row = row;
    b->hit = 0;
    b->x = left ? BIRD_RIGHT_EDGE : BIRD_LEFT_EDGE;
    b->act.flags = left ? AF_MIRROR : 0;
    actor_set(&b->act, ANIM_BIRD);
}

static uint8_t hazards(uint8_t n, uint8_t least)
{
    if (play_opt[OPT_HAZARDS] == 1)
        return 0;
    if (play_opt[OPT_HAZARDS] == 2 && n < least)
        return least;
    return n;
}

void birds_create(void)
{
    int8_t base = pl.rows - X8(actors_bird_base_rows);
    uint8_t p, pairs = hazards(pl.bird_pairs, 1);
    pl.bird_count = 0;
    for (p = 0; p < pairs; p++, base -= FLOOR_ROWS) {
        uint8_t r1 = rng() % X8(actors_bird_row_mod), r2;
        do
            r2 = rng() % X8(actors_bird_row_mod);
        while (r2 == r1);
        bird_add(base + X8(actors_bird_row_add) + r1, 1);
        bird_add(base + X8(actors_bird_row_add) + r2, 0);
    }
}

static int16_t bird_y(const bird_t *b)
{
    return row_top_y(b->row) + X8(actors_bird_dy);
}

static void bird_move(bird_t *b)
{
    int16_t y;
    if (b->act.flags & AF_MIRROR) {
        if (--b->x < BIRD_LEFT_EDGE)
            b->x = BIRD_RIGHT_EDGE;
    } else if (++b->x > BIRD_RIGHT_EDGE) {
        b->x = BIRD_LEFT_EDGE;
    }
    if (pl.felix.super_on)
        return;
    y = bird_y(b);
    box_x = FIX(b->x);
    box_y = FIX(y);
    box_w = box_h = X8(actors_bird_box);
    box_felix_w = X8(ralph_box_felix_w);
    if (felix_hit()) {
        b->hit = 1;
        sfx_play(SND_FELIX_HIT);
        felix_kill();
    }
}

void birds_update(void)
{
    uint8_t i;
    for (i = 0; i < pl.bird_count; i++) {
        bird_t *b = &pl.bird[i];
        if (!row_current(b->row)) {
            actor_show(&b->act, 0);
        } else if (pl.felix.state == FELIX_DEAD) {
            if (b->hit)
                actor_show(&b->act, (ticks & X8(actors_bird_flash_mask)) <= X8(actors_bird_flash_on_last));
        } else {
            actor_show(&b->act, 1);
            if (!(ticks & X8(actors_bird_move_mask)))
                bird_move(b);
        }
    }
}

void birds_reset(void)
{
    uint8_t i;
    for (i = 0; i < pl.bird_count; i++) {
        bird_t *b = &pl.bird[i];
        b->x = (b->act.flags & AF_MIRROR) ? BIRD_RIGHT_EDGE : BIRD_LEFT_EDGE;
        actor_show(&b->act, 1);
        b->hit = 0;
    }
}

void birds_free_current(void)
{
    uint8_t i, n = 0;
    for (i = 0; i < pl.bird_count; i++)
        if (!row_current(pl.bird[i].row))
            pl.bird[n++] = pl.bird[i];
    pl.bird_count = n;
}

void birds_draw(void)
{
    uint8_t i;
    for (i = 0; i < pl.bird_count; i++) {
        bird_t *b = &pl.bird[i];
        actor_draw(&b->act, b->x, bird_y(b));
    }
}

void birds_tick(void)
{
    uint8_t i;
    for (i = 0; i < pl.bird_count; i++)
        actor_tick(&pl.bird[i].act);
}

static uint8_t obstacles_in_row(uint8_t row)
{
    uint8_t s, n = 0;
    for (s = 0; s < SLOTS; s++)
        if (pl.win[row][s].st & WIN_OBSTACLE)
            n++;
    return n;
}

void obstacles_create(void)
{
    int8_t base = pl.rows - X8(actors_obst_base_rows);
    uint8_t k, row, slot, n = hazards(pl.obstacles, 2);
    for (; base >= 0; base -= FLOOR_ROWS) {
        for (k = 0; k < n; k++) {
            do
                row = base + X8(actors_obst_row_add) + rng() % X8(actors_obst_row_mod);
            while (obstacles_in_row(row) > X8(actors_obst_row_full));
            do {
                do
                    slot = rng() % X8(actors_obst_slot_mod);
                while (pl.win[row][slot].st & WIN_OBSTACLE);
            } while (slot_kind(row, slot) == SLOT_ARCH);
            pl.win[row][slot].st |= WIN_OBSTACLE;
            statics_dirty = 1;
        }
    }
}

void obstacles_clear(void) BANKED
{
    uint8_t r, s;
    for (r = 0; r < MAX_ROWS; r++)
        for (s = 0; s < SLOTS; s++)
            pl.win[r][s].st &= (uint8_t)~WIN_OBSTACLE;
    for (r = 0; r < WOBBLES; r++)
        wobbles[r].active = 0;
    statics_dirty = 1;
}

void obstacle_bonk(uint8_t row, uint8_t slot)
{
    uint8_t i;
    wobble_t *w = &wobbles[0];
    sfx_play(SND_BONK);
    for (i = 0; i < WOBBLES; i++) {
        if (!wobbles[i].active || (wobbles[i].row == row && wobbles[i].slot == slot)) {
            w = &wobbles[i];
            break;
        }
    }
    w->row = row;
    w->slot = slot;
    w->idx = 0;
    w->timer = 0;
    w->active = 1;
}

void obstacles_update(void)
{
    uint8_t i;
    for (i = 0; i < WOBBLES; i++) {
        wobble_t *w = &wobbles[i];
        if (!w->active || --w->timer >= 1)
            continue;
        statics_row(w->row);
        w->timer = WOBBLE_STEP;
        if (++w->idx > X8(actors_wobble_last)) {
            w->idx = 0;
            w->active = 0;
        }
    }
}

uint8_t obstacle_wobble(uint8_t row, uint8_t slot)
{
    uint8_t i;
    for (i = 0; i < WOBBLES; i++)
        if (wobbles[i].active && wobbles[i].row == row && wobbles[i].slot == slot)
            return wobbles[i].idx;
    return 0;
}

static uint8_t chunk_free_slot(void)
{
    uint8_t i;
    for (i = 0; i < CHUNKS; i++)
        if (!pl.chunk[i].active)
            return i;
    return 0xFF;
}

static uint8_t chunk_pick_row(uint8_t col)
{
    uint16_t r = rng();
    if (col == 0 || col == X8(actors_chunk_edge_col))
        return r & X8(actors_chunk_row_mask);
    return slot_kind(pl.floor_row, 2) == SLOT_DOOR ? (r & 1) + X8(actors_chunk_row_door_base) : r & X8(actors_chunk_row_mask);
}

static uint8_t chunk_taken(uint8_t row, uint8_t col)
{
    uint8_t i;
    for (i = 0; i < CHUNKS; i++)
        if (pl.chunk[i].active && pl.chunk[i].row == row && pl.chunk[i].col == col)
            return 1;
    return 0;
}

void chunk_spawn(uint8_t col)
{
    uint8_t i, n, row = 0, found = 0;
    chunk_t *c;
    if (pl.chunks_spawned >= pl.chunk_max)
        return;
    i = chunk_free_slot();
    if (i == 0xFF)
        return;
    c = &pl.chunk[i];
    c->hit = 0;
    c->y = FIX(pl.ralph.y + X8(actors_chunk_dy));
    for (n = 0; n < X8(actors_chunk_tries) && !found; n++) {
        row = pl.floor_row + chunk_pick_row(col);
        found = !chunk_taken(row, col);
    }
    if (!found)
        return;
    pl.chunks_spawned++;
    c->active = 1;
    c->x = (win_left(row, col + 1) - win_right(row, col)) / 2 + win_right(row, col);
    c->col = col;
    c->row = row;
    c->target_y = win_sill(row, col);
    c->act.flags = AF_VISIBLE;
    actor_set(&c->act, ANIM_CHUNK);
}

static void chunk_hit_check(chunk_t *c)
{
    if (game_state != ST_PLAY || pl.felix.super_on || pl.felix.state == FELIX_DEAD)
        return;
    box_x = FIX(c->x);
    box_y = c->y;
    box_w = X8(actors_chunk_box_w);
    box_h = X8(actors_chunk_box_h);
    box_felix_w = X8(actors_chunk_felix_w);
    if (felix_hit()) {
        c->hit = 1;
        c->blinks = X8(actors_chunk_blinks);
        c->blink_timer = CHUNK_BLINK;
        felix_kill();
    }
}

static void chunk_blink(chunk_t *c)
{
    if (--c->blink_timer)
        return;
    c->blink_timer = CHUNK_BLINK;
    if (!(c->act.flags & AF_VISIBLE)) {
        actor_show(&c->act, 1);
        return;
    }
    actor_show(&c->act, 0);
    if (!--c->blinks)
        c->active = 0;
}

void chunks_update(void)
{
    uint8_t i;
    for (i = 0; i < CHUNKS; i++) {
        chunk_t *c = &pl.chunk[i];
        fix_t target;
        if (!c->active)
            continue;
        target = FIX(c->target_y);
        if (c->y != target) {
            c->y += FIX_1_9;
            if (c->y > target)
                c->y = target;
        }
        if (!c->hit)
            chunk_hit_check(c);
        else
            chunk_blink(c);
    }
}

void chunks_clear(void) BANKED
{
    uint8_t i;
    pl.chunks_spawned = 0;
    for (i = 0; i < CHUNKS; i++) {
        pl.chunk[i].active = 0;
        actor_show(&pl.chunk[i].act, 0);
    }
}

void chunks_draw(void)
{
    uint8_t i;
    for (i = 0; i < CHUNKS; i++) {
        chunk_t *c = &pl.chunk[i];
        if (c->active)
            actor_draw(&c->act, c->x, fix_trunc(c->y));
    }
}

void pie_init(void) BANKED
{
    pl.pie.pie.flags = 0;
    actor_set(&pl.pie.pie, ANIM_PIE);
    pl.pie.nicelander.flags = 0;
    pl.pie.active = pl.pie.ready = 0;
    pl.pie.timer = pie_delay();
}

void pie_place(void)
{
    uint8_t rows[3], slots[3], n = 0, k, s;
    int8_t r;
    if (play_opt[OPT_SUPER] == 3)
        return;
    for (r = FLOOR_ROWS - 1; r >= 0 && n < X8(actors_pie_candidates); r--) {
        uint8_t row = pl.floor_row + r;
        if (row == pl.felix.row)
            continue;
        for (s = 0; s < SLOTS && n < X8(actors_pie_candidates); s++) {
            k = slot_kind(row, s);
            if (k != SLOT_DOOR && k != SLOT_ARCH && win_lower(row, s) == X8(actors_pie_lower_dmg)) {
                rows[n] = row;
                slots[n++] = s;
            }
        }
    }
    if (!n)
        return;
    k = rng() % n;
    pl.pie.row = rows[k];
    pl.pie.slot = slots[k];
    pl.pie.x = pane_x(rows[k], slots[k]) + X8(actors_pie_dx);
    pl.pie.y = pane_y(rows[k], 1) + X8(actors_pie_dy);
    actor_set(&pl.pie.nicelander, FIRST_NICELANDER + rng() % X8(actors_nicelander_count));
    actor_show(&pl.pie.nicelander, 1);
    pl.pie.ready = 0;
    pl.pie.active = 1;
    pl.pie.timer = pie_delay();
    pl.pie_flag = 1;
}

void pie_hide(void) BANKED
{
    actor_show(&pl.pie.nicelander, 0);
    actor_show(&pl.pie.pie, 0);
    pl.pie.ready = 0;
    pl.pie.active = 0;
}

void pie_update(void)
{
    if (pl.pie.active && pl.pie.nicelander.frame == X8(actors_pie_ready_frame)) {
        actor_show(&pl.pie.pie, 1);
        pl.pie.ready = 1;
    }
    if (pl.pie.nicelander.flags & AF_DONE)
        actor_show(&pl.pie.nicelander, 0);
}

void pie_draw(void)
{
    actor_draw(&pl.pie.pie, pl.pie.x, pl.pie.y);
    actor_draw(&pl.pie.nicelander, pl.pie.x, pl.pie.y);
}

void pie_tick(void)
{
    actor_tick(&pl.pie.nicelander);
    actor_tick(&pl.pie.pie);
}
