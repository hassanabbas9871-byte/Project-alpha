#!/usr/bin/env bash
# Run in the MSYS2 UCRT64 terminal. Stop on a failed command instead of
# pretending that a partially completed build is ready to play.
set -euo pipefail

if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Open MSYS2 UCRT64, enter the physics-lab folder, then run:' >&2
    echo 'bash scripts/build-ucrt64.sh' >&2
    exit 1
fi
if (( $# != 0 )); then
    echo 'This helper uses your installed UCRT64 raylib package; it takes no arguments.' >&2
    exit 1
fi

# Check what PATH actually selects. A terminal can be labelled UCRT64 while
# still selecting an old compiler from another installation earlier on PATH.
for tool in g++ cmake ninja ctest; do
    location=$(command -v "$tool" || true)
    case "$location" in
        /ucrt64/bin/*) ;;
        *) echo "Expected /ucrt64/bin/$tool, found: ${location:-missing}" >&2; exit 1 ;;
    esac
done

# Quotes make paths with spaces stay together as one argument.
lab_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$lab_dir/build-ucrt64"
ucrt_prefix=$(cygpath -m /ucrt64)
compiler=$(cygpath -m "$(command -v g++)")

g++ --version
pacman -Q mingw-w64-ucrt-x86_64-raylib
cmake -S "$lab_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_COMPILER=$compiler" \
    "-DCMAKE_PREFIX_PATH=$ucrt_prefix" \
    "-DFLUX_RAYLIB_ROOT=$ucrt_prefix"
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure

# Explorer does not inherit UCRT64's PATH. Record this actual installation's
# native Windows DLL folder for the batch launcher, inside the ignored build.
printf '%s\n' "$(cygpath -w /ucrt64/bin)" > "$build_dir/ucrt-bin.txt"
printf '\nReady: "%s/flux.exe"\n' "$build_dir"
printf 'From Explorer, double-click physics-lab/scripts/run-ucrt64.bat.\n'
