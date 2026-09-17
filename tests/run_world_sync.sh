#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
WORLD_TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-world-tests.XXXXXX")"
trap 'rm -rf "$WORLD_TEST_DIR"' EXIT
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_native.c src/utils/anchor_world_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/native"
"$WORLD_TEST_DIR/native"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_dynamic_native.c src/utils/anchor_world_dynamic_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/children"
"$WORLD_TEST_DIR/children"
PYTHONPATH=py python3 -m unittest discover -s tests -p 'test_world*.py'
