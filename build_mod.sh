#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

python3 tools/gen_icon_headers.py

MOD_FILENAME="$(awk -F'"' '/^[[:space:]]*mod_filename[[:space:]]*=/ { print $2; exit }' mod.toml)"
if [[ -z "$MOD_FILENAME" ]]; then
    echo "Error: could not read inputs.mod_filename from mod.toml" >&2
    exit 1
fi

RELEASE_NRM="build/${MOD_FILENAME}.nrm"
DEBUG_NRM="build/debug_${MOD_FILENAME}.nrm"
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-mod-build.XXXXXX")"
trap 'rm -rf "$TEMP_DIR"' EXIT

echo "Building release package (debug button disabled)..."
make clean
make DEBUG_BUTTON_ENABLED=0 "$@"
./RecompModTool mod.toml "$TEMP_DIR"

echo "Building debug package (debug button enabled)..."
make clean
make DEBUG_BUTTON_ENABLED=1 "$@"
./RecompModTool mod.toml build
mv "$RELEASE_NRM" "$DEBUG_NRM"
cp "$TEMP_DIR/${MOD_FILENAME}.nrm" "$RELEASE_NRM"

echo "Done: $RELEASE_NRM"
echo "Done: $DEBUG_NRM"
