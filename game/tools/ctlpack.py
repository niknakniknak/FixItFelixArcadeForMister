#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pack the attract-demo input recording (Controls.log) into a run-length C table."""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common
from common import DATA, GEN

FRAME_BYTES = 20
DEMO_MASK = 0x6F
MAX_RUN = 255


def frames(path):
    data = path.read_bytes()
    for i in range(len(data) // FRAME_BYTES):
        ports = struct.unpack_from("<5I", data, i * FRAME_BYTES)
        yield ports[1] & DEMO_MASK


def runs(values):
    out = []
    for v in values:
        if out and out[-1][0] == v and out[-1][1] < MAX_RUN:
            out[-1][1] += 1
        else:
            out.append([v, 1])
    return out


def main():
    common.need_exe()
    values = list(frames(DATA / "Controls.log"))
    table = runs(values)
    body = ",\n".join(f"    {v}, {n}" for v, n in table)
    GEN.mkdir(parents=True, exist_ok=True)
    (GEN / "demo_log.h").write_text(
        "#ifndef FELIX_DEMO_LOG_H\n#define FELIX_DEMO_LOG_H\n\n#include <stdint.h>\n\n"
        f"#define DEMO_FRAMES {len(values)}\n#define DEMO_RUNS {len(table)}\n\n"
        "extern const uint8_t demo_log[DEMO_RUNS * 2];\n\n#endif\n")
    (GEN / "demo_log.c").write_text(
        "#include \"demo_log.h\"\n\n"
        f"const uint8_t demo_log[DEMO_RUNS * 2] = {{\n{body},\n}};\n")
    print(f"demo_log: {len(values)} frames, {len(table)} runs")


if __name__ == "__main__":
    main()
