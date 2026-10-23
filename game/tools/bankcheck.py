#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that banked code only reaches code and data that are mapped while its bank is selected."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

LABEL = re.compile(r"^(_\w+)::?\s*$")
AREA = re.compile(r"^\s*\.area\s+(\w+)")
SYM = re.compile(r"(?<![\w$])(_\w+)")
MAPSYM = re.compile(r"^\s+([0-9A-F]{8})\s+(_\w+)\s+(\w+)?")
FIXED_END = 0x8000
HOME_END = 0xC000


def region(v):
    if v < FIXED_END:
        return "fixed"
    if v < HOME_END:
        return "home"
    if v < 0x10000:
        return "ram"
    if v >> 16 == 2:
        return "home"
    return f"bank{v >> 16}"


def load(asm_dirs, prof_map):
    addr = {}
    values = set()
    for line in Path(prof_map).read_text().splitlines():
        m = MAPSYM.match(line)
        if m:
            addr.setdefault(m.group(2), int(m.group(1), 16))
            if m.group(3):
                addr[(m.group(2), m.group(3))] = int(m.group(1), 16)
                if m.group(3) == "exe_rules":
                    values.add(m.group(2))
    funcs = {}
    for d in asm_dirs:
        for asm in Path(d).glob("*.asm"):
            cur = None
            area = None
            prev = ""
            for raw in asm.read_text(errors="replace").splitlines():
                line = raw.split(";")[0]
                a = AREA.match(line)
                if a:
                    area = a.group(1)
                    continue
                m = LABEL.match(line)
                if m and area not in ("_DATA", "_INITIALIZED", "_BSS", "_DABS", "_GSINIT", "_GSFINAL"):
                    cur = m.group(1)
                    funcs.setdefault(cur, [])
                    continue
                if not cur or not line.strip() or line.strip().startswith("."):
                    prev = line
                    continue
                for s in SYM.findall(line):
                    if s in values:
                        continue
                    banked = "___sdcc_bcall_ehl" in line or ("#b" + s) in prev or s == "___sdcc_bcall_ehl"
                    if s != cur:
                        local = (s, asm.stem)
                        funcs[cur].append((local if local in addr else s, banked))
                prev = line
    return addr, funcs


def name(s):
    return s if isinstance(s, str) else s[0]


def main():
    prof_map = sys.argv[1]
    addr, funcs = load(sys.argv[2:], prof_map)
    errors = []
    for f, refs in funcs.items():
        if f not in addr:
            continue
        rf = region(addr[f])
        for s, banked in refs:
            if s not in addr or banked:
                continue
            rs = region(addr[s])
            if rs.startswith("bank") and rs != rf:
                errors.append(f"{f} ({rf}) uses {name(s)} ({rs}) directly")
    roots = [f for f in funcs if f in addr and region(addr[f]).startswith("bank")]
    for root in roots:
        bank = region(addr[root])
        seen = set()
        stack = [(root, [root])]
        while stack:
            f, path = stack.pop()
            if f in seen:
                continue
            seen.add(f)
            for s, banked in funcs.get(f, []):
                if banked or s not in addr:
                    continue
                rs = region(addr[s])
                if rs == "home" or (rs.startswith("bank") and rs != bank):
                    errors.append(f"{' -> '.join(map(name, path + [s]))}: {name(s)} is in {rs}, not mapped while {bank} runs")
                elif s in funcs and rs in ("fixed", bank):
                    stack.append((s, path + [s]))
    for e in sorted(set(errors)):
        print("bankcheck:", e)
    if errors:
        return 1
    print(f"bankcheck: {len(roots)} banked labels checked, no unmapped references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
