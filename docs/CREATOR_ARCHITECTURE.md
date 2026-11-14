# Creator architecture, Galaxy Editor references and native UI roadmap

This document is the engineering plan for the existing **draft PR #1**. Keep the project on the same Windows/Linux C++17/OpenGL source path. Do not switch engines or build unrelated editor implementations.

## Galaxy Editor as the feature reference

StarCraft II's editor uses connected Terrain, Data, Triggers, UI, Importer, Cutscene, AI and script modules. Its Data Editor has approximately 87 catalogs, but parity depends on the **relationships and authoring capabilities**, not merely matching its category count.

- [Editor modules](https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/003_Navigating_the_Interface/)
- [Data Editor and catalogs](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/)
- [Unit composition](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/059_Units/)
- [Actor event/presentation separation](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/060_Actors/)
- [Terrain layers](https://s2editor-guides.readthedocs.io/New_Tutorials/02_Terrain_Editor/019_Terrain_Editor_Interface/)
- [Recoil RTS / Spring architecture](https://recoilengine.org/) and [0 A.D. / Atlas editor](https://play0ad.com/) are design references, not sources to copy GPL code from.

## What exists today

The native gameplay HUD is already a custom engine OpenGL renderer with a shared pure-C++ layout/hit-test model. The first **native pause menu** now uses `NativePauseMenu.hpp` for pure layout/input and `NativePauseMenuGL.hpp` for the engine renderer: resume, restart, enter editor and exit, with the simulation/world input suspended. No extra GUI libraries were added.

The existing editor workbench **still uses pinned Dear ImGui** as a presentation layer. Keep it functional until our own implementation matches its behavior. The editable project, simulation, terrain, catalogs, triggers, scene placements and undoable `EditorSession` commands are all headless C++ and can be connected to the native system without rewriting data or gameplay.

The Data Editor now has parent/Unit/Actor/Model/trainable-object navigation, recognized incoming reference lists, local-vs-inherited field indicators and validated typed boolean/numeric/string editing. This is a foundation, **not** an SC2-equivalent object editor.

## Engine-owned retained UI: required primitives

1. **Event routing:** stable widget IDs, explicit focus and pointer capture, keyboard navigation, text composition, key repeat, modal focus scope, drag/drop and disjoint GUI/world input.
2. **Layout:** anchors, flex-like rows/columns, min/max size, fractional and DPI scaling, scroll clipping, dock/split tabs, z-order and persisted workspace arrangements. Compute geometry headlessly.
3. **Rendering:** engine-owned batched textured/solid geometry and a glyph-atlas font; preserve OpenGL state across UI/world rendering. Current 5×7 bitmap glyphs are bootstrap-only, not a production Unicode text system.
4. **Widgets:** menus, toolbar, tree, sortable/virtualized table, selector, searchable catalog browser, property field, vector/color picker, popup, tooltip, graph canvas, node ports, text editor, undo status and accessibility affordances.
5. **Data binding:** widgets send validated commands through `EditorSession`, not ad hoc edits to raw model pointers. Transactions are reversible. Inherited fields must distinguish an override from an inherited value.

### UI migration gates

- **G0: done:** native pause, existing custom gameplay HUD, pure layout/hit tests; shared binary feature flags and Linux GPU screenshot smoke.
- **G1:** shared `NativeUI` tree and text/input rendering infrastructure with focus/scroll/clip, headless geometry tests and real screenshot comparison.
- **G2:** build native editor toolbar, catalog browser and inspector while retaining a switchable existing adapter. Parity means working rename, derive, delete, link navigation, field editing and undo/redo.
- **G3:** migrate terrain/scene/trigger panels and true dockable workspace with property schemas; only then remove ImGui FetchContent/backends and associated license from packaging.
- **G4:** extend creator systems, not just UI: ability/effect/behavior/mover/weapon editors, actor/event graph, script debugger, imported asset previews, gameplay UI/layout editor, cinematic timeline and campaign tools.

## Engine foundation needed for actual Galaxy-like authoring

**Catalog schemas and object dependency graph.** Add per-type field descriptors (type, range, enum, allowed reference class, default, inherited/local provenance, mod extension fields). Index forward/reverse references. Renames and deletes must either migrate references transactionally or fail safely.

**Abilities and effects.** Define a bounded, deterministic effect executor: target selection, validation, set/search effect sequences, damage/heal/resource transfers, spawn, modifiers and order dispatch. Keep authored visual actors distinct from authoritative simulation mutations. Test on headless data fixtures, not OpenGL.

**Visual actors/events.** Simulation emits stable semantic events for animation, sound, particles, projectiles, attachments and death; renderer-owned actor presentation consumes these without deciding damage. Actor models/animation state and import pipeline are not yet implemented.

**Movement/visibility.** Independent terrain categories and path layers for ground/flying/cliffs/bridges, footprints, dynamic obstacles, local avoidance, static geometry indexing, radar and cloak/detection are still missing. Preserve the fixed timestep and stable unit identity.

**Triggers and script tooling.** Move from raw ECA JSON to typed, composable Event/Condition/Action nodes with scopes, variable/function libraries, validation errors, scheduling constraints, debugging and source locations. Design bounded instruction/effect budgets.

**Map / UI authoring.** Scene hierarchy, transform gizmos, prefab/actor instances, terrain layers, doodads, play-in-editor, visual layouts, localization, searchable import database, hot reload, project migrations, crash recovery and editable diffs.

## Cross-platform release contract

- Windows x64 and Linux x86_64 compile the **same targets and feature switches**, validated using the actual packaged executable's `--features` output and `tools/verify_build_bundle.py`.
- Both run the same headless CTest suite (simulation, terrain, authoring, UI hit-testing) in Debug and Release; keep assertions enabled in Release test targets.
- Linux runs Mesa/Xvfb real OpenGL startup, production HUD, native pause-menu screenshot, scene/data/terrain/trigger workspaces and editor lifecycle stress against the packaged executable.
- Windows compiles/runs identical code and tests. GitHub Windows CI does not prove interactive Windows GPU stability. Manual driver testing is required before marking the creator suite stable.
- Keep the work in PR #1, keep it a draft and update `archive/new chat/RTS_HANDOFF.md` whenever a major implementation stage changes.

This project is **not** currently a clone of StarCraft II's editor. The priority is a robust and extensible engine/authoring architecture so those features can be implemented incrementally without separate platform forks.


## First graph execution milestone (implemented after native UI)

Authoring now recognizes `Effect` object catalogs with typed operations: Damage, Heal, Sequence. Graph validation enforces cycle detection and bounded complexity; editor links, incoming references, rename/delete migration and region trigger execution are integrated. This is a **headless C++ interpreter**, not copied from Galaxy or Recoil. Next design work: unit ability catalogs, target filters and effect contexts, scheduled behaviors, actor events and gameplay/perception-specific UI widgets.


## First visual ECA authoring milestone

The Trigger Editor now has a true event/condition/action inspector. Edit event kind and parameters, one-shot state, add or reorder typed conditions/actions, edit Unit/Region/Effect references with dropdowns, and use advanced JSON for more complex/extension data. `TriggerDefinition` supplies a single compiled rule validation authority for both authoring and simulation. Invalid edits are transactional and do not corrupt the open project. It is still rendered by the existing workbench adapter while the engine-native widget tree/docking system is developed. Implementation and next-stage constraints: `TRIGGER_EDITOR.md`.
