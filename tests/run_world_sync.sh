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
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_bomb_native.c src/utils/anchor_world_dynamic_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/bomb"
"$WORLD_TEST_DIR/bomb"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_crane_native.c src/utils/anchor_world_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/crane"
"$WORLD_TEST_DIR/crane"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_bridge_native.c src/utils/anchor_world_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/bridge"
"$WORLD_TEST_DIR/bridge"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_gate64_native.c src/utils/anchor_world_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/gate64"
"$WORLD_TEST_DIR/gate64"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-misleading-indentation \
    -O2 -ffast-math -fno-unsafe-math-optimizations -fsanitize=undefined \
    -Iinclude tests/test_world_doll_native.c src/utils/anchor_world_codec.c \
    src/utils/string_utils.c -lm -o "$WORLD_TEST_DIR/doll"
"$WORLD_TEST_DIR/doll"
"${HOST_CC:-cc}" -std=c99 -Wall -Wextra -Werror -O2 -fsanitize=undefined \
    -Iinclude -Itests tests/test_world_equipment_native.c -o "$WORLD_TEST_DIR/equipment"
"$WORLD_TEST_DIR/equipment"
PYTHONPATH=py python3 -m unittest discover -s tests -p 'test_world*.py'
