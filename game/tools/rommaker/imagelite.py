# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Just enough of PIL.Image for gfxconv: 8-bit indexed BMP and 8-bit RGBA PNG, standard library only."""
import struct
import sys
import types
import zlib
from pathlib import Path


class Bmp:
    def __init__(self, data):
        if data[:2] != b"BM":
            raise ValueError("not a BMP file")
        offset = struct.unpack_from("<I", data, 10)[0]
        _, width, height, _, bits, compression, _, _, _, used, _ = struct.unpack_from("<IiiHHIIiiII", data, 14)
        if bits != 8 or compression != 0:
            raise ValueError("BMP is not 8-bit uncompressed")
        colours = used or 256
        palette = [0] * 768
        for i in range(colours):
            b, g, r = data[54 + 4 * i:57 + 4 * i]
            palette[3 * i:3 * i + 3] = [r, g, b]
        stride = (width + 3) & ~3
        rows = [data[offset + y * stride:offset + y * stride + width] for y in range(abs(height))]
        if height > 0:
            rows.reverse()
        self.mode = "P"
        self.size = (width, abs(height))
        self._pixels = b"".join(rows)
        self._palette = palette

    def tobytes(self):
        return self._pixels

    def getpalette(self):
        return list(self._palette)


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


class Png:
    def __init__(self, data):
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError("not a PNG file")
        pos = 8
        idat = []
        while pos < len(data):
            length, kind = struct.unpack_from(">I4s", data, pos)
            body = data[pos + 8:pos + 8 + length]
            if kind == b"IHDR":
                width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            elif kind == b"IDAT":
                idat.append(body)
            pos += 12 + length
        if depth != 8 or colour != 6 or interlace:
            raise ValueError("PNG is not 8-bit RGBA, non-interlaced")
        self.mode = "RGBA"
        self.size = (width, height)
        self._raw = zlib.decompress(b"".join(idat))
        self._rows = []

    def convert(self, mode):
        if mode != "RGBA":
            raise ValueError(mode)
        return self

    def _row(self, y):
        width = self.size[0]
        stride = 4 * width
        while len(self._rows) <= y:
            n = len(self._rows)
            start = n * (stride + 1)
            kind = self._raw[start]
            line = bytearray(self._raw[start + 1:start + 1 + stride])
            prior = self._rows[-1] if self._rows else bytes(stride)
            if kind == 1:
                for i in range(4, stride):
                    line[i] = (line[i] + line[i - 4]) & 0xFF
            elif kind == 2:
                for i in range(stride):
                    line[i] = (line[i] + prior[i]) & 0xFF
            elif kind == 3:
                for i in range(stride):
                    left = line[i - 4] if i >= 4 else 0
                    line[i] = (line[i] + ((left + prior[i]) >> 1)) & 0xFF
            elif kind == 4:
                for i in range(stride):
                    left = line[i - 4] if i >= 4 else 0
                    up_left = prior[i - 4] if i >= 4 else 0
                    line[i] = (line[i] + _paeth(left, prior[i], up_left)) & 0xFF
            elif kind != 0:
                raise ValueError(f"PNG filter {kind}")
            self._rows.append(bytes(line))
        return self._rows[y]

    def getpixel(self, position):
        x, y = position
        return tuple(self._row(y)[4 * x:4 * x + 4])


_cache = {}


def open_image(path):
    path = Path(path)
    if path not in _cache:
        data = path.read_bytes()
        _cache[path] = Bmp(data) if data[:2] == b"BM" else Png(data)
    return _cache[path]


def install():
    image = types.ModuleType("PIL.Image")
    image.open = open_image
    package = types.ModuleType("PIL")
    package.Image = image
    sys.modules["PIL"] = package
    sys.modules["PIL.Image"] = image
