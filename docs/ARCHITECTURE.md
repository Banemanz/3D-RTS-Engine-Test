# Architecture and technical design

## Source units

- `main_entry_point_render_loop.cpp`: GLFW host, input, shaders, procedural meshes,
  HUD and fixed-tick accumulator. Contains **rendering**, not game rules.
- `RTSSkirmish.hpp/.cpp`: owns all units/mineral deposits, resources, routes, jobs,
  construction/production queues, scripted opponent, and victory/defeat.
- `NavigationGrid.hpp/.cpp`: flat XZ passability map, nearest open-cell projection,
  deterministic tie breaks in 8-neighbor A*, no diagonal cutting.
- `Unit.hpp` + `unit_navigationagent3d_implementation.cpp`: existing unit movement
  steering, basic combat state machine and target tracking. Structures are nonmoving
  damageable Units; workers are Units with external gather jobs.
- `UnitData.hpp` + `unitdata_resource_implementation.cpp`: two embedded `.tres`
  presets, typed stats, numeric and faction validation.
- `SelectionManager`, `RTSCamera`, `Ray`, `Math3D`: interaction and math.
- `BitmapHUD.hpp`, `Shaders.hpp`, `GLCore.hpp`: OpenGL presentation.
- `tests`: logic-only tests, independent of GLFW and OpenGL.

## Simulation and deterministic assumptions

The host advances the game at **60 ticks/sec**. Rendering can have its own frame pacing.
A frame can simulate at most 8 ticks to avoid death spirals after multi-second stalls;
excess accumulated time is discarded. The `RTSSkirmish` logic uses fixed `dt` and
stable integer entity IDs. It does **not** yet use an event-buffered two-phase combat
resolver; identical commands at the same tick are expected to replay identically, but
the unit update order can influence simultaneous combat. Do not claim full lockstep
determinism suitable for multiplayer.

## Navigation

Coordinates use Y-up with ground XZ. Passability is represented on a 2-unit
49x49 grid spanning approximately [-49,+49] in X/Z. Static structure occupancy
and mineral patches are marked blocked. A* searches eight directions (octile
cost/heuristic), disallows cutting blocked corners, and projects blocked goal cells
to their nearest walkable neighbor. Routes are persisted as waypoints by integer
unit ID and reissued on arrival. Attack-move orders retain their path across
temporary aggro engagements. Shift-queued ground orders are keyed by stable
unit ID and become active after prior route completion. Combat chasing uses an additional per-unit waypoint route which replans periodically,
while the world prevents occupants from crossing blocked navigation cells. Moving
targets, chokepoints, unit/unit collisions and line-of-sight remain simplified.

## Economy and units

A worker job switches ToMineral -> Harvesting -> ToBase, with mineral cargo
credited on arrival at the friendly Command Center. Deposits have finite
capacity. Barracks construction reserves its 150-mineral cost and uses a timer.
Unit production reserves mineral cost and supply immediately. Queued production
at the same building advances sequentially; destroyed buildings refund unfinished
production costs. Dead player units release supply once after combat; unfinished production at
destroyed buildings refunds minerals and reserved supply.

## CPU/GPU responsibilities

Neither `RTSSkirmish`, `NavigationGrid`, `Unit` nor the test targets
require OpenGL or GLFW. Only the host executable pulls GLFW, GLSL, OpenGL loader
and HUD. Linux GitHub Actions compiles and runs all tests, then starts an actual
OpenGL 3.3 window in Mesa/Xvfb and captures a software-rendered frame.

## Known risks to tackle next

- Separate the renderer and UI host into small reusable classes and manage GL
  resource lifetimes explicitly (GPU objects currently survive until context teardown).
- Improve combat-repath efficiency and dynamic blocker handling; full physics/LOS is absent.
- Use spatial indexing for movement separation, nearest-enemy lookup and ID lookup.
- Add explicit command queue/shift-append, formation reservation and resource dropoff.
- Replace the prototype's permissive embedded data with files and a versioned schema.
- Improve combat by decoupling damage events from update order.

### Ground-level field of vision

`RTSVision.hpp/.cpp` owns explored and currently visible cells; it is
initialized by `RTSSkirmish::reset`, updated at 5 Hz and refreshed when terrain
editing syncs the heightfield. The source-of-truth is simulation-only, so
target eligibility, map icons and GPU rendering cannot diverge by accidentally
using separate visibility algorithms. The scene renderer uploads a compact
R8 texture only when the vision revision changes, and the terrain fragment
shader applies visibility brightness via that texture. The authoring modes
disable terrain fog to permit full scene placement. `sight_range` is stored
as a Unit catalog property (bounded 0..128 world units), and supports per-unit
instance overrides through the standard catalog resolver.

### Deterministic simulation broadphase

`UnitSpatialIndex` uses 8-world-unit buckets and a map of stable unit IDs
to vector indices. Each simulation tick rebuilds membership once, then
incrementally relocates individual entries after movement. Queries return
candidate indices in source-vector order, so a symmetric broadphase does
not arbitrarily change nearest-enemy ties or local separation accumulation.
`Unit::update`, `NavigationAgent3D::computeSteeringVelocity`, combat
acquisition and target ID resolution accept an optional index, preserving
direct all-pairs operation for focused standalone tests.

### Formation slot allocation

`RTSFormation.hpp/.cpp` is a pure geometry module taking stable unit IDs,
current positions and footprint radii plus a desired formation center. It
computes the centroid, travel direction and lateral basis, sorts members
front-to-back and laterally, and assigns unique grid slots in O(N log N).
The result preserves input-vector order, so `RTSSkirmish` can apply slots
without an extra search. `NavigationGrid::findPath` still resolves every
unit's destination and path separately. Project metadata configures
`formation_spacing_multiplier` (default 2.4, clamped 1.5..6), and
spatial hashing handles local dynamic separation while travelling.

### Catalog identity index

`DataCatalog` retains ordered `std::vector<CatalogEntry>` for JSON output
but tracks each ID in `std::unordered_map<std::string,std::size_t>`.
The value is a vector **index, not a pointer**, so reallocation and ordinary
copies/moves keep it valid. The index is maintained by `add` (including
derived-object creation), used by `find`, and checked during validation.
Catalog entries can be enumerated as a const vector; mutable edit operations
go through `find` and `setField`. Direct changes to an entry's ID are
unsupported and rejected by validation; a safe rename/reference migration
API remains an outstanding creator-suite feature.


## Headless trace channel and native focus component

`TriggerEngine` now exposes opt-in, fixed-step ECA trace records and per-rule counters. Each record contains a rule/event ID, monotonic sequence number, simulation timestamp, subject ID, outcome and first failed condition or executed actions. Trace history is ring-bounded to 192 records; project reconfiguration resets diagnostics, and map JSON is unaffected. The creator UI consumes that data without reinterpreting the scripting system. Capture off does not disable scripts. The trace is observational, not a breakpoint debugger.

`NativeFocus.hpp` holds a pure-C++ cyclic focus-list used for keyboard navigation in the custom OpenGL pause menu. Key and mouse actions share one handler. It has no platform or GPU dependencies; later native editor widgets can reuse this input primitive.


## Dynamic path topology, footprint updates and processing order

- `NavigationGrid::revision()` increments on real block/traversal-cost edits, never no-op paints.
- `RTSSkirmish::navigationGeneration_` increments on full occupancy rebuilds. Each active route stores its requested destination and the generation/revision of the grid used to create it.
- `NavigationGrid::routeClear` validates the remaining center-cell corridor and diagonal corner conditions **only when the grid has changed**. Invalid routes are replanned via the existing A*, and wholly sealed legs retire so later queued commands may execute.
- `NavigationGrid::blockDisc` stamps only cells within a circle's axis-aligned bounding box, not the entire world, with the same exact circular inclusion test. Invalid coordinates/radii are ignored.
- `RTSSkirmish::buildBarracks` rejects footprints intersecting living mobile units; those rejects do not charge minerals or change occupancy.
- The route and worker-job stores are ordered by numeric unit ID to make path/harvesting updates independent of hash/insertion order. **This does not yet guarantee bitwise float determinism across Windows/Linux.** Full command-stream/replay determinism remains the priority.

Technical research and missing systems are tracked in [RTS_ENGINE_AUDIT.md](RTS_ENGINE_AUDIT.md).


## Authoritative command ingress and replay consistency

`RTSCommandStream.hpp/.cpp` defines a bounded, versioned command schema with explicit selected unit IDs, target tick, input sequence, source, order kind, target point or entity ID, append semantics, and optional catalog ID. The world applies commands at the **start of each 60 Hz fixed-step update**. Game commands may be deferred a tick; selection changes later cannot retarget them. The existing direct-order methods remain for integration/testing, but calling them outside the command dispatcher invalidates replay export instead of silently recording incomplete input.

The self-contained `RTSReplayTape` stores the starting `RTSProject`, processed input commands and periodic state digests. `verifyRTSReplay` reloads the authored project headlessly, schedules the stored inputs at their recorded ticks and compares canonical 64-bit FNV-1a digests of quantized simulation state (units, resources, production, pathing, visibility and triggers). `RTSSkirmish::takeCommandOutcomes` exposes bounded accepted/rejected results for GUI feedback without perturbing replay state.

CI builds `RTSCommandReplayTests` and `replay_cli`, and compares matching checkpoint files **across Windows and Linux** with a dedicated `replay-parity` job. It detects divergence for the tested scenario; it is not a proof that all maps/platform math behave bit-identically. [REPLAY_COMMANDS.md](REPLAY_COMMANDS.md).


## Map-authored AI combat behavior

The project `metadata` object supports two new validated keys: `"enemy_ai_enabled": false` disables automated enemy Attack orders without removing enemy units or simulation-driven spawns; `"enemy_ai_guard_radius": 16` makes enemy idle units prioritize attacking player units within 16 world units of their base before targeting the player's command center. The guard radius must be a number from 0 to 80; a zero radius disables local defense prioritization, **not** base aggression. Omitted fields use `true` and `14`.

For an example, open `projects/ai_defense_showcase.rtsproject.json` (an original scenario with two frontier scouts advanced near enemy territory). The map's settings can also be edited through JSON or `RTSProjectTool --set-root <map> metadata <json-object>` and are validated at load/save time. AI uses the same source-labelled fixed-step execution as player commands. This is presently a minimal combat policy, not a full RTS build-order/economy editor.


## Runtime Behavior state, stat derivation and deterministic lifecycle

The shared DataCatalog defines a typed BehaviorDefinition with
duration_ticks, max_stacks, armor_bonus, damage_bonus,
speed_multiplier, refresh_duration and dispellable. A new
EffectOperationKind ApplyBehavior or RemoveBehavior includes a
typed Behavior catalog ID. compileEffect validates every linked
Behavior and all nested Sequence edges before authoring save or
gameplay. Effect graph bounds remain 16 levels/64 visits.

RTSSkirmish owns a bounded map keyed by (target Unit ID, Behavior ID)
with stack count and absolute expiry tick. Map key ordering is stable
on Windows/Linux; 256 unique live instances is the hard limit.
Pre-admission checks all new unique status keys before any immediate
operations in that Effect invocation, so saturated status schedules
cannot partially damage/modify a target. Existing timed-effect slot
limits are independent. Reapply increments up to max_stacks and
renews remaining time only if refresh_duration is true. RemoveBehavior
respects dispellable. Expired and dead targets are cleaned at fixed
tick boundaries, independent of render refresh, while reset clears
all live status state.

UnitData remains immutable. Unit instances have derived armor/damage
and navigation speed modifiers. The authoritative combat path uses
derived armor for damage mitigation and derived attack damage on
recorded combat hits; NavigationAgent3D uses a scaled maxSpeed while
active. Removing the final status restores baseline values exactly.
Dense Behavior cleanup uses an ephemeral alive-Unit ID index to avoid
256 repeated scans across large maps; sparse instances skip the
index build. No UI or OS clock influences these decisions.

Nonempty Behavior maps are included in stateDigest with target ID,
Behavior ID, stacks and expiry tick. Legacy matches with no statuses
preserve their older digest shape. Gameplay command tapes already
capture Ability→Effect→Behavior execution through CastAbility,
and Windows/MSVC vs Linux/GCC CI compares a new behavior fingerprint.
