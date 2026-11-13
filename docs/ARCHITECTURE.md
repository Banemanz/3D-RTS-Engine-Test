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
