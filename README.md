# AFTERLIGHT

A C++23 / SFML rooftop parkour game built to be played **and** understood.
Run through a neon city, chain jumps and air dashes, climb walls, collect
light, and reach the end of the skyline. The visuals are drawn in code; the
only bundled external asset is the openly licensed DejaVu Sans font.

![Afterlight: sunset rooftops and the start screen](docs/afterlight.png)

## Build and play

You need a C++23-capable compiler, CMake 3.20 or newer, and **SFML 2.5 or
2.6** development libraries. SFML 3 is a different API and is not supported
by this source code. GCC 13 or newer, recent Clang, or Visual Studio 2022
with a recent C++ toolset are sensible choices.

On Ubuntu or Debian:

```sh
sudo apt-get update
sudo apt-get install g++ cmake libsfml-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/afterlight
```

Run those commands from the folder containing this README. The first two
install development tools; the next two create the executable; the last
one starts the game. A desktop display is needed to play interactively.

On other systems, install SFML **2.x** using the official SFML installation
instructions, then use the same two CMake build commands. If CMake cannot
find the library, add `-DSFML_DIR=/path/to/SFML/lib/cmake/SFML` to the first
command. On Windows, use SFML binaries that match your compiler and its
architecture; a Visual Studio multi-configuration build also needs
`cmake --build build --config Release`, with the program usually at
`build/Release/afterlight.exe`. The matching SFML DLLs must be beside that
executable or on `PATH`. Some package managers default to SFML 3; explicitly
select a 2.x package rather than using the latest version blindly.

The build copies `assets/` beside the executable. Keep that folder with the
program when moving it. A source-folder fallback also allows development
runs from another working directory.

## Controls

| Key | Action |
| --- | --- |
| A / D or Left / Right | Run |
| Space / W / Up | Jump; hold for more height |
| Shift or X | Dash in the direction you face |
| R | Return to the current checkpoint |
| Escape | Pause / resume |
| Enter | Start / restart after finishing |
| M | Mute / unmute sound |
| F3 | Toggle performance information |

Jump toward a wall and press jump again to kick away from it. Touching the
ground restores your dash. Checkpoints keep a mistake from sending you all
the way back to the start. Collectibles are optional: first learn to reach
the finish, then try a faster, cleaner run.

## Checks

```sh
./build/afterlight --self-test
ctest --test-dir build --output-on-failure
```

These invoke the same built-in physics checks. They run without a window,
so a desktop session is not needed. They exercise gameplay rules rather
than only checking whether the project compiles. An interactive playthrough
is still useful for judging how the controls feel.

The rendering smoke test opens a window, runs scripted input for 900 fixed
simulation steps with sound muted, and exits. Screenshot mode renders a
representative gameplay view, saves it, and exits:

```sh
./build/afterlight --smoke-test
./build/afterlight --screenshot afterlight.png
```

On a Linux server without a monitor, install `xvfb` and run those windowed
checks with a virtual display instead:

```sh
xvfb-run -a ./build/afterlight --smoke-test
xvfb-run -a ./build/afterlight --screenshot afterlight.png
```

The screenshot is a local image file, not an interactive web version. The
game is a native desktop program.

Use `--mute` to start silently without opening an audio device, or combine
`--title-screen` with `--screenshot title.png` to capture the title artwork.
Enter while paused restarts the entire run; Escape resumes from that pause.

### This prepared cloud workspace

The cloud machine uses user-local libraries because system packages cannot
be installed as root here. They are already prepared outside the checkout:

```sh
source /workspace/.afterlight-tools/activate.sh
cmake -S /workspace/Project-alpha -B /workspace/Project-alpha/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /workspace/Project-alpha/build --parallel 2
ctest --test-dir /workspace/Project-alpha/build --output-on-failure
```

For local rendering checks in this machine, use an available virtual display
such as `DISPLAY=:99`. A virtual display proves that rendering works; to play
with a keyboard and see the live window, build on a desktop or connect your
own graphical desktop session. These cloud-specific paths are not required
when following the ordinary desktop installation above.

## A beginner's map of the code

Start with `include/World.hpp`: it describes the things in the game without
drawing them. Then read `src/World.cpp` to see how they move and interact.
Read `src/main.cpp` next to see how input, simulation, drawing, and time fit
together. `src/Renderer.cpp` turns the world into a picture; `src/Audio.cpp`
handles sound. `CMakeLists.txt` explains how the compiler combines the files.
The comments explain the purpose of each part and the less obvious math.

Some terms you will meet:

- A **variable** is a named piece of information, such as the player's speed.
- A **type** says what kind of information it holds: `float` is a number
  with a fractional part, and `bool` is either `true` or `false`.
- A **struct** groups related information, like a position and a velocity.
- A **function** is a named set of instructions. Its parameters are inputs;
  a return value, when present, is its output.
- A **class** keeps a group of data and functions together. `World` owns
  the playable level and the rules that update it.
- A **vector** in `std::vector` is a growable list. `sf::Vector2f`, despite
  the similar name, is one pair of coordinates: an x value and a y value.
- A **header** (`.hpp`) declares shared types and functions. A source file
  (`.cpp`) supplies their behavior.
- `std::` names the C++ standard library; `sf::` names SFML. Implementation
  files use `using namespace std;` where helpful. Headers keep the explicit
  prefix so they do not unexpectedly change names in every file including
  them.

SFML's screen coordinates start at the upper-left corner. Positive x moves
right, and positive y moves down. That is why gravity increases vertical
velocity while jumping initially makes it negative. The world is wider
than the window: the camera chooses which part of it you can see.

The main loop collects key presses, advances the simulation in small fixed
time steps, and draws the latest result. A fixed time step means physics
uses the same small slice of time even when drawing speed varies. Velocity
means distance per second; acceleration changes that velocity over time.

Parkour should feel forgiving. **Coyote time** briefly allows jumping after
leaving an edge. **Jump buffering** remembers a jump pressed just before
landing. **Variable jump height** lets releasing the key shorten a jump.
These small rules make a bigger difference than increasing top speed.

## Try a small change

Make one change at a time, rebuild, and play:

1. Find the movement constants in `src/World.cpp` and change running speed.
2. Change a platform's rectangle to move or resize a rooftop.
3. Change a color in `src/Renderer.cpp` to give the city a new palette.

Keep physics measurements in the simulation. Camera offsets and visual
effects belong in rendering, so changing the way a jump looks does not
accidentally change whether it clears a gap.

The bundled font's attribution and redistribution terms are in
`assets/LICENSE-font.txt`.
