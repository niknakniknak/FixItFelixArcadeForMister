#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cut texture2.bmp into Tobikomi board tiles, sprite cells, palettes and C tables."""
import functools
import json
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(1, str(Path(__file__).resolve().parent / "exedata"))
import common
import exefw
import gen
from common import BUILD, DATA, GEN
import gencode

TRANSPARENT = DRAWN_BLACK = CLOTHING = HAMMER = None
SUPER_ANIMS = (4, 9, 10, 11, 12, 13, 14, 16)
FONT_TILE_ENTRY = 22
FONT_COLOURS = 7
FONT_GLYPHS = 64
MAX_TILES = 2048
MAX_CELLS = 2048
PALETTES_PER_LAYER = 32
PENS = 16
CYCLE_VARIANTS = 4
HAMMER_COLOURS = 4

TILE_NAMES = [
    "BAND", "GREEN_BAR", "DOOR0", "DOOR1", "DOOR2", "DOOR3", "ARCH", "ARCH_BROKEN",
    "LOGO", "ROOF", "FLOOR_A", "FLOOR_B", "GROUND_A", "GROUND_B", "FRAME3", "FRAME4",
    "FRAME0", "LIVES_HEAD", "COPYRIGHT", "HIGH_SCORE", "ONE_UP", "PRESS_START_RED",
    "FONT", "PRESS_START", "GAME_OVER", "CREDITS", "BLACK_BAR",
]
BIG_RECTS = tuple(range(187, 199))
BIG_SIZES = (64, 32, 16)
PICTURE_RECTS = {
    202: "SCENERY", 209: "DUMP", 199: "BUBBLE_STUMP", 210: "BUBBLE_WRECK", 212: "BUBBLE_FIX",
    200: "DOOR_CLOSED", 201: "DOOR_OPEN", 181: "MEDAL0", 182: "MEDAL1", 183: "MEDAL2", 184: "MEDAL3",
}
SECTION_MAPS = ["GROUND_A", "GROUND_B", "FLOOR_A", "FLOOR_B"]
KIND_PICTURES = [["FRAME0"], ["DOOR0", "DOOR1", "DOOR2", "DOOR3"], ["ARCH", "ARCH_BROKEN"], ["FRAME3"],
                 ["FRAME4"]]
SLOT_TY = None
DOOR_SLOT = (0, 2)
GROUND_PICTURE = "GREEN_BAR"
GROUND_REPEAT = 16
GROUND_LEFT_END = 32
GROUND_RIGHT_START = 208
LATE_PICTURES = ("GROUND_BUSHES",)
BUILDING_COL = 1
WINDOW_VARIANTS = 4
KIND_SHIFT = None
CUT_DOORS = {200: "DOOR_CLOSED", 201: "DOOR_OPEN"}
CUT_DOOR_X = 96
OPAQUE_PICTURES = ("HIGH_SCORE", "LIVES_HEAD", "BLACK_BAR")
LIVES_X = LIVES_LEFT = LIVES_W = None
LIVES_H = 8
SCREEN_W = 240
SERVICE_BARS = [(31, 31, 31), (31, 31, 0), (0, 31, 31), (0, 31, 0), (31, 0, 31), (31, 0, 0), (0, 0, 31),
                (0, 0, 0)] + [(g, g, g) for g in (4, 8, 12, 16, 20, 24, 28)]
SERVICE_RAMPS = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1)]
GRID_TOP, GRID_LEFT, GRID_BOTTOM, GRID_RIGHT, GRID_DOT = 1, 2, 4, 8, 16


@functools.lru_cache(maxsize=None)
def exe_values():
    return exefw.extract(exefw.PeImage(common.need_exe().read_bytes()), gen.all_records())


def load_exe_layout():
    global TRANSPARENT, DRAWN_BLACK, CLOTHING, HAMMER, SLOT_TY, KIND_SHIFT, LIVES_X, LIVES_LEFT, LIVES_W
    v = exe_values()
    TRANSPARENT, DRAWN_BLACK = v["pal.transparent"], v["pal.drawn_black"]
    CLOTHING, HAMMER = tuple(v["pal.clothing"]), tuple(v["pal.hammer"])
    w = v["bldg.row_win_ty"]
    SLOT_TY = [[0, 0, v["bldg.door_slot_ty"], 0, 0], [w] * 5, [0] * 5, [w] * 5]
    KIND_SHIFT = [v["bldg.k0_left"], 0, 0, v["bldg.k3_left"], v["bldg.k4_left"]]
    col, xs = v["hud.lives_col_raw"], []
    for i in range(v["hud.MAX_LIVES_SHOWN"]):
        xs.append(col * 8 + (0 if i & 1 else v["hud.lives_half"]))
        col -= 2 if i & 1 else 1
    LIVES_X = tuple(xs)
    LIVES_LEFT = min(LIVES_X)
    LIVES_W = SCREEN_W - LIVES_LEFT


@functools.lru_cache(maxsize=None)
def slot_layout():
    _, tables = gen.load_specs()
    by_name = {t["name"]: t for t in tables}
    values = exe_values()
    kinds = gen.table_values(by_name["exe_kinds"], values)
    low = gen.table_values(by_name["exe_tx_low"], values)
    high = gen.table_values(by_name["exe_tx_high"], values)
    return [kinds[5 * t:5 * t + 5] for t in range(4)], [low, low, high, high]


def rgb555(rgb):
    return tuple((c * 31 + 127) // 255 for c in rgb)


def word555(colour):
    r, g, b = colour
    return (r << 10) | (g << 5) | b


class Sheet:
    def __init__(self, path):
        image = Image.open(path)
        if image.mode != "P":
            raise ValueError("texture2.bmp is not indexed")
        self.width, self.height = image.size
        self.pixels = image.tobytes()
        flat = image.getpalette()
        self.rgb = [tuple(flat[3 * i:3 * i + 3]) for i in range(256)]

    def index(self, x, y):
        return self.pixels[y * self.width + x]

    def first_position(self, index):
        offset = self.pixels.index(bytes([index]))
        return offset % self.width, offset // self.width

    def block(self, rect, dx, dy, size):
        rows = []
        for y in range(dy, dy + size):
            row = bytearray([TRANSPARENT]) * size
            if 0 <= y < rect["h"]:
                for x in range(max(dx, 0), min(dx + size, rect["w"])):
                    row[x - dx] = self.index(rect["x"] + x, rect["y"] + y)
            rows.append(bytes(row))
        return b"".join(rows)


def opaque(block):
    return block.replace(bytes([TRANSPARENT]), bytes([DRAWN_BLACK]))


def is_empty(block):
    return block.count(TRANSPARENT) == len(block)


class ColourKeys:
    def __init__(self, sheet):
        self.sheet = sheet

    def key(self, index, cycle):
        if index == TRANSPARENT:
            return None
        if index == DRAWN_BLACK:
            return (0, 0, 0)
        if cycle and index in (HAMMER[0], CLOTHING[0]):
            return ("c", CLOTHING[0])
        if cycle and index in CLOTHING[1:]:
            return ("c", index)
        return rgb555(self.sheet.rgb[index])

    def base_colour(self, key):
        if key[0] == "c":
            return rgb555(self.sheet.rgb[key[1]])
        return key

    def keys(self, block, cycle):
        return {self.key(i, cycle) for i in set(block)} - {None}


class PalettePool:
    def __init__(self):
        self.palettes = []

    def seed(self, keys):
        self.palettes.append(list(keys))
        return len(self.palettes) - 1

    def place(self, keys):
        best = None
        for number, palette in enumerate(self.palettes):
            union = len(keys | set(palette))
            if union > PENS - 1:
                continue
            score = (len(keys & set(palette)), -union)
            if best is None or score > best[0]:
                best = (score, number)
        if best is None:
            return self.seed(sorted(keys, key=str))
        palette = self.palettes[best[1]]
        palette.extend(sorted(keys - set(palette), key=str))
        return best[1]

    def pens(self, number, block, keyer, cycle):
        palette = self.palettes[number]
        out = bytearray(len(block))
        for i, index in enumerate(block):
            key = keyer.key(index, cycle)
            out[i] = 0 if key is None else palette.index(key) + 1
        return bytes(out)


class Graphic:
    def __init__(self, size, limit):
        self.size = size
        self.limit = limit
        self.codes = {bytes(size * size): 0}
        self.blocks = [bytes(size * size)]
        self.runs = {}

    def run(self, cells):
        key = tuple(cells)
        if key not in self.runs:
            self.runs[key] = len(self.blocks)
            for pens in cells:
                self.codes.setdefault(pens, len(self.blocks))
                self.blocks.append(pens)
        return self.runs[key]

    def code(self, pens):
        if pens not in self.codes:
            self.codes[pens] = len(self.blocks)
            self.blocks.append(pens)
        return self.codes[pens]

    def encode(self):
        out = bytearray()
        n = self.size
        for pens in self.blocks:
            for col in range(n):
                for y in range(0, n, 2):
                    out.append(pens[y * n + col] | pens[(y + 1) * n + col] << 4)
        if len(self.blocks) > self.limit:
            raise ValueError(f"{len(self.blocks)} blocks exceed {self.limit}")
        bytes_per_block = n * n // 2
        return bytes(out) + b"\0" * ((self.limit - len(self.blocks)) * bytes_per_block)


def grid_positions(sheet, rect, size):
    return [(x, y) for y in range(0, rect["h"], size) for x in range(0, rect["w"], size)
            if not is_empty(sheet.block(rect, x, y, size))]


def opaque_rows(sheet, rect, x, size):
    rows = []
    for y in range(rect["h"]):
        for xx in range(x, min(x + size, rect["w"])):
            if sheet.index(rect["x"] + xx, rect["y"] + y) != TRANSPARENT:
                rows.append(y)
                break
    return rows


def strip_positions(sheet, rect, size):
    columns = [x for x in range(rect["w"])
               if any(sheet.index(rect["x"] + x, rect["y"] + y) != TRANSPARENT
                      for y in range(rect["h"]))]
    positions = []
    if not columns:
        return positions
    for x in range(columns[0], columns[-1] + 1, size):
        rows = opaque_rows(sheet, rect, x, size)
        while rows:
            y = rows[0]
            positions.append((x, y))
            rows = [r for r in rows if r >= y + size]
    return positions


def cut_positions(sheet, rect, size):
    grid = grid_positions(sheet, rect, size)
    strips = strip_positions(sheet, rect, size)
    return strips if len(strips) < len(grid) else grid


class MaskedSheet:
    def __init__(self, sheet, keep):
        self.sheet = sheet
        self.keep = keep

    def index(self, x, y):
        i = self.sheet.index(x, y)
        return i if i in self.keep else TRANSPARENT

    def block(self, rect, dx, dy, size):
        return bytes(i if i in self.keep else TRANSPARENT
                     for i in self.sheet.block(rect, dx, dy, size))


def anim_rects(anims, numbers):
    rects = set()
    for n in numbers:
        for frame in anims[n]["frames"]:
            rects.add(frame["rect"])
            if frame["mirror"] >= 0:
                rects.add(frame["mirror"])
    return rects


class SpriteBuilder:
    def __init__(self, sheet, tables):
        self.sheet = sheet
        self.keyer = ColourKeys(sheet)
        self.rects = tables["rects"]
        anims = tables["anims"]
        self.used = sorted(anim_rects(anims, range(len(anims))) - set(PICTURE_RECTS) - set(BIG_RECTS))
        self.super_rects = anim_rects(anims, SUPER_ANIMS)
        self.cycle_pool = PalettePool()
        self.pool = PalettePool()
        self.hammer_seed = [self.pool.seed([rgb555(c)]) for c in hammer_colours(sheet)]

    def is_cycle(self, rect_id, block):
        return rect_id in self.super_rects and any(i in CLOTHING for i in block)

    def collect(self):
        self.parts = {}
        self.overlays = {}
        self.patterns = {}
        masked = MaskedSheet(self.sheet, HAMMER)
        for r in self.used:
            rect = self.rects[r]
            self.parts[r] = []
            for x, y in cut_positions(self.sheet, rect, 16):
                block = self.sheet.block(rect, x, y, 16)
                cycle = self.is_cycle(r, block)
                self.parts[r].append((x, y, block, cycle))
                self.patterns[(block, cycle)] = None
            self.overlays[r] = []
            if r in self.super_rects:
                for x, y in cut_positions(masked, rect, 16):
                    self.overlays[r].append((x, y, masked.block(rect, x, y, 16)))

    def pack(self):
        ordered = sorted(self.patterns, key=lambda p: -len(self.keyer.keys(*p)))
        for block, cycle in ordered:
            keys = self.keyer.keys(block, cycle)
            if len(keys) > PENS - 1:
                raise ValueError(f"sprite cell with {len(keys)} colours")
            pool = self.cycle_pool if cycle else self.pool
            self.patterns[(block, cycle)] = pool.place(keys)
        self.cycle_groups = len(self.cycle_pool.palettes)
        self.general_base = self.cycle_groups * (1 + CYCLE_VARIANTS)
        total = self.general_base + len(self.pool.palettes)
        if total > PALETTES_PER_LAYER:
            raise ValueError(f"{total} sprite palettes needed")

    def hardware_palette(self, block, cycle):
        number = self.patterns[(block, cycle)]
        if cycle:
            return number * (1 + CYCLE_VARIANTS)
        return self.general_base + number

    def build(self, cells):
        metasprites = []
        for r in range(len(self.rects)):
            overlay = []
            for x, y, block in self.overlays.get(r, []):
                pens = bytes(1 if i != TRANSPARENT else 0 for i in block)
                overlay.append((x, y, cells.code(pens), self.general_base + self.hammer_seed[0]))
            parts = []
            for x, y, block, cycle in self.parts.get(r, []):
                number = self.patterns[(block, cycle)]
                pool = self.cycle_pool if cycle else self.pool
                pens = pool.pens(number, block, self.keyer, cycle)
                parts.append((x, y, cells.code(pens), self.hardware_palette(block, cycle)))
            metasprites.append({"overlay": overlay, "parts": parts})
        return metasprites

    def palette_words(self, felix):
        words = []
        for palette in self.cycle_pool.palettes:
            words.append(self.palette_row(palette))
            for variant in range(CYCLE_VARIANTS):
                words.append(self.palette_row(palette, felix[variant]))
        for palette in self.pool.palettes:
            words.append(self.palette_row(palette))
        return words

    def palette_row(self, palette, felix=None):
        row = [0]
        for key in palette:
            colour = self.keyer.base_colour(key)
            if felix is not None and key[0] == "c":
                colour = rgb555(felix["yellow"] if key[1] == CLOTHING[0] else felix["clothes"])
            row.append(word555(colour))
        return row


class TileBuilder:
    def __init__(self, sheet, tables):
        self.sheet = sheet
        self.keyer = ColourKeys(sheet)
        self.tables = tables
        self.pool = PalettePool()
        self.tiles = Graphic(8, MAX_TILES)
        font = tables["tiles"][FONT_TILE_ENTRY]
        self.font_rows = [font["y"] + (FONT_COLOURS - 1 - c) * 8 for c in range(FONT_COLOURS)]
        self.font_x = font["x"]
        self.font_pal = [self.pool.seed([self.font_colour(c), (0, 0, 0)]) for c in range(FONT_COLOURS)]

    def font_colour(self, colour):
        y = self.font_rows[colour]
        indices = {self.sheet.index(x, y + dy) for x in range(self.font_x, self.font_x + 512)
                   for dy in range(8)} - {TRANSPARENT}
        if len(indices) != 1:
            raise ValueError(f"font colour {colour} uses {indices}")
        return self.keyer.key(indices.pop(), False)

    def build_font(self):
        self.font_tile = len(self.tiles.blocks)
        for colour in range(FONT_COLOURS):
            for glyph in range(FONT_GLYPHS):
                rect = {"x": self.font_x + glyph * 8, "y": self.font_rows[colour], "w": 8, "h": 8}
                block = self.sheet.block(rect, 0, 0, 8)
                pens = self.pool.pens(self.font_pal[colour], block, self.keyer, False)
                code = self.font_tile + glyph
                if colour == 0:
                    self.tiles.blocks.append(pens)
                    self.tiles.codes.setdefault(pens, code)
                elif self.tiles.blocks[code] != pens:
                    raise ValueError(f"font glyph {glyph} differs in colour {colour}")
        self.font_opaque_tile = len(self.tiles.blocks)
        for colour in range(FONT_COLOURS):
            for glyph in range(FONT_GLYPHS):
                rect = {"x": self.font_x + glyph * 8, "y": self.font_rows[colour], "w": 8, "h": 8}
                block = opaque(self.sheet.block(rect, 0, 0, 8))
                pens = self.pool.pens(self.font_pal[colour], block, self.keyer, False)
                code = self.font_opaque_tile + glyph
                if colour == 0:
                    self.tiles.blocks.append(pens)
                    self.tiles.codes.setdefault(pens, code)
                elif self.tiles.blocks[code] != pens:
                    raise ValueError(f"opaque font glyph {glyph} differs in colour {colour}")

    def pictures(self):
        rects = self.tables["rects"]
        pics = [(name, dict(t, opaque=True) if name in OPAQUE_PICTURES else t)
                for name, t in zip(TILE_NAMES, self.tables["tiles"])]
        door_wall = SECTION_MAPS[DOOR_SLOT[0] + 1]
        pics = [(name, self.section_picture(DOOR_SLOT[0] + 1) if name == door_wall else t) for name, t in pics]
        pics += [(name, self.cut_door(r) if r in CUT_DOORS else rects[r]) for r, name in sorted(PICTURE_RECTS.items())]
        windows, self.window_table = self.window_pictures()
        seen = set()
        for name, rect in windows:
            if name not in seen:
                seen.add(name)
                pics.append((name, rect))
        pics.append(("BUSHES", self.bushes()))
        pics.append(("GROUND_BUSHES", self.ground_bushes()))
        for n in range(1, len(LIVES_X) + 1):
            pics.append((f"LIVES_{n}", self.lives_strip(n)))
        return pics

    def lives_strip(self, n):
        head = dict(zip(TILE_NAMES, self.tables["tiles"]))["LIVES_HEAD"]
        pix = bytearray([TRANSPARENT]) * (LIVES_W * LIVES_H)
        for hx in LIVES_X[:n]:
            for y in range(LIVES_H):
                for x in range(head["w"]):
                    px = hx - LIVES_LEFT + x
                    i = self.sheet.index(head["x"] + x, head["y"] + y)
                    if px < LIVES_W and i != TRANSPARENT:
                        pix[y * LIVES_W + px] = i
        return {"w": LIVES_W, "h": LIVES_H, "pix": bytes(pix), "opaque": True}

    def blocks_of(self, rect):
        if "pix" in rect:
            blocks = [[b"".join(rect["pix"][(y + dy) * rect["w"] + x:(y + dy) * rect["w"] + x + 8]
                                for dy in range(8)) for x in range(0, rect["w"], 8)]
                      for y in range(0, rect["h"], 8)]
        else:
            blocks = [[self.sheet.block(rect, x, y, 8) for x in range(0, rect["w"], 8)]
                      for y in range(0, rect["h"], 8)]
        if rect.get("opaque"):
            blocks = [[opaque(b) for b in row] for row in blocks]
        return blocks

    def bushes(self):
        bush = dict(zip(TILE_NAMES, self.tables["tiles"]))[GROUND_PICTURE]
        w = bush["w"] * GROUND_REPEAT
        pix = bytes(self.sheet.index(bush["x"] + x % bush["w"], bush["y"] + y) for y in range(bush["h"]) for x in range(w))
        return {"w": w, "h": bush["h"], "pix": pix}

    def ground_bushes(self):
        tiles = dict(zip(TILE_NAMES, self.tables["tiles"]))
        bush, wall = tiles[GROUND_PICTURE], tiles[SECTION_MAPS[0]]
        x0, w, h = GROUND_LEFT_END - 8, GROUND_RIGHT_START - GROUND_LEFT_END + 16, bush["h"]
        pix = bytearray([TRANSPARENT]) * (w * h)
        for y in range(h):
            for x in list(range(8)) + list(range(w - 8, w)):
                sx = x0 + x
                i = self.wall_index(0, sx - BUILDING_COL * 8, wall["h"] - h + y)
                if i == TRANSPARENT:
                    i = self.sheet.index(bush["x"] + sx % bush["w"], bush["y"] + y)
                pix[y * w + x] = i
        return {"w": w, "h": h, "pix": bytes(pix)}

    def wall_index(self, row_type, x, y):
        tiles = dict(zip(TILE_NAMES, self.tables["tiles"]))
        wall = tiles[SECTION_MAPS[row_type]]
        if row_type == DOOR_SLOT[0] + 1:
            t, s = DOOR_SLOT
            kinds, txs = slot_layout()
            door = tiles[KIND_PICTURES[kinds[t][s]][0]]
            spill = -SLOT_TY[t][s] * 8
            px, py = x - (txs[t][s] - BUILDING_COL) * 8, y - (wall["h"] - spill)
            if 0 <= px < door["w"] and 0 <= py < spill:
                i = self.sheet.index(door["x"] + px, door["y"] + py)
                if i != TRANSPARENT:
                    return i
        return self.sheet.index(wall["x"] + x, wall["y"] + y)

    def section_picture(self, row_type):
        wall = dict(zip(TILE_NAMES, self.tables["tiles"]))[SECTION_MAPS[row_type]]
        w, h = wall["w"], wall["h"]
        pix = bytes(self.wall_index(row_type, x, y) for y in range(h) for x in range(w))
        return {"w": w, "h": h, "pix": pix}

    def slot_picture(self, row_type, pic, tx, ty, kind, w):
        wall = dict(zip(TILE_NAMES, self.tables["tiles"]))[SECTION_MAPS[row_type]]
        dy = ty * 8
        h = min(dy + pic["h"], wall["h"])
        ox = (tx - BUILDING_COL) * 8
        dx = KIND_SHIFT[kind]
        pix = bytearray(w * h)
        for y in range(h):
            for x in range(w):
                i = TRANSPARENT
                if 0 <= x - dx < pic["w"] and 0 <= y - dy < pic["h"]:
                    i = self.sheet.index(pic["x"] + x - dx, pic["y"] + y - dy)
                if i == TRANSPARENT:
                    i = self.wall_index(row_type, ox + x, y)
                pix[y * w + x] = i
        return h, ox, dx, dy, pix

    def window_pictures(self):
        tiles = dict(zip(TILE_NAMES, self.tables["tiles"]))
        pics, table = [], []
        for row_type, (kind_row, tx_row, ty_row, section) in enumerate(zip(*slot_layout(), SLOT_TY, SECTION_MAPS)):
            slots = []
            for kind, tx, ty in zip(kind_row, tx_row, ty_row):
                variants = []
                for v, name in enumerate(KIND_PICTURES[kind]):
                    pic = tiles[name]
                    w = pic["w"] + 2 * KIND_SHIFT[kind]
                    h, ox, dx, dy, pix = self.slot_picture(row_type, pic, tx, ty, kind, w)
                    label = f"WIN_{section}_{tx}_{name}"
                    pics.append((label, {"w": w, "h": h, "pix": bytes(pix), "wall": section, "pic": name,
                                         "ox": ox, "dx": dx, "dy": dy}))
                    variants.append(label)
                slots.append(variants + [variants[-1]] * (WINDOW_VARIANTS - len(variants)))
            table.append(slots)
        return pics, table

    def building_index(self, row_type, bx, by):
        tiles = dict(zip(TILE_NAMES, self.tables["tiles"]))
        kinds, txs = slot_layout()
        for kind, tx, ty in zip(kinds[row_type], txs[row_type], SLOT_TY[row_type]):
            pic = tiles[KIND_PICTURES[kind][0]]
            w = pic["w"] + 2 * KIND_SHIFT[kind]
            ox = (tx - BUILDING_COL) * 8
            if ox <= bx < ox + w:
                h, ox, dx, dy, pix = self.slot_picture(row_type, pic, tx, ty, kind, w)
                if by < h:
                    return pix[by * w + bx - ox]
        return self.wall_index(row_type, bx, by)

    def cut_door(self, r):
        rect = self.tables["rects"][r]
        w, h = rect["w"], rect["h"]
        bx = CUT_DOOR_X - BUILDING_COL * 8
        pix = bytearray(w * h)
        for y in range(h):
            for x in range(w):
                i = self.sheet.index(rect["x"] + x, rect["y"] + y)
                if i == TRANSPARENT:
                    i = self.building_index(1, bx + x, 48 + y) if y < 8 else self.building_index(0, bx + x, y - 8)
                pix[y * w + x] = i
        return {"w": w, "h": h, "pix": bytes(pix), "door": r, "bx": bx}

    def build_maps(self):
        pictures = [(n, r) for n, r in self.pictures() if n != "FONT"]
        patterns = {}
        for late in (False, True):
            batch = {}
            for name, rect in pictures:
                if (name in LATE_PICTURES) != late:
                    continue
                for row in self.blocks_of(rect):
                    for block in row:
                        if not is_empty(block) and block not in patterns:
                            batch[block] = None
            for block in sorted(batch, key=lambda b: -len(self.keyer.keys(b, False))):
                keys = self.keyer.keys(block, False)
                if len(keys) > PENS - 1:
                    raise ValueError(f"tile with {len(keys)} colours")
                patterns[block] = self.pool.place(keys)
        if len(self.pool.palettes) > PALETTES_PER_LAYER:
            raise ValueError(f"{len(self.pool.palettes)} tile palettes needed")
        maps = []
        for name, rect in self.pictures():
            if name == "FONT":
                maps.append({"name": name, "w": rect["w"] // 8, "h": rect["h"] // 8, "map": None})
                continue
            entries = []
            for row in self.blocks_of(rect):
                for block in row:
                    entries.append(self.entry(block, patterns))
            m = {"name": name, "w": rect["w"] // 8, "h": rect["h"] // 8, "map": entries}
            if "pic" in rect:
                m["window"] = {k: rect[k] for k in ("wall", "pic", "ox", "dx", "dy")}
            if "door" in rect:
                m["door"] = {k: rect[k] for k in ("door", "bx")}
            maps.append(m)
        return maps

    def entry(self, block, patterns):
        if is_empty(block):
            return 0
        number = patterns[block]
        pens = self.pool.pens(number, block, self.keyer, False)
        return self.tiles.code(pens) | number << 11

    def build_service(self):
        self.service_pal = len(self.pool.palettes)
        self.service_rows = [[0] + [word555(c) for c in SERVICE_BARS]]
        for r, g, b in SERVICE_RAMPS:
            levels = [(p * 31 + 7) // 15 for p in range(PENS)]
            self.service_rows.append([word555((r * v, g * v, b * v)) for v in levels])
        self.service_solid = len(self.tiles.blocks)
        for pen in range(1, PENS):
            self.tiles.blocks.append(bytes([pen] * 64))
        self.service_grid = len(self.tiles.blocks)
        for flags in range(32):
            pix = bytearray(64)
            for i in range(8):
                if flags & GRID_TOP:
                    pix[i] = 1
                if flags & GRID_BOTTOM:
                    pix[56 + i] = 1
                if flags & GRID_LEFT:
                    pix[i * 8] = 1
                if flags & GRID_RIGHT:
                    pix[i * 8 + 7] = 1
            if flags & GRID_DOT:
                pix[0] = 1
            self.tiles.blocks.append(bytes(pix))

    def palette_words(self):
        rows = []
        for palette in self.pool.palettes:
            rows.append([0] + [word555(self.keyer.base_colour(k)) for k in palette])
        rows += self.service_rows
        if len(rows) > PALETTES_PER_LAYER:
            raise ValueError(f"{len(rows)} tile palettes needed")
        return rows


def size_code(n):
    return {16: 0, 32: 1, 64: 2}[n]


if list(BIG_RECTS) != list(range(BIG_RECTS[0], BIG_RECTS[0] + len(BIG_RECTS))):
    raise ValueError("BIG_RECTS must be consecutive")


class BigSpriteBuilder:
    def __init__(self, sheet, tables, sprites):
        self.sheet = sheet
        self.rects = tables["rects"]
        self.sprites = sprites
        self.keyer = sprites.keyer

    def opaque(self, rect, x, y, w, h):
        for yy in range(max(y, 0), min(y + h, rect["h"])):
            for xx in range(max(x, 0), min(x + w, rect["w"])):
                if self.sheet.index(rect["x"] + xx, rect["y"] + yy) != TRANSPARENT:
                    return True
        return False

    def strips(self, rect):
        columns = [x for x in range(rect["w"]) if self.opaque(rect, x, 0, 1, rect["h"])]
        x, last, out = columns[0], columns[-1], []
        while x <= last:
            left = last + 1 - x
            w = next(n for n in BIG_SIZES if n < left + 16 or n == 16)
            while w > 16 and not self.opaque(rect, x + w // 2, 0, w // 2, rect["h"]):
                w //= 2
            out.append((x, w))
            x += w
            while x <= last and not self.opaque(rect, x, 0, 1, rect["h"]):
                x += 1
        return out

    def runs(self, rect, x, w):
        rows = [y for y in range(rect["h"]) if self.opaque(rect, x, y, w, 1)]
        out = []
        while rows:
            y = rows[0]
            left = rows[-1] + 1 - y
            h = next(n for n in BIG_SIZES if n < left + 16 or n == 16)
            while h > 16 and not self.opaque(rect, x, y + h // 2, w, h // 2):
                h //= 2
            out.append((y, h))
            rows = [r for r in rows if r >= y + h]
        return out

    def cells(self, rect, x, y, w, h):
        return [self.sheet.block(rect, x + cx, y + cy, 16) for cx in range(0, w, 16) for cy in range(0, h, 16)]

    def fits(self, rect, x, y, w, h):
        blocks = self.cells(rect, x, y, w, h)
        keys = set()
        for b in blocks:
            keys |= self.keyer.keys(b, False)
        empty = sum(1 for b in blocks if is_empty(b))
        return len(keys) <= PENS - 1 and empty * 2 < len(blocks), keys

    def split(self, rect, x, y, w, h):
        if not self.opaque(rect, x, y, w, h):
            return []
        ok, keys = self.fits(rect, x, y, w, h)
        if ok or (w == 16 and h == 16):
            return [(x, y, w, h, keys)]
        if w >= h and w > 16:
            return self.split(rect, x, y, w // 2, h) + self.split(rect, x + w // 2, y, w // 2, h)
        return self.split(rect, x, y, w, h // 2) + self.split(rect, x, y + h // 2, w, h // 2)

    def blocks(self, rect):
        out = []
        for x, w in self.strips(rect):
            for y, h in self.runs(rect, x, w):
                out += self.split(rect, x, y, w, h)
        return out

    def build(self, cells):
        pool = self.sprites.pool
        self.pictures = {}
        for r in BIG_RECTS:
            rect = self.rects[r]
            parts = []
            for x, y, w, h, keys in self.blocks(rect):
                number = pool.place(keys)
                pens = [pool.pens(number, b, self.keyer, False) for b in self.cells(rect, x, y, w, h)]
                code = cells.run(pens)
                pal = self.sprites.general_base + number
                size = size_code(w) | size_code(h) << 2 | ((code >> 10) & 1) << 5
                parts.append((x, y, code, pal, size))
            self.pictures[r] = parts
        total = self.sprites.general_base + len(pool.palettes)
        if total > PALETTES_PER_LAYER:
            raise ValueError(f"{total} sprite palettes needed")
        return self.pictures

    def column_cost(self, r):
        cost = [0] * self.rects[r]["w"]
        for x, y, code, pal, size in self.pictures[r]:
            w = 16 << (size & 3)
            for c in range(x, min(x + w, len(cost))):
                cost[c] += 1 << ((size >> 2) & 3)
        return max(cost)


def colour_at(path, position):
    image = Image.open(path).convert("RGBA")
    return image.getpixel(position)


def hammer_colours(sheet):
    position = sheet.first_position(HAMMER[0])
    return [colour_at(DATA / f"hammer{n}.png", position)[:3] for n in range(HAMMER_COLOURS)]


def felix_variants(sheet):
    yellow_at = sheet.first_position(CLOTHING[0])
    clothes_at = sheet.first_position(CLOTHING[1])
    variants = []
    for n in range(1, CYCLE_VARIANTS + 1):
        path = DATA / f"felix{n}.png"
        variants.append({"yellow": colour_at(path, yellow_at)[:3],
                         "clothes": colour_at(path, clothes_at)[:3]})
        for index in CLOTHING[2:]:
            if colour_at(path, sheet.first_position(index))[:3] != variants[-1]["clothes"]:
                raise ValueError(f"felix{n}.png index {index} differs from {CLOTHING[1]}")
    return variants


def check_base_palette(sheet):
    image = Image.open(DATA / "felix0.png").convert("RGBA")
    for index in set(sheet.pixels) - {TRANSPARENT, DRAWN_BLACK}:
        if image.getpixel(sheet.first_position(index))[:3] != sheet.rgb[index]:
            raise ValueError(f"felix0.png disagrees with texture2.bmp at index {index}")


def super_cycle():
    felix = hammer = 0
    steps = []
    for _ in range(16):
        felix += 3
        hammer += 3
        if hammer % 5 == 0:
            hammer += 1
        steps.append((felix % 5, hammer % 5))
    return steps[1:16]


def palette_rom(tile_rows, sprite_rows):
    words = [0] * (2 * PALETTES_PER_LAYER * PENS)
    for layer, rows in enumerate((tile_rows, sprite_rows)):
        for number, row in enumerate(rows):
            for pen, word in enumerate(row):
                words[layer * 512 + number * PENS + pen] = word
    return bytes(w & 0xFF for w in words) + bytes(w >> 8 for w in words)


def main():
    common.need_exe()
    load_exe_layout()
    tables = json.loads((BUILD / "tables.json").read_text())
    sheet = Sheet(DATA / "texture2.bmp")
    check_base_palette(sheet)

    tile_builder = TileBuilder(sheet, tables)
    tile_builder.build_font()
    tilemaps = tile_builder.build_maps()
    tile_builder.build_service()

    sprites = SpriteBuilder(sheet, tables)
    sprites.collect()
    sprites.pack()
    cells = Graphic(16, MAX_CELLS)
    metasprites = sprites.build(cells)
    big = BigSpriteBuilder(sheet, tables, sprites)
    bigsprites = big.build(cells)

    tile_rows = tile_builder.palette_words()
    sprite_rows = sprites.palette_words(felix_variants(sheet))
    layout = {
        "font_tile": tile_builder.font_tile,
        "font_pal": tile_builder.font_pal[0],
        "font_opaque_tile": tile_builder.font_opaque_tile,
        "service_pal": tile_builder.service_pal,
        "service_solid": tile_builder.service_solid,
        "service_grid": tile_builder.service_grid,
        "cycle_groups": sprites.cycle_groups,
        "cycle_variants": CYCLE_VARIANTS,
        "hammer_pal": sprites.general_base + sprites.hammer_seed[0],
        "super_cycle": super_cycle(),
        "tile_count": len(tile_builder.tiles.blocks),
        "cell_count": len(cells.blocks),
        "tile_palettes": len(tile_rows),
        "sprite_palettes": len(sprite_rows),
    }

    (BUILD / "felix_tiles.bin").write_bytes(tile_builder.tiles.encode())
    (BUILD / "felix_sprites.bin").write_bytes(cells.encode())
    (BUILD / "felix_pal.bin").write_bytes(palette_rom(tile_rows, sprite_rows))
    gfx = {"layout": layout, "metasprites": metasprites, "tilemaps": tilemaps,
           "bigsprites": {str(r): parts for r, parts in bigsprites.items()},
           "big_first": BIG_RECTS[0],
           "picture_rects": {str(k): v for k, v in PICTURE_RECTS.items()},
           "super_rects": sorted(sprites.super_rects), "window_table": tile_builder.window_table}
    (BUILD / "gfx.json").write_text(json.dumps(gfx))
    GEN.mkdir(parents=True, exist_ok=True)
    gencode.write(GEN, tables, gfx)

    parts = sum(len(m["parts"]) + len(m["overlay"]) for m in metasprites)
    print(f"gfxconv: {layout['tile_count']} tiles, {layout['cell_count']} sprite cells "
          f"({parts} metasprite parts), {layout['tile_palettes']} tile palettes, "
          f"{layout['sprite_palettes']} sprite palettes "
          f"({sprites.cycle_groups} Felix cycle groups x {1 + CYCLE_VARIANTS})")
    for r in BIG_RECTS:
        print(f"gfxconv: big sprite rect {r}: {len(bigsprites[r])} sprites, "
              f"{sum((1 << (s & 3)) * (1 << ((s >> 2) & 3)) for *_, s in bigsprites[r])} cells, "
              f"column cost {big.column_cost(r)}")


if __name__ == "__main__":
    sys.exit(main())
