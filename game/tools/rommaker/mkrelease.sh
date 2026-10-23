#!/bin/sh
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
set -e
GAME=$(cd "$(dirname "$0")/../.." && pwd)
: "${REF:?set REF to the folder that holds windows/FixitFelixJr.exe, fifj_upload/res_fif and megadrive/}"
OUT=${OUT:-$GAME/build/release}
WORK=$GAME/build/rommaker
SRC=$GAME/tools/rommaker/felixrommaker
C=$WORK/c
EXE=$REF/windows/FixitFelixJr.exe
ATG=${ATG:-$REF/fifj_upload/res_fif}
MD=${MD:-"$REF/megadrive/Fix-It Felix Jr. (World) (Aftermarket).md"}
DEFS="-DMINIZ_NO_STDIO -DMINIZ_NO_INFLATE_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DMINIZ_NO_TIME"
SOURCES="main.c util.c tables.c gfx.c program.c sound.c exedata.c vendor/miniz.c"

build_c() {
  python3 -I "$SRC/embed.py" "$WORK" "$C/embed_data.c"
  (cd "$SRC" && gcc -std=c99 -Os -Wall -ffp-contract=off -D_POSIX_C_SOURCE=200809L $DEFS -I. -Ivendor \
    -o "$C/felixrommaker" $SOURCES "$C/embed_data.c" -lm)
  (cd "$SRC" && x86_64-w64-mingw32-gcc -std=c99 -Os -s -static -Wall -ffp-contract=off -msse2 -mfpmath=sse \
    -ffunction-sections -fdata-sections -Wl,--gc-sections $DEFS -I. -Ivendor \
    -o "$C/felixrommaker.exe" $SOURCES "$C/embed_data.c" -lm)
}

cd "$GAME"
export FELIX_WIN="$REF/windows" FELIX_ATGAMES="$ATG" FELIX_MD="$MD"
make game probes
python3 -I tools/mkroms.py build/felix_main.bin felixjr.zip
python3 -I tools/rommaker/rommaker.py dist
mkdir -p "$C"
echo '{}' > "$WORK/expected.json"
build_c

sums=""
for v in windows windows+atgames windows+megadrive windows+atgames+megadrive; do
  set -- --exe "$EXE"
  case $v in *atgames*) set -- "$@" --atgames "$ATG";; esac
  case $v in *megadrive*) set -- "$@" --megadrive "$MD";; esac
  python3 -I tools/rommaker/rommaker.py build "$@" -o "$WORK/py.zip" --compare build/felixjr.txt > "$WORK/py.log"
  if grep -E "fj-(p|t|s|pl|ph)" "$WORK/py.log" | grep -v MATCH; then echo "$v: program or graphics differ from the build"; exit 1; fi
  "$C/felixrommaker" "$@" -o "$WORK/c.zip" > "$WORK/c.log"
  python3 -I -c "
import sys, zipfile
a, b = zipfile.ZipFile(sys.argv[1]), zipfile.ZipFile(sys.argv[2])
ca = [(i.filename, i.CRC, i.file_size) for i in a.infolist()]
cb = [(i.filename, i.CRC, i.file_size) for i in b.infolist()]
sys.exit(0 if ca == cb else 'C and Python output differ: %s' % [x for x, y in zip(ca, cb) if x != y])
" "$WORK/py.zip" "$WORK/c.zip"
  py=$(sed -n 's/.*assembled md5 \([0-9a-f]*\).*/\1/p' "$WORK/py.log")
  c=$(sed -n 's/.*assembled md5 \([0-9a-f]*\).*/\1/p' "$WORK/c.log")
  if [ "$py" != "$c" ]; then echo "$v: Python $py, C $c"; exit 1; fi
  echo "$v: $c"
  sums="$sums\"$v\": \"$c\", "
done
echo "{${sums%, }}" > "$WORK/expected.json"
build_c
"$C/felixrommaker" --exe "$EXE" --atgames "$ATG" --megadrive "$MD" -o "$WORK/c.zip" | grep -q "matches the release"

mkdir -p "$OUT"
cp "$C/felixrommaker.exe" "$WORK/felix_prog.bin" "$OUT/"
cp "$WORK/expected.json" "$OUT/expected_md5.json"
cp "$GAME/tools/rommaker/README.txt" "$OUT/README.txt"
ls -l "$OUT/felixrommaker.exe"
echo "release in $OUT"
