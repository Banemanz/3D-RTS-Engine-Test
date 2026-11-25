# Engine implementation status

Baseline: `cce16516a759c764621ed2630ebd55f326aa466d`.
Current PR: `feat/rts-vertical-slice`. Source implementation and CI compilation are
distinct from manual gameplay verification.

| Subsystem | State in PR | Notes |
| --- | --- | --- |
| Window, OpenGL 3.3 context, shaders | Implemented | GLFW; Mesa smoke test in CI |
| Camera, selection and raycasts | Implemented | Window/framebuffer dimensions tracked separately; Q/E yaw |
| Flat terrain, procedural meshes | Implemented | No content importer or terrain elevation |
| Unit HP, armor, cooldown, combat | Implemented | Target lookup is still linear; no projectiles |
| Fixed 60 Hz simulation in app | Implemented | 8-tick catch-up limit; drops excess time |
| A* navigation for move orders | Implemented | 8-neighbor grid; static building/mineral blockers |
| Combat-chase obstacle rerouting | Implemented (basic) | Periodic A* replanning; path blocker collision guard |
| Shift-queued ground orders and control groups | Implemented (basic) | No attack-target/gather queuing yet |
| Contextual mineral harvesting | Implemented | 10 minerals per trip, 1.6-second extraction |
| Command Center and Barracks | Implemented | Placeholder meshes, 9-second Barracks construction |
| Ranger/Worker training, economy/supply | Implemented | Per-building queues, paid minerals, reserved supply |
| Enemy skirmish behavior | Minimal | Scripted reinforcements; simple unit target selection |
| Victory/defeat and restart | Implemented | Destroy Command Center or Bastion |
| In-game HUD | Implemented | Built-in bitmap font, resources/actions/selection |
| UnitData validation | Implemented | Embedded data only; strictly parsed numeric fields |
| Tests and CI | Implemented | Linux full build, Windows headless build, Mesa screenshot |
| True combat pathfinding / group formations | Planned | Prototype separation and direct chase remain |
| Save/load, fog of war, audio, model import | Deferred | Outside first vertical slice |
| Multiplayer / campaigns / modding API | Deferred | Require later architecture work |

## Known constraints

- Entity storage uses a vector of `Unit` with integer IDs; lookup, combat aggro and
  separation still scan vectors. It is not prepared for thousands of units.
- Navigation planning is used for move/attack-move waypoints and tactical chases.
  Dynamic obstacles can invalidate existing routes; crowd-scale movement and combat
  line-of-sight remain to be improved.
- Model rendering and gameplay remain coupled in the main executable. The simulation
  is now separated sufficiently for headless tests, not yet a complete engine API.
- Formations are a simple group destination, not sophisticated flow-field movement.
- The RTS scenario includes just one map and two hardcoded starting factions.
- Building placement validates clear space and worker proximity; it does not simulate
  an actual builder physically constructing the structure.
- Research, upgrades, teams beyond human vs Hybrid and ownership transfers are absent.
- The custom font supports uppercase Latin, digits and selected punctuation only.

## Terrain pathing integration (current draft PR)

The pathfinder now supports weighted cell traversal: steepness raises route
cost, and gradients above the authorable global threshold block cells.
Manual blocked-cell painting remains explicit. This does not model collision
physics or the different movement categories and ramps expected in SC2.

## RTS/HUD reliability pass

A playable RTS-style HUD now replaces the debug-text game overlay, using
resource counters, a tactical minimap, selected unit stats/HP and clickable
command grid. F2 army selection, H base focus and P repeat-patrol have been
added. Editor callbacks are guarded against escaping C++ exceptions, actor
visuals are cached and autosave/recovery can limit data loss. Production
rally points, icon/model assets, fog/LOS, independent unit command schemas
and full event-driven UI remain outstanding.

**Production rally points** now store one target per structure and dispatch
newly trained units through the existing A* navigation. Selected structures
expose their rally location and queue length in the UI. Remaining work:
production progress display, per-building orders, rally target types
(unit or resource), queue cancellation, external actor icons, and a
configurable per-unit command schema.

The command panel now also provides contextual queue cancellation and an
active training progress strip for the selected structure. Cancellation
refunds the reservation exactly once, covered by headless regression tests.

## Fog of war / gameplay vision (this pass)

`RTSVision` tracks **Unexplored / Explored / Visible** states in a per-terrain-cell
grid. Friendly living units contribute circular sight radii defined by the
editable catalog property `sight_range`, with visibility blocked by raised
terrain along sight rays. Explored cells persist until reset; live visibility
refreshes five times per simulation second. The terrain shader samples a
revision-gated 8-bit GPU texture, keeping fog updates independent of terrain
mesh rebuilds. The minimap and enemy right-click/click selection use the same
visibility state, preventing invisible enemies from being selected.

This is deliberately **basic ground-level LOS**: no radar, sonar, cloaking,
stealth detectors, shared vision between multiple human factions, persistent
building ghosts, shadow casting or replay serialization yet. The AI's
targeting uses the existing aggro system rather than omniscient player fog.
Editor modes intentionally show the full map, without fog.

## Spatial broadphase and command determinism

`UnitSpatialIndex` maintains uniform-size XZ buckets plus integer-ID lookup,
rebuilding at the start of each fixed simulation step and moving units between
buckets as they advance. Separation and enemy acquisition query only adjacent
cells; exact distances and original unit-vector ordering still determine
steering and target tie-breaking. Attack target resolution uses O(1) ID lookup
during the fixed tick. Unit movement now clamps to the active navigation map
instead of a hard-coded 48-unit square, enabling authored maps at other scales.

**Hostility is determined by `ownerId`, not appearance or faction.**
Different owners can now choose the same visual faction while remaining
enemies, and differently skinned allies no longer auto-attack each other.

This is the first broadphase foundation; it does not yet include a dynamic
physics BVH, collision avoidance, flow fields, multi-threaded simulation,
multi-layer air/ground spatial categories or heavy-load performance budgets.

### Data Editor catalog search

The F7 inspector now supports case-insensitive object ID/type/name search,
a match counter, filtered previous/next selection and a clear-filter
shortcut. This is a lightweight in-window catalog browser. A full dockable
object tree, property type schemas, references, asset import, configurable
form controls and search across inherited values are still TBD.

### Multi-selection HUD update

The selection dossier can now show a grid of individual cards and HP bars
for selected army groups. Clicking a card isolates the unit, while
Shift+click toggles membership. Shared layout and hit testing keep the
OpenGL renderer and GLFW input coordinates consistent. Cards are currently
procedural and do not yet have actor portraits, localization, page scrolling,
formation commands or a user-customizable interface.

## Command-state reliability (this pass)

- New **Hold Position** state prevents navigation, momentum and separation
  drift while retaining in-range defensive attacks without pursuit.
- Interruption-aware attack-move/patrol routes recompute a path to the
  current leg's goal after a skirmish instead of treating combat as reaching
  the next waypoint.
- The RTS command panel expands to a **4×3 clickable grid**, adds global
  worker selection and a clickable cancel action, and indicates units
  currently holding position.
- Added headless `RTSOrdersTests` for hold/no-drift, defensive attacks,
  restoring normal orders, and interrupted attack-move continuation.

Planned follow-ups: explicit command queues/UI visualization, non-blocking
stuck unit recovery, tactical line-of-fire cover, stance presets, guard/
follow orders, formations, steering crowding benchmarks, hotkey remapping.

## Initial multi-unit formations

A new `RTSFormation` helper uses an **O(N log N)** ordered slot allocator,
assigning separate destination offsets for grouped move/attack-move/queued
move/patrol orders. It preserves original unit IDs across input permutations
and takes account of group travel direction and member footprint radii.
Each slot is individually clamped and solved through existing A* pathfinding.
The default `formation_spacing_multiplier=2.4` is overridable in authored
project metadata (bounded 1.5..6).

This is not yet a cohesive formation AI: groups do not hold an assigned shape
while crossing narrow passages, predict collisions, preserve squad facing
during chase, or use flow-field routing for hundreds of agents. Destination
slotting is nevertheless a substantial improvement over all-group members
being ordered to the identical terrain coordinate.

## Indexed creator data catalog

The DataCatalog now maintains a hash map of **object ID to stable vector
index**, replacing linear scans on each find/add/inheritance reference.
The index survives vector growth and normal catalog copy/move, with
validation rejecting identities mutated behind the catalog's back.
Read-only `entries()` prevents arbitrary unsynchronized vector identity
changes; field editing still uses `setField` or a mutable ID lookup.

A 2,400-derived-object regression tests indexing, validation, copying,
moving, field inheritance and JSON round trips. Full-scale creator
performance still requires cached resolved inheritance, incremental
property validation, virtualized data tables and measured editor timings;
this pass addresses the dominant repeated ID scanning first.

## Catalog ID migration (November 13, 2026)

F7 Ctrl+R and CLI --rename perform a transactional reference migration across
inheritance, Unit/Actor/Model links, scene placements, known instance overrides,
Skirmish role metadata and recognized trigger ECA catalog fields. Arbitrary
mod extension strings remain untouched; full schema-aware reference tracking
for third-party systems is still needed.

## Dynamic nav obstacles and production exits

Production now chooses a deterministic walkable outlet near its producing
building, biased toward the rally direction, from at most 24 candidate cells.
The outlet must be free of static pathing blockers and living mobile occupants;
a completed job remains pending if none is available. Enemy wave spawning uses
the same safe outlet search. Dead structures and depleted mineral patches now
release their blocked navigation cells via an event-driven grid rebuild rather
than leaving permanent invisible obstacles. These are not a full collision or
crowd solver and dynamic units can still congest chokepoints.

## Integrated dockable GUI and platform feature contract (new)

A pinned Dear ImGui docking layer (GLFW/OpenGL3 backend, same source for
Windows and Linux) replaces the old debug-text editor panels with mouse-
accessible browser and inspector workspaces. Terrain tools, brush settings,
catalog navigation/properties, scene hierarchy/transforms, and ECA trigger
authoring use EditorSession's existing editing logic. The original gameplay
HUD remains integrated; the top menu also allows switching directly into
the editor. Input is separated through ImGui's capture flags so clicking a
dock panel does not also issue game orders or terrain brush strokes.

Native binaries now expose a versioned, CI-verified `--features` contract
with the exact Git source SHA. The package verifier runs each built binary
on its host platform and compares flags/commit to the artifact identity.
Editor GUIs, fog, and HUD must all be compiled into both release outputs.
The Windows GUI still requires real local GPU/driver testing beyond headless
native verification.


## November 14: native UI substrate and live data relationships
The custom RTS HUD now has a native pause-menu companion with renderer-independent hit testing and GL presentation. Esc pauses the match and suppresses camera/world commands, with Resume/Restart/Editor/Exit actions. The executable advertises `RTS_FEATURE_NATIVE_PAUSE_MENU=1` on both targets.

Editor F7 now follows stable IDs for inherited links and production objects, shows reverse references and field provenance, and writes validated, undoable bool/numeric/string values. The editor *still uses Dear ImGui*, intentionally retained until native editor widgets reach feature parity. The full migration and missing gameplay systems are tracked in `docs/CREATOR_ARCHITECTURE.md`.


## Creator workflow improvements and sample-content identity

Default and shipped map catalogs have original Ranger/Rift Raider/Rift Bastion examples. Existing maps using the previous embedded catalog IDs are still loadable without replacing their data. The browser now filters by data type/family and virtualizes visible list rows; switching via catalog links selects the target family even when another filter is active. This improves the existing workbench without adding UI dependencies. The editor is still not a complete Galaxy Editor and the game mode remains a prototype.


## Bounded effect runtime — first authored graph

Effects are now named typed catalog entries: damage/heal leaves and ordered child sequences, bounded by traversal budget/depth, with cycle detection. `ApplyEffect` on `OnRegionEnter` applies the compiled graph to the actual entering unit in fixed-step simulation. Effect IDs are migrated and dependency-checked on rename/delete, including trigger references. Default starter catalogs include `kinetic_pulse` and `restorative_pulse`. The `effects_showcase` project contains an authored sequence and target region. The engine still lacks arbitrary weapon/ability activations, projectiles, area queries, behaviors and VFX actor dispatch.


## Mouse-operable ECA composer and shared schema (November 14)

The trigger authoring surface is no longer only raw JSON. The F6 inspector displays a real event/condition/action tree with selectable event types and typed property inputs; map creators can add, remove and reorder actions/conditions, select region/unit/effect references and control one-shot triggers. Complex mod extensions retain the advanced JSON escape hatch. `TriggerDefinition` is a shared, headless rule parser used by both editor and simulation, closing a previous validator mismatch that could leave invalid edits in the model. Each edit is atomic and undoable. The shipped native UI remains planned; these tools temporarily use the existing ImGui adapter while the custom UI toolkit is expanded. `docs/TRIGGER_EDITOR.md` is the detailed reference.


## Visual catalog reference editing

Production lists (`trainable_units`) and Effect `Sequence.children` lists no longer require manually typing arrays. A specialized in-editor reference widget supports Add/Open/Reorder/Remove, resolves only compatible catalog types, and commits through headless `EditorSession::setCatalogField`. Duplicate, missing, invalid, cyclic, or oversized references are rejected by the existing project/effect compilers without mutating the project. Native GUI migration remains a separate future stage.


## New functional creator diagnostics (November 14)

The editor can now inspect actual rule execution with a 192-entry ring, per-rule attempted/fired/blocked counters, failed condition reasons, and subject unit IDs. `CreatorTriggerMonitor.hpp` is UI-only; `TriggerEngine` owns the authoritative trace in shared C++ simulation. Traces reset on project reload and never become saved map state. It adds no third-party dependencies and keeps identical executable capabilities across Windows/Linux. The native pause menu also has reusable keyboard focus. Still missing: breakpoints, stepping, script variable inspection and complete native dockable editor tooling.


## Dynamic obstacles, navigation recovery and stable order (new)

A preexisting failure mode let live move orders continue toward now-blocked waypoints after construction or terrain changes. The engine now tracks changed navigation generations and cell revisions, validates remaining routes, replans affected ones and retires impossible legs so queued commands still execute. New building footprints refuse intersections with live mobile units. `blockDisc` avoids full terrain scans when updating small circular obstacles. Route and worker-harvesting execution is now ordered by unit ID instead of unspecified hash bucket iteration.

Tests include new moving-order reroutes and sealed/queued fallback, a large-map footprint equivalence fixture, invalid/no-op revision assertions, building occupancy protection and identical resource-contest simulations under opposite job insertion orders. These are essential groundwork for a recorded command stream and deterministic multiplayer, not claims those are already complete. [Open-source engine gap audit](RTS_ENGINE_AUDIT.md).


## Sample-independent runtime naming

Runtime production no longer injects fixed `Ranger`, `Worker`, `Barracks` or `Hybrid` prefixes. Names for created units and buildings resolve from the catalog's `name` property; AI reinforcements use the same path. Catalog-driven Field Medic production is verified headlessly. This matters for mod projects and does not require Blizzard-style unit types.


## Command stream and replay validation milestone

All core player mouse/HUD orders now flow through a bounded, tick-indexed command stream with captured unit IDs and stable order. The headless replay tape contains a snapshot of the authored map plus the commands and periodic canonical digests. A dedicated verifier can replay matches from disk without OpenGL, with explicit errors for invalid command data, inconsistent state or unsupported replay versions. A result channel reports rejected gameplay commands asynchronously to the editor/HUD adapter. Edited terrain or legacy immediate C++ order mutations intentionally invalidate export.

Windows and Linux compare each other's checkpoint fingerprints for the same replay scenario in CI. This is a substantive improvement over testing each OS in isolation, but deterministic network transport, snapshots/rollback, full actor/ability systems and native dockable editor widgets are still incomplete.


## AI combat policy milestone

The opponent now plans attacks through the fixed-tick command scheduler. The planner computes targets from a read-only world snapshot and returns source-1 attack commands sorted by unit ID. A basic local-defense behavior gives priority to player threats near the enemy base; absent such threats, enemies advance on the player's base. The guard distance and a combat-AI enable toggle are project metadata, validated with the authored map. AI-spawned reinforcements still come from the existing timed scenario system.

Replay JSON now defaults to v2 to include AI source commands, while v1 is recognized and retains historical direct-AI playback. This closes a replay-input gap but does **not** implement modder-authored AI graphs, broader worker economy, multibase strategy, enemy fog-of-war, squad tactics, networking or simulation-state snapshots.


## Automated cross-platform defense scenario

The new AI planner isn't just unit-tested in isolation: `ai_defense_behavior` runs an original authored match for 180 fixed ticks, asserts that idle enemy fighters issue source-1 attack orders against nearby player scouts, and produces a replay that verifies without rendering. CI compares the resulting fingerprints directly between MSVC/Windows and GCC/Linux, and Linux captures a screenshot of the actual AI skirmish under Mesa/Xvfb. This does **not** establish networking or comprehensive strategic AI parity.


### AI first-tick scheduling correction

The AI combat planner now runs before movement/tactical auto-acquisition each simulation tick, after processing already queued inputs. The order matters: adjacent player units can cause an enemy to leave IDLE during its ordinary unit update, so planning afterward would fail to record the explicit defense order. The original v1 direct-AI fallback remains at the prior position in the loop. The authored AI Defense sandbox's headless test now asserts this ordering and replay reconstruction.

## Trigger globals and deferred event delivery (November 14 continuation)

Added headless validated map-global integer state (`trigger_variables`, up to 64, signed ±100000000), `SetVariable`, `AddVariable`, `VariableCompare` with six comparison operators, and `OnVariableChanged` subscriptions. The event delivery queue coalesces first/last values and executes on the following simulation tick, without recursive ECA execution. Runtime variable maps and pending changes participate in replay state digests. F6 has undoable typed variable controls and the live trigger monitor shows initial/current values. The original `script_relay_showcase.rtsproject.json` exercises multi-rule scripted reinforcements/rewards. `RTSTriggerVariableTests` covers JSON roundtrip, invalid references, editor transactions, reset, repeat subscriptions, net-zero coalescing, sample runtime and replay. Windows and Linux must produce exact equal `trigger_globals_fingerprint.txt` records. This is not a full Galaxy Script language implementation or VM.

## Conditional script execution and reference integrity (November 14)

Real fixed-step `IfThenElse` and `Repeat` ECA actions support validated conditional branches and bounded iteration with optional dynamic integer-expression counts. The authoring parser rejects worst-case paths exceeding 512 action steps per invocation and nested depth above four. The F6 adapter exposes templates, branch summaries and validated Advanced JSON; full recursive GUI editing is still pending. Catalog Unit/Effect rename and safe deletion now traverse nested actions and conditions. Dedicated script-flow, expression, variable and nested-reference CTests run in both platform builds; dual-OS replay fingerprints and feature flags enforce parity. Full Galaxy source compiling and broader game systems remain future work.


## Data-driven timed combat effects and editable ECA (November 14 continuation)

**Engine:** Effect graphs can now emit `PeriodicDamage` and
`PeriodicHeal` operations, with integer tick interval/duration and a
bounded target-specific schedule. The shared 60 Hz simulation evaluates
pulses after combat and passive HP regeneration, removes expired/dead
targets, resets the queue at match reset, includes active effects in
deterministic replay digests and admits graph scheduling atomically up
to 256 active instances. `UnitData.hp_regen` can be authored in the
Unit catalog; default zero preserves existing combat behavior.
The actual playtest monitor exposes the number of active timed effects.

**Editor:** The F6 visual ECA inspector recursively edits nested
branches/repeat actions and typed arithmetic expression subtrees.
The F7 catalog browser can now create standalone combat units,
workers, structures, actors, models and multiple Effect kinds from
valid presets, then change Effect kind without authoring invalid
intermediate JSON. These are real validated/undoable document mutations,
not decorative mockups.

**Still missing for Galaxy-editor-class scope:** reusable scripting
functions, lexical locals, trigger libraries, variable arrays/structs,
behavior stacking, caster-resource management and abilities, weapon
target filters and projectile simulations, actor event graphs, importable
mesh/animation/audio pipelines, custom localization, complete terrain/
doodad layers and standalone editor toolkit parity. Avoid claiming
SC2/Galaxy feature parity from these foundational increments.


## Ability/energy fixed-tick vertical slice

UnitData now supports max_energy and energy_regen; Unit state includes
currentEnergy (initially full) and authoritative fixed-step regen.
DataCatalog defines Ability objects targeting Self/Ally/Enemy/Any
and applying one prevalidated Effect graph per cast. Unit entries
hold up to eight Ability IDs. RTSSkirmish enforces per-caster energy,
global mineral costs, owner/vision/range checks and a persistent
per-caster per-Ability cooldown clock. CastAbility is an appended,
source-restricted command kind in the same recorded game input stream
as movement/combat/production. State digests include energy for units
with a pool and active cooldowns, preserving earlier replay digests
on legacy zero-energy maps.

F7 exposes Ability creation and Unit assignment. F9 shows up to three
assigned skills as real command buttons/hotkeys with target picking.
The included Ability Workshop map uses original Field Mend/Arc Lance
content, and two new CTests and cross-OS fingerprint verification
exercise the entire loop. This is real functionality, but behavior
stacks, area/projectile casts, actor events, tech requirements,
channeling and editable command-card paging are not implemented.


## Authored status Behaviors and live F7/F6 data workflows

**Engine:** Behavior catalog objects are now linked by ApplyBehavior
and RemoveBehavior Effect nodes, which compose with immediate,
periodic, and Sequence operations. Active modifier instances apply
real additive attack/armor and bounded movement speed changes to
Units, stack independently per Behavior ID, and expire/dispel on
fixed simulation ticks. A per-target/status ordered map is limited
to 256 live keys; death and reset clean up state, and a dense-ID
lookup avoids repeated full Unit-vector scans. State digests and
recorded Ability casts verify status outcomes across OS builds.

**Editor:** F7 gains original Behavior presets, a grouped typed
modifier editor with one-step undo/redo, Behavior-linked Effect
templates, type conversion and target selection, nested visual
Effect flow with navigation and Sequence reorder/append/removal.
F6 exposes the current status watch. The live F9 HUD displays
effective attack/armor and status count for the selected Unit.
The Data inspector initial layout is wider/taller and avoids
clipped catalog browser controls.

**Missing:** movement suppression flags, aura propagation, stacking
categories/collisions, status-driven animations, target-area
searches, AoE projectile actors, tech requirements, complete
data schema visualization, editor-defined function libraries,
interruptible/channelled Abilities, networking, real multiplayer,
general asset import and Galaxy-language compatibility.


## Ground area impact implementation

Implemented in the native engine and creator: typed ground-target
Ability, circular AoE radius, Enemy/Ally/Any filters, 0..300 tick
delay, fixed-step impact-time victim detection, 64-target cap,
128 pending-impact cap, atomic multi-victim Effect admission,
authored Effect/Behavior Sequence compatibility, source-0 replay
commands, pending-impact digest hashing, F7 area editor and
undoable targeting conversion, F9 area cursor telegraph and
orange delayed ring, F6 pending impact monitor. A playable
original map and Windows/Linux CI coverage are included.

Still missing: true projectile actor trajectories, collision,
hit reactions, visual/sound impact events, splash falloff, line/
cone-shaped areas, network synchronization, more advanced AI
Ability casting and data-driven actor/weapon construction.


## Typed projectile/actor-delivery milestone

Implemented: authored Projectile catalog object, two original F7
presets, one-click grouped settings with validation/undo, Ground
Ability typed Projectile link, safe rename/deletion and Ground
target conversion, 60Hz flight quantized by speed, vertical
parabolic arcs, optional swept Unit-cylinder interception,
first-contact/Unit-ID stable tie breaks, tick-fresh spatial bucket
broadphase expanded by maximum Unit radius, explosion on contact
or destination, deferred postflight impact delay, shared
Effect/Behavior application and costs, state-digest replay hashing,
native 3D procedural projectile/trail rendering and expanding
impact flash, read-only F6 projectile watch, regression fixtures,
real OpenGL screenshot and Windows/Linux packaging parity.

Not yet complete: imported projectile models, actor-animation
state machines, sounds, particle effects, terrain collisions
midflight, moving/homing Unit-locked projectiles, networking,
true missiles for the normal weapon system and general scene
physics. The current renderer uses procedural geometry while
the simulation owns all gameplay decisions.

## Circle, cone, and line area geometry (November 15)

Runtime Ground Abilities now compile a search shape instead
of assuming every area is circular. Cone uses a bounded sector;
Line uses a forward rectangular strip. The caster-to-impact
direction is captured on command execution, while affected
Unit centers resolve at impact. Stable ID sorting, 64-target
atomic admission, team filters, Effect/Behavior rules and
replay recording remain on the same existing engine path.
Pending noncircular shape properties participate in the
authoritative replay state digest. F7 typed inputs, F6 read-only
monitor, F9 native preview and an original sample map are
wired end to end on shared Windows/Linux code.

## Reusable normal Weapon combat foundation

WeaponDefinition and compileWeapon validate independent normal-attack
profiles. The optional Unit weapon reference resolves into the existing
UnitData attack values at spawn, so legacy unlinked Unit stats behave as
before. CombatHit carries armor penetration into target damage resolution,
including Behavior armor buffs, without affecting Effects. The F7 Weapon
inspector, equip selector, typed project rename/deletion, native F9 HUD,
original Weapon Forge showcase and replayable attack CTests all use the same
headless Windows/Linux runtime. Still missing: ordinary attack projectiles,
multi-weapon loadouts, damage classes and richer Actor events.

## Shared normal Weapon Projectile delivery

Ordinary Weapon attacks can reference a validated Projectile and travel
with deterministic fixed-tick swept collision, launch-point snapshot,
dodge behavior, optional interception and standard armor penetration.
Legacy unlinked attacks remain instantaneous. F7 exposes Weapon to
Projectile authoring and a dockable Data Navigator for typed catalog
dependencies. The original Weapon Forge sample demonstrates this,
with new headless editor/runtime tests, replay fingerprints and Linux
OpenGL smoke captures. Remaining work includes homing, terrain mesh
collision, Warheads, target filters and Actor audiovisual events.

## Data-authored Actor Event presentation layer

The engine now compiles Actor event arrays from inherited catalog
fields and instance overrides, snapshots them when a Unit spawns,
and publishes bounded ephemeral cue records from actual fixed-tick
Attack, Projectile Launch/Impact, Death, Ability Cast and Behavior
Apply events. F9 renders Pulse/Ring/Beacon styles; F6 inspects cues.
All cues are ignored by authoritative replay hashes.

A new headless test compares identical combat state digests with
and without cue definitions every simulation tick. The original
Actor Event Laboratory project and strict schema/editor tests make
the feature portable across Windows/Linux builds. This remains
a visual-cue foundation, not sound, animation, particles, or complete
Galaxy Actor Events parity.

## Gameplay prerequisites and bounded Boolean graphs

Added headless RTSRequirements.hpp/.cpp with compileValidator
and compileRequirement. Validator kinds: HealthBelowPercent,
EnergyAtLeast, HasBehavior, IsStructure, OwnerUnitCountAtLeast.
Boolean Requirement nodes: Predicate, All, Any, Not. Graph compile
rejects cycles, missing typed refs, duplicate children, depth >8,
expanded node count >64 and invalid numeric values.

The live fixed-tick simulation enforces Ability gates when a
command is checked and Unit training gates when a production queue
entry is admitted, both with actual Unit/Behavior/owner state.
Ground/production links cannot inspect nonexistent targets.
Legacy unlinked objects and replay command formats remain unchanged.
F7 authoring, Data Navigator links, standalone Workshop map,
portable regression CTests and native screenshots are integrated.

## Persistent owner research and combat modifiers

RTSUpgrades.hpp/.cpp adds strict Upgrade schemas and producer lists.
RTSSkirmish applies ResearchUpgrade commands at deterministic ticks,
serializes research against production, enforces prerequisites/costs/
max levels, refunds cancel and destroyed producers, and propagates
tech levels to existing and future Units. Temporary Behavior modifiers
stay separate. Active jobs, completed levels and nonzero modifiers
are authoritative replay state; older no-tech fingerprints remain.
UpgradeLevelAtLeast integrates with previously authored Requirements.

## Live tech Show/Use policies

Upgrade, Unit training and Ability catalogs accept independent
Caster-only show_requirement graphs and existing Use requirement
graphs. The fixed-tick simulation filters native producer/ability
choices and enforces visibility during actual admission. All-hidden
producer card states show disabled EMPTY slots rather than generic
fallback Build/Ranger/Worker options; implicit legacy training
lists follow the same gate. Typed references, rename/deletion,
Undo/Redo, project tests and replay parity are covered.


## November 17 — native static asset import

F7 now imports raw glTF/GLB through an in-editor source picker using pinned
cgltf and stb_image, without an external Python process. Native quaternion
rotation, positive nonuniform scale, PNG/JPEG base color, validated publication
and a matching CLI are implemented; the existing Model/Actor registration and
F8 placement workflow remains. This is a single-node/primitive static subset,
not animation or full PBR support. See [native import](NATIVE_ASSET_IMPORT.md).
