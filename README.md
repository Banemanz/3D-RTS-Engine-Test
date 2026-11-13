# 3D RTS Engine Test

A from-scratch, experimental **C++17/OpenGL 3.3/GLFW** RTS. The game currently uses
procedural meshes (units, structures and mineral patches) with a tiny built-in pixel-font HUD.
It is a testbed for core mechanics, **not a complete game engine**.

## Build and test

CMake 3.16+, a C++17 compiler, an OpenGL 3.3-capable driver and GLFW 3.4 are required.
The CMake build tries a system-installed GLFW first, falling back to a pinned GLFW 3.4
Git checkout when necessary.

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/RTSCameraTerrain
```

On Windows with Visual Studio CMake generators, the executable is usually at
`build/Debug/RTSCameraTerrain.exe`. The Linux/Windows headless test suite can be built
without GLFW or a display:

```sh
cmake -S "3D RTS (v0.3.10)" -B build-logic -DRTS_BUILD_DEMO=OFF
cmake --build build-logic --config Debug
ctest --test-dir build-logic -C Debug --output-on-failure
```

To capture a deterministic startup screenshot under software OpenGL (e.g. Mesa + Xvfb):

```sh
xvfb-run -a env LIBGL_ALWAYS_SOFTWARE=1 ./build/RTSCameraTerrain --smoke-frames 120 --screenshot rts.ppm
python3 tools/ppm_to_png.py rts.ppm rts.png
```

The same screenshot smoke test runs in GitHub Actions; download the screenshot artifact
from the Linux CI job. This is a renderer startup test, **not** a gameplay automation test.

## Playable skirmish controls

| Input | Action |
| --- | --- |
| Left-click / drag | Select own units or structures |
| Right-click enemy | Order selected units to attack |
| Right-click ground | Move selected units (A* route) |
| Right-click minerals with selected worker | Gather and return minerals repeatedly |
| A, then right-click | Attack-move command |
| B, then right-click | Place Barracks (selected worker nearby; 150 minerals) |
| T | Train a Marine at first completed Barracks (50 minerals) |
| C | Train a Worker at Command Center (40 minerals) |
| X | Stop selected units |
| Arrow keys / W S D | Pan camera (left pan uses Left Arrow; A reserved for attack-move) |
| Q / E | Rotate camera |
| Mouse wheel | Zoom |
| R | Restart match |
| Esc | Quit |

The player starts with a Command Center, four Marines, two Workers and 220 minerals.
Workers mine three finite deposits; Barracks take 9 seconds to construct; Marines and
Workers take time to train and consume supply. Enemy reinforcements attack periodically.
Destroy the enemy Hive to win; lose if your Command Center is destroyed.

## Project layout

- `3D RTS (v0.3.10)/`: main game source, platform integration, shader and rendering code.
- `NavigationGrid.*`: tested grid A* routing and obstacle occupancy.
- `RTSSkirmish.*`: ownership, simulation, minerals, workers, production, simple AI and win conditions.
- `tests/`: headless C++ unit/integration tests (no OpenGL context).
- `docs/`: architecture, controls, testing procedures and scope/limitations.
- `archive/new chat/`: handoff material for continuing this large PR in a future conversation.

This is **not a Godot runtime**. The unit stats parser accepts a small `.tres`-like subset
embedded in source; runtime loading, modding APIs, terrain heights, sound, proper object
models, fog of war, replay and networking remain future work.

Licensed under MIT: see `LICENSE`.
