/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "hiscore.h"
#include "hud.h"
#include "service.h"
#include "exe.h"

hiscore_t hiscores[HISCORES];
uint8_t entry_pos;


void hiscore_defaults(void) BANKED
{
    uint8_t i;
    for (i = 0; i < HISCORES; i++) {
        memcpy(hiscores[i].name, exe_default_names[i], 4);
        hiscores[i].score = exe_default_scores[i];
    }
    hiscore = X16(hiscore_init);
    nv_hiscore_load();
}

uint8_t hiscore_insert(void) BANKED
{
    uint8_t i, j;
    for (i = 0; i < HISCORES; i++) {
        if (hiscores[i].score < pl.score) {
            for (j = HISCORES - 1; j > i; j--)
                hiscores[j] = hiscores[j - 1];
            hiscores[i].name[0] = hiscores[i].name[1] = hiscores[i].name[2] = X8(entry_letter);
            hiscores[i].name[3] = 0;
            hiscores[i].score = pl.score;
            entry_pos = i;
            return 1;
        }
    }
    return 0;
}
