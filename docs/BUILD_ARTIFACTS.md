# Download the actual RTS Engine executable

**Windows:** [Open the Windows/Linux executable build workflow](https://github.com/Banemanz/3D-RTS-Engine-Test/actions/workflows/build.yml).

1. Open a successful run of **Download Windows EXE and Linux RTS Builds** on the branch you want.
2. Under **Artifacts**, download **WINDOWS-x64-EXE-RTS-Creator-...**.
3. Extract that download **once**. It already contains `RTSCameraTerrain.exe`,
   `RTSProjectTool.exe`, `run-game.bat`, `run-editor.bat`,
   the `projects/` directory and the docs. There is no second ZIP.
4. Run `run-game.bat` to play or `run-editor.bat` to open the terrain editor.
   You can also run `RTSCameraTerrain.exe` directly from the extracted folder.
5. In game/editor: **F5** terrain, **F6** triggers, **F7** data, **F8**
   scene, **F9** test/play.

**Linux:** Download **LINUX-x86_64-RTS-Creator-...** from the same workflow,
extract the GitHub artifact ZIP and then extract the enclosed `.tar.gz`.
The tarball is retained on Linux to preserve executable permissions. Run
`./run-game.sh` or `./run-editor.sh`, with system GLFW/OpenGL/X11 libraries.

## Two workflows, two distinct purposes

- **Download Windows EXE and Linux RTS Builds**
  (`.github/workflows/build.yml`) **produces runnable executables**.
  Every release build runs CTest, verifies executable file signatures and
  required project files, validates both bundled sample maps, and uploads
  the executable artifact. Linux also smoke-runs the packaged editor under Mesa.
- **Tests + screenshots (NOT executables)**
  (`.github/workflows/ci.yml`) builds Debug binaries and uploads test
  screenshots. Its artifacts are **not** download packages.

Each executable build includes `build-info.json` identifying its source SHA
and GitHub Actions run ID, plus `START_HERE.txt` with launcher instructions.

Windows executables are built x64 with the MSVC static runtime and GLFW
statically linked, so a separate Visual C++ redistributable or GLFW DLL is not
intended to be necessary. A graphics driver supporting OpenGL 3.3 is still
required. Linux packages use system shared libraries; they are not universal
standalone executables.

These are unsigned development prototypes rather than installers or finished
public releases. The builds are verified on CI (including native Windows
**command-line** execution), but the Windows graphical editor still needs
interactive testing on actual Windows GPUs/drivers.

To build locally:

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix ./dist/RTS-Creator-local
```

The workflow needs only `contents: read`, does not merge the PR or publish a
GitHub Release, supports manual **Run workflow**, and retains artifacts for 30 days.

**Crash diagnostics:** Windows Release packages also contain
`symbols/RTSCameraTerrain.pdb`, matching the executable. Native crash
dumps written locally by the game can be symbolicated using that PDB.
Neither minidumps nor error logs are uploaded automatically.

The packaged Windows executable is runtime-only: third-party GLFW headers,
CMake package files and static libraries are excluded from the installer
layout. CI treats inclusion of a development SDK in the end-user archive
as a packaging regression. The engine's own debug PDB remains in `symbols/`.

## Runtime verification

Launch the extracted `RTSCameraTerrain.exe --features` on Windows or
`./RTSCameraTerrain --features` on Linux to see the feature list embedded in
the executable itself. Compare `RTS_BUILD_COMMIT` with `build-info.json` to
detect stale/copied binaries. The release packaging script now does this on
both operating systems and rejects missing fog, HUD, creator GUI or editor
workspaces. Bundle format version 2 also includes a native feature manifest.
This does not substitute for a manual Windows OpenGL graphics-driver test.


## Native gameplay menu in shipped builds
Open the game and press **Esc** outside an active order to reach the engine-native pause menu. It is present in the Windows and Linux executable without any additional UI dependencies; `--features` reports `RTS_FEATURE_NATIVE_PAUSE_MENU=1`. The creator/editor still includes the pinned ImGui backend until an in-house dockable replacement is complete. Documentation for migration: `docs/CREATOR_ARCHITECTURE.md`.


## Trigger monitor in shipped builds

Both Windows/Linux Release executables self-report `RTS_FEATURE_TRIGGER_TRACE_MONITOR=1` and existing native UI/visual ECA flags, checked by `tools/verify_build_bundle.py`. GitHub Actions' **Tests + screenshots** workflow now produces `rts-trigger-trace-monitor-screenshot` from a Linux Mesa/Xvfb run of the actual executable, not a mockup. The downloadable executable ZIPs remain in the **Download Windows EXE and Linux RTS Builds** workflow. The new sample map is copied by the existing `projects/` install rule. Native keyboard-focus controls use exactly the same code on Windows and Linux.


## November 14 engine foundation flags

Windows/Linux installed executables both must self-report `RTS_FEATURE_DYNAMIC_NAV_REPLAN=1` and `RTS_FEATURE_STABLE_SIM_ORDER=1`. The bundle verifier checks the flags and installs `docs/RTS_ENGINE_AUDIT.md`, alongside existing authoring/creator docs. All new headless CTest targets are built identically on both operating systems. This audit is not itself a new executable; it documents the source branch and outstanding engine work.


## Portable replay verification in the ZIP

Windows and Linux packages include `RTSProjectTool[.exe]` and `docs/REPLAY_COMMANDS.md`. Both native `RTSCameraTerrain --features` outputs are verified to include `RTS_FEATURE_COMMAND_STREAM=1` and `RTS_FEATURE_REPLAY_VERIFY=1`; the bundle verifier checks the exact installed executable rather than a loose manifest. The command project schema and replay verification tool are the same on Windows/Linux. Each release also runs the command/replay CTest suite with assertions enabled.


## AI planner and replay v2 in packaged builds

Both platform bundles include `projects/ai_defense_showcase.rtsproject.json` and the same `RTSAIPlanner` source compiled into the simulation library. The executable's `--features` output must advertise `RTS_FEATURE_AI_COMMAND_PLANNER=1`, checked by the bundle verifier on both Windows and Linux. CTest validates the sample map and the headless planner, and the replay-parity CI job still compares actual Windows/MSVC and Linux/GCC simulation digests. Original executable ZIPs remain in the Release workflow, not the screenshot artifacts.


## Combat AI sandbox capture

The CI **Tests + screenshots** workflow produces a `rts-ai-defense-screenshot` artifact from a software OpenGL run of the bundled `projects/ai_defense_showcase.rtsproject.json` map. The same map has a headless `ai_defense_behavior` CTest and Windows/Linux replay-state comparison, on top of the already existing generic replay fingerprint. The executable ZIPs are still under the separate **Download Windows EXE and Linux RTS Builds** workflow; screenshots are not executable artifacts.


## New Ability Workshop executable checks

The real Windows x64 and Linux x86_64 Release packages now include
projects/ability_workshop_showcase.rtsproject.json and validate it
with their included RTSProjectTool. Native --features outputs must
advertise RTS_FEATURE_UNIT_ABILITIES and RTS_FEATURE_ABILITY_EDITOR;
tools/verify_build_bundle.py rejects missing flags or the absent
sample. Debug/Release tests and independent cross-OS replay hashes
gate publishing. Separate Linux OpenGL artifacts show F7 Ability
authoring, the selected Unit loadout and the F9 ability command grid.
The executable ZIPs remain under the Download Windows EXE and Linux
RTS Builds workflow, not the screenshot-only workflow.


## Status Behavior editor/runtime bundle contract

Both platform Release archives must include the original
projects/behavior_workshop_showcase.rtsproject.json and validate it
with the bundled native RTSProjectTool. Runtime --features must
self-report RTS_FEATURE_STATUS_BEHAVIORS=1 and
RTS_FEATURE_VISUAL_EFFECT_FLOW=1. The verifier checks these flags
from RTSCameraTerrain[.exe] on Windows and Linux before accepting
a bundle. Both Release CTest suites preserve asserts.

The separate Tests + screenshots workflow captures live F7 Behavior
editing, nested visual Effect flow and an F9 selected caster after
a genuine fixed-tick self-status cast under Linux software OpenGL.
These screenshots are not the executable binaries. All code and
asset formats remain shared across Windows and Linux.


## Ground area binary/package contract

Both native Release archives must contain
projects/ground_impact_showcase.rtsproject.json and run the
bundled RTSProjectTool --validate on it. Native --features must
report RTS_FEATURE_GROUND_AREA_ABILITIES=1 and
RTS_FEATURE_DELAYED_AREA_IMPACTS=1 on Windows and Linux.
The project verifier requires the map and these true binary
features. Debug CI generates Windows/Linux ground replay
fingerprints, plus authentic F7 and F9 Mesa OpenGL screenshots.
Screenshots are not Windows executable download artifacts.


## Projectile Forge native Windows/Linux package requirements

Release build archives must include the original
projects/projectile_forge_showcase.rtsproject.json and run
RTSProjectTool --validate from inside each OS bundle.
Native RTSCameraTerrain --features must advertise
RTS_FEATURE_AUTHORED_PROJECTILES=1 and
RTS_FEATURE_PROJECTILE_INTERCEPTION=1. The existing verifier
rejects missing map files, absent flags, or wrong binary formats.
Both package CTest suites include the projectile runtime and
F7 authoring regressions.

The separate Debug Tests+screenshots workflow records
projectile_fingerprint.txt on Windows and Linux, rejects a
byte mismatch in replay-parity, and captures native GL images
of the Projectile inspector and a moving shot in play mode.
Screenshots are not downloadable executables.

## Weapon Forge bundled sample and feature gate

Windows and Linux share identical Weapon catalog/combat/editor source.
The installed native zip/tar artifacts must contain the Weapon Forge
sample. The packaged RTSProjectTool validates it on both targets.
The native RTSCameraTerrain --features output must report authored
weapons and armor penetration as enabled. Headless tests and replay
fingerprints run on Windows and Linux; Linux Mesa captures the actual
F7 inspector and F9 combat/HUD as screenshot artifacts. Screenshot
artifacts are not substitutes for the executable build workflow.

## Weapon flight artifact verification

Windows and Linux native Release bundles validate the original Weapon
Forge map with Tracer Bolt and practice target. Executables advertise
RTS_FEATURE_WEAPON_PROJECTILES=1 in --features; CTests include the new
Projectile and Navigator tests, and CI strictly compares new replay
fingerprints across MSVC and GCC. Linux captures real OpenGL Weapon
flight and F7 editor screenshots; those are test artifacts, not
substitutes for downloadable executable bundles.
