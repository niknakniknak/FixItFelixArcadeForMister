/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"

void actor_set(actor_t *a, uint8_t anim)
{
    a->anim = anim;
    a->frame = 0;
    a->timer = anims[anim].frames[0].dur;
    a->flags &= (uint8_t)~(AF_DONE | AF_FROZEN);
}

#ifndef __SDCC
void actor_tick(actor_t *a)
{
    const anim_t *an;
    uint8_t f;
    a->flags &= (uint8_t)~AF_STEPPED;
    if ((a->flags & (AF_VISIBLE | AF_DONE | AF_FROZEN)) != AF_VISIBLE)
        return;
    an = &anims[a->anim];
    if (an->mode == ANIM_STATIC)
        return;
    if (a->timer) {
        a->timer--;
        return;
    }
    a->flags |= AF_STEPPED;
    f = a->frame + 1;
    if (f >= an->count) {
        if (an->mode == ANIM_ONCE) {
            f = an->count - 1;
            a->flags |= AF_DONE;
        } else {
            f -= an->count;
        }
    }
    a->frame = f;
    a->timer = an->frames[f].dur;
}
#endif

uint8_t actor_rect(const actor_t *a)
{
    return anims[a->anim].frames[a->frame].rect;
}

void actor_show(actor_t *a, uint8_t on)
{
    if (on)
        a->flags |= AF_VISIBLE;
    else
        a->flags &= (uint8_t)~AF_VISIBLE;
}

void actor_mirror(actor_t *a, uint8_t on)
{
    if (on)
        a->flags |= AF_MIRROR;
    else
        a->flags &= (uint8_t)~AF_MIRROR;
}

extern volatile uint8_t *spr_out;
extern uint8_t spr_free;

#ifndef __SDCC
void frame_cell(const anim_frame_t *fr, int16_t x, int16_t y)
{
    const msprite_t *m = &msprites[fr->rect];
    const spr_part_t *p;
    uint16_t sx, sy;
    volatile uint8_t *out;
    if (m->count != 1 || m->overlay) {
        spr_frame(fr, x, y, 0, 0, 0);
        return;
    }
    p = &spr_parts[m->first];
    sx = (uint16_t)(x - fr->hx + p->dx);
    sy = (uint16_t)(y - fr->hy + p->dy);
    if (!spr_free || sx >= SCREEN_W || sy >= SCREEN_H)
        return;
    out = spr_out;
    out[0] = (uint8_t)sx;
    out[1] = (uint8_t)sy;
    out[2] = p->code;
    out[3] = p->attr | (sy >= 256 ? SPR_ATTR_Y8 : 0);
    spr_out = out + 4;
    spr_free--;
}

void actor_draw_super(const actor_t *a, int16_t x, int16_t y, uint8_t f, uint8_t h)
{
    const anim_frame_t *fr;
    if (!(a->flags & AF_VISIBLE))
        return;
    fr = &anims[a->anim].frames[a->frame];
    x += scroll_x;
    y += scroll_y;
    if (!(a->flags & AF_MIRROR) && !f && !h)
        frame_cell(fr, x, y);
    else
        spr_frame(fr, x, y, a->flags & AF_MIRROR, f, h);
}

void actor_draw(const actor_t *a, int16_t x, int16_t y)
{
    actor_draw_super(a, x, y, 0, 0);
}
#endif

uint8_t static_count;

static uint8_t dyn_last = SPRITE_COUNT;

void sprites_begin(void)
{
    spr_begin(0, SPRITE_COUNT - static_count);
}

uint8_t sprites_finish(void)
{
    uint8_t window = SPRITE_COUNT - static_count;
    uint8_t used = window - spr_free;
    uint8_t last = dyn_last < window ? dyn_last : window;
    uint8_t n = last > used ? last - used : 0;
    volatile uint8_t *out = spr_out;
    dyn_last = used;
    while (n--) {
        out[1] = (uint8_t)SPR_HIDDEN_Y;
        out[3] = SPR_ATTR_Y8;
        out += 4;
    }
    return used;
}
