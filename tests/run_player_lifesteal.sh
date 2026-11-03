#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
HOST_COMPILER="${HOST_CC:-cc}"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-player-lifesteal.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_player_damage.c src/combat/anchor_player_damage.c \
    -o "$TEST_DIR/damage"
"$TEST_DIR/damage"
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_player_freeze.c -o "$TEST_DIR/freeze"
"$TEST_DIR/freeze"
PYTHONPATH=py python3 -m unittest discover -s tests -p 'test_player_lifesteal_transport.py'
