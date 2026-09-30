#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-control-machine-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST_COMPILER="${HOST_CC:-cc}"
TEST_FLAGS=(-std=c99 -Wall -Wextra -Werror -fsanitize=undefined -Iinclude)

for test in \
    test_anchor_control_machine_native \
    test_anchor_control_machine_intro \
    test_anchor_control_machine_sync \
    test_anchor_control_machine_projectiles \
    test_anchor_control_machine_hud; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/${test}.c" \
        -o "$TEST_DIR/$test"
    "$TEST_DIR/$test"
done

"$HOST_COMPILER" "${TEST_FLAGS[@]}" tests/test_world_quest_native.c \
    -o "$TEST_DIR/test_world_quest_native"
"$TEST_DIR/test_world_quest_native"

for test in test_boss_sync_tsurami test_boss_sync_dharumanyo; do
    "$HOST_COMPILER" "${TEST_FLAGS[@]}" "tests/${test}.c" \
        src/utils/json_utils.c src/utils/string_utils.c \
        -o "$TEST_DIR/$test"
    "$TEST_DIR/$test"
done
