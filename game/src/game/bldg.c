/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "bldg.h"
#include "bg.h"
#include "rng.h"
#include "sound.h"
#include "input.h"
#include "ralph.h"
#include "actors.h"
#include "exe.h"

#define ANIM_PANE X8(bldg_anim_pane)
#define ANIM_SHARD X8(bldg_anim_shard)
#define ANIM_POPUP_100 X8(bldg_anim_popup_100)
#define ANIM_POPUP_500 X8(bldg_anim_popup_500)
#define SHARDS 16
#define REPAIR_FRAMES X8(bldg_repair_frames)
#define POPUP_BIG_RISE X8(bldg_popup_big_rise)
#define POPUP_END_RISE X8(bldg_popup_end_rise)
#define ROOF_HEIGHT X8(bldg_roof_height)
#define DOOR_INTACT X8(bldg_door_intact)
#define DOOR_WORST X8(bldg_door_worst)
#define ARCH_INTACT X8(bldg_arch_intact)
#define ARCH_BROKEN X8(bldg_arch_broken)
#define DOOR_SILL X16(bldg_door_sill)
#define ARCH_SILL X16(bldg_arch_sill)
#define SHARD_SPEED X8(bldg_shard_speed)
#define SHARD_END_Y X16(bldg_shard_end_y)

typedef struct {
    int16_t x, y;
    uint8_t frame;
    uint8_t on;
} shard_t;

static const uint8_t *const type_lists[3] = { exe_types9, exe_types13, exe_types17 };

uint8_t rows_shown;
uint8_t building_on;

static shard_t shards[SHARDS];
static uint8_t shards_live;

void level_load(void)
{
    const level_t *lv = &exe_levels[pl.level < X8(bldg_level_cap) ? pl.level : X8(bldg_level_cap)];
    pl.rows = lv->rows;
    pl.row_types = type_lists[lv->types];
    pl.bird_pairs = lv->birds;
    pl.obstacles = lv->obstacles;
    pl.chunk_max = lv->chunks;
}

uint8_t row_type(uint8_t row)
{
    return pl.row_types[row];
}

uint8_t row_height(uint8_t row)
{
    uint8_t t = pl.row_types[row];
    if (t == ROW_ROOF)
        return ROOF_HEIGHT;
    return t == ROW_UPPER_A ? X8(bldg_row_height_upper_a) : X8(bldg_row_height_other);
}

int16_t row_top_y(uint8_t row)
{
    return pl.row_top[row] * 8;
}

static uint8_t row_win_dy(uint8_t row)
{
    uint8_t t = pl.row_types[row];
    return t == ROW_GROUND2 || t == ROW_UPPER_B ? X8(bldg_row_win_dy) : 0;
}

int16_t row_bottom_y(uint8_t row)
{
    return (pl.row_top[row] + row_height(row)) * 8;
}

void bldg_create(void)
{
    int8_t top = BUILDING_BASE_TILE;
    uint8_t r, s;
    level_load();
    for (r = 0; r < pl.rows; r++) {
        top -= row_height(r);
        pl.row_top[r] = top;
        for (s = 0; s < SLOTS; s++) {
            uint8_t k = slot_kind(r, s);
            pl.win[r][s].st = 0;
            pl.win[r][s].dmg = k == SLOT_DOOR ? DOOR_INTACT : k == SLOT_ARCH ? ARCH_INTACT : 0;
        }
    }
    pl.floors_left = (pl.rows - 1) / FLOOR_ROWS;
    statics_dirty = 1;
    pl.broken = 0;
    pl.floor_row = 0;
    pl.win_row = 0;
    pl.chunks_spawned = 0;
    pl.bird_count = 0;
    pl.pie.active = 0;
    pl.pie.ready = 0;
    actor_show(&pl.pie.pie, 0);
    actor_show(&pl.pie.nicelander, 0);
    for (s = 0; s < CHUNKS; s++) {
        pl.chunk[s].active = 0;
        actor_show(&pl.chunk[s].act, 0);
    }
    memset(pl.repair, 0, sizeof pl.repair);
    memset(pl.popup_on, 0, sizeof pl.popup_on);
    ralph_place(FLOOR_ROWS);
}

uint8_t slot_kind(uint8_t row, uint8_t slot)
{
    return exe_kinds[pl.row_types[row]][slot];
}

uint8_t slot_tx(uint8_t row, uint8_t slot)
{
    return pl.row_types[row] >= ROW_UPPER_A ? exe_tx_high[slot] : exe_tx_low[slot];
}

int16_t win_left(uint8_t row, uint8_t slot)
{
    uint8_t k = slot_kind(row, slot);
    return k == SLOT_ROOF ? 0 : slot_tx(row, slot) * 8 + exe_left_off[k];
}

int16_t win_right(uint8_t row, uint8_t slot)
{
    uint8_t k = slot_kind(row, slot);
    return k == SLOT_ROOF ? 0 : slot_tx(row, slot) * 8 + exe_right_off[k];
}

int16_t win_center(uint8_t row, uint8_t slot)
{
    int16_t l = win_left(row, slot);
    return (win_right(row, slot) - l) / 2 + l;
}

int16_t win_sill(uint8_t row, uint8_t slot)
{
    uint8_t k = slot_kind(row, slot);
    if (k == SLOT_DOOR)
        return DOOR_SILL;
    if (k == SLOT_ARCH)
        return ARCH_SILL;
    if (k == SLOT_ROOF)
        return 0;
    return row_top_y(row) + row_win_dy(row) + X8(bldg_win_sill);
}

int16_t pane_x(uint8_t row, uint8_t slot)
{
    return slot_tx(row, slot) * 8 + exe_pane_off[slot_kind(row, slot)];
}

int16_t pane_y(uint8_t row, uint8_t lower)
{
    return row_top_y(row) + row_win_dy(row) + (lower ? X8(bldg_pane_y_lower) : X8(bldg_pane_y_upper));
}

uint8_t row_current(uint8_t row)
{
    return (uint8_t)(row - pl.floor_row) < FLOOR_ROWS;
}

uint8_t win_active(uint8_t row)
{
    return row_current(row) && pl.row_types[row] != ROW_ROOF;
}

uint8_t win_lower(uint8_t row, uint8_t slot)
{
    return pl.win[row][slot].dmg >> 3;
}

uint8_t win_state(uint8_t row, uint8_t slot)
{
    return pl.win[row][slot].st & WIN_BROKEN;
}

static uint8_t repair_index(uint8_t row, uint8_t slot)
{
    uint8_t d = row - pl.win_row;
    return d < FLOOR_ROWS ? d * SLOTS + slot : 0xFF;
}

uint8_t win_repairing(uint8_t row, uint8_t slot)
{
    uint8_t i = repair_index(row, slot);
    return i == 0xFF ? 0 : pl.repair[i];
}

static void set_panes(uint8_t row, window_t *w, uint8_t upper, uint8_t lower)
{
    statics_row(row);
    w->dmg = upper | (lower << 3);
    w->st = (w->st & WIN_OBSTACLE) | (upper ? (lower ? 2 : 1) : (lower ? 1 : 0));
}

static void shard_spawn(uint8_t row, uint8_t slot, uint8_t damage, uint8_t lower)
{
    uint8_t i;
    if (!damage)
        return;
    shard_t *s = shards;
    for (i = 0; i < SHARDS; i++, s++) {
        if (s->on)
            continue;
        s->on = 1;
        shards_live++;
        s->x = pane_x(row, slot);
        s->y = pane_y(row, lower);
        s->frame = exe_shard_frames[damage];
        return;
    }
}

static void damage_special(uint8_t row, window_t *w, uint8_t kind)
{
    statics_row(row);
    uint8_t t;
    if (kind == SLOT_DOOR) {
        t = w->dmg + 1;
        if (t > DOOR_WORST)
            t = DOOR_WORST;
        w->dmg = t;
        w->st = (w->st & WIN_OBSTACLE) | (t == DOOR_INTACT ? 0 : t == DOOR_WORST ? 2 : 1);
    } else {
        w->dmg = ARCH_BROKEN;
        w->st = (w->st & WIN_OBSTACLE) | 2;
    }
}

static uint8_t random_damage(void)
{
    return (uint8_t)(rng() % X8(bldg_damage_mod)) + X8(bldg_damage_add);
}

void window_damage(uint8_t row, uint8_t slot, uint8_t heavy, uint8_t sound)
{
    window_t *w = &pl.win[row][slot];
    uint8_t st = w->st & WIN_BROKEN;
    uint8_t kind = slot_kind(row, slot);
    uint8_t upper = w->dmg & 7, lower = w->dmg >> 3, shard = 0, from_lower = 1;
    if (st == 2 && (!heavy || lower == DOOR_WORST))
        return;
    if (st == 0) {
        pl.broken++;
        if (sound)
            sfx_play(SND_WINDOW_BREAK);
    }
    if (kind == SLOT_DOOR || kind == SLOT_ARCH) {
        damage_special(row, w, kind);
        return;
    }
    if (kind == SLOT_ROOF)
        return;
    if (st == 0) {
        if (heavy) {
            lower = DOOR_WORST;
            shard = DOOR_WORST;
        } else {
            uint8_t which = rng() & X8(bldg_damage_pane_pick);
            shard = random_damage();
            if (which)
                lower = shard;
            else
                upper = shard;
            from_lower = which;
        }
    } else if (heavy) {
        if (lower != DOOR_WORST && sound)
            shard_spawn(row, slot, lower, 1);
        lower = DOOR_WORST;
    } else if (!upper) {
        shard = upper = random_damage();
        from_lower = 0;
    } else if (!lower) {
        shard = lower = random_damage();
    }
    if (sound)
        shard_spawn(row, slot, shard, from_lower);
    set_panes(row, w, upper, lower);
}

void window_fix(uint8_t row, uint8_t slot)
{
    uint8_t i = repair_index(row, slot);
    pl.broken--;
    pl.win[row][slot].st &= WIN_OBSTACLE;
    if (i == 0xFF)
        return;
    pl.points500[i] = pl.broken == 0;
    pl.popup_on[i] = 1;
    pl.popup_age[i] = 0;
    pl.repair[i] = REPAIR_FRAMES;
}

static void add_score(uint8_t big)
{
    uint16_t step = exe_bonus_step[(dsw1 >> 2) & 3];
    uint32_t before = pl.score;
    pl.score += big ? X16(bldg_points_last) : X16(bldg_points_window);
    if (step && before / step < pl.score / step) {
        pl.lives++;
        sfx_stop(SND_MUSIC_FLOOR_CLEAR);
        sfx_play(SND_EXTRA_LIFE);
    }
}

static void clear_damage(uint8_t row, uint8_t slot)
{
    uint8_t k = slot_kind(row, slot);
    window_t *w = &pl.win[row][slot];
    w->dmg = k == SLOT_DOOR ? DOOR_INTACT : k == SLOT_ARCH ? ARCH_INTACT : 0;
}

static uint8_t window_next(uint8_t i)
{
    const uint8_t *rep = pl.repair + i, *pop = pl.popup_on + i;
    for (; i < FLOOR_WINDOWS; i++)
        if (*rep++ | *pop++)
            break;
    return i;
}

static void window_update(uint8_t i)
{
    uint8_t r;
    if ((&pl.win[pl.win_row][0] + i)->st & WIN_BROKEN)
        return;
    if (pl.repair[i]) {
        uint8_t phase = --pl.repair[i] & X8(bldg_repair_phase_mask);
        if (phase == X8(bldg_repair_phase_lt) - 1 || phase == X8(bldg_repair_phase_mask) || !pl.repair[i])
            statics_row(pl.win_row + i / SLOTS);
        if (!pl.repair[i]) {
            if (!demo)
                add_score(pl.points500[i]);
            pl.popup_age[i] = 0;
            r = pl.win_row;
            while (i >= SLOTS) {
                i -= SLOTS;
                r++;
            }
            clear_damage(r, i);
        }
    } else if (ticks & X8(bldg_popup_rise_mask)) {
        if (++pl.popup_age[i] > POPUP_END_RISE)
            pl.popup_on[i] = 0;
    }
}

void windows_update(void)
{
    uint8_t i;
    for (i = window_next(0); i < FLOOR_WINDOWS; i = window_next(i + 1))
        window_update(i);
}

void floor_damage_top(void)
{
    uint8_t r, s;
    for (r = pl.floor_row + 2; r < pl.floor_row + FLOOR_ROWS; r++)
        for (s = 0; s < SLOTS; s++)
            if (rng() % X8(bldg_floor_damage_mod) < X8(bldg_floor_damage_lt))
                window_damage(r, s, 0, 0);
}

void shards_clear(void)
{
    uint8_t i;
    shards_live = 0;
    for (i = 0; i < SHARDS; i++)
        shards[i].on = 0;
}

void shards_update(void)
{
    uint8_t i;
    shard_t *s = shards;
    if (!shards_live)
        return;
    for (i = SHARDS; i; i--, s++) {
        if (!s->on)
            continue;
        s->y += SHARD_SPEED;
        if (s->y > SHARD_END_Y) {
            s->on = 0;
            shards_live--;
        }
    }
}

void shards_draw(void)
{
    uint8_t i;
    const anim_frame_t *frames = anims[ANIM_SHARD].frames;
    shard_t *s = shards;
    if (!shards_live)
        return;
    for (i = SHARDS; i; i--, s++) {
        if (s->on)
            frame_cell(&frames[s->frame], s->x + scroll_x, s->y + scroll_y);
    }
}

static uint8_t popup_next(uint8_t i)
{
    const uint8_t *pop = pl.popup_on + i;
    for (; i < FLOOR_WINDOWS; i++)
        if (*pop++)
            break;
    return i;
}

static void popup_draw(uint8_t i)
{
    uint8_t r, s, age;
    const anim_frame_t *f;
    if (pl.repair[i] || ((&pl.win[pl.win_row][0] + i)->st & WIN_BROKEN))
        return;
    r = pl.win_row;
    for (s = i; s >= SLOTS; s -= SLOTS)
        r++;
    age = pl.popup_age[i];
    if (pl.points500[i])
        f = &anims[ANIM_POPUP_500].frames[0];
    else
        f = &anims[ANIM_POPUP_100].frames[age > POPUP_BIG_RISE ? 0 : 1];
    frame_cell(f, win_center(r, s) + scroll_x, win_sill(r, s) - X8(bldg_popup_start_dy) - age + scroll_y);
}

void popups_draw(void)
{
    uint8_t i;
    for (i = popup_next(0); i < FLOOR_WINDOWS; i = popup_next(i + 1))
        popup_draw(i);
}

#define MAX_STATIC 80
#define ANIM_OBSTACLE X8(bldg_anim_obstacle)

uint8_t statics_dirty;

static uint8_t st_limit = MAX_STATIC;
static uint8_t st_over;
static uint8_t st_rows_pending;
static uint8_t st_row_dirty[MAX_ROWS];
static uint8_t st_row_first[MAX_ROWS], st_row_n[MAX_ROWS];

void statics_row(uint8_t row)
{
    st_row_dirty[row] = 1;
    st_rows_pending = 1;
}

static volatile uint8_t *st_out;
static volatile uint8_t *st_size;
static uint8_t st_n;
static uint8_t st_x8;
static int16_t st_scroll_x, st_scroll_y;
static uint8_t st_rows, st_win_row, st_building;

#define ST_TOP -16
#define ST_DRIFT 16
#define ST_DRIFT_X 120
#define ST_LEFT -136
#define ST_RIGHT 359
#define Y9_WRAP 496

static int16_t st_built_x, st_built_y;

static void st_write(volatile uint8_t *e, int16_t x, int16_t y)
{
    uint16_t y9 = (uint16_t)y & 0x1FF;
    if ((uint16_t)x > 255 || y < ST_TOP || y >= SCREEN_H) {
        e[1] = (uint8_t)SPR_HIDDEN_Y;
        e[3] |= SPR_ATTR_Y8;
        return;
    }
    e[0] = (uint8_t)x;
    e[1] = (uint8_t)y9;
    e[3] = (e[3] & (uint8_t)~SPR_ATTR_Y8) | (y9 >= 256 ? SPR_ATTR_Y8 : 0);
}

static void st_emit(int16_t x, int16_t y, uint8_t code, uint8_t attr)
{
    volatile uint8_t *e;
    if (y < ST_TOP || y >= SCREEN_H)
        return;
    if (st_n >= st_limit) {
        st_over = 1;
        return;
    }
    e = st_out - 4;
    st_out = e;
    *--st_size = x & 0x100 ? SPR_X8 : 0;
    st_x8 |= *st_size;
    e[0] = (uint8_t)x;
    e[1] = (uint8_t)y;
    e[2] = code;
    e[3] = ((uint16_t)y & 0x1FF) >= 256 ? attr | SPR_ATTR_Y8 : attr & (uint8_t)~SPR_ATTR_Y8;
    st_n++;
}

static void st_put(int16_t x, int16_t y, uint8_t code, uint8_t attr)
{
    if (x < ST_LEFT || x > ST_RIGHT)
        return;
    st_emit(x, y, code, attr);
}

static void static_rect(uint8_t rect, int16_t x, int16_t y)
{
    const msprite_t *m = &msprites[rect];
    const spr_part_t *p = &spr_parts[m->first + m->overlay];
    uint8_t n = m->count;
    for (; n; n--, p++)
        st_put(x + p->dx, y + p->dy, p->code, p->attr);
}

static void frame_rect(uint8_t anim, uint8_t frame, int16_t x, int16_t y)
{
    const anim_frame_t *f = &anims[anim].frames[frame];
    static_rect(f->rect, x - f->hx, y - f->hy);
}

static uint8_t pane_code[6], pane_attr[6];

void panes_init(void)
{
    uint8_t f;
    for (f = 0; f < 6; f++) {
        const spr_part_t *p = &spr_parts[msprites[anims[ANIM_PANE].frames[f].rect].first];
        pane_code[f] = p->code;
        pane_attr[f] = p->attr;
    }
}

static uint8_t pn_x, pn_y, pn_f, pn_y8, pn_x8;

static void pane_fast(void)
{
    volatile uint8_t *e;
    uint8_t a;
    if (st_n >= st_limit) {
        st_over = 1;
        return;
    }
    e = st_out - 4;
    st_out = e;
    *--st_size = pn_x8;
    st_x8 |= pn_x8;
    e[0] = pn_x;
    e[1] = pn_y;
    e[2] = pane_code[pn_f];
    a = pane_attr[pn_f];
    e[3] = pn_y8 ? a | SPR_ATTR_Y8 : a & (uint8_t)~SPR_ATTR_Y8;
    st_n++;
}

static void row_panes(uint8_t r, int16_t sy)
{
    static uint8_t s, dmg, rep, ok1, ok2, yb1, yb2, h1, h2, rel, t;
    static const uint8_t *tx, *kind;
    static window_t *w;
    int16_t y1 = sy + X8(bldg_pane_y_upper), y2 = sy + X8(bldg_pane_y_lower);
    ok1 = y1 >= ST_TOP && y1 < SCREEN_H;
    ok2 = y2 >= ST_TOP && y2 < SCREEN_H;
    yb1 = (uint8_t)y1;
    yb2 = (uint8_t)y2;
    h1 = ((uint16_t)y1 & 0x1FF) >= 256;
    h2 = ((uint16_t)y2 & 0x1FF) >= 256;
    t = pl.row_types[r];
    tx = t >= ROW_UPPER_A ? exe_tx_high : exe_tx_low;
    kind = exe_kinds[t];
    rel = r - pl.win_row;
    w = pl.win[r];
    for (s = 0; s < SLOTS; s++, w++) {
        uint8_t k = kind[s];
        int16_t x;
        if (k == SLOT_DOOR || k == SLOT_ARCH)
            continue;
        x = tx[s] * 8 + exe_pane_off[k] + scroll_x;
        if (x < ST_LEFT || x > ST_RIGHT)
            continue;
        pn_x8 = x & 0x100 ? SPR_X8 : 0;
        dmg = w->dmg;
        rep = rel < FLOOR_ROWS ? pl.repair[rel * SLOTS + s] : 0;
        if (rep && !(w->st & WIN_BROKEN) && (rep & X8(bldg_repair_phase_mask)) < X8(bldg_repair_phase_lt))
            dmg = 0;
        pn_x = (uint8_t)x;
        if (ok1) {
            pn_y = yb1;
            pn_y8 = h1;
            pn_f = dmg & 7;
            pane_fast();
        }
        if (ok2) {
            pn_y = yb2;
            pn_y8 = h2;
            pn_f = dmg >> 3;
            pane_fast();
        }
    }
}

static void row_obstacle_sprites(uint8_t r, int16_t sy)
{
    uint8_t s;
    window_t *w = pl.win[r];
    for (s = 0; s < SLOTS; s++, w++)
        if (w->st & WIN_OBSTACLE)
            frame_rect(ANIM_OBSTACLE, 0, win_center(r, s) + scroll_x,
                       sy + X8(bldg_win_sill) + X8(bldg_obstacle_dy) + exe_wobble_dy[obstacle_wobble(r, s)]);
}

static uint8_t statics_same_content(void)
{
    return !statics_dirty && st_rows == rows_shown && st_win_row == pl.win_row &&
           st_building == building_on;
}

static uint8_t near(int16_t a, int16_t b)
{
    return (uint16_t)(a - b + ST_DRIFT - 1) < 2 * ST_DRIFT - 1;
}

#ifdef __SDCC
extern volatile uint8_t *sts_ptr, *sts_size;
extern uint8_t sts_n;
extern int8_t sts_dx;
void st_shift(void);

static void statics_shift_x(int8_t dx)
{
    sts_ptr = &SPRITES[SPRITE_COUNT - static_count].x;
    sts_size = &SPR_SIZE[SPRITE_COUNT - static_count];
    sts_n = static_count;
    sts_dx = dx;
    st_shift();
}
#else
static void statics_shift_x(int8_t dx)
{
    volatile uint8_t *e = &SPRITES[SPRITE_COUNT - static_count].x;
    volatile uint8_t *z = &SPR_SIZE[SPRITE_COUNT - static_count];
    uint8_t n = static_count;
    for (; n; n--, e += 4, z++) {
        uint16_t x = e[0] + ((*z & SPR_X8) ? 256 : 0) + dx;
        e[0] = (uint8_t)x;
        *z = (*z & (uint8_t)~SPR_X8) | (x & 0x100 ? SPR_X8 : 0);
    }
}
#endif

static void statics_translate(int16_t dx, int16_t dy)
{
    volatile uint8_t *e;
    uint8_t n;
    if (!dy) {
        statics_shift_x((int8_t)dx);
        return;
    }
    e = &SPRITES[SPRITE_COUNT - static_count].x;
    for (n = static_count; n; n--, e += 4) {
        int16_t y = e[1] | ((e[3] & SPR_ATTR_Y8) ? 256 : 0);
        if (y >= SCREEN_H && y < Y9_WRAP)
            continue;
        if (y >= Y9_WRAP)
            y -= 512;
        st_write(e, e[0] + dx, y + dy);
    }
}

static void statics_hide_from(uint8_t old_count)
{
    volatile uint8_t *out = &SPRITES[SPRITE_COUNT - old_count].x;
    volatile uint8_t *z = &SPR_SIZE[SPRITE_COUNT - old_count];
    while (old_count-- > st_n) {
        out[1] = (uint8_t)SPR_HIDDEN_Y;
        out[3] = SPR_ATTR_Y8;
        out += 4;
        *z++ = 0;
    }
}

static void row_sprites(uint8_t r, int16_t sy)
{
    sy += row_win_dy(r);
    if (pl.row_types[r] != ROW_ROOF)
        row_panes(r, sy);
    row_obstacle_sprites(r, sy);
}

static uint8_t row_refresh(uint8_t r)
{
    uint8_t first = st_row_first[r], end = first + st_row_n[r];
    volatile uint8_t *out;
    if (!st_row_n[r])
        return 1;
    st_out = &SPRITES[SPRITE_COUNT - first].x;
    st_size = &SPR_SIZE[SPRITE_COUNT - first];
    st_n = first;
    st_limit = end;
    st_over = 0;
    row_sprites(r, row_top_y(r) + scroll_y);
    st_limit = MAX_STATIC;
    if (st_over)
        return 0;
    out = st_out;
    while (st_n < end) {
        out -= 4;
        out[1] = (uint8_t)SPR_HIDDEN_Y;
        out[3] = SPR_ATTR_Y8;
        *--st_size = 0;
        st_n++;
    }
    return 1;
}

static void rows_refresh(void)
{
    uint8_t r;
    for (r = 0; r < MAX_ROWS; r++) {
        if (!st_row_dirty[r])
            continue;
        st_row_dirty[r] = 0;
        if (!statics_dirty && !row_refresh(r))
            statics_dirty = 1;
    }
    st_rows_pending = 0;
    st_n = static_count;
}

void statics_update(uint8_t on)
{
    uint8_t r, old = static_count;
    if (st_rows_pending) {
        if (st_scroll_x != scroll_x || st_scroll_y != scroll_y || !statics_same_content())
            statics_dirty = 1;
        rows_refresh();
    }
    if (!on) {
        st_n = 0;
        statics_hide_from(old);
        static_count = 0;
        statics_dirty = 1;
        return;
    }
    if (statics_same_content()) {
        if (st_scroll_x == scroll_x && st_scroll_y == scroll_y)
            return;
        if (st_scroll_y == scroll_y && (uint16_t)(scroll_x - st_built_x + ST_DRIFT_X - 1) < 2 * ST_DRIFT_X - 1) {
            statics_shift_x((int8_t)(scroll_x - st_scroll_x));
            st_scroll_x = scroll_x;
            return;
        }
        if (!st_x8 && near(scroll_x, st_built_x) && near(scroll_y, st_built_y)) {
            statics_translate(scroll_x - st_scroll_x, scroll_y - st_scroll_y);
            st_scroll_x = scroll_x;
            st_scroll_y = scroll_y;
            return;
        }
    }
    statics_dirty = 0;
    st_scroll_x = st_built_x = scroll_x;
    st_scroll_y = st_built_y = scroll_y;
    st_rows = rows_shown;
    st_win_row = pl.win_row;
    st_building = building_on;
    st_out = &SPRITES[SPRITE_COUNT].x;
    st_size = &SPR_SIZE[SPRITE_COUNT];
    st_n = 0;
    st_x8 = 0;
    for (r = 0; r < MAX_ROWS; r++)
        st_row_n[r] = 0;
    for (r = 0; building_on && r < rows_shown && r < pl.rows; r++) {
        int16_t sy = row_top_y(r) + scroll_y;
        if (sy >= SCREEN_H || sy < -64)
            continue;
        st_row_first[r] = st_n;
        row_sprites(r, sy);
        st_row_n[r] = st_n - st_row_first[r];
    }
    statics_hide_from(old);
    static_count = st_n;
}

uint8_t special_tile(uint8_t row, uint8_t slot)
{
    uint8_t k = slot_kind(row, slot);
    uint8_t rep = win_repairing(row, slot);
    if (rep && !(pl.win[row][slot].st & WIN_BROKEN) && (rep & X8(bldg_repair_phase_mask)) < X8(bldg_repair_phase_lt))
        return k == SLOT_DOOR ? DOOR_INTACT : ARCH_INTACT;
    return pl.win[row][slot].dmg;
}

static uint8_t shown_door, shown_arch;
static uint8_t door_tm = 0xFF, arch_tm = 0xFF;

static uint8_t special_update(uint8_t row, uint8_t tm, uint8_t old)
{
    bg_special(pl.row_top[row], row_height(row), tm, old, slot_tx(row, 2));
    return tm;
}

void doors_refresh(void)
{
    uint8_t d, a;
    if (!building_on || pl.rows < 2)
        return;
    d = special_tile(0, 2);
    a = special_tile(1, 2);
    if (d != shown_door) {
        shown_door = d;
        door_tm = special_update(0, win_tm[row_type(0)][2][d - DOOR_INTACT], door_tm);
    }
    if (a != shown_arch) {
        shown_arch = a;
        arch_tm = special_update(1, win_tm[row_type(1)][2][a != ARCH_INTACT], arch_tm);
    }
}
