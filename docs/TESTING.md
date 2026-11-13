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
