# Windows setup: SFML 3.1, MSYS2 UCRT64, and Embarcadero Dev-C++

This game uses C++23 and the **SFML 3.1 API**. Dev-C++ is your editor; GCC
is the program that compiles your code. Use the GCC installed in
`C:\msys64\ucrt64\bin`, together with libraries built for that same UCRT64
environment. Your MSYS2 installation may be in a different folder.

Do not combine the UCRT64 build with Dev-C++'s bundled TDM-GCC, a MINGW64
package, 32-bit libraries, or SFML binaries intended for Visual C++. A shared
file extension such as `.a` or `.dll` does not make those builds compatible.

## 1. Install or update the matching tools

Install MSYS2 from [msys2.org](https://www.msys2.org/) if necessary. Open the
Start-menu entry **MSYS2 UCRT64**. The prompt should say `UCRT64`.

```sh
pacman -Syu
```

If the update asks you to close the terminal, close it, reopen **MSYS2
UCRT64**, and run the update again. Then install these UCRT64 packages:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sfml
```

`pacman` is MSYS2's package installer. The long `mingw-w64-ucrt-x86_64-`
prefix selects the correct 64-bit compiler environment for every package.
Check what is actually installed:

```sh
echo "$MSYSTEM"
which g++ cmake ninja
g++ --version
pacman -Q mingw-w64-ucrt-x86_64-sfml
```

The first command should print `UCRT64`. The executable paths should begin
with `/ucrt64/bin/`. Use GCC 14 or newer, and SFML **3.1 or a later 3.x
version**. Package versions depend on your mirror and update state; if SFML
is older, follow the custom-source option below. Do not remove the CMake
version requirement to force an older API through the build.

## 2. Download the project

On the [GitHub repository](https://github.com/hassanabbas9871-byte/Project-alpha),
select **Code → Download ZIP**, then extract the archive. Or use Git if you
already have it installed. You need the entire project, including `src`,
`include`, `assets`, `scripts`, and `CMakeLists.txt`.

In UCRT64, change to the extracted project folder. For example:

```sh
cd "/c/Users/YourName/Downloads/Project-alpha-main"
bash scripts/build-ucrt64.sh
```

In this terminal, `/c/` means drive `C:`. Replace the example with your real
folder. Keep the quotation marks if your path includes spaces.

The helper configures CMake, builds all four `.cpp` files into one program,
copies the font assets, and runs the physics checks. It stops if the wrong
compiler environment is selected or a build/test fails. Output goes in a
separate `build-ucrt64` folder, so it cannot reuse an old SFML 2 build cache.

The helper now pins SFML to `/ucrt64` by default. If that package is only
3.0.2, configuration stops instead of silently finding a different 3.1
installation elsewhere. Use the custom-source instructions below in that case.

For an existing SFML 3.1 installation built with your matching UCRT64 GCC,
you can explicitly select it (quote paths containing spaces):

```sh
bash scripts/build-ucrt64.sh "/c/Program Files (x86)/SFML"
```

The script adds this installation's `bin` directory to `PATH` **before**
running tests. Its folder name does not prove its architecture or compiler
compatibility; if it came from a different compiler, build SFML from source
with your UCRT64 GCC instead. A successful compile alone does not establish
that all runtime DLLs are available.

## 3. Run the game

From the same UCRT64 terminal:

```sh
./build-ucrt64/afterlight.exe
```

Alternatively, double-click `scripts/run-ucrt64.bat` in File Explorer. It
adds `C:\msys64\ucrt64\bin` to the game's DLL search path. If MSYS2 is in a
custom folder, set the `MSYS2_ROOT` environment variable or edit the default
folder near the top of that batch file.

If you built with a custom SFML prefix, set `SFML_ROOT` to that same Windows
folder before using the batch launcher. From UCRT64 you can instead run:

```sh
export PATH="/c/Program Files (x86)/SFML/bin:$PATH"
./build-ucrt64/afterlight.exe
```

Keep the `assets` folder beside `afterlight.exe`. The build copies it for
you. This development launcher expects the installed UCRT64 runtime; moving
only the EXE to a different laptop does not create a standalone release.

If sound is unavailable, start with:

```sh
./build-ucrt64/afterlight.exe --mute
```

## 4. Use Embarcadero Dev-C++

Open the `.cpp` and `.hpp` files in Dev-C++ to read and edit them. Then save
and run `bash scripts/build-ucrt64.sh` in your UCRT64 terminal. This is the
recommended workflow: it uses the supplied CMake build instead of depending
on a particular Dev-C++ version's project-file format.

If you configure a compiler set in Dev-C++, point its compiler directory to
`C:\msys64\ucrt64\bin` and its C++ executable to `g++.exe` in that folder.
An old Dev-C++ preset may still insert old include/library directories or
an older `-std=` setting. The CMake helper avoids those presets and requires
C++23 explicitly. Menu labels differ between Dev-C++ releases, so use the
UCRT64 helper as the reference build even when configuring IDE integration.

Do not use “compile current file” on `main.cpp`: that omits `World.cpp`,
`Renderer.cpp`, and `Audio.cpp` and produces undefined-reference errors.
CMake builds and links all of them and supplies the required SFML libraries.

## Optional: build SFML 3.1 with your UCRT64 GCC

Use this when your MSYS2 mirror still provides SFML 3.0/2.x, or when your
existing downloaded SFML binaries were built for a different compiler.
Building SFML itself requires **CMake 3.28 or newer** (the game alone needs
3.20). The current UCRT64 CMake package should supply that; check
`cmake --version` first. Download the **3.1.0 source archive** from the
[official SFML releases](https://github.com/SFML/SFML/releases), extract it,
and run the following from UCRT64. Adapt the source path to the extracted
folder. SFML's own CMake messages will identify any missing development
packages; install their **UCRT64** variants.

```sh
cmake -S "/c/path/to/SFML-3.1.0" -B "/c/path/to/SFML-3.1.0/build-ucrt64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=C:/sfml-3.1-ucrt64 -DBUILD_SHARED_LIBS=ON \
    -DSFML_BUILD_EXAMPLES=OFF -DSFML_BUILD_TEST_SUITE=OFF -DSFML_BUILD_NETWORK=OFF
cmake --build "/c/path/to/SFML-3.1.0/build-ucrt64" --parallel 2
cmake --install "/c/path/to/SFML-3.1.0/build-ucrt64"
```

From the game's project folder, configure a **different, empty build
folder** against this install. Set the new DLL path before both CTest and
the game, so Windows does not load an older package DLL:

```sh
export PATH="/c/sfml-3.1-ucrt64/bin:$PATH"
cmake -S . -B build-sfml31 -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=C:/sfml-3.1-ucrt64
cmake --build build-sfml31 --parallel 2
ctest --test-dir build-sfml31 --output-on-failure
./build-sfml31/afterlight.exe
```

The supplied `run-ucrt64.bat` launches the package-based `build-ucrt64`
workflow, so use the command above for this custom `build-sfml31` folder.
Both SFML's `bin` and UCRT64's `bin` must remain on `PATH` while running.

## Troubleshooting

| Message or symptom | What to check |
| --- | --- |
| `SFML 3.1` not found | Check `pacman -Q mingw-w64-ucrt-x86_64-sfml`. If it is 3.0.2, build 3.1 from source with UCRT64 GCC. For a custom install pass its prefix to the helper. |
| CTest exits `0xc0000135` | Windows could not load a required DLL; the test did not reach game code. Put the selected SFML installation's `bin` on `PATH` before testing and running. A transitive dependency may also be missing. Run `./build-ucrt64/afterlight.exe --self-test` directly to see whether Windows names the missing DLL. |
| Unknown `-std=c++23` | The selected compiler is too old. Check `which g++` and select the current UCRT64 GCC. |
| `SFML/Graphics.hpp` not found | Build through CMake and install the UCRT64 SFML development package; do not compile one file in isolation. |
| Missing `sfml-*.dll`, `libstdc++-6.dll`, or another DLL | Run inside UCRT64 or use the batch launcher. Check the SFML and GCC DLL folders on `PATH`. |
| “Procedure entry point” missing or bad image | Old/incompatible DLLs beside the EXE override `PATH`; remove only DLLs you placed there and rebuild with a consistent UCRT64 toolchain. |
| Undefined references | Include all four source files through CMake; do not mix SFML 2 libraries with SFML 3 headers. |
| CMake mentions an old compiler/SFML path | Use a fresh build folder. Configuration caches remember earlier choices. |

The source is portable, but Linux testing does not verify your particular
Windows installation. If a Windows command fails, share its complete error
and the `g++ --version` / `pacman -Q` outputs; do not post credentials.
