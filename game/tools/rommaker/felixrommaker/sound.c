/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEP_LOW 6
#define STEP_HIGH 8
#define CHIP_RATE 31250
#define VOICE_RATE 16000
#define G_BASE 59828
#define G_VOICE 29985
#define LOOP_PHASES 1024
#define LOW_CUTOFF 0.25
#define PAGE 0x1000
#define SAMPLE_BANK 0x100000
#define BANK_PAGES (SAMPLE_BANK / PAGE)
#define HALF_WIDTH 10
#define KAISER_BETA 8.0
#define COEF_BITS 15
#define GAIN_SHIFT (COEF_BITS + 24)
#define PI 3.141592653589793
#define LOOPED 14
#define SOUNDS 34

enum { SRC_WIN, SRC_ATG, SRC_MD };

typedef struct {
    int n, src, start, end, step, gain;
} recipe_t;

static const recipe_t recipes[SOUNDS] = {
    { 0, SRC_WIN, 6315, 40969, STEP_HIGH, G_BASE }, { 1, SRC_WIN, 0, 15082, STEP_HIGH, G_BASE },
    { 2, SRC_WIN, 5968, 39762, STEP_HIGH, G_BASE }, { 3, SRC_WIN, 1444, 10578, STEP_HIGH, G_BASE },
    { 4, SRC_WIN, 690, 41896, STEP_HIGH, G_BASE }, { 5, SRC_WIN, 724, 21316, STEP_HIGH, G_BASE },
    { 6, SRC_WIN, 717, 9785, STEP_HIGH, G_BASE }, { 7, SRC_WIN, 0, 483908, STEP_LOW, G_BASE },
    { 8, SRC_WIN, 0, 520248, STEP_LOW, G_BASE }, { 9, SRC_WIN, 28468, 691652, STEP_LOW, G_BASE },
    { 10, SRC_WIN, 0, 122292, STEP_LOW, G_BASE }, { 11, SRC_WIN, 0, 156788, STEP_LOW, G_BASE },
    { 12, SRC_WIN, 0, 269012, STEP_LOW, G_BASE }, { 13, SRC_WIN, 0, 77152, STEP_LOW, G_BASE },
    { 14, SRC_WIN, 0, 996156, STEP_HIGH, G_BASE }, { 15, SRC_WIN, 1457, 11073, STEP_HIGH, G_BASE },
    { 16, SRC_WIN, 724, 19112, STEP_HIGH, G_BASE }, { 17, SRC_WIN, 773, 21811, STEP_HIGH, G_BASE },
    { 18, SRC_WIN, 653, 54779, STEP_HIGH, G_BASE }, { 19, SRC_WIN, 704, 66886, STEP_HIGH, G_BASE },
    { 20, SRC_WIN, 0, 14378, STEP_HIGH, G_BASE }, { 21, SRC_WIN, 22776, 120540, STEP_LOW, G_BASE },
    { 22, SRC_WIN, 618, 81486, STEP_LOW, G_BASE }, { 23, SRC_WIN, 716, 44332, STEP_HIGH, G_BASE },
    { 24, SRC_WIN, 1272, 355008, STEP_LOW, G_BASE }, { 25, SRC_WIN, 652, 85260, STEP_LOW, G_BASE },
    { 26, SRC_WIN, 704, 23524, STEP_LOW, G_BASE }, { 27, SRC_WIN, 0, 2302, STEP_HIGH, G_BASE },
    { 28, SRC_WIN, 714, 44492, STEP_HIGH, G_BASE }, { 29, SRC_WIN, 0, 79380, STEP_LOW, G_BASE },
    { 30, SRC_ATG, 0, 177872, STEP_LOW, G_BASE }, { 31, SRC_ATG, 592, 89672, STEP_LOW, G_BASE },
    { 32, SRC_MD, 0x34374, 0x34374 + 15976, STEP_HIGH, G_VOICE },
    { 33, SRC_MD, 0x38410, 0x38410 + 17368, STEP_HIGH, G_VOICE },
};

static int source_rate(int src)
{
    return src == SRC_MD ? VOICE_RATE : 44100;
}

static int bank0_only(int n)
{
    return n == 11 || n == 12 || n == 13 || n == 14 || n == 21 || n == 22;
}

static int bank1_only(int n)
{
    return n == 7 || n == 8 || n == 9 || n == 10;
}

static double dsin(double x)
{
    const double two_pi = 2 * PI;
    double term, total, x2;
    long long k;
    int n;
    if (x >= 0)
        k = (long long)(x / two_pi + 0.5);
    else
        k = (long long)(x / two_pi - 0.5);
    x = x - (double)k * two_pi;
    term = x;
    total = x;
    x2 = x * x;
    for (n = 1; n < 25; n++) {
        term = -term * x2 / (double)((2 * n) * (2 * n + 1));
        total = total + term;
    }
    return total;
}

static double bessel_i0(double x)
{
    double term = 1.0, total = 1.0, q = x * x / 4.0;
    int k;
    for (k = 1; k < 60; k++) {
        term = term * q / (double)(k * k);
        total = total + term;
    }
    return total;
}

static long long gcdll(long long a, long long b)
{
    while (b) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

typedef struct {
    long long src, out, steps;
    int reach;
    double ratio, half, norm;
    int64_t **phases;
} resampler_t;

static void resampler_init(resampler_t *r, long long src, long long out, long long phases, double cutoff)
{
    long long g = gcdll(src, out);
    r->src = src / g;
    r->out = out / g;
    r->steps = phases ? phases : r->out;
    r->ratio = cutoff != 0.0 ? cutoff : fmin(1.0, (double)r->out / (double)r->src);
    r->half = HALF_WIDTH / r->ratio;
    r->reach = (int)r->half + 1;
    r->norm = bessel_i0(KAISER_BETA);
    r->phases = xcalloc((size_t)r->steps + 1, sizeof(int64_t *));
}

static double tap(const resampler_t *r, double t)
{
    double u = t / r->half, window, x, sinc = 1.0;
    if (u <= -1.0 || u >= 1.0)
        return 0.0;
    window = bessel_i0(KAISER_BETA * sqrt(1.0 - u * u)) / r->norm;
    x = r->ratio * t;
    if (x != 0.0)
        sinc = dsin(PI * x) / (PI * x);
    return r->ratio * sinc * window;
}

static int64_t *phase(resampler_t *r, long long f)
{
    double offset;
    int64_t *q, sum = 0;
    int d, n = 2 * r->reach + 1;
    if (r->phases[f])
        return r->phases[f];
    offset = (double)f / (double)r->steps;
    q = xmalloc(sizeof(int64_t) * (size_t)n);
    for (d = -r->reach; d <= r->reach; d++) {
        double c = tap(r, (double)d - offset);
        int64_t v = c >= 0 ? (int64_t)(c * (1 << COEF_BITS) + 0.5) : -(int64_t)(-c * (1 << COEF_BITS) + 0.5);
        q[d + r->reach] = v;
        sum += v;
    }
    q[r->reach] += (1 << COEF_BITS) - sum;
    r->phases[f] = q;
    return q;
}

static int64_t *run(resampler_t *r, const int64_t *x, size_t nx, long long n, int periodic)
{
    size_t reach = (size_t)r->reach, plen = nx + 2 * reach + 2, i;
    int64_t *padded = xcalloc(plen, sizeof(int64_t)), *y;
    long long k;
    int ncoef = 2 * r->reach + 1, j;
    if (n < 0)
        n = ((long long)nx * r->out + r->src - 1) / r->src;
    if (periodic) {
        for (i = 0; i < reach; i++)
            padded[i] = x[nx - reach + i];
        memcpy(padded + reach, x, sizeof(int64_t) * nx);
        for (i = 0; i < reach + 2; i++)
            padded[reach + nx + i] = x[i];
    } else {
        memcpy(padded + reach, x, sizeof(int64_t) * nx);
    }
    y = xmalloc(sizeof(int64_t) * (size_t)(n ? n : 1));
    for (k = 0; k < n; k++) {
        long long p = k * r->src, ii = p / r->out, f = p % r->out;
        int64_t *c, acc = 0;
        if (r->steps != r->out) {
            f = (2 * f * r->steps + r->out) / (2 * r->out);
            if (f == r->steps) {
                ii++;
                f = 0;
            }
        }
        c = phase(r, f);
        for (j = 0; j < ncoef; j++)
            acc += c[j] * padded[ii + j];
        y[k] = acc;
    }
    free(padded);
    return y;
}

static int play_rate16(int step)
{
    return CHIP_RATE * step;
}

static int sound_length(const recipe_t *rc)
{
    long long sr = (long long)source_rate(rc->src) * 16;
    long long length = ((long long)(rc->end - rc->start) * play_rate16(rc->step) + sr - 1) / sr;
    if (rc->n == LOOPED) {
        long long p = (length + PAGE / 2) / PAGE;
        if (p < 1)
            p = 1;
        length = p * PAGE;
    }
    return (int)length;
}

static int pages_of(int length)
{
    return (length + PAGE - 1) / PAGE;
}

typedef struct {
    int nres;
    resampler_t *res[64];
    long long key_src[64], key_out[64];
    double key_cut[64];
    int keyed[64];
} cache_t;

static uint8_t *convert(const recipe_t *rc, const sound_in_t *in, cache_t *cache, int *outlen)
{
    int length = sound_length(rc), i, k;
    int64_t *x = NULL, *acc;
    size_t nx = 0;
    double cutoff;
    resampler_t *r = NULL;
    uint8_t *out;
    long long half;
    *outlen = length;
    if (rc->src == SRC_MD) {
        if (!in->md)
            return xcalloc((size_t)length, 1);
        nx = (size_t)(rc->end - rc->start);
        x = xmalloc(sizeof(int64_t) * nx);
        for (i = 0; i < (int)nx; i++)
            x[i] = ((int64_t)in->md[rc->start + i] - 128) * 256;
    } else {
        const char *dir = rc->src == SRC_ATG ? in->atg : in->win;
        char name[32], *path;
        int64_t *data;
        size_t nd;
        if (!dir)
            return xcalloc((size_t)length, 1);
        sprintf(name, "sound%d.wav", rc->n);
        path = path_join(dir, name);
        read_wav16(path, &data, &nd);
        nx = (size_t)(rc->end - rc->start);
        x = xmalloc(sizeof(int64_t) * nx);
        for (i = 0; i < (int)nx; i++)
            x[i] = (size_t)(rc->start + i) < nd ? data[rc->start + i] : 0;
        free(data);
    }
    cutoff = (rc->step == STEP_LOW || rc->n == LOOPED) ? LOW_CUTOFF : 0.0;
    for (k = 0; k < cache->nres; k++)
        if (cache->keyed[k] && cache->key_src[k] == (long long)source_rate(rc->src) * 16 &&
            cache->key_out[k] == play_rate16(rc->step) && cache->key_cut[k] == cutoff) {
            r = cache->res[k];
            break;
        }
    if (!r) {
        r = xcalloc(1, sizeof(resampler_t));
        resampler_init(r, (long long)source_rate(rc->src) * 16, play_rate16(rc->step), 0, cutoff);
        cache->res[cache->nres] = r;
        cache->key_src[cache->nres] = (long long)source_rate(rc->src) * 16;
        cache->key_out[cache->nres] = play_rate16(rc->step);
        cache->key_cut[cache->nres] = cutoff;
        cache->keyed[cache->nres++] = 1;
    }
    acc = run(r, x, nx, -1, 0);
    if (rc->n == LOOPED) {
        size_t n0 = (size_t)((((long long)nx) * r->out + r->src - 1) / r->src), j;
        int64_t *stage = xmalloc(sizeof(int64_t) * n0);
        resampler_t *loop = xcalloc(1, sizeof(resampler_t));
        for (j = 0; j < n0; j++)
            stage[j] = (acc[j] + (1 << (COEF_BITS - 1))) >> COEF_BITS;
        free(acc);
        resampler_init(loop, (long long)n0, length, LOOP_PHASES, 0.0);
        acc = run(loop, stage, n0, length, 1);
        cache->res[cache->nres] = loop;
        cache->keyed[cache->nres++] = 0;
        free(stage);
    }
    out = xmalloc((size_t)length);
    half = 1LL << (GAIN_SHIFT - 1);
    for (i = 0; i < length; i++) {
        int64_t v = (acc[i] * rc->gain + half) >> GAIN_SHIFT;
        if (v > 127)
            v = 127;
        else if (v < -127)
            v = -127;
        out[i] = (uint8_t)v;
    }
    free(acc);
    free(x);
    return out;
}

int sample_rom(const sound_in_t *in, buf_t *out, char sha1hex[41])
{
    cache_t cache;
    uint8_t *pcm[SOUNDS];
    int len[SOUNDS], page[SOUNDS], bank[SOUNDS], i, p = 0, used = 0, size, k;
    uint8_t *rom;
    sha1_ctx sh;
    uint8_t digest[20];
    memset(&cache, 0, sizeof cache);
    for (i = 0; i < SOUNDS; i++)
        pcm[i] = convert(&recipes[i], in, &cache, &len[i]);
    for (i = 0; i < SOUNDS; i++)
        if (!bank0_only(i) && !bank1_only(i)) {
            page[i] = p;
            bank[i] = 2;
            p += pages_of(len[i]);
        }
    for (k = 0; k < 2; k++) {
        int q = p;
        for (i = 0; i < SOUNDS; i++)
            if (k == 0 ? bank0_only(i) : bank1_only(i)) {
                page[i] = q;
                bank[i] = k;
                q += pages_of(len[i]);
            }
        if (q > BANK_PAGES)
            die("sample bank %d needs %d pages of %d", k, q, BANK_PAGES);
    }
    rom = xcalloc(2 * SAMPLE_BANK, 1);
    for (i = 0; i < SOUNDS; i++)
        for (k = 0; k < 2; k++) {
            int at, end;
            if (bank[i] != 2 && bank[i] != k)
                continue;
            at = k * SAMPLE_BANK + page[i] * PAGE;
            memcpy(rom + at, pcm[i], (size_t)len[i]);
            end = k * SAMPLE_BANK + (page[i] + pages_of(len[i])) * PAGE;
            if (end > used)
                used = end;
        }
    size = (used + 0xFFFF) / 0x10000 * 0x10000;
    out->data = rom;
    out->len = (size_t)size;
    sha1_init(&sh);
    for (k = 0; k < cache.nres; k++) {
        resampler_t *r = cache.res[k];
        long long f;
        for (f = 0; f <= r->steps; f++) {
            int j, n = 2 * r->reach + 1;
            if (!r->phases[f])
                continue;
            for (j = 0; j < n; j++) {
                uint32_t c = (uint32_t)(int32_t)r->phases[f][j];
                uint8_t b[4] = { (uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16), (uint8_t)(c >> 24) };
                sha1_update(&sh, b, 4);
            }
        }
    }
    sha1_final(&sh, digest);
    for (k = 0; k < 20; k++)
        sprintf(sha1hex + 2 * k, "%02x", digest[k]);
    return 1;
}
