# Engine and editor architecture reference survey
2026-11-15. This file records public source/documentation comparisons
for an original independent RTS creator. No third-party code, data,
sounds, model files or proprietary UI assets have been imported.

## StarCraft II Galaxy Editor (capability reference only)

- Data Editor overview and screenshots:
  https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/
- Unit links to Abilities, Actors, Behaviors, Movers and Weapons:
  https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/059_Units/
- Actor Events and their specialized subeditor:
  https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/060_Actors/
- Structured Trigger Editor and ECA screenshots:
  https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/032_Trigger_Editor_Introduction/
- Data module Actor Events examples:
  https://s2editor-guides.readthedocs.io/Classic_Tutorials/03_Data_module/4/

The documented Galaxy Data Editor exposes dozens of typed catalogs,
inherited objects, specialized per-field editors, cross-object navigation
and a Data Navigator. Actor events drive visual/audio responses and are
not authoritative game logic. The Trigger Editor is a separate large ECA
and script-library workspace. The local engine implements a modest
original subset rather than copying Blizzard's object definitions,
UI art or proprietary script runtime.

## Open-source RTS engine references

### OpenRA (GPL-3.0)
- https://github.com/OpenRA/OpenRA
- https://github.com/OpenRA/OpenRA/wiki/Hacking
- https://docs.openra.net/en/release/weapons/

OpenRA composes actor traits and activities and has separately described
Weapon, Projectile and Warhead concepts. Relevant future direction:
keep authored target filtering, payload/impact, projectile delivery and
presentation independent. We did not copy its code or YAML definitions.

### Spring and Recoil (GPL family)
- https://github.com/springrts/spring
- https://github.com/beyond-all-reason/RecoilEngine
- Relevant source paths: rts/Sim/Weapons/Weapon.cpp and
  rts/Sim/Projectiles/Projectile.h

Their simulation distinguishes synced authoritative weapon projectiles
from unsynced graphical effects. The local RTSSkirmish now processes
Weapon and Ability flights at fixed ticks while the native OpenGL
renderer reads projected flight/impact watches. No code imported.

### 0 A.D. (archived GitHub mirror)
- https://github.com/0ad/0ad
- Old source reference:
  binaries/data/mods/public/simulation/components/Attack.js
- Current source migrated to gitea.wildfiregames.com in 2024.

The old Attack.js illustrates data-driven melee/ranged modes, attack
ranges, target preferences and projectile parameters; Atlas is a
separate editor. This supports future modular target validators,
damage classes and typed unit combat modes. No source copied.

## Feature gap / priorities

| System | Present | Missing |
| --- | --- | --- |
| Data catalogs | Unit, Weapon, Projectile, Ability, Effect, Behavior, Model, Actor | Validators, Upgrades, Requirements, Movers, footprints |
| F7 editor | Typed properties, derived overrides, undo/redo, forward/back navigator | Multi-hop live graph, source layers, extensive specialized subeditors |
| Weapons | Independent profiles, normal attack projectiles, armor piercing | Bursts/salvos, guided motion, Warheads, advanced target filters |
| Actors | Models and runtime visual watches | Audio/particle/animation event term-message graph |
| Script triggers | Nested ECA actions/conditions, variable store | Function libraries, custom actions, expanded API and type system |
| Terrain/scene | Terrain painting and placed objects | Full footprint/nav painting, import pipelines, advanced selection gizmos |
| Verification | Linux/Windows CTests, state fingerprints, actual OpenGL screenshots | Scaled-battle performance and fuzz/property regression suite |

Maintain one headless authoritative model used by both editor and
simulation, deterministic replay across operating systems and original
independent content. Visual fidelity to reference products does not
justify borrowing their copyrighted artwork or GPL implementation.
Next priority should be Validators and Requirements tied to orders/
abilities, then Actor events and multi-weapon payload rules.
