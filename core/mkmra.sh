#!/bin/sh
# SPDX-FileCopyrightText: 2026 Chris Watson
# SPDX-License-Identifier: GPL-3.0-or-later
set -e
cd "${JTROOT:?source setprj.sh in your jtcores checkout first}"
jtframe mra felix --nodbg
M="release/mra/Fix-It Felix Jr..mra"
sed -i -e '/<dip name="Coin/ s/\(,-\)\+"/"/' -e 's#vertical (cw)#vertical (ccw)#' -e 's/ asm_md5="[0-9a-f]*"//' -e 's#<rbf>jtfelix</rbf>#<rbf>fifelix</rbf>#' \
    -e 's/ twitter="[^"]*"//' -e 's# source="[^"]*"# source="https://github.com/niknakniknak/FixItFelixArcadeForMister"#' "$M"
echo "$JTROOT/$M"
