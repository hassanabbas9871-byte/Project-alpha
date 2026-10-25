#!/usr/bin/env bash
# Run this file with bash from the MSYS2 UCRT64 terminal. A script is a list
# of commands; "set" makes it stop instead of hiding a failed build command.
set -euo pipefail

# MSYS2 has several different environments. UCRT64 packages belong together;
# combining them with MINGW64, MSYS, or an old Dev-C++ compiler breaks linking.
if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo "Open the MSYS2 UCRT64 terminal, then run: bash scripts/build-ucrt64.sh" >&2
    exit 1
fi

# Check the actual executable selected by PATH, not just the terminal's label.
for tool in g++ cmake ninja ctest; do
    location=$(command -v "$tool" || true)
    case "$location" in
        /ucrt64/bin/*) ;;
        *) echo "Expected UCRT64 $tool in /ucrt64/bin, found: ${location:-missing}" >&2; exit 1 ;;
    esac
done

# BASH_SOURCE finds this script even when the repository path contains spaces.
# Quoting variables keeps a spaced path as one argument instead of many words.
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$project_dir/build-ucrt64"
ucrt_prefix=$(cygpath -m /ucrt64)
compiler=$(cygpath -m "$(command -v g++)")

# Show versions for useful bug reports. CMake enforces SFML >= 3.1 below;
# pacman merely reports what the local MSYS2 package database has installed.
g++ --version
pacman -Q mingw-w64-ucrt-x86_64-sfml
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_COMPILER=$compiler" \
    "-DCMAKE_PREFIX_PATH=$ucrt_prefix"
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure
printf '\nBuild complete. Run: "%s/afterlight.exe"\n' "$build_dir"
