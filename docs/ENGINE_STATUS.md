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
| Marine/Worker training, economy/supply | Implemented | Per-building queues, paid minerals, reserved supply |
| Enemy skirmish behavior | Minimal | Scripted reinforcements; simple unit target selection |
| Victory/defeat and restart | Implemented | Destroy Command Center or Hive |
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
