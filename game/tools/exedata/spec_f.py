# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
SCALARS = {
    "bg_band_top": ("bg.BAND_TOP", None),
    "bg_band_rows": ("bg.BAND_ROWS", None),
    "bg_building_col": ("bg.BUILDING_COL", None),
    "bg_building_cols": ("bg.BUILDING_COLS", None),
    "bg_hud_height": ("bg.HUD_HEIGHT", None),
}
TABLES = [
    {"name": "exe_bg_band_cols", "section": "fixed", "fields": ["s8"], "dims": [2],
     "parts": [{"rec": "bg.band_cols"}], "file": "bg.c", "was": "band_cols"},
    {"name": "exe_bg_section_tm", "section": "fixed", "fields": ["u8"], "dims": [5],
     "parts": [{"rec": "bg.section_tm"}], "file": "bg.c", "was": "section_tm"},
    {"name": "exe_service_lives_text", "section": "BANK6", "type": "char", "fields": ["u8"], "dims": [2],
     "parts": [{"rec": "service.lives_text"}], "file": "service.c"},
    {"name": "exe_service_bonus_text", "section": "BANK6", "type": "char", "fields": ["u8"], "dims": [11],
     "parts": [{"rec": "service.bonus_text"}], "file": "service.c"},
    {"name": "exe_service_super_text", "section": "BANK6", "type": "char", "fields": ["u8"], "dims": [6],
     "parts": [{"rec": "service.super_text"}], "file": "service.c"},
    {"name": "exe_service_kill_text", "section": "BANK6", "type": "char", "fields": ["u8"], "dims": [9],
     "parts": [{"rec": "service.kill_text"}], "file": "service.c"},
]
