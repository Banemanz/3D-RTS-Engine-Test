# Creator-suite feature matrix — StarCraft II Galaxy Editor as a reference

This is a design comparison, **not** an assertion that the RTS engine already has
StarCraft II's editor capabilities or compatible SC2 game data. The intent is to
build an independent engine that exposes every simulation and presentation
parameter to creators through versioned, inspectable, editable projects.

Reference documentation:

- [Data Editor Introduction — 87 data catalogs](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/)
- [Terrain Editor Introduction — seven layers](https://s2editor-guides.readthedocs.io/New_Tutorials/02_Terrain_Editor/018_Terrain_Editor_Introduction/)
- [Trigger Editor Introduction — events, conditions and actions](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/032_Trigger_Editor_Introduction/)
- [SC2 Editor navigation — modules](https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/003_Navigating_the_Interface/)
- [Blizzard SC2 5.0 patch notes — Data Collections](https://news.blizzard.com/en-us/article/23482838/starcraft-ii-5-0-patch-notes)

## Definition of editor freedom

Every object must be represented by data with:
1. a stable ID and catalog/type, with inheritance or composition;
2. typed/editable properties and serialized per-object instance overrides;
3. references to **separate** visual/sound/animation assets;
4. patch/override layers so map mods can replace individual parameters, not entire game binaries;
5. schema/versioned migration and validation;
6. runtime adapters for supported properties and editor previews for unimplemented ones;
7. a scripting API with safe permission limits and debugging/profiling;
8. save/load, undo/redo and configurable game project templates.

Storing an arbitrary `fields` key does **not** mean the runtime supports it yet.
An extension property is currently preserved through save/load, but unimplemented
properties have no gameplay behavior. A limited number of gameplay/actor/model
fields now resolve on individual placed instances.

## Major authoring modules

| Module | Current PR state | Full creator-suite objective |
| --- | --- | --- |
| **Terrain & pathing** | Heightfield sculpt, smooth/flatten, 8 material IDs, blocked-cell pathing, slope-aware weighted A* and brush preview; 49×49 starter | Multiple texture layers, cliffs/ramps, water, roads, decals, foliage, cliffs meshes, destructibles, per-unit movement types, map resizing, symmetry, procedural generation |
| **Unit / doodad placements** | JSON instances, placement brushes, owner/catalog links; gameplay, actor and model overrides on placed instances, persistent transforms | Entity selection/gizmos, multi-select, snapping, placement rules, brushes, prefab variants, fully interactive transform editing |
| **Points / regions / cameras** | F8 scene object workspace, Point/Region placement and radius, selected-object movement/rotation/scale, bounded region-enter trigger; editor camera preset | 2D/3D region shape tools, trigger gizmos, actual camera objects/curves, cinematics and camera transitions |
| **Data Editor** | Arbitrary JSON typed catalogs, ID inheritance, inspector, clone/derived entries, CLI, validation | Full schema-directed inspector, reference graphs, search, inline validation, copy/duplicate with remapped references, variants, data collections, live update without resetting map |
| **Trigger Editor / code** | Bounded event-condition-action JSON runtime with basic events, conditions and actions; F6 browse/create/clone/edit-rule JSON inspector | Visual ECA graph, variables/functions, branches, loops, event dispatch, coroutine/waits, script debugging, profiling, source-language/DSL or sandboxed VM, mod project APIs |
| **Models & Actors** | Separate `Model` and `Actor` catalog IDs and placeholder procedural rendering; actor tint/scale/opacity fields | glTF/GLB content pipeline, skeletal meshes, materials, LODs, sockets/attachments, actor event messages, animation state machine, particles and decals |
| **Audio / VFX / animation** | String fields preserved in catalogs, no runtime loaders | Sound banks, 3D positioning, reverb, footsteps, music, animation clips, event mapping, particle authoring, shader/material editor |
| **Abilities & combat** | Combat fields and simple direct damage, unit state machine | Abilities, effects graphs, validators, targeting filters, projectiles/movers, behaviors, buffs/debuffs, auras, damage types, shields, cooldown/charges and autocast |
| **UI & localization** | Tiny built-in bitmap HUD, project metadata | UI scene/layout editor, nested anchors, cursor/icons, dialog system, hotkeys, localization tables and fallback, accessible fonts |
| **AI & campaign** | Hardcoded enemy reinforcement behavior; first triggers | Behavior trees/utility AI, tactical policies, objectives, waves, campaigns, mission sequencing, bank/persistence |
| **Asset importer / mod packages** | JSON projects, CLI; no external model runtime | File browser, import staging, hashes, dependencies, content packs, mod layering/conflict resolution, export/build and sandboxed publishing |
| **Save, test, debug, profiling** | Project save/load, 32-step cross-editor undo/redo, 25-second autosave/newer-file recovery, guarded GLFW callbacks, logic tests and CI smoke captures | Play-in-editor state snapshot, diff/compare, playtest debugger, tracing and profiling, hot reload, crash recovery and transactional undo, content migrations |
| **Multiplayer / mod distribution** | Not implemented | Deterministic simulation, replay/rollback or lockstep, anti-cheat boundaries, lobby rules, dedicated hosting, versioned package distribution |

## The 87 SC2 catalog categories as reference

Not all are equally important. Initial prioritization deliberately separates
**gameplay**, **actors** and **assets** before building the long tail:

- **Game data (13)**: Abilities, Behaviors, Effects, Footprints, Items, Loot, Movers,
  Requirements, Turrets, Units, Upgrades, Validators, Weapons.
- **Campaign data (13)**: Army Categories, Army Units, Army Upgrades, Bank Conditions,
  Campaigns, Characters, Commanders, Conversations, Conversation States, Locations,
  Maps, Objectives, Preload.
- **Actor data (3)**: Actors, Attach Methods, Beams.
- **Art & sound data (12)**: Buttons, Digital Sound Effects, Lens Flare Sets, Lights,
  Models, Reverb Effects, Shapes, Sounds, Sound Exclusivities, Sound Mix Snapshots,
  Soundtracks, Textures.
- **Terrain data (9)**: Terrain Cliffs, Terrain Cliff Meshes, Fog of War Data,
  Physics Materials, Terrain Types, Terrain Objects, Terrain Textures, Terrain Roads, Water.
- **Hero data (11)**: Artifacts, Artifact Slots, Configs, Heroes, Hero Abilities,
  Hero Statistics, Mounts, Skins, Talents, Talent Profiles, Voice Overs.
- **Interface data (6)**: Alerts, Bundles, Cameras, Cursors, Game UI Data, Pings.
- **Advanced data (20)**: Achievements, Achievement Terms, Boosts, Gameplay Data,
  Herds, Herd Nodes, Item Classes, Item Containers, Kinetics, Races,
  Requirement Nodes, Rewards, Score Results, Score Values, Tactical AI Cooldowns,
  Tactical AI Data, Target Finds, Target Sorts, Trophies, User Types.

We should not blindly reproduce 87 C++ structures. Catalog categories should
be schema plugins backed by a single flexible data model, with engine adapters
that **actually implement** the semantics of each supported category.

## Recommended implementation sequence

**Phase A — Editor substrate (in this PR):** versioned project schema, catalogs,
inheritance, terrain mesh/brush/pathing, stable scene placements, editor UI,
headless CLI, source-friendly format, first event-condition-action runner and tests.

**Phase B — Authoring usability:** dockable UI/toolkit, actual object selection and
transform gizmos, separate layered editor documents, bulk search/replace, value
validation, property widgets, undo/redo across all documents, autosave/recovery,
reference graph and per-project asset browser.

**Phase C — Model & actor semantics:** glTF import, geometry/skeleton, textures and
material assets, animation clips, sound, particle emitters; event-based actors
decoupled from simulation data; prefab/actor inheritance and transforms.

**Phase D — Ability/effect engine:** ECS-like stable entity handles, buff/effect
graph, targeting/validation, projectile movers, fog/pathing/LOS, upgrade and
research systems, data-driven resources and production (no hardcoded IDs).

**Phase E — Scripting suite:** visual trigger canvas compiling to portable
intermediate representation and a sandboxed script VM with typed safe APIs,
breakpoints, debugger and profiler, asset hot reload and UI scene editing.

**Phase F — Publication:** package manifests, dependency and override resolution,
map/mod export, modloader, replay and deterministic multiplayer support.

### Non-negotiable tests

Every new catalog type receives JSON round-trip and inheritance tests; each
runtime-backed property receives a simulation integration test. Scene edits
are checked against replay/collision/pathing. Editor actions get a full-GPU
smoke screenshot and eventually automated input playback.

### Latest visibility implementation

A simulation-owned explored/visible terrain grid, terrain-height sight
occlusion, per-Unit/instance `sight_range`, fog-shaded terrain, hidden
enemy world models and filtered minimap/targeting have been implemented.
Radar, detector rules, allied vision sharing, radar ghosts and separate
sensor layers remain TBD.

### Latest simulation scaling pass

An XZ spatial grid now accelerates local separation and nearest-enemy
queries; direct target-ID lookup is O(1) within each simulation tick.
Owner-based hostilities are independent of visual faction. Heavyweight
collision shape queries, movement classes, flow fields, projectile physics
and networking/prediction remain future engine subsystems.

### Creator navigation iteration

The in-game Data Editor now has ID/type/local-name search and filtered
navigation. This remains a keyboard-oriented prototype, not the SC2
dockable object tree, behavior graph, property form editor or reference
browser.

### Tactical command extension

Stop, attack-move, move, patrol, per-building rally points, Hold Position
(no drift or chasing), F2 combat army and F3 worker selection are now present
in the initial command model. Route interruption handling resumes the
remaining attack-move leg. Still missing: stance/target priority layers,
guard/follow, targeting filters, command queues with visual command cards,
and formation-aware movement for large groups.

### Group order upgrade

Group move, attack-move, shift-queued move and patrol now assign separate
formation destinations with a map-configurable footprint spacing multiplier.
This covers initial formation *destination slotting*, not WC3/SC2 squad
movement fidelity, adaptive rows, formation persistence or tactical
multi-unit flow fields.


## November 14: catalog-object explorer and native UI foundation
Current editor object navigation supports Parent, Unit/Actor/Model links, production references and incoming uses, plus typed primitive property editing with validation/undo. Engine-owned pause-menu geometry and GL UI have been implemented separately from the existing ImGui workbench. This is **not** a complete Galaxy Editor data module, full editor UI or a gameplay abilities/effects system. Track planned data/actor/ability/trigger/terrain/UI modules in `docs/CREATOR_ARCHITECTURE.md`.


## Independent example content and Data Editor module workflow

The example unit definitions have distinct IDs and presentation names; SC2-specific unit names are not shipped wholesale as game content. This engine can load author-created catalogs and is not intended to emulate SC2's copyrighted art, code, story or audio. The F7 catalog list now exposes every present category through a dynamic type selector, works with the inherited/searchable object inspector, and only submits visible list rows to ImGui each frame. Architectural references: [Galaxy Data Editor](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/), [Recoil](https://recoilengine.org/), and [0 A.D.](https://play0ad.com/).


## Small typed effect graph and region trigger (November 14)

`Effect` is a new mod-authored catalog family using `Damage`, `Heal` and bounded `Sequence` nodes. References participate in rename/delete dependency tracking. The trigger runtime can invoke an Effect on a region-entering subject through `ApplyEffect`. `projects/effects_showcase.rtsproject.json` demonstrates a sequence while retaining the same pure-C++ simulation on Windows and Linux. This is an early prerequisite for later abilities/behaviors/validators, not the SC2 effect system.


## GUI trigger workflows and effect references

The F6 Trigger Editor now exposes the ECA tree as typed controls (event, repeat, conditions, actions, inspector properties and ordering), not only raw text. Effect references are taken from authored Effect catalog objects and currently require an `OnRegionEnter` subject. Both visual changes and raw JSON use the same headless parser and validation gate, with undo/redo and tests. This is a useful subset of Galaxy's authoring workflow, **not** its complete trigger language, variable library, debugger or UI system. See `docs/TRIGGER_EDITOR.md`.


## Reference-array inspector improvement

The creator workbench now adds a typed list editor for `Unit.trainable_units` and `Effect.children`, including a catalog-link opener and reordering controls. This builds on the current generalized catalog storage and uses `EditorSession` transactions for correctness. It is a useful Galaxy-inspired authoring workflow but does not imply the engine implements Galaxy Editor's full catalog variety or behavior graph UI.


## Execution monitor and native focus status

Implemented: a searchable trigger trace monitor and per-rule execution statistics, deterministic timestamped ECA records and a bundled original map demonstrating a successful and blocked trigger. New pure-C++ focus navigation controls the existing native pause menu. These are editor/engine fundamentals inspired by public RTS authoring UX, not copied Galaxy code/content. Future: breakpoint/step debugger, variable/function libraries, visual condition grouping and full native editor tree and widgets.


## RTS simulation fundamentals: path changes and repeatable economy

As part of an engine-centric pass, navigation grids now track meaningful topological changes; active paths are repaired only when their remaining corridor becomes invalid. Empty/unreachable paths no longer indefinitely pin move orders when a queued command can be executed. Structure stamping is local to the obstacle footprint, and construction cannot trap mobile units inside newly blocked cells.

Worker jobs and route progression are iterated in unit-ID order to eliminate hash-order dependence for shared mineral contention. This is one required building block for deterministic simulation, **not** full lockstep or rollback support. Priority gaps now include passability classes, air movement, command recording, ability/actor schemas, scenario AI and a native dockable editor. See `RTS_ENGINE_AUDIT.md`.


## Reproducible input and replay foundation

Completed: one tick-indexed, versioned game-command schema covering player move, attack, patrol, rally, gather, production, hold/stop and building orders. The creator no longer dispatches these directly from mouse/HUD callbacks. Replay JSON contains the self-contained project, input timeline and periodic state fingerprints; it can be verified headlessly. CI compares a scripted scenario's Windows/Linux digests and checks malformed/reordered/tampered replays. Failed orders receive asynchronous notifications.

Still missing: replay timeline viewer, arbitrary snapshot/restore, deterministic multiplayer transport, scripting debugger, gameplay AI commanding through the same public queue, richer actor/ability/mover definitions and the completed engine-native editor.


## AI support: baseline gameplay

Implemented: source-1 enemy attack orders go through the shared simulation command queue; enemy combat target selection is read-only, ordered by unit IDs and deterministic for a given world/tick. The minimal policy checks nearby defender threats around the opponent's base and otherwise attacks the player's command center. Enable/disable and defense radius are authorable metadata. There is an original combat AI demonstration map and unit/project validation tests.

Missing: AI harvesting, resource/supply optimization, construction strategies, tech/build-order trees, tactical squads, opponent fog-of-war decision constraints, designer AI behavior editor and multiplayer trust model. The policy's default behavior is intentionally simple rather than claiming StarCraft-like strategic intelligence.

## Bounded shared trigger variables (November 14)

F6 now exposes authorable map-global integer values and reference-aware editor controls. The headless ECA parser validates IDs, signed range, references and comparison operators; SetVariable, AddVariable and VariableCompare execute in the actual fixed-step simulation. Globals are saved as optional project extensions and reset on map restart. Their state is included in replay digests only for authored nonempty sets, preserving old map digests. A dedicated scenario's Windows/Linux fingerprints are compared by CI. This is a **first scoped script-state primitive**, not Galaxy Script language parity: local variables, expression ASTs, user functions, loops, waits/coroutines and VM/debugger remain pending.

## Typed expression foundation (November 14 continuation)

The trigger runtime now evaluates a bounded integer AST in VariableCompare, SetVariable, AddVariable and GrantMinerals rather than only fixed numbers. It supports variables, arithmetic, min/max and clamping, strict authoring validation, undoable literal/reference widgets and global rename migration. The implementation is deterministic and included in strict cross-platform replay fingerprints. This is **not** a Galaxy text compiler, general script function system, event coroutine executor or branch/loop runtime yet.

## Nested ECA control flow (November 14 continuation)

`IfThenElse` and `Repeat` authorable action nodes now run in the fixed-step trigger interpreter. Top-level and nested conditions share the same validator, and nested actions use a recursive budgeted executor. Branches consume one of two ordered action lists; repeated bodies support a static count or a validated integer expression. Static complexity checking rejects graphs that could exceed a 512-step worst-case invocation budget; recursion depth is capped. The F6 GUI offers templates and branch summaries while full recursive node editing still needs a dedicated UI pass. The new `RTSScriptFlowTests` produces a mandatory Windows/Linux replay fingerprint. Full Galaxy control flow/custom functions/local scopes remain pending.


## November 14 large editor/engine pass: progress, not parity

**F6:** nested ECA tree operations and formulas are now visual and
undoable down through supported IfThenElse/Repeat branches; backed by
safe path-based headless mutations and strict validation.

**F7:** new original Unit/Actor/Model/Effect object templates and
undoable operation conversion; periodic damage/healing sequence graphs
now execute at fixed simulation ticks, together with optional Unit
passive regeneration.

**Not implemented:** full Galaxy text language, reusable function/
library graphs, scoped/array variables, switch/coroutine waits,
trigger debugging breakpoints, full visual data schema editor, actor
events, behaviors/buffs with stacking and dispels, castable unit
abilities, weapon/missile actors, tech/upgrade trees, robust pathing
layers and broad asset import. Keep these explicit milestones in
future design passes instead of equating checkboxes with parity.


## November 14 continuation — authored Abilities, still far from Galaxy

Implemented: data-driven Ability catalog type with Effect references;
Unit Ability loadouts (up to eight); Self/Ally/Enemy/Any targets;
owner/fog/range checks, mineral and per-Unit energy costs; energy
regeneration, fixed-tick cooldowns and cast command/replay recording.
F7 supports Ability presets, target/Effect selection and Unit
Ability loadout editing with undoable typed references. F9 exposes
up to three commands (F/J/K, LMB target pick), alongside existing
gameplay HUD and vision systems.

Missing: authored Ability/Effect graphs with arbitrary nodes,
persistent behavior/buff/debuff stacks, cooldown UI pagination,
autocast/AI ability policy, custom projectile actors and animations,
area and terrain targeting, cast interruption/channeling, tech trees
and upgrades, editor user-defined scripting functions/libraries,
arbitrary advanced data schemas and localization. This progress is
not StarCraft/Galaxy feature parity and does not reuse Blizzard assets.

**UI pagination follow-up:** F9 now pages through the full eight-Ability loadout in three-slot pages using TAB or the SKILLS x/y button; the current page does not enter authoritative replay hashes. This closes the initial three-slot reachability gap, though custom card-layout authoring and per-Ability icon/smart hotkey binding remain future work.


## November 14: authored Behavior/status rules and visual Effect flow

**Implemented since previous Ability pass:** new Behavior catalog
type, typed ApplyBehavior/RemoveBehavior Effect nodes, stack limits,
fixed-tick status expiry and optional refresh, dispel immunity,
armor/damage/movement modifiers with bounded combined totals,
256-live-instance runtime cap, death cleanup and deterministic
replay state. These integrate with the existing Ability targeting
and periodic/Sequence Effects, not a separate cosmetic demo.

**Editor/UI:** F7 Behavior presets, grouped validated and undoable
parameter inspector, typed Effect→Behavior links, nested visual
Effect-flow preview, Effect sequence reorder/navigate/append/remove
and a wider initial data workspace. F6 live status watch and F9
effective attack/armor/status count reflect actual runtime objects.
Original Behavior Workshop project and CI screenshots cover F7/F9.

**Further essentials:** Behavior categories/exclusivity, stacking
rules that can be scripted, auras, status actor lifecycle and VFX,
a scalable custom data schema UI, visual graph connections rather
than nested tree, area/terrain targeting, projectile actors,
native editor-owned full GUI, upgrades/tech trees, actual asset
pipeline and complete script libraries. This is a foundation, not
a StarCraft/Galaxy data editor or proprietary content port.
