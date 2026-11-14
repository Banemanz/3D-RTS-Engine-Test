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
