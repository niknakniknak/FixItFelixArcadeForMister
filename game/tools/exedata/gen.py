#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the game numbers read from FixitFelixJr.exe as build/gen/exe_* (headers, tables, link-time symbols).

Scalars become absolute link-time symbols (exe_rules.s), so the compiled program holds them only as
relocated operands that the ROM maker fills in. Tables become const arrays in one generated module per
ROM section (exe_<section>.c), which the ROM maker rebuilds byte for byte.
"""
import glob
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(1, str(HERE.parent))
import common
import exefw
import records

GAME = HERE.parent.parent
GEN = GAME / "build" / "gen"
SECTIONS = ["fixed", "BANK2", "BANK3", "BANK4", "BANK5", "BANK6"]
CTYPES = {"u8": "uint8_t", "s8": "int8_t", "u16": "uint16_t", "s16": "int16_t", "u32": "uint32_t", "s32": "int32_t"}
SIZES = {"u8": 1, "s8": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4}
DELTA = 0x111


def all_records():
    out = list(records.RECORDS)
    for path in sorted(glob.glob(str(HERE / "records_*.py"))):
        spec = importlib.util.spec_from_file_location(Path(path).stem, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        out += module.RECORDS
    return out


def load_specs():
    scalars, tables = {}, []
    for path in sorted(glob.glob(str(HERE / "spec_*.py"))):
        spec = importlib.util.spec_from_file_location(Path(path).stem, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for name, part in getattr(module, "SCALARS", {}).items():
            if name in scalars:
                raise ValueError(f"scalar {name} defined twice")
            scalars[name] = part
        tables += getattr(module, "TABLES", [])
    names = [t["name"] for t in tables]
    if len(set(names)) != len(names):
        raise ValueError("table defined twice")
    kept = []
    for t in tables:
        if not t.get("dims") and len(t["fields"]) == 1 and SIZES[t["fields"][0]] == 4:
            base = t["name"][4:]
            part = dict(t["parts"][0]) if isinstance(t["parts"][0], dict) else {"rec": t["parts"][0][0], "idx": t["parts"][0][1]}
            for k, suffix in enumerate(("lo", "hi")):
                scalars[f"{base}_{suffix}"] = dict(part, word=k, long=base)
        else:
            kept.append(t)
    return scalars, kept


def part_values(part, values):
    rec = part["rec"] if isinstance(part, dict) else part[0]
    idx = part.get("idx") if isinstance(part, dict) else part[1]
    width = part.get("str") if isinstance(part, dict) else None
    v = values[rec]
    if isinstance(v, str):
        data = list(v.encode("latin-1"))
        if width is not None:
            if len(data) > width:
                raise ValueError(f"{rec}: string longer than {width}")
            data += [0] * (width - len(data))
        v = data
    elif isinstance(v, list) and v and isinstance(v[0], str):
        flat = []
        for s in v:
            data = list(s.encode("latin-1"))
            if width is not None:
                data += [0] * (width - len(data))
            flat += data
        v = flat
    if idx is None:
        out = v if isinstance(v, list) else [v]
    elif isinstance(idx, int):
        out = [v[idx]]
    else:
        out = [v[i] for i in idx]
    word = part.get("word") if isinstance(part, dict) else None
    if word is not None:
        out = [(x >> (16 * word)) & 0xFFFF for x in out]
    return out


def scalar_value(part, values):
    out = part_values(part, values)
    if len(out) != 1 or not isinstance(out[0], int):
        raise ValueError(f"scalar {part} is not one integer")
    return out[0]


def table_values(table, values):
    out = []
    for part in table["parts"]:
        out += part_values(part, values)
    count = 1
    for d in table.get("dims", []):
        count *= d
    fields = table["fields"]
    if len(out) != count * len(fields):
        raise ValueError(f"{table['name']}: {len(out)} values for {count} x {len(fields)} fields")
    for i, v in enumerate(out):
        if not isinstance(v, int):
            raise ValueError(f"{table['name']}: value {v!r} is not an integer")
        f = fields[i % len(fields)]
        lo, hi = (-(1 << (8 * SIZES[f] - 1)), (1 << (8 * SIZES[f] - 1)) - 1) if f[0] == "s" else (0, (1 << (8 * SIZES[f])) - 1)
        if not lo <= v <= hi:
            raise ValueError(f"{table['name']}: {v} does not fit {f}")
    return out


def table_bytes(table, values):
    fields = table["fields"]
    out = bytearray()
    for i, v in enumerate(table_values(table, values)):
        size = SIZES[fields[i % len(fields)]]
        out += (v & ((1 << (8 * size)) - 1)).to_bytes(size, "little")
    return bytes(out)


def c_type(table):
    return table.get("type") or CTYPES[table["fields"][0]]


def c_dims(table):
    return "".join(f"[{d}]" for d in table.get("dims", []))


def c_initializer(table, values):
    vals = [str(v) for v in table_values(table, values)]
    fields = len(table["fields"])
    if fields > 1:
        vals = ["{" + ", ".join(vals[i:i + fields]) + "}" for i in range(0, len(vals), fields)]
    dims = table.get("dims", [])
    if not dims:
        return vals[0]

    def nest(items, ds):
        if len(ds) == 1:
            return "{" + ", ".join(items) + "}"
        step = len(items) // ds[0]
        return "{" + ", ".join(nest(items[k:k + step], ds[1:]) for k in range(0, len(items), step)) + "}"
    return nest(vals, dims)


def probe_value(real, k, bit):
    return (real + (DELTA if (k + 1) >> bit & 1 else 0)) & 0xFFFF


def probe_bits(count):
    return max(1, (count + 1).bit_length())


def write_rules(path, names, scalar_vals, k_bit=None):
    lines = ["\t.module exe_rules"]
    for k, name in enumerate(names):
        v = scalar_vals[name] & 0xFFFF
        if k_bit is not None:
            v = probe_value(v, k, k_bit)
        lines += [f"\t.globl\t_exe_{name}", f"_exe_{name} == 0x{v:04X}"]
    path.write_text("\n".join(lines) + "\n")


def generate(exe_path=None, gen=GEN):
    exe_path = exe_path or common.need_exe()
    recs = all_records()
    values = exefw.extract(exefw.PeImage(Path(exe_path).read_bytes()), recs)
    scalars, tables = load_specs()
    names = sorted(scalars)
    scalar_vals = {n: scalar_value(scalars[n], values) for n in names}
    for n, v in scalar_vals.items():
        if not -0x8000 <= v <= 0xFFFF:
            raise ValueError(f"scalar {n} = {v} does not fit 16 bits")
    gen.mkdir(parents=True, exist_ok=True)
    head = ["#ifndef FELIX_EXE_SCALARS_H", "#define FELIX_EXE_SCALARS_H", ""]
    for n in names:
        v = scalar_vals[n]
        head += [f"extern const char exe_{n}[];", f"#define EXEV_{n} ({v})" if v < 0 else f"#define EXEV_{n} {v}"]
    for n in names:
        part = scalars[n]
        if isinstance(part, dict) and part.get("word") == 0:
            full = dict(part)
            del full["word"]
            v = scalar_value(full, values)
            head.append(f"#define EXEV_{part['long']} ({v})" if v < 0 else f"#define EXEV_{part['long']} {v}")
    head += ["", "#endif", ""]
    (gen / "exe_scalars.h").write_text("\n".join(head))
    decl = ["#ifndef FELIX_EXE_TABLES_H", "#define FELIX_EXE_TABLES_H", "", '#include "exe_types.h"', ""]
    for t in tables:
        decl.append(f"extern const {c_type(t)} {t['name']}{c_dims(t)};")
        if not t.get("dims") and len(t["fields"]) == 1:
            decl.append(f"#define EXEV_{t['name'][4:]} {table_values(t, values)[0]}")
    decl += ["", "#endif", ""]
    (gen / "exe_tables.h").write_text("\n".join(decl))
    for section in SECTIONS:
        body = ['#include "exe_types.h"', ""]
        for t in tables:
            if t["section"] == section:
                body.append(f"const {c_type(t)} {t['name']}{c_dims(t)} = {c_initializer(t, values)};")
                body.append("")
        (gen / f"exe_{section.lower()}.c").write_text("\n".join(body))
    write_rules(gen / "exe_rules.s", names, scalar_vals)
    bits = probe_bits(len(names))
    for b in range(bits):
        write_rules(gen / f"exe_rules_probe{b}.s", names, scalar_vals, b)
    (gen / "exe_probes.mk").write_text("EXE_PROBES := " + " ".join(str(b) for b in range(bits)) + "\n")
    print(f"exedata: {len(names)} scalars, {len(tables)} tables from {len(recs)} records")
    return values, scalars, tables, scalar_vals


def layout_json():
    scalars, tables = load_specs()
    return {"scalars": {n: scalars[n] for n in sorted(scalars)}, "tables": tables, "delta": DELTA}


if __name__ == "__main__":
    generate()
