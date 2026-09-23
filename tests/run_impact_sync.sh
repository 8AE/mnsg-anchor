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
    src/bosses/impact/anchor_impact_codec.c -o "$TEST_DIR/codec"
"$TEST_DIR/codec"
python3 tests/impact_boss_stubs.py > "$TEST_DIR/boss_stubs.c"
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_bosses_native.c \
    "$TEST_DIR/boss_stubs.c" -o "$TEST_DIR/bosses"
"$TEST_DIR/bosses"
for module in native damage sounds_native; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/test_impact_${module}.c" -o "$TEST_DIR/$module"
    "$TEST_DIR/$module"
done
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_players_native.c \
    src/bosses/impact/anchor_impact_players_codec.c -o "$TEST_DIR/players"
"$TEST_DIR/players"
for boss in 2 3 4; do "$TEST_DIR/players" "$boss"; done
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_impact_visuals_native.c \
    src/bosses/impact/anchor_impact_visual_codec.c -o "$TEST_DIR/visuals"
"$TEST_DIR/visuals"
# Taisamba skinned root, returning weapon, and translucent whirlwind assets.
for model in 18000E28 4800F5A0 4800FE90; do
    "$TEST_DIR/visuals" "$model"
done
# Balberra body/pod/gun and D'Etoile root/shield/glow recipes.
for model in 48016730 480137C0 48017330; do "$TEST_DIR/visuals" "$model" 3; done
for model in 18000E9C 480199E0; do "$TEST_DIR/visuals" "$model" 4; done
"$TEST_DIR/visuals" 480008B0 4 0x4B6
"$TEST_DIR/visuals" 48000170 4 0x4AE
PYTHONPATH=py python3 -m unittest discover -s tests -p 'test_impact*.py'
