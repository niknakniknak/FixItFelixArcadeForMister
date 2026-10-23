/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "states.h"

void main(void)
{
    uint8_t seen;
    game_init();
    INT_CTRL = INT_ENABLE;
    __asm__("ei");
    seen = vblank_count();
    for (;;) {
        while (vblank_count() == seen || present_busy())
            ;
        seen = vblank_count();
        game_frame();
    }
}
