#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-impact-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST_COMPILER="${HOST_CC:-cc}"
TEST_FLAGS=(-std=c99 -Wall -Wextra -Iinclude)
if [[ "${UBSAN:-0}" == 1 ]]; then
    TEST_FLAGS+=(-fsanitize=undefined -fno-omit-frame-pointer)
fi
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_codec.c \
    src/utils/anchor_impact_codec.c -o "$TEST_DIR/codec"
"$TEST_DIR/codec"
for module in native damage; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/test_impact_${module}.c" -o "$TEST_DIR/$module"
    "$TEST_DIR/$module"
done
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_players_native.c \
    src/utils/anchor_impact_players_codec.c -o "$TEST_DIR/players"
"$TEST_DIR/players"
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_visuals_native.c \
    src/utils/anchor_impact_visual_codec.c -o "$TEST_DIR/visuals"
"$TEST_DIR/visuals"
PYTHONPATH=py python3 -m unittest discover -s tests -p 'test_impact*.py'
