# 3D RTS Engine Test

Experimental, from-scratch **C++17 / OpenGL 3.3 / GLFW** real-time strategy project.

## Build

CMake 3.16+, a C++17 compiler, OpenGL 3.3-capable driver, and GLFW 3.4 are required.
CMake first looks for a system GLFW package, then downloads pinned GLFW 3.4 if absent.
The source currently lives in `3D RTS (v0.3.10)/`.

```sh
cmake -S "3D RTS (v0.3.10)" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build --output-on-failure -C Release
./build/RTSCameraTerrain
```

On Windows, run `build/Release/RTSCameraTerrain.exe` for multi-config generators.

## State of development

The prototype implements a 3D arena, RTS camera, selection, steering, simple unit combat
and basic HUD overlays. Work on reliable input/navigation and a playable skirmish is ongoing.
See `docs/ENGINE_STATUS.md` for audited capabilities and known limitations.

This is **not** a Godot project. Embedded `.tres`-like strings are parsed by C++ code and
do not imply integration with the Godot resource system.

Licensed under MIT. See `LICENSE`.
