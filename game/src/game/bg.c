/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "bg.h"
#include "bldg.h"
#include "tmap.h"
#include "hud.h"
#include "page.h"
#include "exe.h"

#define MAP_ROWS_MASK 63
#define MAP_COLS_MASK 31
#define VIEW_ROWS 41
#define INVALID 0x7FFF
#define BAND_TOP X8(bg_band_top)
#define BAND_ROWS X8(bg_band_rows)
#define GROUND_PICS 2
#define BUILDING_COL X8(bg_building_col)
#define BUILDING_COLS X8(bg_building_cols)


bg_pic_t bg_pics[BG_PICS];
uint8_t ground_on;
int8_t bg_dy;

typedef struct {
    uint8_t tm, on;
    int8_t col, row;
} fg_pic_t;

static fg_pic_t fg_want[FG_PICS];
static fg_pic_t fg_have[FG_PICS];
static uint8_t fg_dirty;

static int16_t content[64];
static int16_t wlo;
static int16_t drawn_top;
static int16_t last_cx;
static int8_t pan_dir;
static uint8_t dirty;
static int16_t job_col, job_row, job_end;
static int16_t lo, hi;
static volatile uint16_t *dst;
static uint8_t vis_sx[2];
static uint16_t vis_sy[2];

void bg_shown(uint8_t sx, uint16_t sy)
{
    vis_sx[1] = vis_sx[0];
    vis_sy[1] = vis_sy[0];
    vis_sx[0] = sx;
    vis_sy[0] = sy;
}

void bg_invalidate(void)
{
    uint8_t i;
    dirty = 1;
    job_end = job_row;
    for (i = 0; i < 64; i++)
        content[i] = INVALID;
}

void bg_reset(void)
{
    uint8_t i;
    for (i = 0; i < BG_PICS; i++)
        bg_pics[i].on = 0;
    ground_on = 0;
    building_on = 0;
    bg_invalidate();
}

void bg_rows_dirty(int16_t top, uint8_t count)
{
    dirty = 1;
    while (count--) {
        content[top & MAP_ROWS_MASK] = INVALID;
        top++;
    }
}

static void pic_dirty(uint8_t i)
{
    bg_pic_t *p = &bg_pics[i];
    if (p->on)
        bg_rows_dirty(p->row, tilemaps[p->tm].h);
}

void bg_pic_set(uint8_t i, uint8_t tm, int8_t col, int16_t row)
{
    bg_pic_t *p = &bg_pics[i];
    pic_dirty(i);
    p->tm = tm;
    p->col = col;
    p->row = row;
    p->on = 1;
    pic_dirty(i);
}

void bg_pic_off(uint8_t i)
{
    pic_dirty(i);
    bg_pics[i].on = 0;
}

#ifdef __SDCC
extern uint8_t tmr_tm, tmr_row, tmr_opaque;
extern int16_t tmr_col, tmr_lo, tmr_hi;
extern volatile uint16_t *tmr_dst;
void tmr_draw(void);

static void put_row(uint8_t tm, int16_t col, uint8_t src_row, uint8_t opaque)
{
    tmr_tm = tm;
    tmr_col = col;
    tmr_row = src_row;
    tmr_opaque = opaque;
    tmr_draw();
}
#else
static void put_row(uint8_t tm, int16_t col, uint8_t src_row, uint8_t opaque)
{
    if (col >= hi || col + tilemaps[tm].w <= lo)
        return;
    tm_row(tm, src_row, dst, col, lo, hi, opaque);
}
#endif

static void put_ground(int16_t t)
{
    uint8_t i;
    if (!ground_on || t < BAND_TOP || t >= BAND_TOP + BAND_ROWS)
        return;
    for (i = 0; i < GROUND_PICS; i++)
        put_row(TM_BUSHES, exe_bg_band_cols[i], t - BAND_TOP, 0);
}

static uint8_t wc_r = NO_ROW;
static uint8_t wc_n;
static uint8_t wc_tx[SLOTS], wc_tm[SLOTS], wc_h[SLOTS];

static void windows_cache(uint8_t r)
{
    uint8_t s, tm, k, v;
    const uint8_t *row;
    wc_r = r;
    wc_n = 0;
    if (row_type(r) == ROW_ROOF)
        return;
    row = win_tm[row_type(r)][0];
    for (s = 0; s < SLOTS; s++, row += 4) {
        k = slot_kind(r, s);
        if (k == SLOT_ROOF)
            break;
        v = 0;
        if (k == SLOT_DOOR)
            v = special_tile(r, s) - X8(bldg_door_intact);
        else if (k == SLOT_ARCH)
            v = special_tile(r, s) != X8(bldg_arch_intact);
        tm = row[v];
        wc_tx[wc_n] = slot_tx(r, s);
        wc_tm[wc_n] = tm;
        wc_h[wc_n] = tilemaps[tm].h;
        wc_n++;
    }
}

static void put_windows(uint8_t r, uint8_t y)
{
    uint8_t i, tx;
    if (r != wc_r)
        windows_cache(r);
    for (i = 0; i < wc_n; i++) {
        tx = wc_tx[i];
        if (tx >= hi || tx + 6 <= lo)
            continue;
        if (y < wc_h[i])
            put_row(wc_tm[i], tx, y, 1);
    }
}

static uint8_t bc_r = NO_ROW;
static int16_t bc_top, bc_end;

static void put_building(int16_t t)
{
    uint8_t r;
    if (!building_on)
        return;
    if (bc_r == NO_ROW || t < bc_top || t >= bc_end) {
        for (r = 0; r < rows_shown && r < pl.rows; r++) {
            int16_t top = pl.row_top[r];
            if (t >= top && t < top + row_height(r))
                break;
        }
        if (r >= rows_shown || r >= pl.rows)
            return;
        bc_r = r;
        bc_top = pl.row_top[r];
        bc_end = bc_top + row_height(r);
    }
    put_row(exe_bg_section_tm[row_type(bc_r)], BUILDING_COL, t - bc_top, 0);
    put_windows(bc_r, t - bc_top);
}

static uint8_t pc_n;
static uint8_t pc_tm[BG_PICS];
static int8_t pc_col[BG_PICS];
static int16_t pc_row[BG_PICS], pc_end[BG_PICS];

static void pics_cache(void)
{
    uint8_t i;
    pc_n = 0;
    for (i = 0; i < BG_PICS; i++) {
        bg_pic_t *p = &bg_pics[i];
        if (!p->on)
            continue;
        pc_tm[pc_n] = p->tm;
        pc_col[pc_n] = p->col;
        pc_row[pc_n] = p->row;
        pc_end[pc_n] = p->row + tilemaps[p->tm].h;
        pc_n++;
    }
}

static void put_pics(int16_t t)
{
    uint8_t i;
    for (i = 0; i < pc_n; i++)
        if (t >= pc_row[i] && t < pc_end[i])
            put_row(pc_tm[i], pc_col[i], t - pc_row[i], 0);
}

extern const uint8_t tm_rows[];

static uint16_t tm_tile(uint8_t tm, uint8_t row, uint8_t i)
{
    const uint8_t *p = tm_rows + tilemaps[tm].map[row];
    uint8_t c, n;
    uint16_t v;
    for (;;) {
        c = *p++;
        n = (c & 0x3F) + 1;
        if (i < n)
            break;
        i -= n;
        p += c & 0xC0 ? 2 : n * 2;
    }
    if (!(c & 0xC0))
        p += i * 2;
    v = p[0] | (p[1] << 8);
    if ((c & 0xC0) == 0x80)
        v += i;
    return v;
}

static int16_t ct_col;
static uint8_t ct_band, ct_band_x, ct_in_bldg, ct_sec_x, ct_r, ct_win, ct_win_x, ct_win_h;
static uint8_t ct_pn, ct_pi[BG_PICS], ct_px[BG_PICS];

static void col_setup(int16_t c)
{
    uint8_t i;
    int16_t x;
    ct_col = c;
    ct_band = 0xFF;
    for (i = 0; i < GROUND_PICS; i++) {
        x = c - exe_bg_band_cols[i];
        if (x >= 0 && x < tilemaps[TM_BUSHES].w) {
            ct_band = i;
            ct_band_x = (uint8_t)x;
        }
    }
    ct_in_bldg = building_on && c >= BUILDING_COL && c < BUILDING_COL + BUILDING_COLS;
    ct_sec_x = (uint8_t)(c - BUILDING_COL);
    ct_r = NO_ROW;
    ct_pn = 0;
    for (i = 0; i < pc_n; i++) {
        x = c - pc_col[i];
        if (x >= 0 && x < tilemaps[pc_tm[i]].w) {
            ct_pi[ct_pn] = i;
            ct_px[ct_pn++] = (uint8_t)x;
        }
    }
}

static uint16_t col_tile(int16_t t)
{
    uint16_t v = 0, e;
    uint8_t i, r;
    if (ground_on && ct_band != 0xFF && t >= BAND_TOP && t < BAND_TOP + BAND_ROWS &&
        (e = tm_tile(TM_BUSHES, t - BAND_TOP, ct_band_x)))
        v = e;
    if (ct_in_bldg) {
        if (bc_r == NO_ROW || t < bc_top || t >= bc_end) {
            for (r = 0; r < rows_shown && r < pl.rows; r++)
                if (t >= pl.row_top[r] && t < pl.row_top[r] + row_height(r))
                    break;
            if (r < rows_shown && r < pl.rows) {
                bc_r = r;
                bc_top = pl.row_top[r];
                bc_end = bc_top + row_height(r);
            } else {
                bc_r = NO_ROW;
            }
        }
        if (bc_r != NO_ROW) {
            if (ct_r != bc_r) {
                ct_r = bc_r;
                if (bc_r != wc_r)
                    windows_cache(bc_r);
                ct_win = 0xFF;
                for (i = 0; i < wc_n; i++) {
                    int16_t x = ct_col - wc_tx[i];
                    if (x >= 0 && x < tilemaps[wc_tm[i]].w) {
                        ct_win = wc_tm[i];
                        ct_win_x = (uint8_t)x;
                        ct_win_h = wc_h[i];
                    }
                }
            }
            r = (uint8_t)(t - bc_top);
            if (ct_win != 0xFF && r < ct_win_h)
                v = tm_tile(ct_win, r, ct_win_x);
            else if ((e = tm_tile(exe_bg_section_tm[row_type(bc_r)], r, ct_sec_x)))
                v = e;
        }
    }
    for (i = 0; i < ct_pn; i++) {
        r = ct_pi[i];
        if (t >= pc_row[r] && t < pc_end[r] && (e = tm_tile(pc_tm[r], t - pc_row[r], ct_px[i])))
            v = e;
    }
    return v;
}

static void compose(int16_t t, int16_t from, int16_t to, uint8_t blank)
{
    lo = from;
    hi = to;
    dst = &BG_MAP[MAP_INDEX(0, t & MAP_ROWS_MASK)];
    tm_clear(dst, lo, hi);
#ifdef __SDCC
    tmr_dst = dst;
    tmr_lo = lo;
    tmr_hi = hi;
#endif
    if (blank)
        return;
    put_ground(t);
    if (hi > BUILDING_COL && lo < BUILDING_COL + BUILDING_COLS)
        put_building(t);
    put_pics(t);
}

static uint8_t row_in(int16_t t, uint16_t sy)
{
    uint16_t y0 = ((uint16_t)(t & MAP_ROWS_MASK) * 8 - sy) & 511;
    return y0 < SCREEN_H || y0 > 504;
}

static uint8_t row_shown(int16_t t)
{
    return row_in(t, vis_sy[0]) || row_in(t, vis_sy[1]);
}

static uint8_t slot_in(int16_t c, uint8_t sx)
{
    uint8_t x0 = (uint8_t)(c * 8) - sx;
    return x0 < SCREEN_W || x0 > 248;
}

static uint8_t slot_shown(int16_t c)
{
    return slot_in(c, vis_sx[0]) || slot_in(c, vis_sx[1]);
}

static void compose_row(int16_t t, uint8_t blank)
{
    uint8_t hidden = row_shown(t);
    if (!hidden)
        pg_bg_both();
    bg_touch_row(t & MAP_ROWS_MASK, hidden);
    compose(t, wlo, wlo + 32, blank);
    if (!hidden)
        pg_hidden();
}

void bg_special(int16_t top, uint8_t count, uint8_t tm, uint8_t old, uint8_t col)
{
    uint8_t i, row, y, h = tilemaps[tm].h, w = tilemaps[tm].w;
    for (i = 0; i < BG_PIC_GROUND; i++) {
        bg_pic_t *p = &bg_pics[i];
        if (p->on && p->col < col + w && p->col + tilemaps[p->tm].w > col && p->row < top + count &&
            p->row + tilemaps[p->tm].h > top) {
            bg_rows_dirty(top, count);
            return;
        }
    }
    for (y = 0; y < h; y++, top++) {
        if (old != 0xFF && tilemaps[old].map[y] == tilemaps[tm].map[y])
            continue;
        row = top & MAP_ROWS_MASK;
        if (content[row] != top * 2) {
            bg_rows_dirty(top, 1);
            continue;
        }
        if (row_shown(top)) {
            bg_touch_part(row);
        } else {
            pg_bg_both();
            bg_touch_row(row, 0);
        }
        tm_row(tm, y, &BG_MAP[MAP_INDEX(0, row)], col, wlo, wlo + 32, 1);
        pg_hidden();
    }
}

#define COLUMN_ROWS_PER_FRAME 9

static void column_job_step(uint8_t budget)
{
    if (job_row >= job_end)
        return;
    pg_bg_both();
    col_setup(job_col);
    for (; job_row < job_end && budget; job_row++, budget--) {
        int16_t c = content[job_row & MAP_ROWS_MASK];
        if (c == INVALID || (c >> 1) != job_row)
            continue;
        if (slot_shown(job_col) && row_shown(job_row)) {
            content[job_row & MAP_ROWS_MASK] = INVALID;
            dirty = 1;
            continue;
        }
        BG_MAP[MAP_INDEX(job_col & MAP_COLS_MASK, job_row & MAP_ROWS_MASK)] = c & 1 ? 0 : col_tile(job_row);
    }
    pg_hidden();
}

static void shift_start(int16_t to, int16_t top)
{
    column_job_step(0xFF);
    job_col = to < wlo ? to : wlo + 32;
    wlo = to;
    job_row = top;
    job_end = top + VIEW_ROWS;
}

void bg_sync(void)
{
    int16_t cx = -scroll_x;
    int16_t cy = -scroll_y + bg_dy;
    int16_t col0 = cx >> 3;
    int16_t cole = (cx + SCREEN_W - 1) >> 3;
    int16_t top = cy >> 3;
    int16_t t, want;
    uint8_t i, tsx = (uint8_t)cx;
    wc_r = NO_ROW;
    bc_r = NO_ROW;
    pics_cache();
    if (cx != last_cx) {
        pan_dir = cx < last_cx ? -1 : 1;
        last_cx = cx;
    }
    if (col0 < wlo - 1 || cole > wlo + 32) {
        column_job_step(0xFF);
        wlo = pan_dir < 0 ? col0 - 1 : col0;
        bg_invalidate();
    } else if (col0 < wlo || cole > wlo + 31) {
        shift_start(col0 < wlo ? wlo - 1 : wlo + 1, top);
        column_job_step(0xFF);
    }
    if (job_row < job_end && job_col >= col0 && job_col <= cole)
        column_job_step(0xFF);
    want = pan_dir < 0 ? col0 - 1 : col0;
    if (job_row >= job_end && (want == wlo - 1 || want == wlo + 1)) {
        int16_t leaving = want < wlo ? wlo + 31 : wlo;
        if (!slot_shown(leaving) && !slot_in(leaving, tsx))
            shift_start(want, top);
    }
    column_job_step(COLUMN_ROWS_PER_FRAME);
    if (!dirty && top == drawn_top)
        return;
    dirty = 0;
    drawn_top = top;
    for (i = 0, t = top; i < VIEW_ROWS; i++, t++) {
        uint8_t blank = t * 8 + scroll_y + 16 <= HUD_HEIGHT;
        int16_t w = t * 2 + blank;
        if (content[t & MAP_ROWS_MASK] != w) {
            content[t & MAP_ROWS_MASK] = w;
            compose_row(t, blank);
        }
    }
}

void fg_pic_set(uint8_t i, uint8_t tm, int8_t col, int8_t row)
{
    fg_pic_t *p = &fg_want[i];
    if (p->on && p->tm == tm && p->col == col && p->row == row)
        return;
    p->tm = tm;
    p->col = col;
    p->row = row;
    p->on = 1;
    fg_dirty = 1;
}

void fg_pic_off(uint8_t i)
{
    if (fg_want[i].on) {
        fg_want[i].on = 0;
        fg_dirty = 1;
    }
}

void fg_pics_forget(void)
{
    memset(fg_want, 0, sizeof fg_want);
    memset(fg_have, 0, sizeof fg_have);
    fg_dirty = 0;
}

extern const uint16_t *fgp_map;
extern int8_t fgp_col, fgp_row;
extern uint8_t fgp_w, fgp_h, fgp_mode;
void fg_pic(void);

static void fg_draw(const fg_pic_t *p, uint8_t mode)
{
    int8_t r;
    uint8_t h = tilemaps[p->tm].h;
    for (r = p->row; h; h--, r++)
        if (r >= 0 && r < TEXT_ROWS)
            fg_touch(r);
    fgp_map = tilemaps[p->tm].map;
    fgp_col = p->col;
    fgp_row = p->row;
    fgp_w = tilemaps[p->tm].w;
    fgp_h = tilemaps[p->tm].h;
    fgp_mode = mode;
    fg_pic();
}

void fg_pics_sync(void)
{
    static uint8_t i;
    static fg_pic_t *o, *n;
    int8_t d;
    if (!fg_dirty)
        return;
    fg_dirty = 0;
    for (i = 0, o = fg_have, n = fg_want; i < FG_PICS; i++, o++, n++) {
        if (!memcmp(o, n, sizeof *o))
            continue;
        d = n->col - o->col;
        if (o->on && (!n->on || n->row != o->row || d > 1 || d < -1))
            fg_draw(o, 1);
        if (n->on)
            fg_draw(n, o->on ? 0 : 2);
        *o = *n;
    }
}
