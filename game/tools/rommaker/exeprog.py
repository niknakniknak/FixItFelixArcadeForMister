# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Game numbers in the program ROM: find where the linker put them (dist) and put them back from the user's exe (build)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "exedata"))
import exefw
import gen

LO = 0x11


def norm_part(part):
    if isinstance(part, dict):
        return dict(part)
    return {"rec": part[0], "idx": part[1]}


def values_of(layout, exe):
    return exefw.extract(exefw.PeImage(exe), layout["records"])


def scalar_values(layout, values):
    return {n: gen.scalar_value(p, values) for n, p in layout["scalars"].items()}


def find_relocs(real, probes, names, vals, ignore):
    bits = len(probes)
    diff = sorted({o for p in probes for o in range(len(real)) if p[o] != real[o] and o not in ignore})
    relocs = []
    done = set()
    for o in diff:
        if o in done:
            continue
        mask = sum(1 << b for b in range(bits) if probes[b][o] != real[o])
        k = mask - 1
        if not 0 <= k < len(names):
            raise ValueError(f"byte {o:#x} changes in probes {mask:#x}, no such scalar")
        name = names[k]
        v = vals[name] & 0xFFFF
        word = real[o] | real[o + 1] << 8
        if all((probes[b][o] | probes[b][o + 1] << 8) == (word + (gen.DELTA if mask >> b & 1 else 0)) & 0xFFFF
               for b in range(bits)):
            relocs.append([o, "w", name, (word - v) & 0xFFFF])
            done |= {o, o + 1}
        elif all(probes[b][o] == (real[o] + (LO if mask >> b & 1 else 0)) & 0xFF for b in range(bits)):
            relocs.append([o, "lo", name, (real[o] - v) & 0xFF])
            done.add(o)
        else:
            addend = next((a for a in sorted(range(-1024, 1024), key=abs)
                           if (v + a) >> 8 & 0xFF == real[o]
                           and all(probes[b][o] == ((v + a + (gen.DELTA if mask >> b & 1 else 0)) >> 8) & 0xFF
                                   for b in range(bits))), None)
            if addend is None:
                raise ValueError(f"byte {o:#x} ({name}) is not a word, low-byte or high-byte operand")
            relocs.append([o, "hi", name, addend & 0xFFFF])
            done.add(o)
    return relocs


def apply_relocs(rom, relocs, vals):
    for o, kind, name, addend in relocs:
        v = (vals[name] + addend) & 0xFFFF
        if kind == "hi":
            rom[o] = v >> 8
            continue
        rom[o] = v & 0xFF
        if kind == "w":
            rom[o + 1] = v >> 8


def section_bytes(layout, section, values):
    out = bytearray()
    for t in layout["tables"]:
        if t["section"] == section:
            out += gen.table_bytes(t, values)
    return bytes(out)


def dist(rom, blanked, probes, symbols, exe, rom_offset, ignore):
    scalars, tables = gen.load_specs()
    layout = {"records": gen.all_records(), "delta": gen.DELTA,
              "scalars": {n: norm_part(scalars[n]) for n in sorted(scalars)},
              "tables": [dict(t, parts=[norm_part(p) for p in t["parts"]]) for t in tables]}
    values = values_of(layout, exe)
    vals = scalar_values(layout, values)
    names = sorted(vals)
    relocs = find_relocs(rom, probes, names, vals, ignore)
    for b, p in enumerate(probes):
        check = bytearray(rom)
        probe_vals = {n: gen.probe_value(vals[n], k, b) for k, n in enumerate(names)}
        apply_relocs(check, relocs, probe_vals)
        if any(check[o] != p[o] for o in range(len(p)) if o not in ignore):
            raise ValueError(f"relocations do not reproduce probe {b}")
    used = {r[2] for r in relocs}
    missing = [n for n in names if n not in used]
    if missing:
        raise ValueError(f"scalars never used in the program: {missing}")
    sections = []
    for section in gen.SECTIONS:
        blob = section_bytes(layout, section, values)
        if not blob:
            continue
        first = next(t["name"] for t in layout["tables"] if t["section"] == section)
        addr = symbols["_" + first]
        offset = rom_offset(addr)
        if bytes(rom[offset:offset + len(blob)]) != blob:
            raise ValueError(f"exe tables of {section} differ from the program at {offset:#x}")
        sections.append({"section": section, "offset": offset, "length": len(blob)})
    out = bytearray(blanked)
    for s in sections:
        out[s["offset"]:s["offset"] + s["length"]] = bytes(s["length"])
    for o, kind, name, addend in relocs:
        out[o] = 0
        if kind == "w":
            out[o + 1] = 0
    layout["sections"] = sections
    layout["relocs"] = relocs
    return out, layout


def fill(rom, layout, exe):
    values = values_of(layout, exe)
    for s in layout["sections"]:
        blob = section_bytes(layout, s["section"], values)
        if len(blob) != s["length"]:
            raise ValueError(f"exe tables of {s['section']} are {len(blob)} bytes, the program expects {s['length']}")
        rom[s["offset"]:s["offset"] + s["length"]] = blob
    apply_relocs(rom, layout["relocs"], scalar_values(layout, values))
