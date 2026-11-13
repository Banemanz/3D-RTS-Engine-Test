# RTS Engine — continuation handoff (new chat)

**Repository:** https://github.com/Banemanz/3D-RTS-Engine-Test
**PR:** https://github.com/Banemanz/3D-RTS-Engine-Test/pull/1
**Branch:** `feat/rts-vertical-slice`
**Base:** `main` at `cce16516a759c764621ed2630ebd55f326aa466d`

The author originally implemented an OpenGL3.3/GLFW RTS combat demo (camera,
procedural mesh drawing, selections, simple steering and combat). This PR adds a
standalone passability/A* navigation grid, a testable offline skirmish controller
with worker harvesting, structures, training/economy/production, basic enemy AI,
win conditions, fixed-tick simulation, in-game bitmap HUD, validated unit stats,
headless tests, Linux/Windows CI, and Mesa screenshot smoke test.

**Current priority TODO:**
1. Verify all CI jobs and inspect software-rendered screenshot artifact.
2. Stress-test tactical A* chase routing and corner cases; static blocker
   collision guard exists, dynamic blockers still need routing invalidation.
3. Replace O(N²) combat/steering scans with a reusable spatial grid and ID index;
   stress-test with 50/250/500 units.
4. Improve the command/UI model: true shift-queued orders, control groups, unit/
   building selection panel, visible construction and production progress.
5. Extract GL objects and renderer into lifetime-managed class; cache uniform
   locations; fix shader creation cleanup.
6. Extract external validated world/unit data and add asset loader.
7. End-to-end keyboard/mouse functional test + native Windows OpenGL smoke test.

**Design constraints:** Preserve C++17 + GLFW + OpenGL3.3, existing camera and
unit combat where possible, use multiple reviewable commits under the one draft
PR. Do not merge without maintainer review; never claim unverified performance
gains. Paths contain `3D RTS (v0.3.10)` with spaces and parentheses.

**Testing:**
`cmake -S "3D RTS (v0.3.10)" -B build -DRTS_BUILD_DEMO=OFF && cmake --build build && ctest --test-dir build --output-on-failure`.
See `docs/TESTING.md` for Mesa smoke screenshot steps.
