#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rewrite build/gen/gfx.c with run-length compressed tilemaps (row-indexed) as build/gen/gfx_game.c."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import GEN

MAX_RUN = 64
OP_LITERAL = 0x00
OP_SEQUENCE = 0x80
OP_REPEAT = 0xC0


def parse_maps(src):
    maps = {}
    for name, body in re.findall(r"static const uint16_t (tm_\w+)\[\] = \{(.*?)\};", src, re.S):
        maps[name] = [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]+", body)]
    return maps


def parse_table(src):
    body = re.search(r"const tilemap_t tilemaps\[TILEMAP_COUNT\] = \{(.*?)\n\};", src, re.S).group(1)
    return re.findall(r"\{(\d+),\s*(\d+),\s*(\w+)\}", body)


def run_length(row, i, step):
    j = i + 1
    while j < len(row) and row[j] == row[j - 1] + step and j - i < MAX_RUN:
        j += 1
    return j - i


def encode_row(row):
    out = bytearray()
    literal = []

    def flush():
        while literal:
            chunk = literal[:MAX_RUN]
            del literal[:MAX_RUN]
            out.append(OP_LITERAL + len(chunk) - 1)
            for v in chunk:
                out.extend(v.to_bytes(2, "little"))

    i = 0
    while i < len(row):
        same = run_length(row, i, 0)
        seq = run_length(row, i, 1)
        n, op = (same, OP_REPEAT) if same >= seq else (seq, OP_SEQUENCE)
        if n >= 2:
            flush()
            out.append(op + n - 1)
            out.extend(row[i].to_bytes(2, "little"))
            i += n
        else:
            literal.append(row[i])
            i += 1
    flush()
    return out


class RowStore:
    def __init__(self):
        self.data = bytearray()
        self.index = {}

    def add(self, stream):
        key = bytes(stream)
        if key not in self.index:
            self.index[key] = len(self.data)
            self.data += key
        return self.index[key]


def main():
    src = (GEN / "gfx.c").read_text()
    maps = parse_maps(src)
    table = parse_table(src)
    store = RowStore()
    offsets = {}
    for w, h, name in table:
        if name in maps:
            w, h = int(w), int(h)
            offsets[name] = [store.add(encode_row(maps[name][r * w:(r + 1) * w])) for r in range(h)]
    out = re.sub(r"static const uint16_t (tm_\w+)\[\] = \{.*?\};\n",
                 lambda m: "static const uint16_t %s[] = {%s};\n" % (
                     m.group(1), ", ".join(str(o) for o in offsets.get(m.group(1), []))),
                 src, flags=re.S)
    rows = ", ".join(str(b) for b in store.data)
    out = out.replace("const tilemap_t tilemaps[TILEMAP_COUNT]",
                      "const uint8_t tm_rows[] = {%s};\n\nconst tilemap_t tilemaps[TILEMAP_COUNT]" % rows, 1)
    (GEN / "gfx_game.c").write_text(out)
    raw = sum(len(v) * 2 for v in maps.values())
    packed = len(store.data) + 2 * sum(len(v) for v in offsets.values())
    print(f"tmpack: tilemaps {raw} -> {packed} bytes")


if __name__ == "__main__":
    main()
