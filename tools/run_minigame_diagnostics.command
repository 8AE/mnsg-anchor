#!/bin/bash
set -u

game_bin="${1:-/Applications/Goemon64Recompiled.app/Contents/MacOS/Goemon64Recompiled}"
if [[ ! -x "$game_bin" ]]; then
    printf 'Cannot launch the game: %s\n' "$game_bin" >&2
    exit 1
fi

log_file="$(/usr/bin/mktemp /private/tmp/mnsg-mini-ebisumaru.XXXXXX)" || exit 1
printf 'Mini Ebisumaru diagnostic log: %s\n\n' "$log_file"
printf 'Use this launcher after quitting the game.\n'
printf 'Connect online, enter Mini Ebisumaru, and reproduce the frozen timer or invitation issue.\n'
printf 'Then quit the game normally to finish the recording.\n\n'

# A terminal keeps the game's stdout line buffered; -F flushes the recording.
/usr/bin/script -q -F "$log_file" "$game_bin"
game_status=$?

printf '\nRecording saved: %s\n' "$log_file"
printf 'Game exit status: %d\n' "$game_status"
read -r -p 'Press Return to close this launcher. ' || true
exit "$game_status"
