#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-tsurami-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST_COMPILER="${HOST_CC:-cc}"
TEST_FLAGS=(-std=c99 -Wall -Wextra -Iinclude)
if [[ "${UBSAN:-0}" == 1 ]]; then
    TEST_FLAGS+=(-fsanitize=undefined -fno-omit-frame-pointer)
fi
for test in test_tsurami_damage test_anchor_tsurami_native test_miracle_star; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/${test}.c" -o "$TEST_DIR/$test"
    "$TEST_DIR/$test"
done
"$HOST_COMPILER" "${TEST_FLAGS[@]}" -DANCHOR_TSURAMI_DAMAGE_HOST_TEST \
    -include tests/tsurami_reflection_damage_adapter.h \
    -c src/anchor_tsurami_damage.c -o "$TEST_DIR/reflection_damage.o"
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_tsurami_reflection_integration.c \
    "$TEST_DIR/reflection_damage.o" -o "$TEST_DIR/reflection"
"$TEST_DIR/reflection"
"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_tsurami_codec.c \
    src/utils/anchor_tsurami_codec.c -o "$TEST_DIR/codec"
"$TEST_DIR/codec"
"$HOST_COMPILER" "${TEST_FLAGS[@]}" -DANCHOR_TSURAMI_SYNC_HOST_TEST \
    tests/test_tsurami_sync.c src/anchor_tsurami_sync.c \
    src/utils/anchor_tsurami_codec.c -o "$TEST_DIR/sync"
"$TEST_DIR/sync"
for test in test_boss_sync_tsurami test_boss_sync_dharumanyo; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/${test}.c" \
        src/utils/json_utils.c src/utils/string_utils.c -o "$TEST_DIR/$test"
    "$TEST_DIR/$test"
done
python3 -m unittest discover -s tests -p 'test_tsurami_transport.py'
