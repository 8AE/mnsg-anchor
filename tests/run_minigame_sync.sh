#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
TEST_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-minigame-tests.XXXXXX")"
trap 'rm -rf "$TEST_DIR"' EXIT
HOST_COMPILER="${HOST_CC:-cc}"
case "$(uname -s)" in
    Darwin) HUD_DEAD_CODE_FLAG="-Wl,-dead_strip" ;;
    *) HUD_DEAD_CODE_FLAG="-Wl,--gc-sections" ;;
esac
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -O2 -ffast-math \
    -ffunction-sections -fdata-sections "$HUD_DEAD_CODE_FLAG" -fsanitize=undefined \
    -Iinclude tests/test_player_model_sound_position.c -o "$TEST_DIR/test_player_model_sound_position"
"$TEST_DIR/test_player_model_sound_position"

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

"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -O2 -ffast-math -fsanitize=undefined \
    -Iinclude tests/test_minigame_yae_native.c -o "$TEST_DIR/test_minigame_yae_native"
"$TEST_DIR/test_minigame_yae_native"
"$TEST_DIR/test_minigame_yae_native" --export > "$TEST_DIR/yae-native.json"

# Preserve native four-byte pointer fields while executing the actual guarded
# world-transfer predicates on a64-bit host. Darwin fortify otherwise expands
# memcpy before the fixture's pointer-slot translator can observe the read.
"$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
    -Iinclude -D_FORTIFY_SOURCE=0 -DANCHOR_BOSS_INVITE_WORLD_HOST_TEST \
    -Dmemcpy=launch_native_pointer_copy \
    -c src/bosses/anchor_boss_invite_world.c -o "$TEST_DIR/yae-launch-world.o"
for YAE_TEST_DEBUG in 0 1; do
    "$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
        -Iinclude -DANCHOR_MINIGAME_INVITES_HOST_TEST \
        -DDEBUG_BUTTON_ENABLED="$YAE_TEST_DEBUG" \
        -c src/world/anchor_minigame_invites.c -o "$TEST_DIR/yae-launch-invites.o"
    "$HOST_COMPILER" -std=c99 -Wall -Wextra -Werror -fsanitize=undefined \
        -Iinclude -DDEBUG_BUTTON_ENABLED="$YAE_TEST_DEBUG" \
        tests/test_minigame_yae_launch.c "$TEST_DIR/yae-launch-world.o" \
        "$TEST_DIR/yae-launch-invites.o" src/utils/json_utils.c src/utils/string_utils.c \
        -o "$TEST_DIR/test_minigame_yae_launch"
    "$TEST_DIR/test_minigame_yae_launch"
done

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
from test_minigame_invitation_transport import (
    verify_native_ebisumaru_invitation, verify_native_yae_invitation,
)

pending = json.loads(Path(sys.argv[1], 'ebisumaru-cold-pending.json').read_text())
ready = json.loads(Path(sys.argv[1], 'ebisumaru-cold-ready.json').read_text())
verify_native_ebisumaru_invitation(pending, ready)
print('Ebisumaru native cold-start readiness -> actual metadata -> same invitation Yes preparation passed')

yae_native = json.loads(Path(sys.argv[1], 'yae-native.json').read_text())
verify_native_yae_invitation(yae_native)
print('Yae native capture -> fragmented invitation/metadata -> unready/ready accepted-host preparation passed')

for name, game in (('sasuke', 3), ('goemon', 1), ('ebisumaru', 2), ('yae', 4)):
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
"$TEST_DIR/test_minigame_yae_native" --validate "$TEST_DIR/yae-status-0.json"
"$TEST_DIR/test_minigame_yae_native" --validate "$TEST_DIR/yae-status-1.json"
