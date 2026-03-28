#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

python3 tools/gen_icon_headers.py

make clean && make "$@"
./RecompModTool mod.toml build

echo "Done: build/mnsg_recomp_mod_template.nrm"
