# RTS engine foundation audit — original creator engine

Updated November 14, 2026. This is the implementation checklist for Draft PR #1 (`feat/rts-vertical-slice`), not a promise that unimplemented systems are production-ready. Preserve **one C++17 simulation and project format** for Windows x64 and Linux x86_64.

## External engineering references (read-only design study)

| Project | Useful engine principle | Direct implementation here |
|---|---|---|
| [Recoil/Spring](https://github.com/beyond-all-reason/RecoilEngine) | Its `IPathManager` supports terrain-change path updates; `MoveDef` distinguishes mobility/footprints; a synchronized simulation is separate from visual/UI Lua | Local grid-change revision, route corridor validation, route repair. Move classes/air/hover and asynchronous budgets are **not** implemented |
| [0 A.D.](https://docs.wildfiregames.com/entity-docs/r28/components/unitmotion/) | UnitMotion has passability class, acceleration, turn behavior, weight/pushing | Our movement speed/separation is still rudimentary; authorable passability and acceleration/turn schemas are future work |
| [OpenRA](https://github.com/OpenRA/OpenRA) | Independently configurable traits, order/activity model, map scripting and headless simulation | DataCatalog+Effect+ECA exist as a smaller foundation; full component traits and serialized orders are **not** implemented |
| [Zero-K](https://github.com/ZeroK-RTS/Zero-K) | Large game logic lives in data/modules separate from the general engine; synced rules and unsynced UI have different responsibilities | The editable project is separate from engine simulation and UI; scripting plugin API and mod dependency resolution remain missing |
| [0 A.D. Atlas](https://play0ad.com/) / [Galaxy editor docs](https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/003_Navigating_the_Interface/) | Map authoring, typed data links, script/event tools and test-in-editor are interoperable modules | Native GUI migration, asset import and richer world editor remain substantial open work |

**Intellectual property:** These are architecture and UX references only. Do not copy GPL source into this repository without proper project-wide licensing review. No borrowed proprietary characters, maps, art, data, sounds or editor graphics are needed. The included sample roster is original and legacy user map IDs remain loadable.

## Core movement work completed in this pass

1. **Topology change versions:** `NavigationGrid::revision()` increments only when a blocked cell or terrain traversal cost actually changes. A separate `RTSSkirmish::navigationGeneration_` covers complete grid rebuilds (e.g. destroyed structures). No-op paints do not invalidate routes.
2. **Corridor validation:** The engine tests remaining contiguous grid waypoints when a version changes, including diagonal side-cell blockage. An actually interrupted corridor is replanned from current unit position to the original command target. Unaffected routes are kept rather than rerunning A* every tick.
3. **Unreachable route recovery:** Unreachable active legs end instead of leaving units permanently walking into blocked cells. Existing *queued* move commands advance when the earlier leg becomes impossible.
4. **Construction safety:** Structure footprints reject placement on living mobile units before spending resources. Placed structures become grid obstacles; their removal and mineral depletion update navigation.
5. **Local blocker stamping:** `NavigationGrid::blockDisc` visits only the grid cells in a bounding rectangle around the circular footprint, then performs the exact radius test. This avoids scanning an entire large map for each small structure.
6. **Predictable processing:** `routes_` and `jobs_` use numeric unit-ID ordered maps, so simultaneous waypoint and resource-contention processing is not controlled by unordered hash-bucket iteration. This is only one step toward fully deterministic cross-platform multiplayer.

### Invariants and regression gates

- `RTSNavigationTests`: corner-cut prevention, weighted paths, revision/no-op behavior, remaining-corridor invalidation, footprint stamping exactness across a large off-center grid and edge/off-map cases.
- `RTSDynamicNavigationTests`: an order detours around a newly placed blocker, avoids blocked cells and reaches its destination; a fully sealed first leg is abandoned and a reachable queued order executes.
- `RTSProductionSpawnTests`: no building placement atop living units or premature resource deduction, while a separate legal building placement updates blocked cells.
- `RTSResourceDeterminismTests`: two games with identical map/state but opposite worker job insertion order match on every fixed simulation tick while harvesting a contested deposit.
- All tests run in Windows/Linux Debug and with assertions preserved in Release; real OpenGL smoke remains separate. Runtime `--features` must self-report `RTS_FEATURE_DYNAMIC_NAV_REPLAN=1` and `RTS_FEATURE_STABLE_SIM_ORDER=1` in both platform bundles.

## Engineering gaps and sensible execution order

| Priority | Missing/incomplete foundation | Next implementation slice and acceptance |
|---|---|---|
| P0 | Authoritative command stream / replays | Serialize typed, tick-indexed user/AI commands; feed only a simulation command queue; replay fixtures reproduce a versioned state digest on both OS targets |
| P0 | Simulation determinism beyond iteration order | Audit all unordered iteration, random sources, floating-point accumulators and entity lifetimes; use stable IDs, define quantization/fixed precision policy; compare long headless replay hashes cross-platform |
| P0 | Pathing at RTS scale | Introduce unit-specific footprint/passability profiles; cache occupancy masks; budget and prioritize replan requests under heavy construction; test 1,000 units and multiple terrain sizes |
| P1 | Movement and collision | Ground/air/hover layers, acceleration/turning, avoid shoving immobile objects, squeeze/choke behavior, movement cost updates and valid goal projection |
| P1 | Data-driven game actors | Ability/behavior/weapon/mover definitions, actor/event-based animation/audio/VFX independent of authoritative damage, source-asset import/validation and native model previews |
| P1 | Gameplay AI | Typed policies for economy, production, defense, expansion and threat perception, using the same command API as human players; deterministic headless scenarios |
| P1 | Map-world creator | True native dockable editor widgets, content/search browser, transform gizmos and snaps, multiple terrain layers, region tools, diagnostics, previews, undo/autosave recovery |
| P2 | Scripting | Composable event/condition/action groups, scopes/variables, typed reference validation, breakpoints/step/watch and versioned script migration; preserve bounded execution |
| P2 | Content deployment | Portable map/mod archives, explicit dependency/version rules, original license metadata, schema migrations, safe imports and error reporting |
| P2 | Network/multiplayer | Decide lockstep versus server authority only after state determinism/command replay benchmarks; no speculative socket code before authoritative tests |

### Non-goals for this increment

This pass does **not** create a flow-field solver, hierarchical pathfinder, aerial movement, full multiplayer determinism, a new renderer, or native editor UI parity. As map and unit counts grow, benchmark CPU/path-query time and seek-cost distribution rather than assuming a linear speedup.

## CI/packaging contract

- Same CMake source libraries and `RTSCameraTerrain` feature flags on Windows and Linux; no OS-specific simulation branches.
- `ctest --output-on-failure` for every test in both Debug and Release; the installed executable's `--features` output and embedded commit are validated by `tools/verify_build_bundle.py`.
- Linux Mesa/Xvfb renders the actual gameplay and authoring workspaces (terrain, scene, data, triggers, HUD, native pause and trigger monitor). Screenshots are debug artifacts, **not executable ZIPs**.
- Manual Windows 10/11 GPU/editor input validation is still required in addition to automated parity tests.
- Keep working inside the existing **Draft PR #1**, keep this audit and `archive/new chat/RTS_HANDOFF.md` updated and do not merge the PR without review.
