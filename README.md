# 3D RTS Engine Test

Experimental **from-scratch C++17 + GLFW + OpenGL 3.3** RTS foundation and
in-progress **creator suite**, inspired by the broad scope of WC3 World Editor /
StarCraft II Galaxy Editor. This is an independently developed engine, not a
StarCraft/Galaxy-compatible game data loader.

## Build and run

CMake 3.16+, C++17 compiler, GPU/OpenGL 3.3 driver and GLFW 3.4 or installed
system GLFW required. The build downloads GLFW 3.4 if not installed locally.

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/RTSCameraTerrain
./build/RTSCameraTerrain --editor --project projects/terrain_showcase.rtsproject.json
./build/RTSProjectTool --validate projects/default.rtsproject.json
```

For Visual Studio multi-configuration generators use
`build/Debug/RTSCameraTerrain.exe` and `build/Debug/RTSProjectTool.exe`.
To compile only the logic, no OpenGL dependencies are needed:

```sh
cmake -S "3D RTS (v0.3.10)" -B build-logic -DRTS_BUILD_DEMO=OFF
cmake --build build-logic --config Debug
ctest --test-dir build-logic -C Debug --output-on-failure
```

## Editor

**F5** terrain editor, **F7** Data Editor, **F9** play/test current map,
**Ctrl+S** save project.

The first editor includes raise/lower/smooth/flatten brushes, material
painting, blocked-path overlays, unit/mineral/doodad placement, in-window
property editor, inheritance-based unit/actor/model catalogs, JSON project
serialization, editor test screenshots, and a headless CLI.
Trigger JSON can define a few events/conditions/actions which run at
map start or during play. An example project with sculpted terrain and
triggers is `projects/terrain_showcase.rtsproject.json`.

For the full controls and map-code format see [Authoring guide](docs/AUTHORING.md).
For a sober, extensive SC2-scope comparison of what is implemented versus what
still needs to be developed, see [Creator suite matrix](docs/SC2_FEATURE_MATRIX.md).

## Gameplay

- Left-click/drag/Shift-click selects friendly units.
- Right-click terrain moves; right-click enemy attacks; Workers right-click
  minerals to gather.
- **A** then right-click attack-moves; **Shift+right-click** queues more orders.
- **B** then right-click places Barracks with a selected Worker nearby;
  **T** queues Marines, **C** queues Workers.
- **X** stops units; **Ctrl+1–9** creates control groups; **1–9** recalls.
- **W/S/D + arrows** pans, **Q/E** rotates, mouse wheel zooms.
- **R** restarts skirmish and **Esc** exits.

The prototype is a single offline skirmish with procedural graphics, simplified
economy, basic enemy waves and no model/animation/sound import yet. Arbitrary
catalog fields are stored and inherited but *only properties wired to runtime
systems have actual gameplay/presentation effects*.

## Engineering

- `RTSSimulation`: fixed-tick unit combat, A* orders, production, economy,
  trigger dispatcher and win/loss rules.
- `RTSAuthoring`: schema-validated project, heightfield terrain,
  generic unit/actor/model catalogs and edit-session state.
- `projects/`: plain editable JSON projects with version and extension fields.
- `tools/rts_project_tool.cpp`: CLI for project/catalog/scene editing.
- `tests/`: CTest executable suites (no GPU required).
- `.github/workflows/ci.yml`: full Linux/Windows builds and logic tests;
  Linux Mesa/Xvfb checks both game and terrain editor, uploads screenshots.
- `archive/new chat/RTS_HANDOFF.md`: cross-chat handoff.

MIT licensed. Vendored PicoJSON has its own BSD-style license at
`3D RTS (v0.3.10)/third_party/PICOJSON_LICENSE`.
