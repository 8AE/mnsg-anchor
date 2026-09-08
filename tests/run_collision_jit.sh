#!/usr/bin/env bash
# Optional: requires a built Goemon64Recomp checkout with LiveRecomp libraries.
# Usage: bash tests/run_collision_jit.sh [matching package/build directory]
# Override MNSG_RECOMP_ROOT or MNSG_RECOMP_BUILD for other checkout/build paths.
set -euo pipefail
test_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
recomp_root="${MNSG_RECOMP_ROOT:-$(dirname "$test_root")/Goemon64Recomp}"
recomp_source="$recomp_root/lib/N64ModernRuntime/N64Recomp"
recomp_build="${MNSG_RECOMP_BUILD:-$recomp_root/build-vscode/lib/N64ModernRuntime/librecomp/N64Recomp}"
package_dir="${1:-$test_root/build}"
test_cxx="${CXX:-c++}"
if [[ -z "${CXX:-}" && "$(uname -s)" == Darwin ]]; then
    test_cxx=/usr/bin/clang++
fi
test_temp="$(mktemp -d "${TMPDIR:-/tmp}/mnsg-collision-jit.XXXXXX")"
trap 'rm -rf "$test_temp"' EXIT
"$test_cxx" -std=c++20 -Wall -Wextra \
    -I"$recomp_source/include" -I"$recomp_source/lib/rabbitizer/include" \
    -I"$recomp_source/lib/rabbitizer/cplusplus/include" \
    -I"$recomp_source/lib/fmt/include" -I"$recomp_source/lib/tomlplusplus/include" \
    "$test_root/tests/test_collision_jit.cpp" \
    "$recomp_build/libLiveRecomp.a" "$recomp_build/libN64Recomp.a" \
    "$recomp_build/librabbitizer.a" "$recomp_build/lib/fmt/libfmtd.a" \
    -o "$test_temp/test_collision_jit"
"$test_temp/test_collision_jit" "$package_dir/mod_syms.bin" \
    "$package_dir/mod_binary.bin" "$package_dir/mod.map"
