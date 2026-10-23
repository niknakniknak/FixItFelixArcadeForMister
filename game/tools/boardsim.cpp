/* SPDX-FileCopyrightText: 2026 Chris Watson */
/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tobikomi.h"

namespace {

using namespace tobikomi;

class Sim : public Board<Sim> {
public:
    void on_copy(bool) {}
};

}

int main(int argc, char **argv) {
    if (argc < 6) {
        std::fprintf(stderr,
                     "usage: boardsim main.bin samples.bin frames dump_prefix dump_frames [dsw1 dsw2 inputs]\n"
                     "  dump_frames: comma list of frame numbers\n"
                     "  inputs: frame:port=hex,... (port 0-2 = IN0-IN2, value held until changed)\n"
                     "  env: CPU_MHZ=6 for a 6 MHz CPU (default 8)\n");
        return 2;
    }
    Sim b;
    b.log = stdout;
    b.cpu_div = std::getenv("CPU_MHZ") && std::atoi(std::getenv("CPU_MHZ")) == 6 ? 8 : 6;
    b.load_rom(read_file(argv[1]));
    b.samples = read_file(argv[2]);
    unsigned frames = std::atoi(argv[3]);
    std::string prefix = argv[4];
    std::vector<unsigned> dumps = parse_list(argv[5]);
    if (argc > 7) {
        b.in[3] = std::strtoul(argv[6], nullptr, 16);
        b.in[4] = std::strtoul(argv[7], nullptr, 16);
    }
    std::vector<InputEvent> inputs = parse_inputs(argc > 8 ? argv[8] : nullptr);

    unsigned last_frame = ~0u;
    unsigned long long frame_start = 0;
    while (b.frame < frames) {
        if (b.frame != last_frame) {
            for (const InputEvent &e : inputs)
                if (e.frame == b.frame && e.port < 3)
                    b.in[e.port] = e.value;
            for (unsigned d : dumps)
                if (d == b.frame)
                    b.dump(prefix);
            last_frame = b.frame;
            frame_start = b.clk;
        }
        if (b.int_pending && b.on_handle_active_int())
            continue;
        b.on_step();
        if (b.clk - frame_start > 10ull * b.frame_clk()) {
            std::printf("frame %u: no frame boundary, pc %04x\n", b.frame, (unsigned)b.get_pc());
            return 1;
        }
    }
    std::printf("ran %u frames, pc %04x, sprite copies %u, held %u\n", b.frame, (unsigned)b.get_pc(), b.copies,
                b.held);
    return 0;
}
