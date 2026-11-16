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


## Playtest trigger diagnostics and native UI navigation (November 14)

The creator now has an optional **live trigger monitor** in View or F6: per-rule attempt/fired/blocked counts, timestamped ECA execution/failure records, search, and one-click navigation to the rule. The trace is bounded to 192 entries and never serialized. `projects/trigger_diagnostics_showcase.rtsproject.json` provides a ready-to-run example. See [Trigger Editor guide](docs/TRIGGER_EDITOR.md).

The native game pause interface supports mouse **and** keyboard (arrows/W/S, Enter/Space, Esc) through reusable dependency-free `NativeFocus` logic; the editor's ImGui adapter remains temporary. Both features are compiled from identical source and tested on Windows/Linux.


## Engine fundamentals: live navigation changes and stable simulation ordering (November 14)

New buildings, exhausted mineral deposits and terrain/pathing changes no longer leave active movement routes silently stuck against invalid waypoints. The engine marks grid/terrain generations, checks only routes affected by topology changes and replans those whose remaining corridors are blocked. A fully unreachable route retires so queued move orders can continue. New structure placement rejects living units in its footprint before spending resources. Circular path blockers are now stamped using a local cell bounding rectangle rather than traversing the whole terrain grid.

Movement and gathering updates now run in stable numeric unit-ID order, an important foundation for reproducible multiplayer/replay testing, although **full cross-platform deterministic networking is not implemented**. Windows/Linux CTest includes new dynamic navigation and contested-resource determinism fixtures. The full original-engine gap audit, based on Recoil/Spring, 0 A.D., OpenRA and Zero-K architecture, is in [docs/RTS_ENGINE_AUDIT.md](docs/RTS_ENGINE_AUDIT.md).


## Authored unit names in the runtime (November 14)

Production-spawned units, AI reinforcements and newly built structures now use the `name` field resolved from their catalog definition. This removes remaining hardcoded starter-spawn labels from the simulation, allowing custom maps to train named units without exposing a baked-in faction identity. The fallback text data also uses the original Frontier Ranger example. The data-driven production regression suite asserts a custom **Field Medic** actually spawns with that name.


## Fixed-tick commands and first replay tooling (November 14)

User-issued RTS commands now enter a shared `RTSCommandStream`, keyed by a simulation tick and captured unit IDs, instead of directly mutating gameplay from mouse/HUD callbacks. During Play mode use **Ctrl+Shift+L** or **View → Export playtest replay** to save a self-contained `.rtsreplay.json` file next to the loaded project. Run `RTSProjectTool --verify-replay path.rtsreplay.json` on Windows or Linux to rerun and verify all saved state checkpoints. The simulation also reports rejected queued orders asynchronously to the gameplay notification layer.

A new CI gate compares replay state fingerprints **between Windows and Linux**, not just within each OS. This is an early deterministic foundation; it does not yet provide a networked or rollback multiplayer session or visual replay scrubbing. See [Replay and command guide](docs/REPLAY_COMMANDS.md).


## AI combat orders and replay compatibility (November 14 continuation)

The opposing skirmish AI now enters **Attack** orders through the same source-labeled fixed-tick command stream used by human orders, instead of modifying units directly. An original, read-only `RTSAIPlanner` selects targets deterministically, with local defense around the opponent's base and fallback aggression toward the player's command center. Its choices are saved in replay **format v2** and can be verified headlessly on Windows/Linux. Existing replay format v1 remains loadable using the historical AI behavior.

The creator's map metadata supports `enemy_ai_enabled` (boolean, default true) and `enemy_ai_guard_radius` (number 0–80, default 14 world units; 0 disables local defense but not strategic attacks). Load the included [AI Defense Combat Lab](projects/ai_defense_showcase.rtsproject.json) to test these choices. This first AI policy does not implement economy/build orders, squads, terrain-awareness beyond local targeting, or sophisticated strategic decision trees.

## Event-driven creator scripting: relay demonstration (November 14)

The F6 Trigger Editor now exposes bounded shared integer map variables with typed Set/Add/Compare ECA nodes and a deferred `OnVariableChanged` event. These are **real simulation scripts**, not an unimplemented field-only catalog. The live trigger monitor has a variable watch, and state/queued events are included in deterministic replay fingerprints. `projects/script_relay_showcase.rtsproject.json` is an original playable example: a timed perimeter alarm causes enemy reinforcements, increments another variable, and awards resources when the raid counter changes. Open this map from the bundled `projects/` directory and switch to F6; refer to `docs/TRIGGER_EDITOR.md` for limitations. The real OpenGL screenshot is produced by the screenshot-test workflow, while the separate build workflow supplies runnable Windows/Linux binaries. Full Galaxy scripting syntax, custom functions, branch/loop statements, local variables and in-house editor GUI are still future work.

## Script control-flow and expressions (November 14 continuation)

Creator-authored triggers support bounded signed integer formulas, variable comparisons, `IfThenElse` branches, and static/dynamic-count `Repeat` actions in the headless fixed-step simulation. This is original, sandboxed logic, with strict validation and replay parity on Windows/Linux. F6 can create initial branch/loop nodes, display nested summaries and edit complex expressions/branches in Advanced JSON. Safe script-global and Unit/Effect catalog renames traverse known nested references without touching custom text. The `script_relay_showcase` map now uses a branch, repeated body and variable-derived mineral reward. See `docs/TRIGGER_EDITOR.md`. This is not Galaxy-language source compatibility or a finished UI/actor/ability system.


### Recursive map scripting and catalog effects (November 14)

F6 now has recursive visual editing for conditional branches, repeat
actions, validated ECA node properties and arithmetic expression trees;
nested edits can be undone/redone without switching to raw JSON. F7
adds standalone Unit/Actor/Model/Effect templates and an Effect type
selector. The shared gameplay engine executes authored periodic damage
and healing Effect graphs, plus optional per-Unit HP regeneration on
fixed ticks. Open `projects/timed_effects_showcase.rtsproject.json`
for an original playable example. The same code is built on Windows
and Linux with strict replay-parity and CTest gates. See
`docs/TRIGGER_EDITOR.md` and `docs/SC2_FEATURE_MATRIX.md`
for remaining Galaxy-class editor gaps.


## Authored Unit Abilities and energy

F7 can now create original Ability catalog objects with Self, Ally, Enemy,
or Any-unit target policies, link each to an existing Effect, and edit
cast range, energy/mineral costs and fixed-tick cooldowns. Unit objects
have a validated, undoable, ordered Ability loadout of up to eight IDs.
Units can have custom max_energy and energy_regen gameplay stats.

In F9 playtest, select one caster and use its first three ability slots:
F/J/K. Self skills execute on the next simulation tick. For targeted
skills, left-click a visible valid unit in range; right-click cancels.
The HUD shows energy and unavailable/cooling abilities. Abilities use the
recorded authoritative command stream and strict Windows/Linux replay
parity. See projects/ability_workshop_showcase.rtsproject.json for an
original playable demo; see docs/AUTHORING.md for creator instructions.


## Original status behaviors, visual Effect flow and F7 authoring

Unit Abilities can now apply **Behavior** catalog objects through
Effect steps. Behaviors modify real armor, weapon damage and navigation
speed, can stack up to an authored cap, expire on fixed simulation
ticks, and optionally refresh duration or resist dispelling.
New Effect kinds **ApplyBehavior** and **RemoveBehavior** compose with
Damage, Heal, timed effects and nested Sequences. Nothing copies
an existing RTS's unit identities or media.

**F7 Data Editor:** New catalog object offers Behavior templates
(Armor shield, Attack focus, Movement slow). The selected Behavior
has a grouped modifier editor that commits all parameters as one
validated undo transaction. Effect objects show typed Behavior
references; a new navigable visual Effect-flow tree previews nested
Sequence operations, and designers can append, reorder, remove and
open their components without editing raw JSON. The F7 inspector
has a wider responsive initial layout.

**F6 monitor:** live status watch exposes Unit ID, Behavior, stacks and
remaining simulation time. **F9:** a selected unit's dossier displays
modified armor/damage and the number of active status instances.
The status and cooldown state is deterministic and replay-verified.

Start with projects/behavior_workshop_showcase.rtsproject.json for
original Brace Stance, Tether Bolt and Break Tether mechanics.
Windows/Linux builds, tests, map validation, replay fingerprints and
native OpenGL screenshot checks share the same source.


## Native ground-targeted area abilities and delayed impacts

The creator now supports Ability.target = Ground alongside Self/Ally/Enemy/Any.
F7 creates Ground Ability presets and edits radius, Enemy/Ally/Any filter,
and impact delay as one undoable transaction. F9 uses the existing Ability
command card and left-click terrain aiming, with a teal/red range/radius
preview and orange persistent ring for delayed impacts. These are real
fixed-tick simulator events, not fabricated graphics. The original
projects/ground_impact_showcase.rtsproject.json illustrates Ember Arc
(delayed Damage+ApplyBehavior Sequence) and Repair Circle (allied Heal).

Ground centers must be visible and within the authored cast range.
Victims are determined at impact from current living Unit positions,
so moving out of the zone dodges a delayed cast. Affects are limited
to 64 Units per impact; at most 128 impacts may be scheduled.
Area effect/Behavior scheduler admission is checked across all
victims before mutation. A delayed impact that hits no targets or
overruns capacity fizzles; resources were spent at cast time.
Native Windows/Linux CTests, replay parity and renderer screenshots
validate this shared code. A delayed area telegraph is NOT yet a
physical projectile mesh, collision model or actor animation.


## Authored 3D projectiles and deterministic interception

The engine/editor now support reusable **Projectile** catalog objects.
F7 has original Arcing Spark and Intercepting Bolt presets; authors can
adjust launch speed, parabolic arc, mesh color/size, contact radius and
whether a projectile intercepts Units along its flight. Ground Abilities
may reference a Projectile or keep the existing timer-only area impact.
The same Effect/Behavior graph, costs, cooldowns and team filters remain
authoritative. Projectiles are rendered as original procedural 3D meshes
with a short trail and on-impact flash; no third-party art is included.

Swept contact uses the tick-updated spatial buckets to narrow
candidates and includes maximum living Unit radius in the search,
so large scaled actors are not missed. Exact swept-contact
ordering remains stable independently of entity vector order.

Flight advances at 60 fixed simulation ticks/second. Time-to-target is
ceil(XZ distance / speed * 60), minimum one tick, with the Ground
Ability's impact_delay_ticks added **after** flight arrival. Optional
swept cylinder contact against eligible living Units can detonate early.
Ties resolve by first segment contact then stable Unit ID. Explosion
effects still resolve all eligible Units in the authored area radius,
with the existing capacity/atomicity limits. Pending projectile
trajectories are included in deterministic replay checksums.

Try projects/projectile_forge_showcase.rtsproject.json: Aether Arc
(arcing delayed area blast), Pulse Bolt (direct-flight interception),
and Repair Circle (legacy instant area Heal). Both Windows and Linux
use the same C++17 project, native F7 editor and F9 renderer.

## Data-authored cone and line area search (November 15)

Ground Abilities now provide **Circle**, **Cone**, and **Line** targeting.
F7 exposes the shape selector and angle/width controls as validated,
undoable project properties. The F9 native OpenGL overlay previews
the corresponding shape while aiming, and F6 identifies pending
shaped attacks. Targets are resolved inside the existing 60 Hz
simulation, using cast-time facing and target positions at impact.
Older projects continue to use unchanged circular search semantics.

The new original Area Shape Laboratory example is
projects/area_shapes_showcase.rtsproject.json. F7 edit Fan Surge
and Rail Line; F9 select the Ranger and cast each to compare their
impact volumes. Windows and Linux compile the same runtime and editor.

### F7 live area footprint inspection

The Ground Ability form now draws an interactive-property preview canvas:
the chosen Circle/Cone/Line geometry is shown at the authored radius,
angle and width, alongside green/red example target dots based on the
same simulation predicate. No map edits occur until Apply is selected.
The preview uses the existing editor adapter; F9 targeting remains the
separate engine-owned OpenGL overlay.

## Normal-attack Weapon catalog and original Weapon Forge (November 15)

The F7 creator now supports an independent, reusable Weapon catalog family.
Weapons have authored damage, attack reach, cooldown and armor penetration.
Units can equip a Weapon through a typed dropdown, navigate to it, or use
their legacy Unit stats. Equip settings and Weapon edits are validated and
undoable; reference rename/deletion and derived Unit inheritance are handled.

Normal Weapon hits resolve in deterministic two-phase combat. Penetration
reduces the target armor (including active status bonuses) for ordinary
attacks, but does not change ability or Effect damage. The F9 HUD shows
PEN on armor-piercing units. The original playable Weapon Forge map has
a Piercer Rifle and Impact Maul. Both platform CTests cover combat and F7
workflows, with replay fingerprint comparisons and native GUI screenshots.

## November 15: Native Weapon projectiles and F7 Data Navigator

Basic Weapons may now reference Projectile catalog objects. Ordinary Attack
orders fire deterministic swept-collision shots instead of dealing immediate
damage, while unlinked Weapons remain hitscan. Targets can dodge the
original launch trajectory; collide_units on the Projectile enables enemy
interception. Authoritative damage/penetration occurs on contact, not in
OpenGL. The original Weapon Forge project has a Tracer Bolt and practice
target. The shared scheduler reserves 512 Weapon flight and 128 Ability
area slots independently.

The new F7 Data Navigator is a dockable catalog dependency explorer:
forward links, reverse "Used by" references, inheritance indicators,
searchable jump navigation. It uses the same typed project catalog as
gameplay and preserves unknown extension data. Windows/Linux tests
verify combat, editor transactions and replay parity. Reference research
is archived in docs/EDITOR_REFERENCE_RESEARCH.md.

## November 15 continuation: event-driven Actor presentation

The native engine/editor now supports authored Actor cues for real
Attack, Launch, Impact, Death, Cast and Status events. F7 exposes
grouped, validated Actor event rules for Pulse, Ring and Beacon effects,
RGB color, radius and fixed-tick lifetime; creation, undo/redo and
per-instance/inherited Actor settings are supported. F6 observes live
events, and F9 displays original procedural GL effects.

Actor cues are bounded presentation snapshots, NEVER authoritative
damage or replay state. A new regression suite verifies identical
gameplay digests with and without cues. The new Actor Event Laboratory
project and Windows/Linux CI screenshots exercise real gameplay.
See docs/ACTOR_EVENT_AUTHORING.md for schema and current limits.

## Validator and Requirement graph workshop

F7 now has first-class Validator and Requirement catalog editors.
Validators inspect authoritative live gameplay (HP %, energy,
active Behaviors, structure flag, or owner Unit count). Requirement
graphs combine predicates through All, Any and Not nodes with bounded
depth, node count, typed references and cycle detection. Abilities
and trained Unit catalog items can reference a Requirement; the
engine evaluates it when a cast or production order is submitted.
Existing maps with no Requirement remain unchanged.

The original Requirement Graph Workshop includes an Arc Lance gate
and a Worker training prerequisite. Windows/Linux Release and
Debug CI validate it and byte-compare requirement_fingerprint.txt;
F7 captures a native graph inspector screenshot. See
docs/REQUIREMENT_AUTHORING.md for workflow, data schema and limits.

## Authorable Upgrades and Research

The native F7 editor now has original multi-level Upgrade catalogs,
per-level mineral cost and fixed duration, typed Unit targets and
reusable Requirement prerequisites. UpgradeLevelAtLeast Validators
gate other research, Unit training and Abilities. F9 producer command
cards issue authoritative ResearchUpgrade orders; F6 displays active
progress. Completion modifies existing and future Unit attack/armor,
while canceling research refunds the deposit. The original Technology
Forge Laboratory demonstrates tech chains. See
docs/TECHNOLOGY_RESEARCH.md for complete authoring and limitations.

## Show versus Use technology permissions

F7 now authors independent Show and Use Requirement graphs on Upgrades,
Unit training and Abilities. False Show hides the command; failed Use
leaves it visible but disabled. F9 producer/ability pages pack only
visible entries. Raw gameplay commands cannot bypass hidden conditions.
The updated Technology Forge map demonstrates reveal at research level
1, unlock at level 2, and removal of a completed tech at level 3.
See docs/TECH_SHOW_USE.md.


## Creator Technology Map and headless tech dependency export

The F7 Data Editor now provides **View → Technology dependency map**, a
dockable original graph of producing Units, research Upgrades, gated Units,
and Abilities. Lines distinguish training, research, equipment, stat targets,
and nested Show/Use Requirement references (including NOT/ANY context).
Clicking a node navigates to its normal validated F7 inspector; the map
does not invent or rewrite gameplay rules. The shared headless projection
also exports safe deterministic Graphviz DOT:

`RTSProjectTool --tech-graph projects/technology_forge_showcase.rtsproject.json`

See [Technology Map](docs/TECH_TREE_MAP.md). Native CI screenshots and
a Windows/Linux test+packaging parity gate exercise the same source.


## Owner-aware enemy research AI and F7 planner (November 16)

The skirmish enemy can now run an opt-in **economic research policy** using an independent fixed mineral budget and real typed research structures. A priority-ordered planner considers current Show/Use Requirements, producer capability, queued jobs, level costs, affordability and completion; it submits only authorized source-1 ResearchUpgrade commands to the deterministic replay stream. Enemy research improves existing/future enemy units, without spending player minerals. Open **F7 → View → Opponent research planner** to edit the policy via fully validated undo/redo, or view the live research budget/levels in Play. A new [Opponent Technology Academy](projects/ai_research_showcase.rtsproject.json) map and [AI research manual](docs/AI_RESEARCH.md) demonstrate chained, level-specific research. Windows/Linux CTests, replay fingerprints and genuine F7/F9 screenshots cover the workflow. This does not yet provide enemy worker economy or autonomous construction.


## New desktop editor workspaces and accessibility scaling (November 16)

The creator now separates **Terrain (F5), Triggers (F6), Data (F7) and Scene (F8)** into individually titled, position-persistent editor windows, each with a resizable Navigator/Properties split and its own Save/Undo/Redo controls. Normal native Windows/Linux builds enable Dear ImGui multi-viewports: the selected module opens in an independent decorated OS window (and can move to another monitor). The Play/F9 world and in-house HUD remain independent. **Appearance** offers 85–250% UI scale presets, system/active-monitor DPI following, enhanced text contrast and a switch back to the old floating layout. Fonts are recreated at their actual target resolution. Preferences save per-user (not in maps/replays). See [Creator Windows & Accessibility](docs/CREATOR_WINDOWS_ACCESSIBILITY.md), with a dedicated real native module-window CI capture and a separate 175% readability screenshot.


## F10 Interface Studio — editable gameplay HUD

The **Interface Editor (F10)** is a dedicated native creator module for the
actual playable RTS HUD. Its interactive blue-on-blue canvas projects real
`RTSHUDLayout` geometry at 800×600, 1280×720 and 1920×1080. Select, drag or
use precise sliders to adjust the tactical minimap, 12-button command grid,
bottom panel and upper resource strip; **Apply** commits one validated,
undoable `hud_profile` map transaction. The **gameplay renderer and mouse
input both use the authored rectangles**, not a static editor-only mockup.
Old projects retain their exact default layout. The new sample
`projects/interface_studio_showcase.rtsproject.json` and
[Interface Editor manual](docs/INTERFACE_EDITOR.md) ship in Windows/Linux
bundles. F7's resolved-property inspector now has a searchable,
resizable Scope / Field / Value table with wrapped text and clear
local/inherited indicators, designed to work at 150–250% UI scaling.
