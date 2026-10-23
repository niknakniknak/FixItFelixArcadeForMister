/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game.h"
#include "sound.h"
#include "samples.h"

#define MUSIC_VOICE 0
#define FIRST_SFX 1
#define NO_SAMPLE 0xFF
#define KEY_OFF_TRIES 8
#define WAIT_LOOPS 8

static const uint8_t sample_info[SAMPLES][4] = SAMPLE_TABLE;
static uint8_t voice_sample[VOICES];
static uint8_t next_sfx;
static uint8_t music;
static uint8_t muted;
static uint8_t music_loop;
static uint8_t music_wait;

static uint8_t lead_in(uint8_t sample)
{
    switch (sample) {
    case SND_MUSIC_INTRO: return 39;
    case SND_MUSIC_GAME_OVER: return 31;
    case SND_MUSIC_FLOOR_CLEAR: return 31;
    }
    return 0;
}

static void key_off(volatile uint8_t *ch)
{
    uint8_t tries = KEY_OFF_TRIES;
    while ((*ch & X1_KEYON) && tries--) {
        volatile uint8_t i;
        *ch = 0;
        for (i = WAIT_LOOPS; i; i--) {
        }
    }
}

static void voice_start(uint8_t v, uint8_t sample)
{
    volatile uint8_t *ch = &X1_CTRL(v);
    const uint8_t *info = sample_info[sample];
    key_off(ch);
    voice_sample[v] = sample;
    if (info[3] != SAMPLE_BANK_ANY)
        SOUND_BANK = info[3];
    X1_VOL(v) = X1_VOL_FULL;
    X1_FREQ(v) = info[2];
    X1_START(v) = info[0];
    X1_END(v) = info[1];
    X1_CTRL(v) = X1_KEYON;
}

static void voice_stop(uint8_t v)
{
    voice_sample[v] = NO_SAMPLE;
    key_off(&X1_CTRL(v));
}

void sound_init(void) BANKED
{
    SOUND_BANK = 0;
    sound_stop_all();
    next_sfx = FIRST_SFX;
}

void sound_mute(uint8_t on) BANKED
{
    muted = on;
    if (on)
        sound_stop_all();
}

void sound_stop_all(void) BANKED
{
    uint8_t v;
    for (v = 0; v < VOICES; v++)
        voice_stop(v);
    music = NO_SAMPLE;
    music_wait = 0;
}

void sound_pause(uint8_t on) BANKED
{
    uint8_t v;
    for (v = 0; v < VOICES; v++) {
        if (voice_sample[v] == NO_SAMPLE)
            continue;
        X1_FREQ(v) = on ? 0 : sample_info[voice_sample[v]][2];
        X1_VOL(v) = on ? 0 : X1_VOL_FULL;
    }
}

void sound_tick(void) BANKED
{
    if (music_wait && !--music_wait && music != NO_SAMPLE && !muted)
        voice_start(MUSIC_VOICE, music);
    else if (music_loop && !music_wait && music != NO_SAMPLE && voice_sample[MUSIC_VOICE] == music &&
             !(X1_CTRL(MUSIC_VOICE) & X1_KEYON))
        voice_start(MUSIC_VOICE, music);
}

void music_play(uint8_t sample, uint8_t loop) BANKED
{
    if (sample == music)
        return;
    music = sample;
    music_loop = loop;
    music_wait = lead_in(sample);
    if (muted || music_wait) {
        voice_stop(MUSIC_VOICE);
        return;
    }
    voice_start(MUSIC_VOICE, sample);
}

void music_stop(void) BANKED
{
    music = NO_SAMPLE;
    music_wait = 0;
    voice_stop(MUSIC_VOICE);
}

uint8_t music_current(void) BANKED
{
    return music;
}

void sfx_play(uint8_t sample) BANKED
{
    uint8_t v;
    if (muted && sample != SND_COIN)
        return;
    for (v = FIRST_SFX; v < VOICES; v++) {
        if (voice_sample[v] == sample) {
            voice_start(v, sample);
            return;
        }
    }
    v = next_sfx;
    voice_start(v, sample);
    next_sfx = v + 1 < VOICES ? v + 1 : FIRST_SFX;
}

void sfx_stop(uint8_t sample) BANKED
{
    uint8_t v;
    if (music == sample) {
        music_stop();
        return;
    }
    for (v = FIRST_SFX; v < VOICES; v++)
        if (voice_sample[v] == sample)
            voice_stop(v);
}
