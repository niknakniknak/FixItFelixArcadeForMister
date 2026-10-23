# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
FIXI = [["trunc"], ["mul", 65536]]

RECORDS = [
    {"name": "felix.START_X_fix", "kind": "data", "va": 0x416C08, "type": "f32", "xf": FIXI},
    {"name": "felix.WALK_IN_X_fix", "kind": "data", "va": 0x416C0C, "type": "f32", "xf": FIXI},
    {"name": "felix.MIN_X_fix", "kind": "data", "va": 0x416B58, "type": "f32", "xf": FIXI},
    {"name": "felix.MAX_X_fix", "kind": "data", "va": 0x416B48, "type": "f32", "xf": FIXI},
    {"name": "felix.HEAD_ROOM_fix", "kind": "data", "va": 0x416B60, "type": "f64", "xf": FIXI},
    {"name": "felix.hit_height_fix", "kind": "data", "va": 0x416AF8, "type": "f64", "xf": FIXI},
    {"name": "felix.LEFT_LIMIT_fix", "kind": "calc", "op": "sum", "from": ["felix.slot0_tx_base", "felix.slot0_tx_adj"], "xf": [["mul", 8], ["mul", 65536]]},
    {"name": "felix.RIGHT_LIMIT_fix", "kind": "calc", "op": "sum", "from": ["felix.slot4_left", "felix.win_width"], "xf": [["mul", 65536]]},
]
