#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
HOST_COMPILER="${HOST_CC:-cc}"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-player-chain.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror \
    -DANCHOR_PLAYER_ATTACK_HOST_TEST -DMNSG_ARRAY_UTILS_HOST_TEST -Iinclude \
    tests/test_player_attack.c src/combat/anchor_player_attack.c \
    src/utils/array_utils.c -o "$TEST_DIR/attack"
"$TEST_DIR/attack"

for diagnostic_build in 0 1; do
    "$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror \
        -DDEBUG_BUTTON_ENABLED="$diagnostic_build" \
        -DMNSG_ARRAY_UTILS_HOST_TEST -Iinclude \
        tests/test_player_chain.c src/utils/array_utils.c -lm \
        -o "$TEST_DIR/chain_$diagnostic_build"
    "$TEST_DIR/chain_$diagnostic_build"
done

if [[ "$(uname -s)" == Darwin ]]; then
    UNUSED_SECTIONS=(-Wl,-dead_strip)
else
    UNUSED_SECTIONS=(-Wl,--gc-sections)
fi
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections -Iinclude \
    tests/test_player_chain_projectiles.c "${UNUSED_SECTIONS[@]}" \
    -o "$TEST_DIR/chain_projectiles"
"$TEST_DIR/chain_projectiles"
