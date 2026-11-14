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
