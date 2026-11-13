# Downloadable builds and GitHub Actions

The repository includes two complementary workflows:

- **C++ CI** (`.github/workflows/ci.yml`): Debug compilation and CTest on
  Linux/Windows; Mesa/Xvfb OpenGL screenshots of game, terrain, and trigger UI.
- **Build downloadable RTS engine** (`.github/workflows/build.yml`): Release
  compilation, all CTest checks, install staging, bundled-project validation,
  packaged-editor smoke on Linux and one portable archive per platform.

## Get a build without installing a compiler

1. Open **Actions** on GitHub and select **Build downloadable RTS engine**.
2. Open the newest run with a green check mark for your preferred branch.
3. Under **Artifacts** download `rts-creator-windows-x64-<run>` or
   `rts-creator-linux-x86_64-<run>`.
4. Extract the Actions artifact, then extract the contained
   `RTS-Creator-windows-x64.zip` or `RTS-Creator-linux-x86_64.tar.gz`.
5. Windows: run `run-game.bat` for the skirmish or `run-editor.bat`
   for the terrain editor. Linux: run `./run-game.sh` or
   `./run-editor.sh` from a machine with OpenGL and the usual GLFW/X11
   system libraries. The Linux archive preserves executable permissions.

The archive includes `RTSCameraTerrain`, `RTSProjectTool`,
both sample `.rtsproject.json` maps, authoring documentation,
`build-info.json` (exact commit/run), required notices, and launchers.
Everything is arranged to work with relative project paths after extraction.

These are **prototype build artifacts**, not signed installers or
production-ready releases. There is no Windows installer, code signature,
auto-updater, or bundled Linux system driver. Linux requires an OpenGL 3.3
driver, system `libglfw3`, and the standard desktop/GL libraries.
Windows uses the operating system's OpenGL interface and must have a capable
graphics driver. Real Windows interactive editor testing still matters;
a successful GitHub Actions compilation does not prove the GUI works on
every GPU/driver.

## Manually build from source

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix ./dist/RTS-Creator-local
```

The workflow is deliberately **read-only** to GitHub contents
(`permissions: contents: read`) and does not merge the PR,
create a GitHub Release, publish packages, or change any settings.
It also supports **Run workflow** for manual builds. Successful
artifacts are retained for 30 days under GitHub Actions.
