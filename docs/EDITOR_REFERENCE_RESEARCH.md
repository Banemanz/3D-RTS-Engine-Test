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

## 2026-11-15 implementation follow-up

Validator/Requirement data objects now exist as reusable typed
headless catalogs with bounded All/Any/Not Boolean graphs. They
directly gate Ability casting and Unit production queue admission,
and F7 edits their references rather than hardcoded rule branches.
The broader Galaxy Validator and Requirement editor capabilities
remain incomplete, particularly scripted predicates, research and
upgrade trees, field provenance and advanced graph editing. The
new original schema and exact implementation limits are documented
in docs/REQUIREMENT_AUTHORING.md.

## Upgrade authoring reference follow-up

Public Galaxy Editor tutorial materials illustrate Upgrade field
operations, research levels and separate Show/Use Requirements;
OpenRA documentation illustrates production prerequisites. The
RTS project now independently implements reusable typed Upgrade
catalogs, shared research command cards, owner level progression,
combat modifiers and UpgradeLevelAtLeast Requirement gates.
Full schema, boundaries and cited reference URLs are in
docs/TECHNOLOGY_RESEARCH.md. No SC2 or GPL code/assets copied.

## Galaxy/OpenRA Show vs Use reference comparison

Public archived StarCraft II Editor Q&A documents separate
Requirement Show and Use sections to control visibility and
availability. OpenRA's Buildable prerequisites document a
hide-if-unavailable modifier. Independent C++ implementations
now support the conceptual distinction on Upgrades, trained
Units and Abilities, including true raw command admission.
Exact schema and limitations: docs/TECH_SHOW_USE.md.


## Native technology dependency projection (November 15)

The Technology Map draws on the *design patterns* of the public
StarCraft II Data Editor (typed catalogs and Requirement graphs) and
OpenRA's explicit production prerequisites, without importing code,
assets, proprietary text or widget implementations. The authored
Show/Use and Any/Not expression semantics are kept intact by treating
graph arrows as references rather than universal boolean conclusions.
The independent C++ implementation and native F7 screenshot are
documented in `docs/TECH_TREE_MAP.md`.

- Galaxy catalog reference:
  https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/
- Galaxy Tech Tree scripting reference:
  https://mapster.talv.space/galaxy/reference
- OpenRA Buildable/Prerequisite trait docs:
  https://docs.openra.net/en/playtest/traits/


## Galaxy UI frame/dialog comparison (November 16)

Public Galaxy dialog documentation and native references distinguish
framed parent dialogs, dialog controls, relative anchor/offset
positions and UI events. See:
- https://mapster.talv.space/galaxy/reference
- https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/043_Dialogs/
- https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/049_UI_Events/

RTS Creator uses those ideas **only for capability direction**:
original `ui_frames` project metadata (Panel/Label/Button/Bar),
bounded parent-first hierarchy/anchor math and clipped native
OpenGL rendering/input. Unlike Galaxy's broader dialog trigger
support, only whitelisted native actions are exposed; arbitrary
trigger callback routing and image/skin/frame import are future
separate milestones. No Blizzard XML, names, icons or licensed
third-party implementation was copied.


## UI event as a replay-recordable input

Publicly documented RTS trigger editors separate UI/dialog events
from the presentation widget tree and execute typed trigger action
sequences. The original RTS Creator now has corresponding
`OnUIFrameClicked` events, but first records frame clicks as
authoritative 60Hz commands; the F6 script runner executes only
safe existing ECA actions. This intentionally avoids injecting
untracked GUI callbacks into deterministic combat/resources, and
does not copy Blizzard API code, proprietary UI assets or GPL engine
implementations. See docs/UI_TRIGGER_EVENTS.md.


## Responsive Panel containers (November 16)

In addition to named anchors and F6 dialog events, modern authoring requires composable layouts that do not force every widget into absolute coordinates. The original RTS Creator now implements bounded StackRow/StackColumn Panels with padding/gap, cross-axis stretch and F6-variable-driven visible-child reflow. This is a homegrown C++17 layout math model rather than copied Galaxy XML or GPL engines. It intentionally does not yet implement full constraint solving, wrapping, text autosize or texture-based frames. See `docs/UI_RESPONSIVE_LAYOUT.md`.

## Model inspection and scene assemblies (November 25)

Reviewed the Galaxy Model Data Viewer tutorial screenshots (geometry,
materials and attachment-point tree alongside the model), Data Editor
Introduction (typed searchable catalogs), and Importer (asset paths).
https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/068_Model_Data_Viewer/
https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/
https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/014_The_Importer/

Recoil's Blender glTF guide describes the model node/piece hierarchy and
engine model coordinate frame:
https://recoilengine.org/docs/guides/getting-started/blender-gltf-import/
0 A.D. entity documentation distinguishes actor visuals from model-based
selection bounds: https://docs.wildfiregames.com/entity-docs/a27.html

Original implementation: F7 searchable compiled assets, invalid-only filter,
expandable model bounds/texture memory details, and bounded native static
glTF scene flattening with parent TRS and inverse-transpose normals. It
does not yet retain a runtime node tree or display a rendered model viewer,
animation timeline or attachments. No third-party art or GPL code copied.
