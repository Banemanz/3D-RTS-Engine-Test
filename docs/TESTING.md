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

\`RTSRecursiveTriggerEditorTests\` tests nested Add/Duplicate/Move/
Delete, typed property edits, formulas and nested arithmetic replacement,
branch constraints, invalid paths, rollback and global rename migration.

\`RTSCatalogCreationTests\` creates each standalone F7 type/preset,
tests Effect operation conversions, validates all catalogs, serialized
roundtrips and undo/redo. \`RTSTimedEffectTests\` applies a composed
periodic damage/healing sequence from a real OnRegionEnter event, checks
regeneration and pulse deadlines at individual ticks, enforces the
256-slot scheduler limit, rejects malformed intervals, verifies reset
and full authoritative replay, and emits \`timed_effect_fingerprint.txt\`.

All tests run in both Debug and Release with \`NDEBUG\` disabled and a
compile-time assertion guard. Both Windows and Linux upload the timed
effect fingerprint; the mandatory replay-parity job requires an exact
byte-for-byte match in addition to existing fingerprints. Release bundles
must validate \`timed_effects_showcase.rtsproject.json\` and self-report
\`RTS_FEATURE_RECURSIVE_ECA_UI=1\` and
\`RTS_FEATURE_TIMED_EFFECTS=1\`.
Linux Mesa/Xvfb separately captures \`rts-recursive-trigger-editor-screenshot\`
and \`rts-timed-effects-screenshot\` from the real OpenGL executable.
Actual Windows 10/11 interactive use remains a manual smoke requirement.
