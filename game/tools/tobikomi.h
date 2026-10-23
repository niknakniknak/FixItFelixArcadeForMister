/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TOBIKOMI_H
#define TOBIKOMI_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "z80.h"

namespace tobikomi {

using z80::fast_u8;
using z80::fast_u16;

const unsigned CLK_PER_LINE = 3073;
const unsigned DMA_HPOS = 356 * 7;
const unsigned LINES = 262;
const unsigned VBLANK_LINE = 240;
const unsigned DMA_LINE = 238;
const unsigned LATE_END = 258;
const unsigned ROM_MAX = 0x20000;
const unsigned FIXED_SIZE = 0x8000;
const unsigned BANK_SIZE = 0x4000;
const unsigned RESET_BANK = 2;
const unsigned OBJ_RAM = 0x400;
const unsigned OBJ_COPY = 0x280;
const unsigned NVRAM_BASE = 0xF400;
const unsigned NVRAM_SIZE = 0x400;
const unsigned LINE_REC = 0x2000 + 8;

const unsigned X1_CHANNELS = 16;
const unsigned X1_TICKS_PER_LINE = 2;

struct Voice {
    unsigned long pos = 0;
};

struct InputEvent {
    unsigned frame;
    unsigned port;
    unsigned value;
};

template <typename D>
class Board : public z80::z80_cpu<D> {
public:
    std::vector<unsigned char> rom = std::vector<unsigned char>(ROM_MAX, 0xFF);
    std::vector<unsigned char> mem = std::vector<unsigned char>(0x10000, 0);
    std::vector<unsigned char> fg[2] = {std::vector<unsigned char>(0x1000, 0), std::vector<unsigned char>(0x1000, 0)};
    std::vector<unsigned char> bg[2] = {std::vector<unsigned char>(0x1000, 0), std::vector<unsigned char>(0x1000, 0)};
    std::vector<unsigned char> samples;
    std::vector<unsigned char> nvram = std::vector<unsigned char>(NVRAM_SIZE, 0xFF);
    std::string nvram_path;
    unsigned char copy[OBJ_COPY] = {};
    unsigned char in[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char regs[0x30] = {};
    unsigned char x1[0x100] = {};
    Voice voice[X1_CHANNELS];
    unsigned snd_bank = 0;
    unsigned cpu_div = 8;
    unsigned long long clk = 0;
    unsigned bank = RESET_BANK;
    unsigned fg_show = 0, fg_req = 0, fg_cpu = 0, fg_both = 0;
    unsigned bg_show = 0, bg_req = 0, bg_cpu = 0, bg_both = 0;
    bool cmode = false, pending = false, copied = false;
    bool int_pending = false;
    unsigned frame = 0;
    unsigned copies = 0, held = 0;
    unsigned long long obj_writes = 0;
    unsigned long long shown_writes[2] = {0, 0};
    bool recording = false;
    std::vector<unsigned char> lines;
    FILE *log = stdout;

    unsigned line() const { return (unsigned)((clk / CLK_PER_LINE) % LINES); }
    unsigned hclk() const { return (unsigned)(clk % CLK_PER_LINE); }
    unsigned long long frame_clk() const { return (unsigned long long)CLK_PER_LINE * LINES; }
    unsigned long long frame_cycles() const { return frame_clk() / cpu_div; }

    void load_rom(const std::vector<unsigned char> &image) {
        std::memcpy(rom.data(), image.data(), image.size() < ROM_MAX ? image.size() : ROM_MAX);
    }

    unsigned rom_addr(unsigned addr) const {
        return addr < FIXED_SIZE ? addr : bank * BANK_SIZE + (addr - FIXED_SIZE);
    }

    fast_u8 on_read(fast_u16 addr) {
        if (addr < 0xC000)
            return rom[rom_addr(addr)];
        if (addr >= 0xE000 && addr < 0xF000)
            return fg[fg_cpu][addr - 0xE000];
        if (addr >= 0xD000 && addr < 0xE000)
            return bg[bg_cpu][addr - 0xD000];
        if (addr >= 0xF000 && addr < 0xF000 + OBJ_RAM)
            return mem[addr];
        if (addr >= 0xF800 && addr <= 0xF804) {
            unsigned char v = in[addr - 0xF800];
            if (addr == 0xF802) {
                v &= 0x3F;
                if (line() >= VBLANK_LINE)
                    v |= 0x80;
                if (pending)
                    v |= 0x40;
            }
            return v;
        }
        if (addr >= NVRAM_BASE && addr < NVRAM_BASE + NVRAM_SIZE)
            return nvram[addr - NVRAM_BASE];
        if (addr == 0xF805)
            return 0;
        if (addr >= 0xF900 && addr < 0xFA00)
            return x1[addr - 0xF900];
        if (addr == 0xF806)
            return in[6];
        if (addr == 0xF807)
            return in[7];
        if (addr >= 0xF000)
            return 0xFF;
        return mem[addr];
    }

    void on_write(fast_u16 addr, fast_u8 n) {
        if (addr < 0xC000) {
            std::fprintf(log, "frame %u: write %02x to ROM %04x\n", frame, (unsigned)n, (unsigned)addr);
            return;
        }
        if (addr >= 0xE000 && addr < 0xF000) {
            if ((fg_both || fg_cpu == fg_show) && line() < VBLANK_LINE)
                shown_writes[0]++;
            if (fg_both || !fg_cpu)
                fg[0][addr - 0xE000] = n;
            if (fg_both || fg_cpu)
                fg[1][addr - 0xE000] = n;
            return;
        }
        if (addr >= 0xD000 && addr < 0xE000) {
            if ((bg_both || bg_cpu == bg_show) && line() < VBLANK_LINE)
                shown_writes[1]++;
            if (bg_both || !bg_cpu)
                bg[0][addr - 0xD000] = n;
            if (bg_both || bg_cpu)
                bg[1][addr - 0xD000] = n;
            return;
        }
        if (addr < 0xF000 + OBJ_RAM) {
            if (addr >= 0xF000)
                obj_writes++;
            mem[addr] = n;
            return;
        }
        if (addr >= NVRAM_BASE && addr < NVRAM_BASE + NVRAM_SIZE) {
            nvram[addr - NVRAM_BASE] = n;
            return;
        }
        if (addr >= 0xF810 && addr < 0xF840) {
            regs[addr - 0xF810] = n;
            switch (addr) {
            case 0xF813: cmode = n & 0x10; break;
            case 0xF814: if (!(n & 1)) int_pending = false; break;
            case 0xF816:
                pending = true;
                if (copied && line() >= DMA_LINE && line() < LATE_END)
                    sprite_copy();
                break;
            case 0xF817:
                fg_req = n & 1;
                fg_cpu = (n >> 1) & 1;
                fg_both = (n >> 2) & 1;
                bg_req = (n >> 3) & 1;
                bg_cpu = (n >> 4) & 1;
                bg_both = (n >> 5) & 1;
                break;
            case 0xF818: bank = n & 7; break;
            case 0xF819: snd_bank = n & 1; break;
            }
            if (addr == 0xF815 && n)
                std::fprintf(log, "frame %u: %04x <- %02x\n", frame, (unsigned)addr, (unsigned)n);
            return;
        }
        if (addr >= 0xF900 && addr < 0xFA00) {
            x1_write(addr - 0xF900, n);
            return;
        }
        std::fprintf(log, "frame %u: write %02x to unmapped %04x\n", frame, (unsigned)n, (unsigned)addr);
    }

    void load_nvram(const char *path) {
        nvram_path = path ? path : "";
        if (nvram_path.empty())
            return;
        FILE *f = std::fopen(path, "rb");
        if (!f)
            return;
        size_t n = std::fread(nvram.data(), 1, NVRAM_SIZE, f);
        std::fclose(f);
        if (n != NVRAM_SIZE)
            std::fprintf(log, "%s: %zu bytes, rest left FF\n", path, n);
    }

    void save_nvram() const {
        if (nvram_path.empty())
            return;
        FILE *f = std::fopen(nvram_path.c_str(), "wb");
        if (!f) {
            std::perror(nvram_path.c_str());
            return;
        }
        std::fwrite(nvram.data(), 1, NVRAM_SIZE, f);
        std::fclose(f);
    }

    void x1_write(unsigned a, unsigned n) {
        unsigned ch = a >> 3;
        if (ch < X1_CHANNELS && (a & 7) == 0 && !(x1[a] & 1) && (n & 1)) {
            voice[ch].pos = 0;
            std::fprintf(log, "frame %u: voice %u plays page %02x-%02x bank %u step %u\n", frame, ch,
                         x1[a + 4], 0xFF - x1[a + 5], snd_bank, x1[a + 2]);
        }
        x1[a] = n;
    }

    void advance_line() {
        for (unsigned ch = 0; ch < X1_CHANNELS; ch++) {
            unsigned char *r = &x1[ch * 8];
            if (!(r[0] & 1) || (r[0] & 2))
                continue;
            for (unsigned t = 0; t < X1_TICKS_PER_LINE; t++) {
                unsigned long page = r[4] + ((voice[ch].pos >> 4) >> 12);
                if (page >= 0x100u - r[5]) {
                    r[0] &= 0xFE;
                    break;
                }
                voice[ch].pos += r[2] >> ((r[0] >> 7) & 1);
            }
        }
    }

    void sprite_copy() {
        bool go = !cmode || pending;
        pending = false;
        if (go) {
            std::memcpy(copy, &mem[0xF000], OBJ_COPY);
            copies++;
        } else {
            held++;
        }
        static_cast<D *>(this)->on_copy(go);
    }

    void record_line() {
        if (!recording)
            return;
        lines.insert(lines.end(), bg[bg_show].begin(), bg[bg_show].end());
        lines.insert(lines.end(), fg[fg_show].begin(), fg[fg_show].end());
        lines.insert(lines.end(), regs, regs + 8);
    }

    void on_tick(unsigned t) {
        unsigned before = line();
        unsigned hbefore = hclk();
        clk += (unsigned long long)t * cpu_div;
        if (!copied && line() == DMA_LINE && (before != DMA_LINE || hbefore < DMA_HPOS) && hclk() >= DMA_HPOS) {
            copied = true;
            sprite_copy();
        }
        if (line() == before)
            return;
        advance_line();
        if (line() == VBLANK_LINE) {
            fg_show = fg_req;
            bg_show = bg_req;
            if (regs[4] & 1)
                int_pending = true;
        }
        if (line() == 0) {
            frame++;
            copied = false;
        }
        if (line() < VBLANK_LINE)
            record_line();
    }

    void dump(const std::string &prefix) const {
        std::string path = prefix + std::to_string(frame) + ".bin";
        FILE *f = std::fopen(path.c_str(), "wb");
        std::vector<unsigned char> view(mem);
        std::memcpy(&view[0xD000], bg[bg_show].data(), 0x1000);
        std::memcpy(&view[0xE000], fg[fg_show].data(), 0x1000);
        std::memcpy(&view[0xF000], copy, OBJ_COPY);
        std::memcpy(&view[NVRAM_BASE], nvram.data(), NVRAM_SIZE);
        std::fwrite(view.data(), 1, 0x10000, f);
        std::fwrite(regs, 1, sizeof regs, f);
        std::fclose(f);
    }

    void dump_lines(const std::string &prefix, unsigned n) {
        std::string path = prefix + "L" + std::to_string(n) + ".bin";
        FILE *f = std::fopen(path.c_str(), "wb");
        std::fwrite(copy, 1, OBJ_COPY, f);
        std::fwrite(lines.data(), 1, lines.size(), f);
        std::fclose(f);
        lines.clear();
    }
};

inline std::vector<unsigned char> read_file(const char *path) {
    std::vector<unsigned char> data;
    FILE *f = std::fopen(path, "rb");
    if (!f) {
        std::perror(path);
        std::exit(1);
    }
    int c;
    while ((c = std::fgetc(f)) != EOF)
        data.push_back((unsigned char)c);
    std::fclose(f);
    return data;
}

inline std::vector<InputEvent> parse_inputs(const char *spec) {
    std::vector<InputEvent> events;
    while (spec && *spec) {
        InputEvent e;
        int used = 0;
        if (std::sscanf(spec, "%u:%x=%x%n", &e.frame, &e.port, &e.value, &used) != 3)
            break;
        events.push_back(e);
        spec += used;
        if (*spec == ',')
            spec++;
    }
    return events;
}

inline std::vector<unsigned> parse_list(const char *p) {
    std::vector<unsigned> v;
    while (p && *p) {
        char *end;
        v.push_back(std::strtoul(p, &end, 10));
        if (end == p)
            break;
        p = end;
        if (*p == ',')
            p++;
    }
    return v;
}

}

#endif
