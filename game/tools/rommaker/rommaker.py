#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fix-It Felix Jr. (Tobikomi rev B) ROM maker prototype.

build: make felixjr.zip from the user's own copy of the game (FixitFelixJr.exe with its Data folder,
optionally the AtGames res_fif folder and the Mega Drive ROM). Standard library only.
dist:  developer step, turns build/felix_main.bin into the shippable program template (game-derived
tables blanked) and records where the tool fills them in.
"""
import argparse
import hashlib
import json
import math
import re
import struct
import sys
import tempfile
import wave
import zipfile
import zlib
from operator import mul
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
GAME = TOOLS.parent
sys.path.insert(0, str(HERE))
sys.path.insert(1, str(TOOLS))
import common
import cpack
import exeprog

WINDOWS_MD5 = {
    "FixitFelixJr.exe": "2bfc8fac7af8ddac15159f9a82b0f1e1",
    "Data/texture2.bmp": "32ec1ee5d870fedc3f1a6cb0492e8f93",
    "Data/Controls.log": "96a30e8f61235a06eb18fd9da8b7fc79",
    "Data/felix0.png": "88e611430040badcfd9e8ba9c0e8d40b",
    "Data/felix1.png": "f5cf8b74c8d131f429456ea73ddaa651",
    "Data/felix2.png": "237a0df0c1c0401c1dbfe0df480abb76",
    "Data/felix3.png": "40d5262ef1dec4394a2b7e1a340c37e6",
    "Data/felix4.png": "01e5d542077432923d855cb7cb154675",
    "Data/hammer0.png": "470321401a0cfa3e41e725b8bdc0816a",
    "Data/hammer1.png": "23441d0434b1f8f103c7f9525e5eaf20",
    "Data/hammer2.png": "1464db2f434b9bb7792459339a620109",
    "Data/hammer3.png": "b6cb11e2a715caf36256e21a81966a6c",
    "Data/sound0.wav": "10b3936acfc41f8208a74c9b4c7b7b23",
    "Data/sound1.wav": "a3e0a0279385296eed6a2317e4eb3459",
    "Data/sound2.wav": "1bd14a6b321cf3097c790150efef3adc",
    "Data/sound3.wav": "bd567e61939a1f8b4166ac71685025f2",
    "Data/sound4.wav": "49b254072510d8c4c5cd52c5a7f135e1",
    "Data/sound5.wav": "7ebc0ef4a13abf0adab0478d326ae6ac",
    "Data/sound6.wav": "f0be968efe94c2b464cb6f6b27e007e6",
    "Data/sound7.wav": "6f1033478b665ce8ba71920eedab0e79",
    "Data/sound8.wav": "114ce7fc2bfb9c8c122d2793cbdc66fb",
    "Data/sound9.wav": "87e0e8355d1190fe783e0b1ef3563df7",
    "Data/sound10.wav": "32f2bfec52b9daa88afe29b84b8a8069",
    "Data/sound11.wav": "bac4005a8fb20b1ebb02ab8e9ea4d718",
    "Data/sound12.wav": "2c2cf477300c98f5ea16eb53bd1dc693",
    "Data/sound13.wav": "a7645ad32cba5f458a2fc3ea867e5059",
    "Data/sound14.wav": "23a015d39d52e44aebf7979ddb670468",
    "Data/sound15.wav": "48c7e5d16b77559e26eeaddeb87438c5",
    "Data/sound16.wav": "50fa2dc5dc74bfa91759e2c64f9c71c4",
    "Data/sound17.wav": "74bd9873202a12d4d4c1ad51342044cc",
    "Data/sound18.wav": "966559ffca7baa77f4aa1fb796ecb5ba",
    "Data/sound19.wav": "d5cb21668642447ab78024083c48e69d",
    "Data/sound20.wav": "07fa0e310144bea1c2c9ffc5a10297cc",
    "Data/sound21.wav": "1d4edcee5bf56ddbd6a028f499bc6e2d",
    "Data/sound22.wav": "d4acf74466587a82bb233fec59153531",
    "Data/sound23.wav": "dc455f888d2ac80ea945ea6c256ddd2b",
    "Data/sound24.wav": "02533080b2435fae52fad115ac0076f1",
    "Data/sound25.wav": "578f4890257a780ce9672c6c3855021e",
    "Data/sound26.wav": "6cb47377dbe6cb7641728594155b193b",
    "Data/sound27.wav": "46821cc98ef458b57f04e02c7a1e47f9",
    "Data/sound28.wav": "6d87993a12ad28bdf990343018b19f9b",
    "Data/sound29.wav": "0571b09853944a47395f8bb9a278f2f7",
}
ATGAMES_MD5 = {
    "sound30.wav": "2fed29063b33a80fbbb3b7bbaec2e1b3",
    "sound31.wav": "3a572ed2cc464935abb172de7f18ddbe",
}
MEGADRIVE_MD5 = "38a3555768b31dc2abfc34d20ef546ca"

STEP_LOW = 6
STEP_HIGH = 8
CHIP_RATE = 31250
VOICE = 16000
G_BASE = 59828
G_VOICE = 29985
LOOPED = {14}
LOOP_PHASES = 1024
LOW_CUTOFF = 0.25

RECIPE = [
    (0, "win", 6315, 40969, STEP_HIGH, G_BASE),
    (1, "win", 0, 15082, STEP_HIGH, G_BASE),
    (2, "win", 5968, 39762, STEP_HIGH, G_BASE),
    (3, "win", 1444, 10578, STEP_HIGH, G_BASE),
    (4, "win", 690, 41896, STEP_HIGH, G_BASE),
    (5, "win", 724, 21316, STEP_HIGH, G_BASE),
    (6, "win", 717, 9785, STEP_HIGH, G_BASE),
    (7, "win", 0, 483908, STEP_LOW, G_BASE),
    (8, "win", 0, 520248, STEP_LOW, G_BASE),
    (9, "win", 28468, 691652, STEP_LOW, G_BASE),
    (10, "win", 0, 122292, STEP_LOW, G_BASE),
    (11, "win", 0, 156788, STEP_LOW, G_BASE),
    (12, "win", 0, 269012, STEP_LOW, G_BASE),
    (13, "win", 0, 77152, STEP_LOW, G_BASE),
    (14, "win", 0, 996156, STEP_HIGH, G_BASE),
    (15, "win", 1457, 11073, STEP_HIGH, G_BASE),
    (16, "win", 724, 19112, STEP_HIGH, G_BASE),
    (17, "win", 773, 21811, STEP_HIGH, G_BASE),
    (18, "win", 653, 54779, STEP_HIGH, G_BASE),
    (19, "win", 704, 66886, STEP_HIGH, G_BASE),
    (20, "win", 0, 14378, STEP_HIGH, G_BASE),
    (21, "win", 22776, 120540, STEP_LOW, G_BASE),
    (22, "win", 618, 81486, STEP_LOW, G_BASE),
    (23, "win", 716, 44332, STEP_HIGH, G_BASE),
    (24, "win", 1272, 355008, STEP_LOW, G_BASE),
    (25, "win", 652, 85260, STEP_LOW, G_BASE),
    (26, "win", 704, 23524, STEP_LOW, G_BASE),
    (27, "win", 0, 2302, STEP_HIGH, G_BASE),
    (28, "win", 714, 44492, STEP_HIGH, G_BASE),
    (29, "win", 0, 79380, STEP_LOW, G_BASE),
    (30, "atg", 0, 177872, STEP_LOW, G_BASE),
    (31, "atg", 592, 89672, STEP_LOW, G_BASE),
    (32, "md", 0x34374, 0x34374 + 15976, STEP_HIGH, G_VOICE),
    (33, "md", 0x38410, 0x38410 + 17368, STEP_HIGH, G_VOICE),
]
SOURCE_RATE = {"win": 44100, "atg": 44100, "md": VOICE}

PAGE = 0x1000
SAMPLE_BANK = 0x100000
BANK_PAGES = SAMPLE_BANK // PAGE
BANK0_ONLY = {11, 12, 13, 14, 21, 22}
BANK1_ONLY = {7, 8, 9, 10}
HALF_WIDTH = 10
KAISER_BETA = 8.0
COEF_BITS = 15
GAIN_SHIFT = COEF_BITS + 24

MAIN_SIZE = 0x20000
MAIN_CHIP = 0x8000
SAMPLE_CHIP = 0x80000
CHIPS = [
    ("main", MAIN_CHIP, ["fj-p1.1d", "fj-p2.1e", "fj-p3.1f", "fj-p4.1g"]),
    ("tiles", 0x8000, ["fj-t1.5h", "fj-t2.5j"]),
    ("sprites", 0x8000, ["fj-s1.7a", "fj-s2.7b", "fj-s3.7c", "fj-s4.7d",
                         "fj-s5.7e", "fj-s6.7f", "fj-s7.7g", "fj-s8.7h"]),
    ("samples", SAMPLE_CHIP, ["fj-v1.3a", "fj-v2.3b", "fj-v3.3c", "fj-v4.3d"]),
    ("pal", 0x400, ["fj-pl.6b", "fj-ph.6c"]),
]
FILL = {"main": 0xFF, "tiles": 0, "sprites": 0, "samples": 0xFF, "pal": 0}
HOLES = [("gfx", "_spr_parts", ["gfx_game.c"], ["gfx.h"]),
         ("demo", "_demo_log", ["demo_log.c"], ["demo_log.h"])]

PI = 3.141592653589793


def md5(path):
    return hashlib.md5(Path(path).read_bytes()).hexdigest()


def check_inputs(root, table, label):
    bad = []
    for name, want in table.items():
        path = root / name
        if not path.exists():
            bad.append(f"{label}: missing {name}")
        elif md5(path) != want:
            bad.append(f"{label}: {name} md5 {md5(path)} is not the known good {want}")
    return bad


def dsin(x):
    k = int(x / (2 * PI) + (0.5 if x >= 0 else -0.5))
    x = x - k * (2 * PI)
    term = x
    total = x
    x2 = x * x
    for n in range(1, 25):
        term = -term * x2 / ((2 * n) * (2 * n + 1))
        total = total + term
    return total


def bessel_i0(x):
    term = 1.0
    total = 1.0
    q = x * x / 4.0
    for k in range(1, 60):
        term = term * q / (k * k)
        total = total + term
    return total


def gcd(a, b):
    while b:
        a, b = b, a % b
    return a


class Resampler:
    def __init__(self, src, out, phases=0, cutoff=0.0):
        g = gcd(src, out)
        self.src, self.out = src // g, out // g
        self.steps = phases or self.out
        self.ratio = cutoff or min(1.0, self.out / self.src)
        self.half = HALF_WIDTH / self.ratio
        self.reach = int(self.half) + 1
        self.norm = bessel_i0(KAISER_BETA)
        self.phases = {}

    def tap(self, t):
        u = t / self.half
        if u <= -1.0 or u >= 1.0:
            return 0.0
        window = bessel_i0(KAISER_BETA * math.sqrt(1.0 - u * u)) / self.norm
        x = self.ratio * t
        sinc = 1.0 if x == 0.0 else dsin(PI * x) / (PI * x)
        return self.ratio * sinc * window

    def phase(self, f):
        if f not in self.phases:
            offset = f / self.steps
            taps = [self.tap(d - offset) for d in range(-self.reach, self.reach + 1)]
            q = [int(c * (1 << COEF_BITS) + 0.5) if c >= 0 else -int(-c * (1 << COEF_BITS) + 0.5) for c in taps]
            q[self.reach] += (1 << COEF_BITS) - sum(q)
            self.phases[f] = q
        return self.phases[f]

    def run(self, x, n=None, periodic=False):
        if n is None:
            n = -(-len(x) * self.out // self.src)
        if periodic:
            padded = x[-self.reach:] + x + x[:self.reach + 2]
        else:
            padded = [0] * self.reach + x + [0] * (self.reach + 2)
        y = []
        for k in range(n):
            i, f = divmod(k * self.src, self.out)
            if self.steps != self.out:
                f = (2 * f * self.steps + self.out) // (2 * self.out)
                if f == self.steps:
                    i, f = i + 1, 0
            coefs = self.phase(f)
            y.append(sum(map(mul, coefs, padded[i:i + len(coefs)])))
        return y


def read_wav16(path):
    with wave.open(str(path), "rb") as w:
        width, frames = w.getsampwidth(), w.readframes(w.getnframes())
        if w.getnchannels() != 1:
            raise ValueError(f"{path} is not mono")
    if width == 1:
        return [(b - 128) << 8 for b in frames]
    return list(struct.unpack(f"<{len(frames) // 2}h", frames))


def play_rate16(step):
    return CHIP_RATE * step


def sound_length(n, start, end, src, step, looped=True):
    length = -(-(end - start) * play_rate16(step) // (SOURCE_RATE[src] * 16))
    if n in LOOPED and looped:
        length = max(1, (length + PAGE // 2) // PAGE) * PAGE
    return length


def pages_of(length):
    return -(-length // PAGE)


def layout(lengths):
    place = {}
    page = 0
    for n in range(len(RECIPE)):
        if n not in BANK0_ONLY and n not in BANK1_ONLY:
            place[n] = (page, 2)
            page += pages_of(lengths[n])
    for bank, group in ((0, BANK0_ONLY), (1, BANK1_ONLY)):
        p = page
        for n in sorted(group):
            place[n] = (p, bank)
            p += pages_of(lengths[n])
        if p > BANK_PAGES:
            raise ValueError(f"sample bank {bank} needs {p} pages of {BANK_PAGES}")
    return place


def convert(n, src, start, end, step, gain, inputs, resamplers):
    length = sound_length(n, start, end, src, step)
    if src == "md":
        if "md" not in inputs:
            return bytes(length)
        x = [(b - 128) << 8 for b in inputs["md"][start:end]]
    else:
        if src not in inputs:
            return bytes(length)
        data = read_wav16(inputs[src] / f"sound{n}.wav")
        x = data[start:end] + [0] * max(0, end - max(start, len(data)))
    cutoff = LOW_CUTOFF if step == STEP_LOW or n in LOOPED else 0.0
    key = (SOURCE_RATE[src] * 16, play_rate16(step), cutoff)
    if key not in resamplers:
        resamplers[key] = Resampler(*key[:2], 0, cutoff)
    acc = resamplers[key].run(x)
    if n in LOOPED:
        rounding = 1 << (COEF_BITS - 1)
        x = [(a + rounding) >> COEF_BITS for a in acc]
        loop = Resampler(len(x), length, LOOP_PHASES)
        acc = loop.run(x, length, periodic=True)
        resamplers[("loop", n)] = loop
    half = 1 << (GAIN_SHIFT - 1)
    out = bytearray()
    for a in acc:
        v = (a * gain + half) >> GAIN_SHIFT
        out.append((127 if v > 127 else -127 if v < -127 else v) & 0xFF)
    if len(out) != length:
        raise ValueError(f"sound{n}: {len(out)} samples, expected {length}")
    return bytes(out)


def sample_rom(inputs):
    resamplers = {}
    pcm = [convert(*entry, inputs, resamplers) for entry in RECIPE]
    lengths = [len(p) for p in pcm]
    place = layout(lengths)
    rom = bytearray(2 * SAMPLE_BANK)
    used = 0
    for n, data in enumerate(pcm):
        page, bank = place[n]
        for b in ((0, 1) if bank == 2 else (bank,)):
            at = b * SAMPLE_BANK + page * PAGE
            rom[at:at + len(data)] = data
            used = max(used, b * SAMPLE_BANK + (page + pages_of(len(data))) * PAGE)
    size = -(-used // 0x10000) * 0x10000
    coef = hashlib.sha1()
    for r in resamplers.values():
        for f in sorted(r.phases):
            coef.update(struct.pack(f"<{len(r.phases[f])}i", *r.phases[f]))
    return bytes(rom[:size]), place, lengths, coef.hexdigest()


def run_converters(exe, work):
    import imagelite
    imagelite.install()
    common.BUILD = work
    common.GEN = work / "gen"
    common.REFERENCE = exe.parent
    common.EXE = exe
    common.DATA = exe.parent / "Data"
    import ctlpack
    import gfxconv
    import tables
    import tmpack
    tables.main()
    gfxconv.main()
    tmpack.main()
    ctlpack.main()
    return {k: (work / f"felix_{k}.bin").read_bytes() for k in ("tiles", "sprites", "pal")}


def rom_offset(v):
    if v < 0x10000:
        return v
    return (v >> 16) * 0x4000 + (v & 0xFFFF) - 0x8000


def pack_hole(gen, sources, headers, cpu):
    header = "\n".join((gen / h).read_text() for h in headers)
    blob, _ = cpack.pack([(gen / s).read_text() for s in sources], header, cpu)
    return blob


def chip_checks(rom):
    for start in range(0, MAIN_SIZE, MAIN_CHIP):
        end = start + MAIN_CHIP - 1
        rom[end] = (-sum(rom[start:end])) & 0xFF


def program_rom(template, holes, gen, layout, exe):
    rom = bytearray(template)
    for kind, offset, cpu, length, sources, headers in holes:
        blob = pack_hole(gen, sources, headers, cpu)
        if len(blob) != length:
            raise ValueError(f"{kind} tables are {len(blob)} bytes, the program expects {length}")
        rom[offset:offset + length] = blob
    exeprog.fill(rom, layout, exe)
    chip_checks(rom)
    return bytes(rom)


def split(regions):
    files = {}
    for region, size, names in CHIPS:
        blob = regions[region]
        blob = blob + bytes([FILL[region]]) * (size * len(names) - len(blob))
        if len(blob) != size * len(names):
            raise ValueError(f"{region} is {len(blob):#x} bytes")
        for i, name in enumerate(names):
            files[name] = blob[i * size:(i + 1) * size]
    return files


def write_zip(path, files):
    with zipfile.ZipFile(path, "w") as z:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 0
            info.external_attr = 0
            z.writestr(info, data, compresslevel=9)


def assembled_md5(files):
    h = hashlib.md5()
    for _, _, names in CHIPS:
        for name in names:
            h.update(files[name])
    return h.hexdigest()


def compare_samples(new, old, place, lengths):
    lines = []
    worst = 0
    for n in range(len(RECIPE)):
        page, bank = place[n]
        at = (0 if bank == 2 else bank) * SAMPLE_BANK + page * PAGE
        x = [v - 256 if v > 127 else v for v in new[at:at + lengths[n]]]
        y = [v - 256 if v > 127 else v for v in old[at:at + lengths[n]]]
        d = [p - q for p, q in zip(x, y)]
        peak = max(abs(v) for v in d)
        rms = (sum(v * v for v in d) / len(d)) ** 0.5
        level = (sum(v * v for v in y) / len(y)) ** 0.5
        worst = max(worst, peak)
        same = sum(v == 0 for v in d)
        lines.append(f"  sound{n:<2d} {lengths[n]:6d} samples, max diff {peak:3d} rms {rms:5.2f} "
                     f"(signal rms {level:5.1f}) identical {100 * same / len(d):5.1f}%")
    return lines, worst


def cmd_dist(args):
    build = Path(args.build)
    rom = bytearray((build / "felix_main.bin").read_bytes())
    pristine = bytes(rom)
    symbols = dict((m[1], int(m[0], 16)) for m in re.findall(r"^\s+([0-9A-F]{8})\s+(_\w+)", (build / "felix_main.map").read_text(), re.M))
    holes = []
    gen = build / "gen"
    for kind, symbol, sources, headers in HOLES:
        cpu = symbols[symbol] & 0xFFFF
        offset = rom_offset(symbols[symbol])
        blob = pack_hole(gen, sources, headers, cpu)
        if bytes(rom[offset:offset + len(blob)]) != blob:
            raise SystemExit(f"{kind}: packed tables differ from felix_main.bin at {offset:#x}")
        rom[offset:offset + len(blob)] = bytes(len(blob))
        holes.append([kind, offset, cpu, len(blob), sources, headers])
    checks = {start + MAIN_CHIP - 1 for start in range(0, MAIN_SIZE, MAIN_CHIP)}
    probes = [p.read_bytes() for p in sorted((build / "probe").glob("felix_probe*.bin"), key=lambda p: int(p.stem[11:]))]
    exe = (Path(args.exe) if args.exe else common.need_exe()).read_bytes()
    rom, layout = exeprog.dist(pristine, rom, probes, symbols, exe, rom_offset, checks)
    for o in checks:
        rom[o] = 0
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "felix_prog.bin").write_bytes(rom)
    (out / "holes.json").write_text(json.dumps(holes, indent=1) + "\n")
    (out / "exe.json").write_text(json.dumps(layout) + "\n")
    print(f"exe numbers: {len(layout['relocs'])} operands for {len(layout['scalars'])} scalars, "
          f"{len(layout['tables'])} tables in " + ", ".join(f"{s['section']} {s['offset']:#07x}+{s['length']:#x}" for s in layout["sections"]))
    print(f"program template {out / 'felix_prog.bin'}: " + ", ".join(f"{k} {o:#07x}+{n:#x}" for k, o, c, n, *_ in holes))


def cmd_build(args):
    exe = Path(args.exe).resolve()
    problems = check_inputs(exe.parent, WINDOWS_MD5, "Windows build")
    inputs = {"win": exe.parent / "Data"}
    if args.atgames:
        folder = Path(args.atgames)
        problems += check_inputs(folder, ATGAMES_MD5, "AtGames res_fif")
        inputs["atg"] = folder
    if args.megadrive:
        data = Path(args.megadrive).read_bytes()
        if hashlib.md5(data).hexdigest() != MEGADRIVE_MD5:
            problems.append(f"Mega Drive ROM md5 {hashlib.md5(data).hexdigest()} is not the known good {MEGADRIVE_MD5}")
        inputs["md"] = data
    if problems:
        raise SystemExit("\n".join(problems))
    template = Path(args.template)
    holes = json.loads((template.parent / "holes.json").read_text())
    layout = json.loads((template.parent / "exe.json").read_text())
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        gfx = run_converters(exe, work)
        main = program_rom(template.read_bytes(), holes, work / "gen", layout, exe.read_bytes())
    samples, place, lengths, coef_sha1 = sample_rom(inputs)
    files = split({"main": main, "tiles": gfx["tiles"], "sprites": gfx["sprites"] + bytes(0x40000 - len(gfx["sprites"])),
                   "samples": samples, "pal": gfx["pal"]})
    write_zip(Path(args.output), files)
    variant = "+".join(["windows"] + [k for k in ("atg", "md") if k in inputs])
    print(f"wrote {args.output} ({variant}), assembled md5 {assembled_md5(files)}, resampler coefficients sha1 {coef_sha1}")
    if args.compare:
        today = {}
        for line in Path(args.compare).read_text().splitlines():
            parts = line.split()
            today[parts[0]] = parts[-1]
        for name, data in files.items():
            crc = f"{zlib.crc32(data):08x}"
            print(f"  {name:10s} {crc} today {today.get(name, '-')}  {'MATCH' if crc == today.get(name) else 'differs'}")
    if args.samples_ref:
        lines, worst = compare_samples(samples, Path(args.samples_ref).read_bytes(), place, lengths)
        print(f"sample ROM against {args.samples_ref}: worst sample difference {worst} (8-bit steps)")
        print("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--exe", required=True, help="FixitFelixJr.exe, with its Data folder next to it")
    b.add_argument("--atgames", help="AtGames res_fif folder (sound30, sound31)")
    b.add_argument("--megadrive", help="Mega Drive ROM (voice clips 32, 33)")
    b.add_argument("--template", default=str(GAME / "build" / "rommaker" / "felix_prog.bin"))
    b.add_argument("-o", "--output", default="felixjr.zip")
    b.add_argument("--compare", help="felixjr.txt CRC list from mkroms")
    b.add_argument("--samples-ref", help="felix_samples.bin to compare the sample ROM with")
    d = sub.add_parser("dist")
    d.add_argument("--build", default=str(GAME / "build"))
    d.add_argument("--out", default=str(GAME / "build" / "rommaker"))
    d.add_argument("--exe", help="FixitFelixJr.exe (default: the one in $FELIX_WIN)")
    args = parser.parse_args()
    {"build": cmd_build, "dist": cmd_dist}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
