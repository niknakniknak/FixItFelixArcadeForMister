#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write embed_data.c for felixrommaker from the release files (felix_prog.bin, holes.json, exe.json, expected.json)."""
import json
import sys
import zlib
from pathlib import Path

TYPES = ["", "u8", "i8", "u16", "i16", "u32", "i32", "f32", "f64", "str"]
KINDS = {"data": "K_DATA", "imm": "K_IMM", "calc": "K_CALC"}
XF = ["fix16", "fix16up", "sqrt", "fix8", "round", "trunc", "add", "sub", "mul", "div", "neg", "and", "shr",
      "pick", "slice", "every", "reverse", "magicdiv", "strip0", "lowbit", "log2", "oddpart", "inv16", "rfloordiv"]
CALC = ["", "concat", "sum", "diff", "vadd", "quot", "product", "zip", "vmin", "vmax"]
SECTIONS = ["fixed", "BANK2", "BANK3", "BANK4", "BANK5", "BANK6"]
FIELDS = ["u8", "s8", "u16", "s16", "u32", "s32"]
RELOCS = {"w": 0, "lo": 1, "hi": 2}


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fits(v, bits, signed=True):
    lo, hi = (-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed else (0, (1 << bits) - 1)
    if not lo <= v <= hi:
        raise ValueError(f"{v} does not fit {bits} bits")
    return v


def rows(name, ctype, items, per=16):
    out = [f"const {ctype} {name}[] = {{"]
    for i in range(0, len(items), per):
        out.append("    " + ", ".join(items[i:i + per]) + ",")
    if not items:
        out.append("    0,")
    out.append("};")
    return out


def main():
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    prog = (src / "felix_prog.bin").read_bytes()
    holes = json.loads((src / "holes.json").read_text())
    layout = json.loads((src / "exe.json").read_text())
    expected = json.loads((src / "expected.json").read_text())
    packed = zlib.compress(prog, 9)
    lines = ['#include "embed.h"', ""]
    lines.append(f"const uint32_t embed_prog_len = {len(prog)}, embed_prog_zlen = {len(packed)};")
    lines += rows("embed_prog_z", "uint8_t", [str(b) for b in packed], 32)
    records = layout["records"]
    index = {r["name"]: i for i, r in enumerate(records)}
    ints, xfs, froms, recs = [], [], [], []
    for r in records:
        steps = r.get("xf") or []
        first_xf = len(xfs)
        for step in steps:
            op = XF.index(step[0])
            arg = step[1] if len(step) > 1 else None
            if arg is None:
                xfs.append(f"{{0.0, 0, 0, {op}, A_NONE, 0}}")
            elif isinstance(arg, list):
                xfs.append(f"{{0.0, 0, {len(ints)}, {op}, A_LIST, {len(arg)}}}")
                ints += [str(fits(int(v), 32)) for v in arg]
            elif isinstance(arg, float):
                xfs.append(f"{{{arg!r}, 0, 0, {op}, A_FLOAT, 0}}")
            else:
                xfs.append(f"{{0.0, {fits(arg, 32)}, 0, {op}, A_INT, 0}}")
        frm = r.get("from") or []
        first_from = len(froms)
        froms += [str(index[n]) for n in frm]
        typ = TYPES.index(r.get("type") or "")
        calc = CALC.index(r.get("op") or "") if r["kind"] == "calc" else 0
        recs.append(f"{{{r.get('va', 0)}u, {fits(r.get('off', 0), 16)}, {fits(r.get('count', 1), 16)}, "
                    f"{fits(r.get('stride', 0), 16)}, {first_xf}, {first_from}, {KINDS[r['kind']]}, {typ}, {calc}, "
                    f"{len(steps)}, {len(frm)}}}")
    parts = []

    def part(p):
        idx = p.get("idx")
        if idx is None:
            kind, val, start, n = 0, 0, 0, 0
        elif isinstance(idx, int):
            kind, val, start, n = 1, idx, 0, 0
        else:
            kind, val, start, n = 2, 0, len(ints), len(idx)
            ints.extend(str(v) for v in idx)
        width = p.get("str")
        word = p.get("word")
        return f"{{{val}, {index[p['rec']]}, {-1 if width is None else width}, {start}, {n}, {kind}, {-1 if word is None else word}}}"

    scalars = [part(p) for p in layout["scalars"].values()]
    names = list(layout["scalars"])
    fields, tables = [], []
    for table in layout["tables"]:
        first_part, first_field = len(parts), len(fields)
        parts += [part(p) for p in table["parts"]]
        fields += [str(FIELDS.index(f)) for f in table["fields"]]
        tables.append(f"{{{first_field}, {first_part}, {len(table['parts'])}, {SECTIONS.index(table['section'])}, "
                      f"{len(table['fields'])}}}")
    lines += rows("embed_ints", "int32_t", ints)
    lines += rows("embed_xf", "xf_t", xfs, 4)
    lines += rows("embed_from", "int16_t", froms)
    lines += rows("embed_records", "record_t", recs, 2)
    lines += rows("embed_record_names", "char *const", [c_str(r["name"]) for r in records], 4)
    lines.append(f"const int embed_nrecords = {len(recs)};")
    lines += rows("embed_scalars", "part_t", scalars, 4)
    lines.append(f"const int embed_nscalars = {len(scalars)};")
    lines += rows("embed_parts", "part_t", parts, 4)
    lines += rows("embed_fields", "uint8_t", fields)
    lines += rows("embed_tables", "etable_t", tables, 4)
    lines += rows("embed_table_names", "char *const", [c_str(t["name"]) for t in layout["tables"]], 4)
    lines.append(f"const int embed_ntables = {len(tables)};")
    lines += rows("embed_sections", "esection_t",
                  [f"{{{SECTIONS.index(s['section'])}, {s['offset']}, {s['length']}}}" for s in layout["sections"]], 4)
    lines.append(f"const int embed_nsections = {len(layout['sections'])};")
    lines += rows("embed_relocs", "reloc_t",
                  [f"{{{o}, {RELOCS[k]}, {names.index(n)}, {a}}}" for o, k, n, a in layout["relocs"]], 4)
    lines.append(f"const int embed_nrelocs = {len(layout['relocs'])};")
    lines += rows("embed_holes", "ehole_t", [f"{{{c_str(h[0])}, {h[1]}, {h[2]}, {h[3]}}}" for h in holes], 2)
    lines.append(f"const int embed_nholes = {len(holes)};")
    lines += rows("embed_expected", "expected_t", [f"{{{c_str(k)}, {c_str(v)}}}" for k, v in expected.items()] + ["{0, 0}"], 1)
    lines.append(f"const int embed_nexpected = {len(expected)};")
    out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
