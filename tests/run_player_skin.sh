#!/usr/bin/env bash
# Host tests for the alternative Ebisumaru skin toggle, appearance bit transport,
# and the remote graphics-scratch preflight/redirect.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
CC="${HOST_CC:-cc}"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-player-skin-tests.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

"$CC" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_player_skin.c -o "$TMP/skin"
"$TMP/skin"

"$CC" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_alternative_model.c src/alternative_ebisumaru/anchor_alternative_model.c -o "$TMP/alternative_model"
"$TMP/alternative_model"

"$CC" -std=c99 -Wall -Wextra -Werror -Iinclude \
    tests/test_remote_appearance.c src/utils/anchor_remote_appearance.c \
    -o "$TMP/remote_appearance"
"$TMP/remote_appearance"

"$CC" -std=c99 -Wall -Wextra -Werror -DANCHOR_RENDER_SCRATCH_HOST_TEST -Iinclude \
    tests/test_render_scratch.c src/anchor_render_scratch.c \
    src/utils/anchor_render_budget.c -o "$TMP/render_scratch"
"$TMP/render_scratch"

PYTHONPATH=py python3 -m unittest tests.test_appearance_skin_flags
