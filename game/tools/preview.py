#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Decode the graphics ROMs as the hardware does, check every picture against the sheet, draw previews."""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import BUILD, DATA
import gfxconv
from gfxconv import (BUILDING_COL, DOOR_SLOT, GROUND_PICTURE, KIND_PICTURES, LIVES_H, OPAQUE_PICTURES, SECTION_MAPS,
                     TILE_NAMES, slot_layout)

BACKDROP = (40, 40, 56, 255)
SCALE = 2


class Roms:
    def __init__(self):
        self.tiles = (BUILD / "felix_tiles.bin").read_bytes()
        self.cells = (BUILD / "felix_sprites.bin").read_bytes()
        prom = (BUILD / "felix_pal.bin").read_bytes()
        self.colours = [self.expand(prom[i] | prom[0x400 + i] << 8) for i in range(1024)]

    @staticmethod
    def expand(word):
        channels = ((word >> 10) & 31, (word >> 5) & 31, word & 31)
        return tuple(c << 3 | c >> 2 for c in channels) + (255,)

    def tile_pen(self, code, x, y):
        byte = self.tiles[code * 32 + x * 4 + y // 2]
        return byte >> 4 if y & 1 else byte & 15

    def cell_pen(self, code, x, y):
        byte = self.cells[code * 128 + x * 8 + y // 2]
        return byte >> 4 if y & 1 else byte & 15


def pixels(image):
    data = image.tobytes()
    return [tuple(data[i:i + 4]) for i in range(0, len(data), 4)]


def quantised(rgba):
    if rgba[3] == 0:
        return (0, 0, 0, 0)
    return Roms.expand(sum(((c * 31 + 127) // 255) << s for c, s in zip(rgba[:3], (10, 5, 0))))


class Original:
    def __init__(self):
        sheet = Image.open(DATA / "texture2.bmp")
        flat = sheet.getpalette()
        lut = [tuple(flat[3 * i:3 * i + 3]) + (255,) for i in range(256)]
        lut[gfxconv.TRANSPARENT] = (0, 0, 0, 0)
        lut[gfxconv.DRAWN_BLACK] = (0, 0, 0, 255)
        self.normal = Image.new("RGBA", sheet.size)
        self.normal.putdata([lut[i] for i in sheet.tobytes()])
        self.felix = [Image.open(DATA / f"felix{n}.png").convert("RGBA") for n in range(1, 5)]
        self.hammer = [Image.open(DATA / f"hammer{n}.png").convert("RGBA") for n in range(4)]

    def crop(self, rect, felix=0, hammer=0):
        box = (rect["x"], rect["y"], rect["x"] + rect["w"], rect["y"] + rect["h"])
        image = (self.felix[felix - 1] if felix else self.normal).crop(box)
        if hammer:
            image.alpha_composite(self.hammer[hammer - 1].crop(box))
        image.putdata([quantised(p) for p in pixels(image)])
        return image


def draw_parts(roms, image, parts, palette_add):
    for dx, dy, code, pal in reversed(parts):
        for x in range(16):
            for y in range(16):
                pen = roms.cell_pen(code, x, y)
                px, py = dx + x, dy + y
                if pen and px < image.width and py < image.height:
                    image.putpixel((px, py), roms.colours[512 + (pal + palette_add) * 16 + pen])


def render_metasprite(roms, gfx, rect_id, rect, felix=0, hammer=0):
    meta = gfx["metasprites"][rect_id]
    cycle_end = gfx["layout"]["cycle_groups"] * (1 + gfx["layout"]["cycle_variants"])
    image = Image.new("RGBA", (rect["w"], rect["h"]), (0, 0, 0, 0))
    base = [(dx, dy, code, pal + (felix if pal < cycle_end else 0))
            for dx, dy, code, pal in meta["parts"]]
    draw_parts(roms, image, base, 0)
    if hammer:
        draw_parts(roms, image, meta["overlay"], hammer - 1)
    return image


def render_bigsprite(roms, parts, rect):
    image = Image.new("RGBA", (rect["w"], rect["h"]), (0, 0, 0, 0))
    for dx, dy, code, pal, size in reversed(parts):
        cw, ch = 1 << (size & 3), 1 << ((size >> 2) & 3)
        for cx in range(cw):
            for cy in range(ch):
                cell = (code + cx * ch + cy) & 2047
                for x in range(16):
                    for y in range(16):
                        pen = roms.cell_pen(cell, x, y)
                        px, py = dx + cx * 16 + x, dy + cy * 16 + y
                        if pen and px < image.width and py < image.height:
                            image.putpixel((px, py), roms.colours[512 + pal * 16 + pen])
    return image


def render_tilemap(roms, tilemap, opaque=False):
    image = Image.new("RGBA", (tilemap["w"] * 8, tilemap["h"] * 8), (0, 0, 0, 0))
    for i, entry in enumerate(tilemap["map"]):
        code, pal = entry & 0x7FF, entry >> 11
        tx, ty = i % tilemap["w"] * 8, i // tilemap["w"] * 8
        for x in range(8):
            for y in range(8):
                pen = roms.tile_pen(code, x, y)
                if pen or opaque:
                    image.putpixel((tx + x, ty + y), roms.colours[pal * 16 + pen])
    return image


def font_map(gfx, colour, tiles_wide, base=None):
    layout = gfx["layout"]
    base = layout["font_tile"] if base is None else base
    entries = [(base + g) | (layout["font_pal"] + colour) << 11 for g in range(64)]
    return {"w": tiles_wide, "h": 1, "map": entries}


def mismatches(a, b):
    return sum(1 for p, q in zip(pixels(a), pixels(b)) if p != q)


def verify(roms, gfx, tables, original):
    failures = []
    pictures = {int(k) for k in gfx["picture_rects"]} | {int(k) for k in gfx["bigsprites"]}
    used = {f[key] for a in tables["anims"] for f in a["frames"] for key in ("rect", "mirror")}
    for r, rect in enumerate(tables["rects"]):
        if r not in used or r in pictures:
            continue
        combos = [(0, 0)]
        if r in gfx["super_rects"]:
            combos += [(f, h) for f in range(5) for h in range(1, 5)]
        for f, h in combos:
            bad = mismatches(render_metasprite(roms, gfx, r, rect, f, h), original.crop(rect, f, h))
            if bad:
                failures.append(f"rect {r} felix{f} hammer{h}: {bad} pixels differ")
    for r, parts in gfx["bigsprites"].items():
        bad = mismatches(render_bigsprite(roms, parts, tables["rects"][int(r)]), original.crop(tables["rects"][int(r)]))
        if bad:
            failures.append(f"big sprite rect {r}: {bad} pixels differ")
    sources = tables["tiles"] + [tables["rects"][int(k)] for k in sorted(gfx["picture_rects"], key=int)]
    for tilemap, rect in zip(gfx["tilemaps"], sources):
        if tilemap["map"] is None:
            for colour in range(7):
                row = dict(rect, y=rect["y"] + (6 - colour) * 8, h=8)
                bad = mismatches(render_tilemap(roms, font_map(gfx, colour, 64)), original.crop(row))
                if bad:
                    failures.append(f"font colour {colour}: {bad} pixels differ")
            continue
        target = original.crop(rect)
        if "door" in tilemap:
            target = door_original(original, tables, tilemap)
        if tilemap["name"] in SECTION_MAPS:
            target = wall_original(original, dict(zip(TILE_NAMES, tables["tiles"])),
                                   SECTION_MAPS.index(tilemap["name"]))
        if tilemap["name"] in OPAQUE_PICTURES:
            target = on_black(target)
        bad = mismatches(render_tilemap(roms, tilemap), target)
        if bad:
            failures.append(f"tilemap {tilemap['name']}: {bad} pixels differ")
    font = tables["tiles"][22]
    for colour in range(7):
        row = dict(font, y=font["y"] + (6 - colour) * 8, h=8)
        opaque_map = font_map(gfx, colour, 64, gfx["layout"]["font_opaque_tile"])
        bad = mismatches(render_tilemap(roms, opaque_map), on_black(original.crop(row)))
        if bad:
            failures.append(f"opaque font colour {colour}: {bad} pixels differ")
    tiles = dict(zip(TILE_NAMES, tables["tiles"]))
    windows = [m for m in gfx["tilemaps"] if "window" in m]
    for tilemap in windows:
        bad = mismatches(render_tilemap(roms, tilemap), window_original(original, tiles, tilemap))
        if bad:
            failures.append(f"tilemap {tilemap['name']}: {bad} pixels differ")
    if not windows:
        failures.append("no composed window tilemaps")
    bushes = [m for m in gfx["tilemaps"] if m["name"] == "BUSHES"]
    if bushes:
        bush = original.crop(dict(zip(TILE_NAMES, tables["tiles"]))[GROUND_PICTURE])
        target = Image.new("RGBA", (bushes[0]["w"] * 8, bushes[0]["h"] * 8))
        for x in range(0, target.width, bush.width):
            target.paste(bush, (x, 0))
        bad = mismatches(render_tilemap(roms, bushes[0]), target)
        if bad:
            failures.append(f"tilemap BUSHES: {bad} pixels differ")
    else:
        failures.append("no ground bushes tilemap")
    head = tiles["LIVES_HEAD"]
    for n in range(1, len(gfxconv.LIVES_X) + 1):
        tilemap = next(m for m in gfx["tilemaps"] if m["name"] == f"LIVES_{n}")
        target = Image.new("RGBA", (gfxconv.LIVES_W, LIVES_H), (0, 0, 0, 255))
        for hx in gfxconv.LIVES_X[:n]:
            target.alpha_composite(original.crop(dict(head, h=LIVES_H)), (hx - gfxconv.LIVES_LEFT, 0))
        bad = mismatches(render_tilemap(roms, tilemap), target)
        if bad:
            failures.append(f"tilemap {tilemap['name']}: {bad} pixels differ")
    return failures


def on_black(image):
    out = Image.new("RGBA", image.size, (0, 0, 0, 255))
    out.alpha_composite(image)
    return out


def wall_original(original, tiles, row_type):
    image = original.crop(tiles[SECTION_MAPS[row_type]])
    t, s = DOOR_SLOT
    if row_type == t + 1:
        kinds, txs = slot_layout()
        door = tiles[KIND_PICTURES[kinds[t][s]][0]]
        spill = -gfxconv.SLOT_TY[t][s] * 8
        image.alpha_composite(original.crop(dict(door, h=spill)),
                              ((txs[t][s] - BUILDING_COL) * 8, image.height - spill))
    return image


def place_picture(image, original, pic, x, dy):
    top = max(-dy, 0)
    h = min(pic["h"] - top, image.height - max(dy, 0))
    if h > 0:
        image.alpha_composite(original.crop(dict(pic, y=pic["y"] + top, h=h)), (x, max(dy, 0)))


def window_original(original, tiles, tilemap):
    info = tilemap["window"]
    pic = tiles[info["pic"]]
    w, h = tilemap["w"] * 8, tilemap["h"] * 8
    wall = wall_original(original, tiles, SECTION_MAPS.index(info["wall"]))
    image = wall.crop((info["ox"], 0, info["ox"] + w, h))
    place_picture(image, original, pic, info["dx"], info["dy"])
    image.putdata([quantised(p) for p in pixels(image)])
    return image


def building_section(original, tiles, row_type):
    image = wall_original(original, tiles, row_type)
    kinds, txs = slot_layout()
    for kind, tx, ty in zip(kinds[row_type], txs[row_type], gfxconv.SLOT_TY[row_type]):
        place_picture(image, original, tiles[KIND_PICTURES[kind][0]], (tx - BUILDING_COL) * 8 + gfxconv.KIND_SHIFT[kind],
                      ty * 8)
    return image


def door_original(original, tables, tilemap):
    tiles = dict(zip(TILE_NAMES, tables["tiles"]))
    rect = tables["rects"][tilemap["door"]["door"]]
    bx = tilemap["door"]["bx"]
    image = Image.new("RGBA", (rect["w"], rect["h"]))
    image.paste(building_section(original, tiles, 1).crop((bx, 48, bx + rect["w"], 56)), (0, 0))
    image.paste(building_section(original, tiles, 0).crop((bx, 0, bx + rect["w"], rect["h"] - 8)), (0, 8))
    image.alpha_composite(original.crop(rect))
    image.putdata([quantised(p) for p in pixels(image)])
    return image


def pair(label, left, right):
    width = left.width + right.width + 12
    image = Image.new("RGBA", (width, max(left.height, right.height) + 12), BACKDROP)
    image.alpha_composite(left, (0, 12))
    image.alpha_composite(right, (left.width + 12, 12))
    ImageDraw.Draw(image).text((0, 0), label, fill=(255, 255, 255, 255))
    return image


def sheet_of(items, columns_width=900):
    rows, row, x = [], [], 0
    for item in items:
        if row and x + item.width > columns_width:
            rows.append(row)
            row, x = [], 0
        row.append(item)
        x += item.width + 8
    rows.append(row)
    height = sum(max(i.height for i in r) + 8 for r in rows)
    out = Image.new("RGBA", (columns_width, height), BACKDROP)
    y = 0
    for r in rows:
        x = 0
        for item in r:
            out.alpha_composite(item, (x, y))
            x += item.width + 8
        y += max(i.height for i in r) + 8
    return out.resize((out.width * SCALE, out.height * SCALE), Image.NEAREST)


def sprite_preview(roms, gfx, tables, original):
    rects = tables["rects"]
    items = []
    for r in (33, 63, 12, 16, 75, 82, 92, 141, 130, 156, 6, 185, 215, 163, 214):
        items.append(pair(f"r{r}", original.crop(rects[r]),
                          render_metasprite(roms, gfx, r, rects[r])))
    for r, parts in sorted(gfx["bigsprites"].items(), key=lambda p: int(p[0])):
        items.append(pair(f"big r{r}", original.crop(rects[int(r)]),
                          render_bigsprite(roms, parts, rects[int(r)])))
    for f, h in gfx["layout"]["super_cycle"][:6]:
        items.append(pair(f"r33 f{f} h{h}", original.crop(rects[33], f, h),
                          render_metasprite(roms, gfx, 33, rects[33], f, h)))
    return sheet_of(items)


def tile_preview(roms, gfx, tables, original):
    sources = tables["tiles"] + [tables["rects"][int(k)] for k in sorted(gfx["picture_rects"], key=int)]
    wanted = {"BAND", "DOOR1", "ARCH", "LOGO", "ROOF", "FLOOR_A", "GROUND_A", "FRAME0", "COPYRIGHT",
              "SCENERY", "BUBBLE_FIX", "MEDAL3"}
    items = []
    for tilemap, rect in zip(gfx["tilemaps"], sources):
        if tilemap["name"] in wanted:
            items.append(pair(tilemap["name"], original.crop(rect), render_tilemap(roms, tilemap, True)))
    font = tables["tiles"][22]
    for colour in (0, 3, 6):
        row = dict(font, y=font["y"] + (6 - colour) * 8, h=8)
        items.append(pair(f"font {colour}", original.crop(row),
                          render_tilemap(roms, font_map(gfx, colour, 64))))
    return sheet_of(items, 1100)


def main():
    gfxconv.load_exe_layout()
    tables = json.loads((BUILD / "tables.json").read_text())
    gfx = json.loads((BUILD / "gfx.json").read_text())
    roms = Roms()
    original = Original()
    failures = verify(roms, gfx, tables, original)
    sprite_preview(roms, gfx, tables, original).save(BUILD / "preview_sprites.png")
    tile_preview(roms, gfx, tables, original).save(BUILD / "preview_tiles.png")
    for failure in failures:
        print("MISMATCH", failure)
    print(f"preview: {'all pictures match' if not failures else f'{len(failures)} mismatches'}; "
          f"wrote build/preview_sprites.png, build/preview_tiles.png")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
