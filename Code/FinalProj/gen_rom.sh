#!/bin/bash
# gen_rom.sh -- compile a game source file and embed it as rom_data.h
#
# Usage:
#   ./gen_rom.sh path/to/game.c

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPILER="$REPO_ROOT/Code/FinalProj/build/bin/compiler"
GAME_API_INC="$REPO_ROOT/Code/FinalProj/res/langs"
DST_DIR="$REPO_ROOT/Workspace/TFTHomemadeClaude"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <game.c>"
    exit 1
fi

GAME_C="$1"
ROM_TMP="$(mktemp /tmp/rom_XXXXXX.bin)"
PY_TMP="$(mktemp /tmp/rom_conv_XXXXXX.py)"

echo "Compiling $GAME_C..."
"$COMPILER" "$GAME_C" -I "$GAME_API_INC" -o "$ROM_TMP" 2>&1 | grep -v "^Used\|^\[" || true

# Write the converter script to a temp file (avoids all heredoc/stdin quirks)
cat > "$PY_TMP" << 'ENDPY'
import sys

src = sys.argv[1]
dst = sys.argv[2]
data = open(src, 'rb').read()

rows = []
row = []
for i, b in enumerate(data):
    row.append('0x{:02x}'.format(b))
    if len(row) == 12 or i == len(data) - 1:
        rows.append('  ' + ', '.join(row))
        row = []

with open(dst, 'w') as f:
    f.write('#ifndef _ROM_DATA_H\n')
    f.write('#define _ROM_DATA_H\n')
    f.write('#include <stdint.h>\n\n')
    f.write('static const uint8_t rom_data[] __attribute__((aligned(4))) = {\n')
    f.write(',\n'.join(rows))
    f.write('\n};\n')
    f.write('static const uint32_t rom_data_size = {};\n\n'.format(len(data)))
    f.write('#endif /* _ROM_DATA_H */\n')
ENDPY

echo "Generating rom_data.h..."
python3 "$PY_TMP" "$ROM_TMP" "$DST_DIR/rom_data.h"
rm -f "$ROM_TMP" "$PY_TMP"
echo "Done -- $DST_DIR/rom_data.h updated. Rebuild the CCS project to flash."
