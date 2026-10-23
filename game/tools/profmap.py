#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Add static function labels from the assembler listings to a link map, for gamesim profiles."""
import re
import sys
from pathlib import Path

LABEL = re.compile(r"^\s+([0-9A-F]{8})\s+\d+\s+(_\w+):(:?)\s*$")
AREA = re.compile(r"^\s+\d+\s+\.area\s+(\w+)")
MAPSYM = re.compile(r"^\s+([0-9A-F]{8})\s+(_\w+)\s+(\w+)")


def main():
    map_path, out_path = Path(sys.argv[1]), Path(sys.argv[2])
    lst_dirs = [Path(p) for p in sys.argv[3:]]
    text = map_path.read_text()
    known = {}
    for line in text.splitlines():
        m = MAPSYM.match(line)
        if m:
            known[m.group(2)] = int(m.group(1), 16)
    extra = []
    for d in lst_dirs:
        for lst in sorted(d.glob("*.lst")):
            area = None
            labels = {}
            for line in lst.read_text(errors="replace").splitlines():
                a = AREA.match(line)
                if a:
                    area = a.group(1)
                    continue
                m = LABEL.match(line)
                if m and area and area not in ("_DATA", "_INITIALIZED", "_BSS"):
                    labels.setdefault(area, []).append((int(m.group(1), 16), m.group(2), m.group(3) == ":"))
            for area, items in labels.items():
                base = None
                for rel, name, glob in items:
                    if glob and name in known:
                        base = known[name] - rel
                        break
                if base is None:
                    continue
                for rel, name, glob in items:
                    if not glob and name not in known:
                        extra.append(f"     {base + rel:08X}  {name}  {lst.stem}")
    out_path.write_text(text + "\n" + "\n".join(extra) + "\n")


if __name__ == "__main__":
    sys.exit(main())
