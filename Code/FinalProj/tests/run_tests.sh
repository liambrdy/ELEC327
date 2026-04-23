#!/usr/bin/env bash
# run_tests.sh [compiler] [vm] [tests_dir] [include_dir]
#
# Positional args are optional; defaults assume the script lives in tests/
# and the project was built at ../build/.
#
# Each test file must have "// expected: N" on its first line.
# The VM exit code is compared against N (0-255).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

COMPILER="${1:-$PROJECT_DIR/build/bin/compiler}"
VM="${2:-$PROJECT_DIR/build/bin/vm}"
TESTS_DIR="${3:-$SCRIPT_DIR}"
INCLUDE_DIR="${4:-$PROJECT_DIR/res/langs}"

# Colour codes (silently no-ops if the terminal doesn't support them)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

pass=0
fail=0
skip=0

if [ ! -x "$COMPILER" ]; then
    echo "error: compiler not found at $COMPILER" >&2
    echo "       run cmake --build <build_dir> first" >&2
    exit 1
fi
if [ ! -x "$VM" ]; then
    echo "error: vm not found at $VM" >&2
    exit 1
fi

for test_file in "$TESTS_DIR"/*.c; do
    [ -f "$test_file" ] || continue
    test_name="$(basename "$test_file" .c)"

    # Extract expected value: first line must contain "// expected: N"
    expected=$(head -1 "$test_file" | sed -n 's|.*// expected: \(-\{0,1\}[0-9][0-9]*\).*|\1|p')
    if [ -z "$expected" ]; then
        printf "${YELLOW}SKIP${NC}  %s\n" "$test_name"
        skip=$((skip + 1))
        continue
    fi

    rom_file=$(mktemp /tmp/test_XXXXXX.rom)

    # Compile — suppress all output; non-zero exit is a compile failure
    if ! "$COMPILER" "$test_file" -I"$INCLUDE_DIR" -o "$rom_file" >/dev/null 2>&1; then
        printf "${RED}FAIL${NC}  %-35s  compile error\n" "$test_name"
        fail=$((fail + 1))
        rm -f "$rom_file"
        continue
    fi

    # Run — capture exit code; suppress any VM output
    "$VM" "$rom_file" >/dev/null 2>&1
    actual=$?
    rm -f "$rom_file"

    if [ "$actual" -eq "$expected" ]; then
        printf "${GREEN}PASS${NC}  %s\n" "$test_name"
        pass=$((pass + 1))
    else
        printf "${RED}FAIL${NC}  %-35s  expected %s, got %s\n" \
               "$test_name" "$expected" "$actual"
        fail=$((fail + 1))
    fi
done

printf "\n${BOLD}Results: %d passed, %d failed, %d skipped${NC}\n" \
       "$pass" "$fail" "$skip"

[ "$fail" -eq 0 ]
