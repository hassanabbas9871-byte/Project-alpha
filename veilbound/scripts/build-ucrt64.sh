#!/usr/bin/env bash
# Run this from MSYS2 UCRT64. Stop at the first failed command instead of
# launching a stale program after an unsuccessful build or test.
set -euo pipefail

if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Open MSYS2 UCRT64, enter the veilbound folder, then run:' >&2
    echo 'bash scripts/build-ucrt64.sh' >&2
    exit 1
fi
if (( $# != 0 )); then
    echo 'This helper uses the installed UCRT64 raylib package; it takes no arguments.' >&2
    exit 1
fi

# The shell label alone does not prove the correct compiler is selected.
# Reject older Dev-C++ or another MSYS environment earlier on PATH.
for tool in g++ cmake ninja ctest; do
    location=$(command -v "$tool" || true)
    case "$location" in
        /ucrt64/bin/*) ;;
        *) echo "Expected /ucrt64/bin/$tool, found: ${location:-missing}" >&2; exit 1 ;;
    esac
done

# Quoting every path keeps spaces in your Windows account/folder name safe.
game_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$game_dir/build-ucrt64"
ucrt_prefix=$(cygpath -m /ucrt64)
compiler=$(cygpath -m "$(command -v g++)")

g++ --version
pacman -Q mingw-w64-ucrt-x86_64-raylib
cmake -S "$game_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_COMPILER=$compiler" \
    "-DCMAKE_PREFIX_PATH=$ucrt_prefix" \
    "-DVEIL_RAYLIB_ROOT=$ucrt_prefix"
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure

# Explorer does not inherit your terminal's DLL search path. The launcher
# reads this saved native Windows path and restores it for the game process.
printf '%s\n' "$(cygpath -w /ucrt64/bin)" > "$build_dir/ucrt-bin.txt"
printf '\nReady: "%s/veilbound.exe"\n' "$build_dir"
printf 'From Explorer, double-click veilbound/scripts/run-ucrt64.bat.\n'
