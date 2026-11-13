# Architecture notes

The original prototype combines GLFW callbacks, GL resources, game state and simulation
in one main loop. The implementation deliberately evolves it incrementally instead of
replacing the author's renderer.

- Coordinate convention: Y up, ground plane XZ, world units as floats.
- Navigation: uniform XZ grid, 8-neighbor A*, diagonal corner cutting disabled.
- Simulation: fixed timestep to avoid frame-rate-dependent combat and movement.
- Rendering: OpenGL 3.3 Core, GLSL 330, no GPU dependency for logic tests.
- Source of truth: real C++ unit stats; `.tres`-like embedded presets are not Godot assets.
- Ownership: selection and commands should use player ownership, not appearance/faction.
- Performance: avoid quadratic scans and GL calls until measured; future spatial hash.

Avoid introducing a full ECS or a second rendering backend solely for the prototype.
