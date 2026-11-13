# Engine status

This document distinguishes **implemented** from **proven functional**. Baseline source
audit: `cce16516a759c764621ed2630ebd55f326aa466d`.

| System | Status | Notes |
| --- | --- | --- |
| GLFW / OpenGL 3.3 loop | Implemented | Host GL driver required |
| Procedural units/terrain | Implemented | No asset importer |
| Camera and mouse selection | Implemented | Verify HiDPI behavior |
| Steering, basic combat, health | Implemented | Prototype rules, no projectiles |
| A* grid planner | Added | Standalone foundation; integration ongoing |
| Resource economy / workers | Planned | Not present in audited main |
| Structures / production | Planned | Not present in audited main |
| Strategic enemy AI | Planned | Not present in audited main |
| Save/load / networking | Deferred | Not required for first skirmish |
| Tests / CI | In progress | Headless navigation test initially |

See `docs/ARCHITECTURE.md` for design constraints and migration strategy.
