/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_HW_H
#define FELIX_HW_H

#include <stdint.h>

#define REG8(addr) (*(volatile uint8_t *)(addr))

#define ROM_BASE 0x0000
#define ROM_SIZE 0xC000
#define RAM_BASE 0xC000
#define RAM_SIZE 0x1000

#define MAP_COLS 32
#define MAP_ROWS 64
#define BG_MAP ((volatile uint16_t *)0xD000)
#define FG_MAP ((volatile uint16_t *)0xE000)
#define MAP_INDEX(col, row) ((uint16_t)(row) * MAP_COLS + (col))
#define SCREEN_W 240
#define SCREEN_H 320
#define TEXT_COLS 30
#define TEXT_ROWS 40

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t code;
    uint8_t attr;
} sprite_t;

#define SPRITES ((volatile sprite_t *)0xF000)
#define SPR_SIZE ((volatile uint8_t *)0xF200)
#define SPRITE_COUNT 128
#define SPR_ATTR_Y8 0x04
#define SPR_ATTR_CODE_HI 0x03
#define SPR_PAL_SHIFT 3
#define SPR_HIDDEN_X 240
#define SPR_HIDDEN_Y 320

#define IN0 REG8(0xF800)
#define IN1 REG8(0xF801)
#define IN2 REG8(0xF802)
#define DSW1 REG8(0xF803)
#define DSW2 REG8(0xF804)
#define DSW3 REG8(0xF806)

#define IN_RIGHT 0x01
#define IN_LEFT 0x02
#define IN_DOWN 0x04
#define IN_UP 0x08
#define IN_JUMP 0x10
#define IN_FIX 0x20

#define SYS_COIN_A 0x01
#define SYS_COIN_B 0x02
#define SYS_START1 0x04
#define SYS_START2 0x08
#define SYS_SERVICE 0x10
#define SYS_TEST 0x20
#define SYS_INPUTS 0x3F
#define SYS_VBLANK 0x80

#define BG_SCROLL_Y_LO REG8(0xF810)
#define BG_SCROLL_Y_HI REG8(0xF811)
#define BG_SCROLL_X REG8(0xF812)
#define VIDEO_CTRL REG8(0xF813)
#define INT_CTRL REG8(0xF814)
#define COIN_COUNTER REG8(0xF815)
#define SPR_COMMIT REG8(0xF816)
#define FG_PAGE REG8(0xF817)
#define ROM_BANK REG8(0xF818)

#define VIDEO_COMMIT 0x10
#define SYS_COMMIT_PENDING 0x40
#define FG_SHOW 0x01
#define FG_CPU 0x02
#define FG_BOTH 0x04
#define SPR_W32 0x01
#define SPR_W64 0x02
#define SPR_H32 0x04
#define SPR_H64 0x08
#define SPR_X8 0x10
#define SPR_CODE10 0x20
#define SPR_BEHIND 0x40

#define VIDEO_FLIP 0x01
#define VIDEO_BG 0x02
#define VIDEO_FG 0x04
#define VIDEO_SPRITES 0x08

#define INT_ENABLE 0x01

#define VOICES 6
#define X1_REG(voice, reg) REG8(0xF900 + (voice) * 8 + (reg))
#define X1_CTRL(voice) X1_REG(voice, 0)
#define X1_VOL(voice) X1_REG(voice, 1)
#define X1_FREQ(voice) X1_REG(voice, 2)
#define X1_START(voice) X1_REG(voice, 4)
#define X1_END(voice) X1_REG(voice, 5)
#define X1_KEYON 0x01
#define X1_VOL_FULL 0xFF
#define SOUND_BANK REG8(0xF819)

static inline void bg_scroll(uint8_t x, uint16_t y)
{
    BG_SCROLL_X = x;
    BG_SCROLL_Y_LO = (uint8_t)y;
    BG_SCROLL_Y_HI = (uint8_t)(y >> 8) & 1;
}

static inline void sprite_set(uint8_t n, uint8_t x, uint16_t y, uint16_t code, uint8_t pal)
{
    volatile sprite_t *s = &SPRITES[n];
    s->x = x;
    s->y = (uint8_t)y;
    s->code = (uint8_t)code;
    s->attr = ((uint8_t)(code >> 8) & SPR_ATTR_CODE_HI) | (y & 0x100 ? SPR_ATTR_Y8 : 0) |
              (uint8_t)(pal << SPR_PAL_SHIFT);
}

static inline void sprite_hide(uint8_t n)
{
    SPRITES[n].x = SPR_HIDDEN_X;
    SPRITES[n].y = (uint8_t)SPR_HIDDEN_Y;
    SPRITES[n].attr = SPR_ATTR_Y8;
}

static inline void interrupts_on(void)
{
    INT_CTRL = INT_ENABLE;
    __asm__("ei");
}

#endif
