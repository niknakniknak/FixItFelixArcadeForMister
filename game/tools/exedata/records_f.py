# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
def _const(name, src, c):
    return {"name": name, "kind": "calc", "op": "sum", "from": [src], "xf": [["mul", 0], ["add", c]]}


def _digit(name, src, k):
    return [
        {"name": name + "_a", "kind": "calc", "op": "sum", "from": [src], "xf": [["div", 10 ** k]]},
        {"name": name + "_b", "kind": "calc", "op": "sum", "from": [src], "xf": [["div", 10 ** (k + 1)], ["mul", 10]]},
        {"name": name, "kind": "calc", "op": "diff", "from": [name + "_a", name + "_b"], "xf": [["add", 48]]},
    ]


def _text(name, src, before, digits, after):
    recs, parts = [], []
    for i, c in enumerate(before):
        recs.append(_const(f"{name}_p{i}", src, ord(c)))
        parts.append(f"{name}_p{i}")
    for k in range(digits - 1, -1, -1):
        recs += _digit(f"{name}_d{k}", src, k)
        parts.append(f"{name}_d{k}")
    for i, c in enumerate(after + "\0"):
        recs.append(_const(f"{name}_s{i}", src, ord(c)))
        parts.append(f"{name}_s{i}")
    recs.append({"name": name, "kind": "calc", "op": "concat", "from": parts})
    return recs


RECORDS = (_text("service.lives_text", "service.lives_label", "", 1, "")
           + _text("service.bonus_text", "bldg.bonus_step0", "EVERY ", 4, "")
           + _text("service.super_text", "service.super_secs_label", "", 1, " SEC")
           + _text("service.kill_text", "service.kill_level_label", "LEVEL ", 2, ""))
