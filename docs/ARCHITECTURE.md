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
temporary aggro engagements. **Direct combat chasing still ignores A***; it
requires an explicit tactical-repath system.

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
- Add combat path planning/replanning and collision resolution against blockers.
- Use spatial indexing for movement separation, nearest-enemy lookup and ID lookup.
- Add explicit command queue/shift-append, formation reservation and resource dropoff.
- Replace the prototype's permissive embedded data with files and a versioned schema.
- Improve combat by decoupling damage events from update order.
