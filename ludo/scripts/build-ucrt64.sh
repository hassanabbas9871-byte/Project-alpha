#!/usr/bin/env bash
# Run in MSYS2 UCRT64. A failing command stops the helper immediately,
# so an old executable is never mistaken for a successful new build.
set -euo pipefail

if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Open MSYS2 UCRT64, enter the ludo folder, then run:' >&2
    echo 'bash scripts/build-ucrt64.sh' >&2
    exit 1
fi
if (( $# != 0 )); then
    echo 'This helper uses the installed UCRT64 raylib package; it takes no arguments.' >&2
    exit 1
fi

# Dev-C++ may have its own older compiler. Check actual executable paths,
# not just the terminal label, before configuring a cached CMake build.
for tool in g++ cmake ninja ctest; do
    location=$(command -v "$tool" || true)
    case "$location" in
        /ucrt64/bin/*) ;;
        *) echo "Expected /ucrt64/bin/$tool, found: ${location:-missing}" >&2; exit 1 ;;
    esac
done

# Quotes preserve folder names containing spaces. cygpath converts MSYS2
# paths to native Windows paths understood by the compiler and CMake.
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
    "-DLUDO_RAYLIB_ROOT=$ucrt_prefix"
# ZIP extraction can preserve timestamps from an earlier download. Compile all
# source files again so an old text renderer cannot survive in a cached object.
cmake --build "$build_dir" --clean-first --parallel 2
ctest --test-dir "$build_dir" --output-on-failure

# File Explorer does not inherit the terminal's DLL search path. Save the
# successful toolchain's folder so the batch launcher can restore that path.
printf '%s\n' "$(cygpath -w /ucrt64/bin)" > "$build_dir/ucrt-bin.txt"
printf '\nReady: "%s/ludo.exe"\n' "$build_dir"
printf 'From Explorer, double-click ludo/scripts/run-ucrt64.bat.\n'
