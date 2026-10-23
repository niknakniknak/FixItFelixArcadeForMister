# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
SCALARS = {
    "hiscore_init": ("hiscore.hiscore_init", None),
    "entry_letter": ("hiscore.entry_letter", None),
    "title_idle": ("attract.TITLE_IDLE", None),
}
TABLES = [
    {"name": "exe_default_names", "section": "BANK2", "type": "char", "fields": ["u8"], "dims": [5, 4],
     "parts": [{"rec": "hiscore.default_names", "str": 4}], "file": "hiscore.c", "was": "default_names"},
    {"name": "exe_default_scores", "section": "BANK2", "fields": ["u16"], "dims": [5],
     "parts": [{"rec": "hiscore.default_scores"}], "file": "hiscore.c", "was": "default_scores"},
]
