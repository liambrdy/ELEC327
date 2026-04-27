#!/bin/bash
# gen_rom.sh -- compile a game source file to a .rom binary
#
# Usage:
#   ./gen_rom.sh <game.c> [output.rom] [-H rom_data.h]
#
# If no output path is given the ROM is written alongside the source file.
# -H <path>  also generate a C header (rom_data.h) for DEBUG_ROM firmware builds.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPILER="$REPO_ROOT/Code/FinalProj/build/bin/compiler"
GAME_API_INC="$REPO_ROOT/Code/FinalProj/res/langs"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <game.c> [output.rom] [-H rom_data.h]"
    exit 1
fi

GAME_C="$1"
GAME_BASE="$(basename "$GAME_C" .c)"
OUT_ROM=""
OUT_H=""

# Parse remaining arguments
shift
while [ $# -gt 0 ]; do
    case "$1" in
        -H) shift; OUT_H="$1" ;;
        *)  OUT_ROM="$1" ;;
    esac
    shift
done

if [ -z "$OUT_ROM" ]; then
    OUT_ROM="$(dirname "$GAME_C")/$GAME_BASE.rom"
fi

echo "Compiling $GAME_C -> $OUT_ROM ..."
"$COMPILER" "$GAME_C" -I "$GAME_API_INC" -o "$OUT_ROM" 2>&1 | grep -v "^Used\|^\[" || true

SIZE=$(wc -c < "$OUT_ROM")
echo "Done -- $OUT_ROM ($SIZE bytes)."

if [ -n "$OUT_H" ]; then
    echo "Generating header $OUT_H ..."
    python3 - "$OUT_ROM" "$OUT_H" "$GAME_BASE" <<'PYEOF'
import sys

rom_path, out_path, name = sys.argv[1], sys.argv[2], sys.argv[3]
data = open(rom_path, 'rb').read()

with open(out_path, 'w') as f:
    f.write('#ifndef _ROM_DATA_H\n')
    f.write('#define _ROM_DATA_H\n')
    f.write('#include <stdint.h>\n\n')
    f.write('static const char rom_data_name[] = "{}";\n\n'.format(name))
    f.write('static const uint8_t rom_data[] __attribute__((aligned(4))) = {\n')
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        comma = ',' if i + 12 < len(data) else ''
        f.write('  ' + ', '.join('0x{:02x}'.format(b) for b in chunk) + comma + '\n')
    f.write('};\n')
    f.write('static const uint32_t rom_data_size = {};\n\n'.format(len(data)))
    f.write('#endif /* _ROM_DATA_H */\n')

print('Header written -- {} bytes, name "{}".'.format(len(data), name))
PYEOF
    echo "  Copy $OUT_ROM to SD card root, or rebuild firmware with #define DEBUG_ROM."
else
    echo "  Copy to SD card root to play."
    echo "  To bake into firmware: $0 $GAME_C -H path/to/rom_data.h"
fi
