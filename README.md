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
./build/RTSCameraTerrain --trigger-editor --project projects/terrain_showcase.rtsproject.json
./build/RTSCameraTerrain --scene-editor --project projects/terrain_showcase.rtsproject.json
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

**Download the actual executable:** [GitHub Actions → Download Windows EXE and Linux RTS Builds](https://github.com/Banemanz/3D-RTS-Engine-Test/actions/workflows/build.yml). The **WINDOWS-x64-EXE** artifact is a single ZIP containing `RTSCameraTerrain.exe` and launch scripts—extract once and run. **Tests + screenshots** is a separate workflow and does not distribute executables. [Exact download instructions](docs/BUILD_ARTIFACTS.md).

## Editor

**F5** terrain editor, **F6** Trigger Editor (JSON inspector),
**F7** Data Editor, **F8** Scene Editor, **F9** play/test current map,
**Ctrl+S** save project.

The first editor includes raise/lower/smooth/flatten brushes, material
painting, blocked-path overlays, unit/mineral/doodad placement, in-window
property editor, inheritance-based unit/actor/model catalogs, JSON project
serialization, editor test screenshots, and a headless CLI.
Trigger JSON can define a few events/conditions/actions which run at
map start or during play. Placed units can override their own gameplay
and actor/model properties without altering their shared catalog. An example project with sculpted terrain and
triggers is `projects/terrain_showcase.rtsproject.json`.

For the full controls and map-code format see [Authoring guide](docs/AUTHORING.md).
For a sober, extensive SC2-scope comparison of what is implemented versus what
still needs to be developed, see [Creator suite matrix](docs/SC2_FEATURE_MATRIX.md).

## Gameplay HUD

Play mode now has a proper RTS-style resource bar, clickable tactical minimap,
selection details with HP, and a 3×3 command grid. Buttons and keyboard
shortcuts share the same commands; the HUD consumes pointer events separately
from the world. The first iteration uses procedural UI geometry pending a
proper texture/icon/theme and localization pipeline.

## Gameplay

- Left-click/drag/Shift-click selects friendly units.
- Right-click terrain moves; right-click enemy attacks; Workers right-click
  minerals to gather.
- **A** then right-click attack-moves; **Shift+right-click** queues more orders.
- **B** then right-click places Barracks with a selected Worker nearby;
  **T** queues Rangers, **C** queues Workers.
- **X** stops units; **P** then right-click orders repeating patrol routes.
- **F2** selects combat army; **H** selects/focuses the command center.
- **Ctrl+1–9** creates control groups; **1–9** recalls.
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

## Fog of war

Gameplay now has **terrain-aware fog of war**. Your units reveal terrain within
their editable `sight_range`, with high hills blocking ground sight.
Previously visited terrain stays explored, but enemy units are only rendered,
shown on the minimap or clickable while currently visible. The map editor
continues to reveal the full scene for authoring. See
[authoring](docs/AUTHORING.md) for `sight_range` overrides and
[engine status](docs/ENGINE_STATUS.md) for remaining radar/stealth limitations.

## Simulation scaling

The engine now maintains an incremental spatial grid each fixed simulation
tick. Local unit separation, enemy acquisition and ID-targeting no longer
require all-pairs scans. Ownership is independent from visual faction so
modders can create enemy factions using the same assets. The spatial
broadphase is deterministic and covered by a 2,000-unit regression test;
it is not a full physics/formation system.

## Data catalog browsing

In the F7 Data Editor, use **Ctrl+F** to filter by catalog object name,
ID or type. Navigate results with Tab/Shift+Tab and clear with
**Ctrl+Shift+F**. Property editing, derived objects and per-instance
overrides remain supported.

## Group selection HUD

Selecting multiple units now displays clickable individual cards with
health stripes rather than just a single total-HP display. Click a card
to isolate a unit, or Shift+click to remove it from the selection. UI
hitboxes remain separate from world commands. CI includes a selected-army
OpenGL screenshot and a searchable Data Editor screenshot.

## Tactical command refinements

The gameplay HUD now has a **4×3 command grid** with Hold Position (G),
F3 worker selection, and clickable cancel. Held units stay in place but shoot
enemies already in weapon range. Attack-move and patrol routes resume their
intended destinations after combat interruption rather than skipping
waypoints. See [controls](docs/CONTROLS.md).

## Creator catalog ID rename

F7 Data Editor now supports **Ctrl+R** to rename a selected catalog ID,
migrating recognized inheritance, Unit/Actor/Model, placed-unit, instance,
skirmish-role metadata and trigger ECA references in one undoable action.
Arbitrary extension strings are intentionally not rewritten. CLI equivalent:
`RTSProjectTool --rename project.rtsproject.json old-id new-id`.

## Group movement formation slots

Selecting multiple mobile units and issuing Move, Attack-Move, or Patrol now
assigns **separate destinations** in a compact formation instead of
concentrating the group on one destination cell. The layout is deterministic
even when a unit's position in the source list changes. Project authors may
tune `formation_spacing_multiplier` under JSON `metadata`; default
`2.4`, accepted value range `1.5`–`6.0`. Navigation still finds
walkable paths for each unit independently.

## Data-driven production / context commands

Unit catalog definitions can now declare `trainable_units` (up to 12 Unit IDs)
on buildings. Each building trains its own authorized unit types using the
unit's catalog costs, supply, build duration, actor and worker flag; queues,
cancellation and rally points are supported. Select exactly one producer to
access contextual B/T/C training commands; **Tab** pages through more than three
types. Legacy commands still apply with no producer selected. The F7 Data
Editor edits these lists as JSON. See docs/AUTHORING.md for validation and
project format.

### Try the working production creator showcase

The bundled `projects/production_showcase.rtsproject.json` has a prebuilt
Fabrication Bay with **six creator-defined trainable unit types**, two pages of
B/T/C buttons, and a second producer with a different unit list. Launch
`run-production-lab.bat` on Windows or `./run-production-lab.sh` on Linux.
Select the Fabrication Bay (gold-tinted building), train units, set a rally
point, cancel production with V, or open F7 to modify the catalog live.
The CI OpenGL smoke test captures the *actual contextual production HUD* with
`--smoke-select-producer`, not a generated mockup.

## Integrated creator workbench (Windows + Linux)

The game and editor now share **Dear ImGui docking** (pinned v1.91.9b-docking).
The editor no longer relies on the original full-screen keyboard-only debug
text. Open any executable's **Workspace** menu, switch between Play, Terrain,
Data, Triggers and Scene, and use dockable **Browser** and **Inspector** panels
with mouse-driven controls. F5–F9 continue to work as keyboard shortcuts.
World painting and unit commands are gated while GUI widgets capture input.

The release workflow validates the *native executable's* `--features` output,
including its embedded commit SHA, before uploading either platform package.
Run `RTSCameraTerrain.exe --version` on Windows (or `./RTSCameraTerrain --version`
on Linux) to check whether an older build has been launched. Both packages
use the same creator GUI, gameplay HUD, fog, terrain and editor source.


## Engine-native gameplay pause menu (November 14, 2026)

In play, **Esc** opens a custom engine-rendered pause screen with Resume, Restart, Open World Editor and Exit. Gameplay simulation and world commands stop while this modal is open. The native layout and button hit-testing have no GLFW/ImGui dependency, and the OpenGL UI adapter reuses the game's own HUD resources. The existing creator workbench continues using pinned ImGui **temporarily**; the full retained-mode replacement roadmap and Galaxy Editor architecture are documented in [docs/CREATOR_ARCHITECTURE.md](docs/CREATOR_ARCHITECTURE.md).

The F7 Data Editor also supports clickable Parent / Unit / Actor / Model / trainable references, incoming-reference inspection, inherited/local property indicators and transactional boolean, numeric and text editing. Both Windows and Linux feature manifests include `RTS_FEATURE_NATIVE_PAUSE_MENU=1`.


## Original starter roster and scalable Data Editor (2026-11-14)

The included starter data now uses **Frontier Ranger** (`ranger`), **Rift Raider** (`rift_raider`) and **Rift Bastion** (`rift_bastion`) as original example units/structures. These are starter templates, not mandated engine types. Existing serialized projects with older IDs remain loadable; the engine resolves role fallbacks from the embedded catalog. The F7 Data Editor now has a dynamic type/family selector (including custom mod-defined families) and clipped object-list rendering to keep large catalogs navigable. See `docs/AUTHORING.md` and the `archive/new chat` handoff.


## New authored effect demo

The engine now exposes the beginning of an original **data-driven effect graph**: `Effect` catalog nodes with Damage, Heal and Sequence, validated for cycles and complexity, and a bounded `ApplyEffect` ECA action for the unit entering a map Region. Use `--project projects/effects_showcase.rtsproject.json` to play the included map, or open it in the editor with `--editor --project ...` and inspect the Effect catalog, region and trigger. There is no SC2 content dependency. This is not yet a complete ability/weapon/behavior graph.


## Visual Event–Condition–Action editor (November 14, 2026)

The F6 Trigger Editor now has a functional ECA form/tree: choose event types, configure intervals, regions and once/repeat; add conditions and ordered actions; edit typed numeric/text/catalog fields; reorder/remove nodes; and open advanced JSON when needed. Invalid rule modifications are rejected before changing the project, using **one shared parser with the runtime**, with undo/redo for each committed change. The authored Effect catalog can be selected for region-entering unit effects. No extra GUI dependency was introduced. Read [docs/TRIGGER_EDITOR.md](docs/TRIGGER_EDITOR.md) for the complete workflow and supported event/action list.


## Mouse-editable catalog reference lists (November 14)

F7 Data Editor now provides a visual list editor for **trainable units** on Unit catalogs and **Effect sequence children** on Effect catalogs. Select the corresponding field in the Inspector to open, add, remove, and reorder referenced objects without writing JSON. The `Open` action follows an object link into the same Data Editor. All mutations call `EditorSession::setCatalogField`, so the shared validators reject invalid, duplicate, cyclic, or out-of-range changes atomically and one undo covers each successful edit. No new GUI dependency was introduced.
