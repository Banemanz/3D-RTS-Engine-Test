# Testing and verification

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DRTS_BUILD_DEMO=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The headless suite tests A* routing around blockers, disconnection and diagonal
corner behavior, skirmish initial state, economy/production, bounded commands,
worker harvesting, and UnitData validation. The Linux build additionally compiles
the full OpenGL program and starts it with Mesa software rendering under Xvfb:

```sh
xvfb-run -a -s "-screen 0 1280x720x24" env LIBGL_ALWAYS_SOFTWARE=1 \
  ./build/RTSCameraTerrain --smoke-frames 120 --screenshot rts-smoke.ppm
python3 tools/ppm_to_png.py rts-smoke.ppm rts-smoke.png
```

The screenshot is uploaded by Actions as `rts-software-opengl-screenshot`.
This proves the renderer reached a rendered frame without crashing, but does
not validate HUD legibility or full interactive gameplay. Manual regression
should exercise selection, movement, attack-move, mining, construction, training,
combat, restart, and high-DPI picking. Record GPU and OS when reporting results.

## Optimized build artifact workflow

See [Build artifacts](BUILD_ARTIFACTS.md). The Release workflow runs CTest
with `assert()` re-enabled on the test executables; otherwise `NDEBUG`
would silently disable much of the regression suite. A successful run also
verifies the installed project maps and starts the *packaged* Linux terrain
editor under Xvfb. Windows GUI runtime behavior still requires manual testing.

### Fog of war and LOS

`RTSVisionTests` cover live-to-explored transition, hill occlusion,
unchanged-revision suppression of GPU updates, fog texture bytes and
data-driven Unit sight range. Linux OpenGL smoke tests compile and run the
new terrain shader/texture under software Mesa; they do not guarantee the
same results on all Windows GPU drivers.

### Unit broadphase and ownership

`RTSSpatialTests` creates 2,000 deterministic test units, compares radial
broadphase hits against exhaustive scans, exercises negative coordinates,
incremental movement and ID lookup, and verifies that indexed steering
matches direct steering. An additional test confirms owner-based hostility
regardless of appearance/faction. Existing skirmish and chase tests continue
to exercise the new fixed-tick integration on Windows and Linux.

### Visual selection-card regression

Linux C++ CI now launches a gameplay capture with
`--smoke-select-army --smoke-frames 80 --screenshot rts-group-hud.ppm`,
verifies the OpenGL HUD renders, converts the result to PNG and uploads a
`rts-group-selection-hud-screenshot` artifact. A separate
`--data-editor` capture exercises the searchable catalog panel.
`RTSHUDTests` verify individual card hit boxes and multi-selection
unit-ID mapping independently of GLFW or OpenGL.

### Cross-workspace editor lifecycle / crash regression

CI runs `RTSCameraTerrain --editor-stress-smoke --smoke-frames 240` with a
loaded project under Linux Mesa/Xvfb, alternating real terrain sculpt,
scene-object rotation, trigger creation/JSON edit, catalog search/clone and
undo/redo while reloading the project into the rendering engine.
The Release package runs an additional 192-frame version from its installed
folder. Tests do not save changes and fail if a C++ exception escapes.
This targets editor lifecycle faults that simple screenshot startup tests
would miss. It still does not prove stability on every Windows GPU.

### Attack-move interruption and stance regression

`RTSOrdersTests` exercises a nonmoving Hold Position unit that attacks an
enemy structure in range, verifies that it cannot drift from separation or
momentum, checks Stop and Move clear the hold flag, and verifies that an
attack-move interrupted by a fight still completes its original destination.
`RTSHUDTests` covers the new 12-button hit regions at 1280×720 and 800×600.
Both are compiled as headless CTest executables and checked by Windows/Linux
CI. The OpenGL smoke test catches HUD shader/viewport regressions.

### Formation slotting and group movement

`RTSFormationTests` verifies nonoverlapping 9-unit target slots, centered
layout, stable per-ID assignments independent of source ordering, single-unit
exact destinations, designer-configured spacing and four selected units
travelling to a formation instead of a single point. It runs headless in
Debug and optimized Release CTest builds.

### Creator catalog scaling

`RTSCatalogTests` now creates 2,400 derived object definitions with stable
IDs, verifies constant-time indexed lookup through repeated vector growth,
copy/move and serialization, checks inherited field isolation, and
confirms out-of-band ID mutation is detected before project save.
This is a correctness/scaling regression, not a portable timing benchmark.

### Executable build identity and GUI parity

Each Windows/Linux Release package is rejected unless the shipped
`RTSCameraTerrain[.exe] --features` output includes the expected Git commit
SHA and the fog/gameplay HUD/creator GUI/terrain/data/scene/trigger editor
capabilities. This verification executes the **native binary** on each
runner, not a Python mirror or manually authored manifest. The Linux CI
OpenGL smoke captures provide graphical regression evidence of the docked
creator workspace; Windows Release executes the binary without an OpenGL
context for feature/identity parity but still needs a real GPU UI test.


## Native pause menu parity / screenshot
`RTSNativeMenuTests` verifies the dependency-free layout and action targets across 1280×720, 640×480, 320×240, 1920×1080 and the degenerate 1×1 viewport, on both CTest runners. Linux Debug also boots the engine with `--smoke-pause-menu --smoke-frames 35 --screenshot rts-pause.ppm` under Mesa/Xvfb, converts PNG and uploads it. The release packager enforces `RTS_FEATURE_NATIVE_PAUSE_MENU=1` on both binaries; feature text is read from the native executable, never guessed from a manifest. The editor lifecycle stress remains mandatory. Test actual Windows GPU/editor interaction separately.


## Legacy starter-content and catalog family regression

Headless tests now exercise all-new starter IDs and a previously serialized map's original catalog names. The Data Editor test covers family switching, nonexistent categories and link-based navigation across type filters; the Linux OpenGL UI smoke exercises the real browser. Windows/Linux feature flags, binary packaging and CTests stay aligned.


## Typed ECA authoring and real GUI smoke

`RTSTriggerAuthoringTests` runs on **Windows and Linux** under CTest Debug and Release. Tests cover action/condition lists, event types and timing, catalog/effect references, index boundaries, no-op edits, invalid values, project save/load, raw JSON rollback and chronological undo/redo. `CreatorTriggerWidgets.hpp` renders the actual F6 inspector in the Linux Mesa/Xvfb screenshot test using `projects/effects_showcase.rtsproject.json`. The installed Windows and Linux `RTSCameraTerrain` executables advertise `RTS_FEATURE_VISUAL_ECA_EDITOR=1`, checked directly by the release bundle verifier. Windows desktop/GPU UI testing remains necessary independently of CI.


## Trainable and Effect reference-list regressions

`RTSReferenceAuthoringTests` checks that adding/reordering production and Effect sequence references uses the same validated, undoable project transaction. Tests cover duplicate training IDs, illegal structure training targets, effect graph cycles, empty sequences, undo/redo and JSON round trips. It is built and run through CTest in both Windows/Linux Debug and Release configurations. The visual reference list itself is rendered by `CreatorReferenceWidgets.hpp` in the regular Linux GUI smoke, with manual Windows GUI verification still required.


## New regression tests and GPU captures

`RTSTriggerTraceTests` runs on Windows/Linux CTest and checks chronological timestamps, fired/blocked event reasons, opt-in capture, default gameplay equivalence with tracing on/off, bounded 192-record history, and reset semantics. `RTSNativeMenuTests` now exercises focus movement/wrapping and focused action selection, in addition to size-dependent hit-testing. `RTSProjectTool --validate projects/trigger_diagnostics_showcase.rtsproject.json` runs in both suites.

Linux Debug additionally executes the real `--smoke-trigger-monitor` OpenGL editor with the sample project, converts the screenshot to PNG and uploads `rts-trigger-trace-monitor-screenshot`. Windows/Linux Release packaging verifies `RTS_FEATURE_TRIGGER_TRACE_MONITOR=1` from the actual executable. Native Windows GPU testing remains a manual acceptance step, not implied by Linux software OpenGL verification.


## Movement and order recovery regression (November 14)

New `RTSDynamicNavigationTests` issues movement orders before adding blockers: verifies a successful detour, no visits to blocked cells and correct execution of a reachable queued command when the first leg is later separated by an impassable wall. `RTSNavigationTests` exercises revision changes only on actual edits, diagonal corner invalidation and exact optimized footprint stamping over a large off-origin grid with overlapping/off-map circles. `RTSProductionSpawnTests` rejects building a structure on a live worker, proves no mineral debit on rejection, then accepts a clear neighboring build site.

`RTSResourceDeterminismTests` issues identical two-worker resource orders in opposite insertion sequences, compares all fixed-tick resource/position/HP/state outcomes and proves identical contention results. All tests are in Windows/Linux CTest Debug/Release with Release assertions retained, and feature flags in both installed native executables are checked. GPU editor screenshot smoke remains unchanged. This is **not** a cross-platform bitwise-deterministic multiplayer guarantee.


## Command/replay platform acceptance

`RTSCommandReplayTests` checks fixed-tick scheduling, immutable selection ID snapshots, command ordering and duplicate rejection, replay JSON and disk roundtrip, rejecting tampered checksums/commands, out-of-horizon behavior, unrecorded direct gameplay changes, and rejected-order notifications. A separate `replay_cli` CTest invokes the actual `RTSProjectTool --verify-replay` against its generated disk fixture. Both Windows and Linux run the tests with Release assertions enabled.

Each Debug CI job uploads a `command_replay_fingerprint.txt` containing **every checkpoint** from the same scenario. The mandatory `replay-parity` job downloads both OS artifacts and requires exact line-by-line equality. This tests real MSVC/GCC divergence (for one deterministic fixture) beyond independent green suites; it is not a universal bit-for-bit reproducibility proof. Native binary feature parity includes `RTS_FEATURE_COMMAND_STREAM` and `RTS_FEATURE_REPLAY_VERIFY`. See `REPLAY_COMMANDS.md`.


## New AI and replay regression gates

`RTSAIPlannerTests` verifies sorted stable-id planning independent of vector insertion order, defensive threat prioritization, strategic base fallback, enabled/disabled policy, guard-radius handling and a full headless replay with AI off. `RTSProjectTests` validates round-trip metadata and rejects wrong-type/out-of-range AI settings. `RTSCommandReplayTests` now requires that live AI commands are actually recorded, rejects command-tampering/unauthorized AI actions and verifies legacy format-1 replay compatibility. A new `ai_defense_showcase` CTest validates the example map.

Windows/Linux Debug and Release all compile the same planner and preserve assertions. Windows/Linux replay fingerprints are still compared in a dedicated job; the installed executable advertises `RTS_FEATURE_AI_COMMAND_PLANNER=1` on both platforms. Interactive Windows GPU behavior remains a separate manual acceptance check.


## AI-defense cross-OS acceptance (November 14)

The `ai_defense_behavior` CTest loads the original `projects/ai_defense_showcase.rtsproject.json`, confirms that the first recorded AI orders target the advanced Frontier scouts rather than the remote player's base, runs 180 fixed simulation steps, verifies the full self-contained replay and writes `ai_defense_fingerprint.txt`. Each Windows/MSVC and Linux/GCC CI job uploads that fingerprint beside the existing standard replay fixture. The `replay-parity` job now **diffs both scenarios**, so local defense and real gameplay/combat movement are cross-platform-tested, not merely base-target scripting.

Linux Mesa/Xvfb additionally launches the actual game on the AI defense map for 180 smoke frames and uploads an image artifact named `rts-ai-defense-screenshot`. It is a real executable capture, not a constructed mock-up. Windows Release and Linux Release keep assertion-based headless tests, the original AI sample and self-reported executable feature parity. If the current HEAD fails any of these checks, review the corresponding CTest/renderer logs rather than claiming completion.


### First-tick AI guard regression

The `ai_defense_behavior` fixture intentionally begins with player scouts close enough to trigger normal tactical auto-targeting. Its first two fixed frames must nevertheless contain explicit source-1 recorded defensive attacks. This guards against moving the planner behind `Unit::update` and silently bypassing the command log. It also runs 180 ticks, verifies the complete replay, exports cross-platform fingerprints and feeds the mandatory parity comparator.


### Windows upload contract for dual-scenario parity

Both `linux` and `windows` jobs must upload **both** `build/command_replay_fingerprint.txt` and `build/ai_defense_fingerprint.txt`. The `replay-parity` job downloads those artifacts and fails on a missing file or differing digest. The initial AI-sandbox CI pass exposed a Windows upload step that still listed only the original file, even though `ai_defense_behavior` itself passed. The artifact step was corrected without relaxing or removing the comparison.

## Script-global parity and editor transaction regression (November 14)

`RTSTriggerVariableTests` exercises map-local script globals: declaration validation, bounded signed integers, set/add, operator comparisons, blocked/once rule behavior, reset, editor undo/redo and safe rejection of dangling variable references. It writes `trigger_globals_fingerprint.txt` for the same 120-tick fixed-step scenario under both MSVC and GCC. CI uploads the Windows and Linux files and the required `replay-parity` job compares exact tick/digest lines. Replay verification re-simulates the actual map, and the state digest includes variables only when present so existing v1/v2 projects retain old hashes. Linux OpenGL and native Windows packaging must still pass on the branch head.

## Nested script action regression (November 14)

`RTSScriptFlowTests` checks actual fixed-step `IfThenElse` and `Repeat` execution, dynamic expression counts, live variable-change notifications, typed condition routing, deterministic mineral effects, project serialization, replay verification, script-global rename through nested actions, and undo/redo. It also rejects oversized loop execution budgets and invalid branch shape. Both Linux/GCC and Windows/MSVC upload `script_flow_fingerprint.txt`; the mandatory replay-parity job requires byte-identical tick/digest rows. Assert remains enabled in Debug and Release. The scripted relay project includes a working branch and bounded repeat action.

## Nested ECA reference integrity and binary flags

`RTSNestedCatalogReferenceTests` makes Unit/Effect catalog refs inside nested IfThenElse and Repeat actions, checks both inbound-usage discovery and dependency-safe deletion, renames both catalog IDs, and confirms serialized branches retain valid references while ordinary text is unchanged. The executable package must expose `RTS_FEATURE_SCRIPT_EXPRESSIONS=1` and `RTS_FEATURE_SCRIPT_CONTROL_FLOW=1` alongside its earlier `RTS_FEATURE_SCRIPT_GLOBALS=1` on **both** Windows and Linux. The bundle verifier rejects missing implementation flags and mismatched source SHAs.

## Verified regression gate after nested ECA and script-expression changes

Functional commit `b9e84cf3` passed Release Windows/Linux jobs in [workflow run 43634965257](https://github.com/Banemanz/3D-RTS-Engine-Test/actions/runs/43634965257), with all 44 CTests, portable packaging and the required binary self-report feature flags. Debug push/PR runs [43634965221](https://github.com/Banemanz/3D-RTS-Engine-Test/actions/runs/43634965221) and [43634970215](https://github.com/Banemanz/3D-RTS-Engine-Test/actions/runs/43634970215) passed Linux/Windows tests, Mesa/Xvfb GUI editor smoke and strict five-file cross-compiler replay parity.

**Regression root causes corrected:** Four newly added assertion-driven test executables had not been listed in the Release `-UNDEBUG`/`/UNDEBUG` target list, causing whole `assert(...)` calls to be optimized away in Release. This produced misleading false passes and a `nested_catalog_references` fixture abort. The target list now includes all new tests, CTest's `trigger_trace` command is restored, and each new test source includes a compile-time `#error` when `NDEBUG` is defined. The initial Debug replay-parity failure was a Windows artifact upload omission (new `script_expression_fingerprint.txt` and `script_flow_fingerprint.txt` files). Both are now uploaded for Windows and Linux. Parity comparisons remain strict, not disabled or skipped.


## Recursive authoring, new catalog creators and timed effects (November 14)

`RTSRecursiveTriggerEditorTests` tests nested Add/Duplicate/Move/
Delete, typed property edits, formulas and nested arithmetic replacement,
branch constraints, invalid paths, rollback and global rename migration.

`RTSCatalogCreationTests` creates each standalone F7 type/preset,
tests Effect operation conversions, validates all catalogs, serialized
roundtrips and undo/redo. `RTSTimedEffectTests` applies a composed
periodic damage/healing sequence from a real OnRegionEnter event, checks
regeneration and pulse deadlines at individual ticks, enforces the
256-slot scheduler limit, rejects malformed intervals, verifies reset
and full authoritative replay, and emits `timed_effect_fingerprint.txt`.

All tests run in both Debug and Release with `NDEBUG` disabled and a
compile-time assertion guard. Both Windows and Linux upload the timed
effect fingerprint; the mandatory replay-parity job requires an exact
byte-for-byte match in addition to existing fingerprints. Release bundles
must validate `timed_effects_showcase.rtsproject.json` and self-report
`RTS_FEATURE_RECURSIVE_ECA_UI=1` and
`RTS_FEATURE_TIMED_EFFECTS=1`.
Linux Mesa/Xvfb separately captures `rts-recursive-trigger-editor-screenshot`
and `rts-timed-effects-screenshot` from the real OpenGL executable.
Actual Windows 10/11 interactive use remains a manual smoke requirement.

### Meaningful screenshot capture

The screenshot-only `--smoke-live-trigger-monitor` argument advances one authoritative fixed 60Hz simulation step per smoke frame without affecting normal paused F6 editor behavior. Thus the timed-effects monitor at smoke frame 180 should show real elapsed simulation time, an executed trigger and live queued effects, rather than the initial zero-state. `--smoke-select-trigger-id relay_support_award` opens the authored nested ECA branch during the recursive editor screenshot; unknown IDs fail startup. Both are CI-only presentation/simulation harness controls and do not affect replay format or normal gameplay. Widened initial F6 inspector/adjacent monitor placement remains user-customizable.


## Ability editor, energy and replay tests (November 14 continuation)

RTSAbilityCastTests validates authored Ability/Unit dependencies,
targeting rules, actual CastAbility input, target health changes,
cooldown rejections, mineral and energy deductions, fixed-tick
regeneration, reset, full recorded replay and tamper rejection.
RTSAbilityEditorTests checks F7 creation presets, typed field
validation, Unit loadout editing, dependency-safe catalog rename
and deletion, project save/load and undo/redo. Both have Release
assertion-on compile flags and fail compilation with NDEBUG enabled.

The Windows/Linux replay-parity job compares the newly generated
ability_cast_fingerprint.txt byte-for-byte, in addition to earlier
command, AI, script, trigger and timed-Effect fingerprints.
Windows/Linux Release ZIPs validate the real
projects/ability_workshop_showcase.rtsproject.json with the bundled
RTSProjectTool and require RTS_FEATURE_UNIT_ABILITIES=1 and
RTS_FEATURE_ABILITY_EDITOR=1 from the actual native executable.

Linux Mesa/Xvfb renders real screenshots for three editor/game views:
rts-ability-editor-screenshot (F7 authored Arc Lance object),
rts-unit-ability-loadout-screenshot (F7 Ranger's Ability references)
and rts-ability-command-hud-screenshot (selected F9 caster).
These are visual smoke tests, not automated user-click equivalence
or proof of flawless interactive Windows runtime behavior.

### Visual smoke verification refinement

In ability_workshop_showcase.rtsproject.json, command_center derives from ranger for legacy stat inheritance. Unit Ability assignment also inherits unless explicitly overridden. We added a zero-length abilities override to the base and tightened --smoke-select-ability-caster to pick a nonstructure, nonworker Unit, ensuring CI screenshots prove an actual playable Ability button grid rather than a selected producer. Real gameplay always respects the catalog's intentional inheritance rules; the smoke helper does not change them.


## New Behavior engine/editor CI gates (November 14)

Two active-assert CTest executables are included in Debug and Release:

- RTSBehaviorRuntimeTests verifies two-layer and capped three-layer
  application, effective armor/damage/speed, live watch results,
  exact-tick expiration, refresh versus nonrefresh semantics,
  dispellable versus immune status, reset, 256-instance bounds,
  atomic rejection of an Effect Sequence containing immediate
  damage when the status limit is full, catalog rename/deletion,
  malformed field rejection, replay verification and tampering.
  Its behavior_fingerprint.txt is mandatory in both CI platforms.
- RTSBehaviorEditorTests exercises actual EditorSession transactions
  for Fortify/Hinder/Sharpen templates, grouped modifier edit and
  invalid-value rollback, Apply/RemoveBehavior Effect kinds,
  nested/ordered Sequence editing, undo/redo, rename migration,
  inbound-reference safety, cycle rejection and JSON roundtrip.

Linux/GCC and Windows/MSVC Debug run identical tests. The CI replay
job performs a byte-for-byte diff of behavior_fingerprint.txt after
existing 7 parity artifacts. The optimized Release workflow builds
both runtime packages and executes all CTests with assertions on,
validates projects/behavior_workshop_showcase.rtsproject.json using
the native bundled RTSProjectTool, and requires feature flags
RTS_FEATURE_STATUS_BEHAVIORS=1 and
RTS_FEATURE_VISUAL_EFFECT_FLOW=1 from actual native executables.

Linux Mesa/Xvfb uses the real OpenGL renderer for three captures:
rts-behavior-editor-screenshot (F7 grouped status parameters),
rts-effect-flow-screenshot (F7 nested Sequence tree), and
rts-active-behavior-hud-screenshot (F9 caster using a real Self
CastAbility command followed by fixed simulation updates). The
screenshot-only --smoke-cast-self-ability flag enqueues the same
valid recorded command as a human input; it does NOT inject mock
status state. Normal gameplay/editor behavior is unchanged.


## Ground AoE/impact regression and release parity

RTSGroundAreaTests covers Ground catalog validation, range/vision,
invalid Unit-target rejection, enemy and allied team filters,
fixed-tick delayed impacts, moving out of the zone before arrival,
resource/cooldown accounting, replay tamper, pending queue reset,
atomic periodic saturation refusal, the 64-Unit cap, and malformed
radius/team/delay fields. It writes ground_area_fingerprint.txt.
RTSGroundAreaEditorTests exercises F7 Ground preset, grouped
radius/team/delay editing, invalid transactional rollback,
Self/Ally/Ground target conversion, undo/redo, typed Unit
references/rename guards and project JSON roundtrip.

Both Debug and Release CTests run on Windows and Linux with
assertions enabled. CI uploads the new fingerprint from both
systems, byte-compares it, and uses real Mesa OpenGL to capture
rts-ground-ability-editor-screenshot and
rts-delayed-area-impact-screenshot. The screenshot-only
--smoke-cast-ground-ability flag enqueues a genuine game command.
The Release workflow packages and validates the original
ground_impact_showcase map on both OSes, checking actual
native executable feature flags rather than metadata only.


## Projectile CTests, native screenshots and Release bundles

RTSProjectileRuntimeTests covers valid typed Projectile and
Ground Ability references, instant-versus-delayed delivery,
fixed-tick position/arc projection, owner/team impact behavior,
per-cast resources, exact arrival, 3D swept early interception,
out-of-area dodge, oversized scaled-Unit broadphase coverage,
procedural hit-flash watch, replay roundtrip/
tampering, projectile ID rename/deletion protection and invalid
speed/arc/scale/contact/type fields. Writes a mandatory
projectile_fingerprint.txt in the CTest working directory.

RTSProjectileEditorTests creates Arc and Bolt presets with
the headless EditorSession API, modifies all fields as one
undoable action, checks invalid rollback, typed Ground Ability
projectile links, undo/redo, rename, blocked deletion,
Ground -> Self reference cleanup, and JSON roundtrip.

Both CTests run Debug/Release (Release asserts enabled) on
Windows and Linux. CI uploads and byte-compares the projectile
replay fingerprint, plus uses Mesa/Xvfb and the real GL renderer
to capture F7 Projectile inspector, an actual F9
--smoke-cast-projectile flight and a later-frame
rts-projectile-impact-screenshot showing genuine hit feedback.
The opt-in --smoke-fixed-ticks flag advances exactly one 60Hz
simulation step per rendered smoke frame. It prevents software
renderer speed from shifting the captured state; it never
changes normal real-time gameplay or nonplay editor timing. No fake flight positions are
injected by screenshot modes. Both Release archives validate
projects/projectile_forge_showcase.rtsproject.json using their
own native RTSProjectTool and require executable flags
RTS_FEATURE_AUTHORED_PROJECTILES=1 and
RTS_FEATURE_PROJECTILE_INTERCEPTION=1.

## Reusable swept projectile geometry CTest

RTSProjectileGeometryTests directly exercises the standalone
RTSProjectileCollision.hpp first-contact helper: XZ quadratic
entry (not closest approach), vertical slab entry and rejection,
grazing tangent, zero horizontal motion, degenerate segments,
and a large collider entered before a smaller nearer-center
collider. Release builds keep assertions enabled, as with the
other engine and editor suites.

## Cross-platform area shape acceptance (November 15)

The new headless area_shape_geometry test covers circle fallback,
cone angular front/side rejection, line rectangle bounds,
rotated facing, coincident orientation and rendered outlines.
area_shape_runtime covers fixed 60 Hz casts, delayed impacts,
team filtering, moving targets, serialized replay roundtrip,
tampered replay rejection and invalid schema inputs.
area_shape_editor exercises F7 grouped transactions, invalid
rollback, derived-object inheritance and undo/redo.
The geometry boundary fixture deliberately uses an epsilon
inside/outside 45 degrees to avoid unstable tan(pi/4) rounding
on MSVC versus GCC.

Release keeps assertions enabled for all three tests.
The area_shapes_fingerprint.txt recorded by the runtime test is
byte-compared across Windows and Linux in mandatory replay-parity.
CI also runs two actual Mesa OpenGL screenshots (F7 inspector and
F9 cone telegraph) and validates area_shapes_showcase in headless
and packaged executables. Both bundled binaries must report
RTS_FEATURE_AREA_SHAPES=1. Check current CI run before claiming
the new milestone fully green.

## New Weapon runtime/editor and Release parity coverage

weapon_runtime verifies Weapon bounds and typed refs, penetration versus
ordinary Effect armor mitigation, stacked armor, CombatHit data, an actual
queued Attack command, replay roundtrip and tampered replay rejection.
It writes weapon_fingerprint.txt for mandatory byte comparison across
Windows/MSVC and Linux/GCC. weapon_editor validates F7 preset creation,
grouped field transactions, invalid edit rollback, undo/redo, inherited
Unit weapon refs, rename/deletion protection and JSON persistence.
Release asserts are enabled for both new CTests.

The original Weapon Forge project is validated by both headless CTest
and the native packaged RTSProjectTool on both platforms. Build bundle
checks require RTS_FEATURE_AUTHORED_WEAPONS=1 and
RTS_FEATURE_ARMOR_PENETRATION=1 from the executable itself.
Linux Mesa OpenGL smoke jobs capture the F7 Weapon inspector and the F9
native game/HUD, not simulated UI images.

### F9 single-unit Weapon HUD image

The Weapon Forge gameplay screenshot smoke uses the native
--smoke-select-ability-caster option to select precisely one
armed Ranger, ensuring that the F9 detailed unit dossier
shows PEN next to damage/armor rather than the group-selection grid.
The run is fixed 60 Hz smoke, not a mocked interface.

## Native Weapon flight and Data Navigator regressions

weapon_projectile_runtime covers launched-hit delay, moving-target
dodge, earliest eligible enemy interception, armor mitigation,
authoritative replay roundtrip and invalid Projectile links.
weapon_projectile_editor verifies F7 typed link transactions,
Undo/Redo, inherited references, rename/delete safety and JSON.
catalog_navigator covers typed forward/reverse/parent/inherited links,
explicit empty overrides, unknown extension fields and stable ordering.
Release assertions remain on; Windows and Linux CI byte-compare
weapon_projectile_fingerprint.txt. The native Mesa OpenGL screenshot
pipeline captures F7 Weapon and gameplay Projectile flight.
The packaged executable reports RTS_FEATURE_WEAPON_PROJECTILES=1.

## Actor Event CTests and native screenshot acceptance

actor_event_runtime verifies strict Actor cue schema, normal Attack
and Projectile Launch/Impact events, Unit Death events, instance
overrides, timed expiration, Ability Cast/Impact, replay roundtrip
and tamper detection. It also verifies presentation never changes
authoritative stateDigest on an identical 110-tick decorated versus
unadorned combat sequence. actor_event_editor verifies F7 preset
creation, grouped edits, rollback, Undo/Redo, inheritance and
JSON persistence. actor_events_showcase is validated in CTest
and the Windows/Linux packaged RTSProjectTool.

The new actor_cues_fingerprint.txt is uploaded and byte-compared
in replay parity CI. Native Linux OpenGL captures an F7 Actor event
inspector and F9 live event gameplay screenshot. The Release
bundle verifier requires RTS_FEATURE_ACTOR_EVENT_CUES=1.

## Requirement graph testing matrix

requirement_runtime covers positive/negative Ability admission,
strict less-than health thresholds, energy checks, All/Any/Not
short-circuiting, status Behavior presence and expiration,
production Unit-count gating, targetless rejection rules, invalid
graph references/cycles/bounds, replay roundtrip and rejected
command fingerprints. requirement_editor covers create/edit/preset,
invalid transactional rollback, undo/redo, inherited Unit
references, rename/delete prevention, graph browser forward/reverse
links, per-instance typed override migration and JSON roundtrip.
requirements_workshop_showcase is validated in CTest and with
the packaged ProjectTool on Windows/Linux.

CI compares requirement_fingerprint.txt byte-for-byte across
Linux/GCC and Windows/MSVC, runs
native F7 Requirement editor capture under xvfb/Mesa, and verifies
RTS_FEATURE_REQUIREMENT_GRAPHS=1 in packaged binary metadata.
All Release assertions remain active.

### Native Requirement authoring visual checks

The Linux CI additionally captures the F7 Validator predicate inspector
and the linked Ability Requirement field in the same original Workshop
project, complementing the nested Requirement graph screenshot.
These are actual GUI/software-GL frames, not separate mockup assets.

## Research technology acceptance

upgrade_runtime tests actual research order admission/refusal,
owner level progression, per-level payment/timing, max-level cap,
worker tech unlock, present/future Unit modifiers and mitigation,
cancellation refund, typed schema rejection, replay roundtrip/tamper
and read-only research snapshots. upgrade_editor exercises F7 presets,
invalid rollback, Undo/Redo, typed Upgrade/Validator/Requirement links,
inheritance, safe rename/deletion and placed producer research overrides.
technology_forge_showcase is validated by CTest and installed project
utilities on Windows/Linux. Total Release suite: 81 CTests per platform.
Debug CI byte-compares upgrade_research_fingerprint.txt and captures
native F7 Upgrade and F9 research-producer screenshots.

## Show/Use test matrix (83 Release tests)

tech_visibility_runtime checks hidden, locked and available
Unit/Upgrade/Ability commands, real tech level advancement,
rejected raw orders, cost protection, old implicit Unit training,
all-options-hidden producer cards, scoped graph rejection,
JSON replay roundtrip/tamper, and determinism fingerprints.
tech_visibility_editor checks atomic Upgrade Show/Use changes,
invalid rollback, Undo/Redo, typed reverse references, graph
navigation, safe rename/deletion, placed Unit Show override
migration and persistence. Native CI captures initial F9 cards,
the real level-1 post-research F9 state, and F7 Upgrade editing.
These run alongside all existing Windows/Linux Release tests.
