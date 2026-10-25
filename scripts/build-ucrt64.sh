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

# An optional argument selects a separate SFML installation. The default is
# the MSYS2 package. Pin it explicitly so CMake cannot select some other copy.
if (( $# > 1 )); then
    echo 'Usage: bash scripts/build-ucrt64.sh [SFML-install-folder]' >&2
    exit 1
fi
sfml_prefix=$(cygpath -u "${1:-/ucrt64}")
if [[ ! -f "$sfml_prefix/lib/cmake/SFML/SFMLConfig.cmake" ]]; then
    echo "No SFML CMake package in: $sfml_prefix/lib/cmake/SFML" >&2
    echo 'Install UCRT64 SFML >= 3.1, or pass the prefix of a matching UCRT64 source build.' >&2
    exit 1
fi

# Windows needs the selected installation's DLLs for CTest as well as play.
# Prepending its bin folder prevents older package DLLs from winning on PATH.
export PATH="$sfml_prefix/bin:$PATH"
sfml_windows=$(cygpath -m "$sfml_prefix")
printf 'Selected SFML installation: %s\n' "$sfml_windows"
g++ --version
pacman -Q mingw-w64-ucrt-x86_64-sfml || true
cmake -S "$project_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_COMPILER=$compiler" \
    "-DCMAKE_PREFIX_PATH=$sfml_windows;$ucrt_prefix" \
    "-DAFTERLIGHT_SFML_ROOT=$sfml_windows"
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure
# Explorer does not inherit this terminal's PATH. Remember the exact Windows
# DLL directories in plain text so the batch launcher can recreate it later.
# These files stay inside the ignored build folder, never in Git.
printf '%s\n' "$(cygpath -w "$sfml_prefix/bin")" > "$build_dir/sfml-bin.txt"
printf '%s\n' "$(cygpath -w /ucrt64/bin)" > "$build_dir/ucrt-bin.txt"
printf '\nBuild complete. Run: "%s/afterlight.exe"\n' "$build_dir"
printf 'Or double-click scripts/run-ucrt64.bat in File Explorer.\n'
