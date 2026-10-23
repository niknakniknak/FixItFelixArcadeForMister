/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FELIX_SOUND_H
#define FELIX_SOUND_H

#include <stdint.h>
#include "bank.h"

enum {
    SND_FELIX_HIT, SND_HAMMER, SND_PIE_EAT, SND_DEFLECT, SND_COIN, SND_WINDOW_BREAK,
    SND_BONK, SND_MUSIC_STORY1, SND_MUSIC_STORY2, SND_MUSIC_INTRO, SND_MUSIC_INTRO_END,
    SND_MUSIC_CLEAR1, SND_MUSIC_CLEAR2, SND_MUSIC_CLEAR3, SND_MUSIC_GAME, SND_JUMP,
    SND_POUND, SND_DROP, SND_RALPH_LAND, SND_RALPH_FALL, SND_START, SND_MUSIC_GAME_OVER,
    SND_MUSIC_FLOOR_CLEAR, SND_RALPH_THROWN, SND_SUPER, SND_EXTRA_LIFE, SND_MEDAL,
    SND_NICELANDER_STEP, SND_INTRO_IMPACT, SND_RALPH_CLIMB, SND_HISCORE_ENTRY, SND_PLAYER_CHANGE, SND_VOICE_WRECK, SND_VOICE_FIX
};

void sound_init(void) BANKED;
void sound_mute(uint8_t on) BANKED;
void music_play(uint8_t sample, uint8_t loop) BANKED;
void music_stop(void) BANKED;
uint8_t music_current(void) BANKED;
void sfx_play(uint8_t sample) BANKED;
void sfx_stop(uint8_t sample) BANKED;
void sound_stop_all(void) BANKED;
void sound_pause(uint8_t on) BANKED;
void sound_tick(void) BANKED;

#endif
