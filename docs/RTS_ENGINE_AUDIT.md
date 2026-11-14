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


## Data-driven display names

The game runtime now resolves spawned entity names from `UnitData::resourceName` (inherited Unit catalog `name`) for production, construction and AI reinforcement paths. Previous hardcoded example roles are no longer responsible for spawned display labels. The `RTSProductionCatalogTests` fixture verifies that a custom `Field Medic` produces an appropriately named live unit. Scene placements continue honoring explicit per-instance names. These changes are original and retain compatibility with existing project/catalog identities.


## Implemented: recorded simulation commands

The previous roadmap item for *typed, tick-indexed command streams* is now a functional first stage, not purely planned. `RTSCommandStream` accepts bounded commands targeting a future 60 Hz simulation tick with explicit unit IDs, typed kind, point/target/catalog and insertion sequence. The engine executes them at the beginning of the authoritative fixed step and reports accepted/denied results separately from gameplay logic. All player mouse/HUD action dispatches were migrated to this queue; UI-only camera/selection/page controls remain presentation state.

`RTSReplayTape` records a versioned map snapshot, successful **and failed** scheduled commands and **per 60 simulation ticks** canonical state digests. `verifyRTSReplay` and `RTSProjectTool --verify-replay` reconstruct and check the run. Windows/Linux CI compares every checkpoint of a scripted scenario via the dedicated `replay-parity` job. Recording is bounded (24 hours, 32 MiB, 32,768 commands), while live gameplay input continues beyond those limits. The new `REPLAY_COMMANDS.md` is included in both packaged executables. No new runtime GUI dependency or third-party networking library was introduced.

Next work on the command system: AI-originated and network-originated authorized commands, richer failure reasons, snapshot/restore and timeline scrubbing. Replay tools do **not** yet constitute live multiplayer or cryptographic anti-cheat.


## First recorded-AI control boundary

The first AI command-routing foundation is complete. `RTSAIPlanner` selects defensive threats only near the enemy base using deterministic distance/unit-ID tie breaking; otherwise enemies attack the player's base. Its output is ordinary source-1 `RTSCommand` objects, not immediate mutations. AI attacks are validated for source/owner/kind/target on entry. The engine's earlier automatic reinforcement spawner is still fixed-simulation logic, and the AI does not yet issue economy, production, or building commands.

`RTSReplayTape` upgraded to format 2 to record human **and AI** orders; the verifier disables AI planning during v2 playback. Format-1 inputs remain loadable using the old direct-AI path. This completes the earlier roadmap entry *route opponent combat AI through command stream*, but not AI-authoring/strategic economy tasks, replay scrubbing or networking. Suggested next slice: design reusable faction-agnostic strategy tasks (economy, production, scouting, defense), then execute via the source-safe command scheduler, with scenarios that demonstrate measurable outcomes.

## Script-value and expression foundations (November 14)

Following the authoritative command and replay work, scripted ECA operands now accept a compact validated integer expression AST. Nested operations use 64-bit intermediates with saturating ±100000000 results and a strict depth/node budget. Validation is performed in the headless TriggerDefinition authoring module, while fixed-tick execution uses the identical syntax. Project-level variable rename traverses only recognized expression nodes; arbitrary extension data is not rewritten. Independent test coverage includes malformed ASTs, safe arithmetic, full-map event/replay integration, editor undo/redo, and mandatory Linux/GCC vs Windows/MSVC digest comparison. This is a step toward typed logic libraries; an untrusted native-code or full Galaxy interpreter is not introduced.


## Ability data and Unit energy milestone

The first Unit Ability runtime now uses an explicit ability→effect
catalog link and Unit→ability ordered references. It checks target
policy, vision/range, energy/mineral affordability and per-caster
cooldown in authoritative fixed simulation ticks; player casting
is serialized into the existing deterministic command/replay input.
The F7 editor can create/link/edit these objects, the F9 command grid
actually casts them, and Windows/MSVC and Linux/GCC replay fingerprints
are required to match.

Remaining priorities: generalized behavior stacks, area/projectile
targeting, data-driven weapons and actor events, robust ability
requirements and cast states, command-card pagination and richer F7
schema/property editors. AI still lacks a general authored production,
economy and ability-usage policy. Preserve both OSes and all tests.
