# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Lay out the constant tables of a generated C file as SDCC places them in Z80 ROM."""
import re

SCALARS = {"uint8_t": 1, "int8_t": 1, "char": 1, "uint16_t": 2, "int16_t": 2}
POINTER = 2


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def header_symbols(text):
    text = strip_comments(text)
    values = {}
    for name, value in re.findall(r"#define\s+(\w+)\s+(-?(?:0x[0-9a-fA-F]+|\d+))\s*$", text, re.M):
        values[name] = int(value, 0)
    for body in re.findall(r"enum\s*\{(.*?)\}", text, re.S):
        n = 0
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, value = (s.strip() for s in item.split("="))
                n = evaluate(value, values)
            else:
                name = item
            values[name] = n
            n += 1
    types = {}
    for body, name in re.findall(r"typedef\s+struct\s*\{(.*?)\}\s*(\w+)\s*;", text, re.S):
        fields = []
        for decl in body.split(";"):
            decl = decl.strip()
            if not decl:
                continue
            m = re.match(r"(?:const\s+)?(\w+)\s+(.*)", decl)
            ctype, names = m.groups()
            for item in names.split(","):
                fields.append(POINTER if "*" in item else SCALARS[ctype])
        types[name] = fields
    return values, types


def evaluate(expr, values):
    expr = expr.strip()
    if re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+)", expr):
        return int(expr, 0)
    if expr in values:
        return values[expr]
    raise ValueError(f"cannot evaluate {expr!r}")


def flatten(body):
    return [t.strip() for t in re.split(r"[{},]", body) if t.strip()]


def declarations(text, values):
    text = strip_comments(text)
    pattern = r"(?:static\s+)?const\s+(\w+)\s+(\w+)((?:\[[^\]]*\])+)\s*=\s*(\{.*?\});"
    return [(ctype, name, dims, body) for ctype, name, dims, body in re.findall(pattern, text, re.S)]


def element_sizes(ctype, types):
    return types[ctype] if ctype in types else [SCALARS[ctype]]


def pack(sources, header, base):
    values, types = header_symbols(header)
    decls = []
    for text in sources:
        decls += declarations(text, values)
    addresses = {}
    at = base
    layout = []
    for ctype, name, dims, body in decls:
        sizes = element_sizes(ctype, types)
        tokens = flatten(body)
        if len(tokens) % len(sizes):
            raise ValueError(f"{name}: {len(tokens)} values for {len(sizes)}-field elements")
        addresses[name] = at
        layout.append((name, sizes, tokens))
        at += sum(sizes) * (len(tokens) // len(sizes))
    out = bytearray()
    for name, sizes, tokens in layout:
        for i, token in enumerate(tokens):
            size = sizes[i % len(sizes)]
            m = re.fullmatch(r"&\s*(\w+)\s*\[\s*(\w+)\s*\]", token)
            if m:
                target = m.group(1)
                elem = sum(next(s for n, s, _ in layout if n == target))
                value = addresses[target] + elem * evaluate(m.group(2), values)
            elif token in addresses:
                value = addresses[token]
            else:
                value = evaluate(token, values)
            out += (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")
    return bytes(out), addresses
