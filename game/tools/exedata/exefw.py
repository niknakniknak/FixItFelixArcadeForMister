# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read game numbers from FixitFelixJr.exe: PE reader, record reader, transforms.

A record is a dict: name, kind (data: values stored at va; imm: an instruction operand at va + off; calc: made from other records with op), type (u8 i8 u16 i16 u32 i32 f32 f64 str), count, stride, xf (transform steps).
"""
import math
import struct
import sys

IMAGE_BASE = 0x400000
SIZES = {"u8": 1, "i8": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4, "f32": 4, "f64": 8, "str": 1}
FORMATS = {"u8": "<B", "i8": "<b", "u16": "<H", "i16": "<h", "u32": "<I", "i32": "<i",
           "f32": "<f", "f64": "<d"}


class PeImage:
    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        count = struct.unpack_from("<H", data, pe + 6)[0]
        opt_size = struct.unpack_from("<H", data, pe + 20)[0]
        self.sections = []
        for i in range(count):
            off = pe + 24 + opt_size + 40 * i
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, off + 8)
            self.sections.append((vaddr, rsize, rptr))

    def offset(self, va):
        rva = va - IMAGE_BASE
        for vaddr, rsize, rptr in self.sections:
            if vaddr <= rva < vaddr + rsize:
                return rptr + rva - vaddr
        raise ValueError("VA %#x not in file data" % va)

    def read(self, typ, va):
        return struct.unpack_from(FORMATS[typ], self.data, self.offset(va))[0]


def read_record(pe, rec, done):
    kind = rec["kind"]
    typ = rec.get("type")
    count = rec.get("count", 1)
    if kind == "calc":
        values = calc(rec["op"], [done[n] for n in rec["from"]], rec.get("arg"))
    elif typ == "str":
        start = pe.offset(rec["va"])
        values = pe.data[start:start + count].decode("latin-1")
    elif kind == "imm":
        values = [pe.read(typ, rec["va"] + rec["off"])]
    elif kind == "data":
        stride = rec.get("stride", SIZES[typ])
        values = [pe.read(typ, rec["va"] + k * stride) for k in range(count)]
    else:
        raise ValueError("bad kind " + kind)
    for step in rec.get("xf", []):
        values = transform(step, values)
    if isinstance(values, list) and count == 1 and kind != "calc" and len(values) == 1:
        return values[0]
    return values


def c_round(v):
    return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))


def each(values, fn):
    if isinstance(values, list):
        return [fn(v) for v in values]
    return fn(values)


def transform(step, values):
    op = step[0]
    arg = step[1] if len(step) > 1 else None
    if op == "fix16":
        return each(values, lambda v: c_round(v * 65536))
    if op == "fix16up":
        return each(values, lambda v: int(math.ceil(v * 65536)))
    if op == "sqrt":
        return each(values, math.sqrt)
    if op == "fix8":
        return each(values, lambda v: c_round(v * 256))
    if op == "round":
        return each(values, c_round)
    if op == "trunc":
        return each(values, lambda v: int(v))
    if op == "add":
        return each(values, lambda v: v + arg)
    if op == "sub":
        return each(values, lambda v: v - arg)
    if op == "mul":
        return each(values, lambda v: v * arg)
    if op == "div":
        return each(values, lambda v: v // arg)
    if op == "neg":
        return each(values, lambda v: -v)
    if op == "and":
        return each(values, lambda v: v & arg)
    if op == "shr":
        return each(values, lambda v: v >> arg)
    if op == "pick":
        return [values[k] for k in arg]
    if op == "slice":
        return values[arg[0]:arg[1]]
    if op == "every":
        return values[arg[0]::arg[1]]
    if op == "reverse":
        return list(reversed(values))
    if op == "magicdiv":
        return each(values, lambda v: c_round(2 ** (32 + arg) / v))
    if op == "lowbit":
        return each(values, lambda v: v & -v)
    if op == "log2":
        return each(values, lambda v: v.bit_length() - 1)
    if op == "oddpart":
        return each(values, lambda v: v // (v & -v))
    if op == "inv16":
        return each(values, lambda v: pow(v, -1, 65536))
    if op == "rfloordiv":
        return each(values, lambda v: arg // v)
    if op == "strip0":
        return values.split("\0")[0]
    raise ValueError("bad transform " + op)


def calc(op, inputs, arg):
    if op == "concat":
        out = []
        for v in inputs:
            out += v if isinstance(v, list) else [v]
        return out
    if op == "sum":
        return sum(inputs)
    if op == "diff":
        return inputs[0] - inputs[1]
    if op == "vadd":
        return [a + b for a, b in zip(inputs[0], inputs[1])]
    if op == "quot":
        return inputs[0] / inputs[1]
    if op == "product":
        return inputs[0] * inputs[1]
    if op == "vmin":
        return [min(group) for group in zip(*inputs)]
    if op == "vmax":
        return [max(group) for group in zip(*inputs)]
    if op == "zip":
        out = []
        for group in zip(*inputs):
            out += list(group)
        return out
    raise ValueError("bad calc " + op)


def extract(pe, records):
    done = {}
    for rec in records:
        if rec["name"] in done:
            raise ValueError("duplicate record " + rec["name"])
        done[rec["name"]] = read_record(pe, rec, done)
    return done


