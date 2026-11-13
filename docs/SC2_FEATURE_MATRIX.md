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
| **Terrain & pathing** | Heightfield sculpt, smooth/flatten, 8 material IDs, blocked-cell pathing and brush preview; 49×49 starter | Multiple texture layers, cliffs/ramps, water, roads, decals, foliage, cliffs meshes, destructibles, per-unit movement types, map resizing, symmetry, procedural generation |
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
| **Save, test, debug, profiling** | Project save/load, 32-step chronological cross-editor undo/redo, logic tests, CI, engine smoke captures | Play-in-editor state snapshot, diff/compare, playtest debugger, tracing and profiling, hot reload, crash recovery and transactional undo, content migrations |
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
