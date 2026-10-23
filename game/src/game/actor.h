/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_ACTOR_H
#define FELIX_ACTOR_H

#include <stdint.h>

typedef struct {
    uint8_t anim;
    uint8_t frame;
    uint8_t timer;
    uint8_t flags;
} actor_t;

#define AF_VISIBLE 0x01
#define AF_DONE 0x02
#define AF_MIRROR 0x04
#define AF_FROZEN 0x08
#define AF_STEPPED 0x10

void actor_set(actor_t *a, uint8_t anim);
void actor_tick(actor_t *a);
uint8_t actor_rect(const actor_t *a);
void actor_show(actor_t *a, uint8_t on);
void actor_mirror(actor_t *a, uint8_t on);
void actor_draw(const actor_t *a, int16_t x, int16_t y);
void frame_cell(const anim_frame_t *fr, int16_t x, int16_t y);
void actor_draw_super(const actor_t *a, int16_t x, int16_t y, uint8_t f, uint8_t h);
extern uint8_t static_count;
void sprites_begin(void);
uint8_t sprites_finish(void);

#endif
