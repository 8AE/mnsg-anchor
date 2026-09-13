#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_binary="$(mktemp "${TMPDIR:-/tmp}/mnsg-dungeon-maps-test.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT
cc -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined \
    -DMNSG_ARRAY_UTILS_HOST_TEST -Iinclude tests/test_dungeon_maps.c \
    src/utils/array_utils.c src/utils/json_utils.c src/utils/string_utils.c -o "$test_binary"
"$test_binary"
