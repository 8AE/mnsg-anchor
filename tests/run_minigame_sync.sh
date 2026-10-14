#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-minigame-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST_COMPILER="${HOST_CC:-cc}"
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude tests/test_minigame_sync_native.c -o "$TEST_DIR/test_minigame_sync_native"
"$TEST_DIR/test_minigame_sync_native"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude tests/test_minigame_returns_native.c src/utils/json_utils.c \
    src/utils/string_utils.c -o "$TEST_DIR/test_minigame_returns_native"
"$TEST_DIR/test_minigame_returns_native"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude tests/test_minigame_goemon_native.c -o "$TEST_DIR/test_minigame_goemon_native"
"$TEST_DIR/test_minigame_goemon_native"
"$TEST_DIR/test_minigame_goemon_native" --export > "$TEST_DIR/goemon-native.json"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude tests/test_minigame_sasuke_native.c -o "$TEST_DIR/test_minigame_sasuke_native"
"$TEST_DIR/test_minigame_sasuke_native"
"$TEST_DIR/test_minigame_sasuke_native" --export > "$TEST_DIR/sasuke-native.json"

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude tests/test_minigame_ebisumaru_native.c -o "$TEST_DIR/test_minigame_ebisumaru_native"
"$TEST_DIR/test_minigame_ebisumaru_native"
"$TEST_DIR/test_minigame_ebisumaru_native" --export > "$TEST_DIR/ebisumaru-native.json"
"$TEST_DIR/test_minigame_ebisumaru_native" --export-cold-pending > "$TEST_DIR/ebisumaru-cold-pending.json"
"$TEST_DIR/test_minigame_ebisumaru_native" --export-cold-ready > "$TEST_DIR/ebisumaru-cold-ready.json"
python3 - "$TEST_DIR" <<'PY'
import json
import sys
from pathlib import Path

sys.path.insert(0, 'tests')
from test_minigame_sync_transport import make_native_status_fixture
from test_minigame_invitation_transport import verify_native_ebisumaru_invitation

pending = json.loads(Path(sys.argv[1], 'ebisumaru-cold-pending.json').read_text())
ready = json.loads(Path(sys.argv[1], 'ebisumaru-cold-ready.json').read_text())
verify_native_ebisumaru_invitation(pending, ready)
print('Ebisumaru native cold-start readiness -> actual metadata -> same invitation Yes preparation passed')

for name, game in (('sasuke', 3), ('goemon', 1), ('ebisumaru', 2)):
    native = json.loads(Path(sys.argv[1], f'{name}-native.json').read_text())
    for ready in (0, 1):
        status = make_native_status_fixture(native, game=game, local_ready=ready)
        Path(sys.argv[1], f'{name}-status-{ready}.json').write_text(
            json.dumps(status, separators=(',', ':')))
PY
"$TEST_DIR/test_minigame_sasuke_native" --validate "$TEST_DIR/sasuke-status-0.json"
"$TEST_DIR/test_minigame_sasuke_native" --validate "$TEST_DIR/sasuke-status-1.json"
"$TEST_DIR/test_minigame_goemon_native" --validate "$TEST_DIR/goemon-status-0.json"
"$TEST_DIR/test_minigame_goemon_native" --validate "$TEST_DIR/goemon-status-1.json"
"$TEST_DIR/test_minigame_ebisumaru_native" --validate "$TEST_DIR/ebisumaru-status-0.json"
"$TEST_DIR/test_minigame_ebisumaru_native" --validate "$TEST_DIR/ebisumaru-status-1.json"
