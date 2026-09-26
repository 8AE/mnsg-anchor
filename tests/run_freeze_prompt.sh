#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
HOST_COMPILER="${HOST_CC:-cc}"
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-freeze-prompt.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_freeze_prompt.c -o "$TEST_DIR/prompt"
"$TEST_DIR/prompt"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_anchor_dialog.c -o "$TEST_DIR/dialog"
"$TEST_DIR/dialog"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_freeze_cube_visual.c -o "$TEST_DIR/visual"
"$TEST_DIR/visual"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror \
    -DANCHOR_RENDER_SCRATCH_HOST_TEST -Iinclude \
    tests/test_render_scratch.c src/combat/anchor_render_scratch.c \
    src/combat/anchor_render_budget.c -o "$TEST_DIR/scratch"
"$TEST_DIR/scratch"
