/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "tobikomi.h"

namespace {

using namespace tobikomi;

const char *STATE_NAMES[] = {"title", "cutscene", "hiscores", "play", "switch", "scroll",
                             "level_end", "game_over", "entry", "boot", "howto", "service"};
const unsigned STATES = 12;

unsigned long key(unsigned long addr, unsigned bank) {
    return addr < FIXED_SIZE ? addr : ((unsigned long)bank << 16) | addr;
}

unsigned long map_key(unsigned long v) {
    if (v >= 0x10000)
        return v;
    return v < FIXED_SIZE ? v : (unsigned long)RESET_BANK << 16 | v;
}

struct Symbols {
    std::vector<unsigned long> addr;
    std::vector<std::string> name;
    std::map<std::string, unsigned long> by_name;

    int owner(unsigned long k) const {
        auto it = std::upper_bound(addr.begin(), addr.end(), k);
        if (it == addr.begin())
            return -1;
        return (int)(it - addr.begin()) - 1;
    }

    unsigned long find(const std::string &n) const {
        auto it = by_name.find(n);
        return it == by_name.end() ? ~0ul : it->second;
    }
};

Symbols load_map(const char *path) {
    Symbols s;
    FILE *f = std::fopen(path, "r");
    char line[512];
    std::vector<std::pair<unsigned long, std::string>> v;
    while (f && std::fgets(line, sizeof line, f)) {
        unsigned long a;
        char n[256];
        if (std::sscanf(line, " %lx %255s", &a, n) != 2 || n[0] != '_' || std::strstr(line, "l__") ||
            std::strstr(line, "s__"))
            continue;
        s.by_name[n] = a;
        if ((a & 0xFFFF) < 0xC000)
            v.push_back({map_key(a), n});
    }
    if (f)
        std::fclose(f);
    std::sort(v.begin(), v.end());
    for (auto &p : v) {
        s.addr.push_back(p.first);
        s.name.push_back(p.second);
    }
    return s;
}

struct Stat {
    unsigned calls = 0, over = 0, frames = 0, dropped = 0;
    unsigned long long total = 0, worst = 0, shown = 0;
    std::vector<unsigned long long> prof;
};

class Game : public Board<Game> {
public:
    bool in_call = false;
    bool copy_in_call = false;
    unsigned long long writes_at_entry = 0, writes_at_copy = 0;
    unsigned torn = 0;

    void on_copy(bool go) {
        if (go && in_call) {
            copy_in_call = true;
            writes_at_copy = obj_writes;
        }
    }
};

}

int main(int argc, char **argv) {
    if (argc < 7) {
        std::fprintf(stderr,
                     "usage: gamesim main.bin samples.bin main.map frames dump_prefix dump_frames [dsw1 dsw2 inputs]\n"
                     "  inputs: frame:port=hex,... (port 0-2 = IN0-IN2, 3 4 6 = DSW1 DSW2 DSW3, 7 = pause; a hex port >= 100 pokes that address)\n"
                     "  env: CPU_MHZ=6 (default 8), CPU_DIV=n, LINES=frame,... (line-accurate dumps), PROF_FROM=frame, STATS=file,\n"
                     "       NVRAM=file (loaded if present, written back at the end), NVRAM_AT=frame (load it then, not at reset)\n");
        return 2;
    }
    Game b;
    b.log = stdout;
    b.cpu_div = std::getenv("CPU_MHZ") && std::atoi(std::getenv("CPU_MHZ")) == 6 ? 8 : 6;
    if (std::getenv("CPU_DIV"))
        b.cpu_div = std::atoi(std::getenv("CPU_DIV"));
    b.load_rom(read_file(argv[1]));
    b.samples = read_file(argv[2]);
    unsigned nvram_at = std::getenv("NVRAM_AT") ? std::atoi(std::getenv("NVRAM_AT")) : 0;
    if (!nvram_at)
        b.load_nvram(std::getenv("NVRAM"));
    Symbols syms = load_map(argv[3]);
    unsigned frames = std::atoi(argv[4]);
    std::string prefix = argv[5];
    std::vector<unsigned> dumps = parse_list(argv[6]);
    std::vector<unsigned> line_dumps = parse_list(std::getenv("LINES"));
    if (argc > 8) {
        b.in[3] = std::strtoul(argv[7], nullptr, 16);
        b.in[4] = std::strtoul(argv[8], nullptr, 16);
    }
    std::vector<InputEvent> inputs = parse_inputs(argc > 9 ? argv[9] : nullptr);
    unsigned prof_from = std::getenv("PROF_FROM") ? std::atoi(std::getenv("PROF_FROM")) : 0;
    bool by_call = std::getenv("INPUT_BY_CALL") != nullptr;
    unsigned calls = 0;
    FILE *hash_log = std::getenv("HASH") ? std::fopen(std::getenv("HASH"), "w") : nullptr;
    unsigned long long long_call = std::getenv("LONG") ? std::strtoull(std::getenv("LONG"), nullptr, 10) : 0;
    std::vector<unsigned long long> call_prof(syms.name.size() + 1, 0);

    unsigned long a_frame = syms.find("_game_frame");
    unsigned long a_state = syms.find("_game_state");
    if (a_frame == ~0ul || a_state == ~0ul) {
        std::fprintf(stderr, "map has no _game_frame/_game_state\n");
        return 1;
    }
    unsigned long k_frame = map_key(a_frame);
    std::vector<Stat> stats(STATES + 1);
    for (Stat &s : stats)
        s.prof.assign(syms.name.size() + 1, 0);
    FILE *log = std::fopen(std::getenv("STATS") ? std::getenv("STATS") : "frames.txt", "w");

    unsigned last_frame = ~0u;
    unsigned long long frame_start = 0, call_start = 0, shown_at = 0;
    unsigned call_sp = 0, call_state = STATES, done_in_frame = 0, frame_state = STATES;
    const unsigned long long per_frame = b.frame_cycles();
    while (b.frame < frames) {
        if (b.frame != last_frame) {
            if (last_frame != ~0u && last_frame >= prof_from) {
                Stat &s = stats[frame_state < STATES ? frame_state : STATES];
                s.frames++;
                if (!done_in_frame)
                    s.dropped++;
                s.shown += b.shown_writes[0] + b.shown_writes[1] - shown_at;
            }
            shown_at = b.shown_writes[0] + b.shown_writes[1];
            frame_state = b.mem[a_state & 0xFFFF];
            if (nvram_at && b.frame == nvram_at)
                b.load_nvram(std::getenv("NVRAM"));
            done_in_frame = 0;
            for (const InputEvent &e : inputs)
                if (!by_call && e.frame == b.frame) {
                    if (e.port < 8)
                        b.in[e.port] = e.value;
                    else
                        b.mem[e.port & 0xFFFF] = e.value;
                }
            for (unsigned d : dumps)
                if (d == b.frame)
                    b.dump(prefix);
            if (b.recording)
                b.dump_lines(prefix, last_frame);
            b.recording = std::find(line_dumps.begin(), line_dumps.end(), b.frame) != line_dumps.end();
            last_frame = b.frame;
            frame_start = b.clk;
        }
        unsigned pc = b.get_pc();
        unsigned long k = key(pc, b.bank);
        if (!b.in_call && k == k_frame) {
            if (by_call)
                for (const InputEvent &e : inputs)
                    if (e.frame == calls) {
                        if (e.port < 8)
                            b.in[e.port] = e.value;
                        else
                            b.mem[e.port & 0xFFFF] = e.value;
                    }
            calls++;
            b.in_call = true;
            b.copy_in_call = false;
            b.writes_at_entry = b.obj_writes;
            if (long_call)
                std::fill(call_prof.begin(), call_prof.end(), 0ull);
            call_sp = b.get_sp();
            call_start = b.clk;
            call_state = b.mem[a_state & 0xFFFF];
            if (call_state > STATES)
                call_state = STATES;
        } else if (b.in_call && b.get_sp() > call_sp) {
            b.in_call = false;
            unsigned long long cyc = (b.clk - call_start) / b.cpu_div;
            bool torn = b.copy_in_call && b.writes_at_copy > b.writes_at_entry && b.obj_writes > b.writes_at_copy;
            b.torn += torn;
            done_in_frame++;
            if (hash_log) {
                unsigned long long h = 1469598103934665603ull;
                auto mix = [&h](const unsigned char *p, size_t n) {
                    for (size_t i = 0; i < n; i++)
                        h = (h ^ p[i]) * 1099511628211ull;
                };
                mix(&b.mem[0xD000], 0x1000);
                if (!std::getenv("HASH_BG_ONLY"))
                    mix(b.fg[b.fg_show].data(), 0x1000);
                if (!std::getenv("HASH_NO_SPR"))
                    mix(&b.mem[0xF000], 0x280);
                if (!std::getenv("HASH_BG_ONLY"))
                    mix(b.regs, 4);
                std::fprintf(hash_log, "%u %016llx\n", b.frame, h);
            }
            if (b.frame >= prof_from) {
                Stat &s = stats[call_state];
                s.calls++;
                s.total += cyc;
                s.worst = std::max(s.worst, cyc);
                s.over += cyc > per_frame;
            }
            if (log)
                std::fprintf(log, "F %u st %s cyc %llu%s%s\n", b.frame,
                             call_state < STATES ? STATE_NAMES[call_state] : "?", cyc, cyc > per_frame ? " OVER" : "",
                             torn ? " TORN" : "");
            if (long_call && cyc > long_call) {
                std::vector<std::pair<unsigned long long, std::string>> top;
                for (size_t j = 0; j < syms.name.size(); j++)
                    if (call_prof[j])
                        top.push_back({call_prof[j], syms.name[j]});
                std::sort(top.rbegin(), top.rend());
                std::printf("long call frame %u (%s) %llu cycles:", b.frame,
                            call_state < STATES ? STATE_NAMES[call_state] : "?", cyc);
                for (size_t j = 0; j < top.size() && j < 16; j++)
                    std::printf(" %s %llu", top[j].second.c_str(), top[j].first);
                std::printf("\n");
            }
        }
        if (b.frame >= prof_from) {
            int o = syms.owner(k);
            Stat &s = stats[b.in_call ? call_state : STATES];
            unsigned long long before = b.clk;
            if (b.int_pending && b.on_handle_active_int()) {
                s.prof[o < 0 ? syms.name.size() : (size_t)o] += (b.clk - before) / b.cpu_div;
                continue;
            }
            b.on_step();
            s.prof[o < 0 ? syms.name.size() : (size_t)o] += (b.clk - before) / b.cpu_div;
            if (long_call)
                call_prof[o < 0 ? syms.name.size() : (size_t)o] += (b.clk - before) / b.cpu_div;
        } else {
            if (b.int_pending && b.on_handle_active_int())
                continue;
            b.on_step();
        }
        if (b.clk - frame_start > 20ull * b.frame_clk()) {
            std::printf("frame %u: no frame boundary, pc %04x bank %u\n", b.frame, pc, b.bank);
            return 1;
        }
    }
    if (log)
        std::fclose(log);
    b.save_nvram();
    std::printf("ran %u frames at %u MHz, %llu CPU cycles per frame; sprite copies %u, held %u, torn %u\n", b.frame,
                48 / b.cpu_div, per_frame, b.copies, b.held, b.torn);
    std::printf("writes to the shown page in active display: FG %llu, BG %llu\n", b.shown_writes[0],
                b.shown_writes[1]);
    std::printf("%-10s %6s %6s %8s %8s %6s %6s %7s %7s\n", "state", "frames", "calls", "mean", "worst", "over", "drop",
                "speed", "shown");
    for (unsigned i = 0; i < STATES; i++) {
        const Stat &s = stats[i];
        if (!s.calls && !s.frames)
            continue;
        std::printf("%-10s %6u %6u %8llu %8llu %6u %6u %6.0f%% %7llu\n", STATE_NAMES[i], s.frames, s.calls,
                    s.calls ? s.total / s.calls : 0, s.worst, s.over, s.dropped,
                    s.frames ? 100.0 * (s.frames - s.dropped) / s.frames : 0.0, s.shown);
    }
    for (unsigned i = 0; i < STATES; i++) {
        const Stat &s = stats[i];
        if (!s.calls)
            continue;
        std::vector<std::pair<unsigned long long, std::string>> top;
        for (size_t j = 0; j < syms.name.size(); j++)
            if (s.prof[j])
                top.push_back({s.prof[j], syms.name[j]});
        std::sort(top.rbegin(), top.rend());
        std::printf("%s, cycles per call by function:\n", STATE_NAMES[i]);
        for (size_t j = 0; j < top.size() && j < 12; j++)
            std::printf("  %-28s %8llu\n", top[j].second.c_str(), top[j].first / s.calls);
    }
    return 0;
}
