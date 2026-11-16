# Creator architecture, Galaxy Editor references and native UI roadmap

This document is the engineering plan for the existing **draft PR #1**. Keep the project on the same Windows/Linux C++17/OpenGL source path. Do not switch engines or build unrelated editor implementations.

## Galaxy Editor as the feature reference

StarCraft II's editor uses connected Terrain, Data, Triggers, UI, Importer, Cutscene, AI and script modules. Its Data Editor has approximately 87 catalogs, but parity depends on the **relationships and authoring capabilities**, not merely matching its category count.

- [Editor modules](https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/003_Navigating_the_Interface/)
- [Data Editor and catalogs](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/)
- [Unit composition](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/059_Units/)
- [Actor event/presentation separation](https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/060_Actors/)
- [Terrain layers](https://s2editor-guides.readthedocs.io/New_Tutorials/02_Terrain_Editor/019_Terrain_Editor_Interface/)
- [Recoil RTS / Spring architecture](https://recoilengine.org/) and [0 A.D. / Atlas editor](https://play0ad.com/) are design references, not sources to copy GPL code from.

## What exists today

The native gameplay HUD is already a custom engine OpenGL renderer with a shared pure-C++ layout/hit-test model. The first **native pause menu** now uses `NativePauseMenu.hpp` for pure layout/input and `NativePauseMenuGL.hpp` for the engine renderer: resume, restart, enter editor and exit, with the simulation/world input suspended. No extra GUI libraries were added.

The existing editor workbench **still uses pinned Dear ImGui** as a presentation layer. Keep it functional until our own implementation matches its behavior. The editable project, simulation, terrain, catalogs, triggers, scene placements and undoable `EditorSession` commands are all headless C++ and can be connected to the native system without rewriting data or gameplay.

The Data Editor now has parent/Unit/Actor/Model/trainable-object navigation, recognized incoming reference lists, local-vs-inherited field indicators and validated typed boolean/numeric/string editing. This is a foundation, **not** an SC2-equivalent object editor.

## Engine-owned retained UI: required primitives

1. **Event routing:** stable widget IDs, explicit focus and pointer capture, keyboard navigation, text composition, key repeat, modal focus scope, drag/drop and disjoint GUI/world input.
2. **Layout:** anchors, flex-like rows/columns, min/max size, fractional and DPI scaling, scroll clipping, dock/split tabs, z-order and persisted workspace arrangements. Compute geometry headlessly.
3. **Rendering:** engine-owned batched textured/solid geometry and a glyph-atlas font; preserve OpenGL state across UI/world rendering. Current 5×7 bitmap glyphs are bootstrap-only, not a production Unicode text system.
4. **Widgets:** menus, toolbar, tree, sortable/virtualized table, selector, searchable catalog browser, property field, vector/color picker, popup, tooltip, graph canvas, node ports, text editor, undo status and accessibility affordances.
5. **Data binding:** widgets send validated commands through `EditorSession`, not ad hoc edits to raw model pointers. Transactions are reversible. Inherited fields must distinguish an override from an inherited value.

### UI migration gates

- **G0: done:** native pause, existing custom gameplay HUD, pure layout/hit tests; shared binary feature flags and Linux GPU screenshot smoke.
- **G1:** shared `NativeUI` tree and text/input rendering infrastructure with focus/scroll/clip, headless geometry tests and real screenshot comparison.
- **G2:** build native editor toolbar, catalog browser and inspector while retaining a switchable existing adapter. Parity means working rename, derive, delete, link navigation, field editing and undo/redo.
- **G3:** migrate terrain/scene/trigger panels and true dockable workspace with property schemas; only then remove ImGui FetchContent/backends and associated license from packaging.
- **G4:** extend creator systems, not just UI: ability/effect/behavior/mover/weapon editors, actor/event graph, script debugger, imported asset previews, gameplay UI/layout editor, cinematic timeline and campaign tools.

## Engine foundation needed for actual Galaxy-like authoring

**Catalog schemas and object dependency graph.** Add per-type field descriptors (type, range, enum, allowed reference class, default, inherited/local provenance, mod extension fields). Index forward/reverse references. Renames and deletes must either migrate references transactionally or fail safely.

**Abilities and effects.** Define a bounded, deterministic effect executor: target selection, validation, set/search effect sequences, damage/heal/resource transfers, spawn, modifiers and order dispatch. Keep authored visual actors distinct from authoritative simulation mutations. Test on headless data fixtures, not OpenGL.

**Visual actors/events.** Simulation emits stable semantic events for animation, sound, particles, projectiles, attachments and death; renderer-owned actor presentation consumes these without deciding damage. Actor models/animation state and import pipeline are not yet implemented.

**Movement/visibility.** Independent terrain categories and path layers for ground/flying/cliffs/bridges, footprints, dynamic obstacles, local avoidance, static geometry indexing, radar and cloak/detection are still missing. Preserve the fixed timestep and stable unit identity.

**Triggers and script tooling.** Move from raw ECA JSON to typed, composable Event/Condition/Action nodes with scopes, variable/function libraries, validation errors, scheduling constraints, debugging and source locations. Design bounded instruction/effect budgets.

**Map / UI authoring.** Scene hierarchy, transform gizmos, prefab/actor instances, terrain layers, doodads, play-in-editor, visual layouts, localization, searchable import database, hot reload, project migrations, crash recovery and editable diffs.

## Cross-platform release contract

- Windows x64 and Linux x86_64 compile the **same targets and feature switches**, validated using the actual packaged executable's `--features` output and `tools/verify_build_bundle.py`.
- Both run the same headless CTest suite (simulation, terrain, authoring, UI hit-testing) in Debug and Release; keep assertions enabled in Release test targets.
- Linux runs Mesa/Xvfb real OpenGL startup, production HUD, native pause-menu screenshot, scene/data/terrain/trigger workspaces and editor lifecycle stress against the packaged executable.
- Windows compiles/runs identical code and tests. GitHub Windows CI does not prove interactive Windows GPU stability. Manual driver testing is required before marking the creator suite stable.
- Keep the work in PR #1, keep it a draft and update `archive/new chat/RTS_HANDOFF.md` whenever a major implementation stage changes.

This project is **not** currently a clone of StarCraft II's editor. The priority is a robust and extensible engine/authoring architecture so those features can be implemented incrementally without separate platform forks.


## First graph execution milestone (implemented after native UI)

Authoring now recognizes `Effect` object catalogs with typed operations: Damage, Heal, Sequence. Graph validation enforces cycle detection and bounded complexity; editor links, incoming references, rename/delete migration and region trigger execution are integrated. This is a **headless C++ interpreter**, not copied from Galaxy or Recoil. Next design work: unit ability catalogs, target filters and effect contexts, scheduled behaviors, actor events and gameplay/perception-specific UI widgets.


## First visual ECA authoring milestone

The Trigger Editor now has a true event/condition/action inspector. Edit event kind and parameters, one-shot state, add or reorder typed conditions/actions, edit Unit/Region/Effect references with dropdowns, and use advanced JSON for more complex/extension data. `TriggerDefinition` supplies a single compiled rule validation authority for both authoring and simulation. Invalid edits are transactional and do not corrupt the open project. It is still rendered by the existing workbench adapter while the engine-native widget tree/docking system is developed. Implementation and next-stage constraints: `TRIGGER_EDITOR.md`.


## Current G0 increment: keyboard focus plus observable scripting

The native gameplay HUD/pause interface now has a reusable `FocusList` controller, keyboard arrows/W/S navigation, Enter/Space activation and Esc handling. Geometry, input selection and CTest remain independent of any GUI library; actual rendering still uses the engine's OpenGL resources.

The headless ECA runtime publishes a bounded execution history. The current editor adapter exposes a searchable playtest monitor and direct links back to ECA rules. This is **not** full native editor docking, rich text input, a variable-watch debugger or breakpoint stepping. The migration remains: native widget hierarchy/text/scroll/input → editor browser/inspector/terrain/trigger parity → remove the temporary ImGui adapter.


## Command capture and replay support

The headless simulation now includes `RTSCommandStream`, an immutable fixed-tick player-command queue and versioned map-embedded replay tape. `RTSSkirmish` captures commands from the mouse/HUD on submission with stable selected-unit IDs, consumes them in tick order, tracks accepted/rejected results and keeps an optional exportable digest timeline. A saved `.rtsreplay.json` can be validated without graphics via `RTSProjectTool --verify-replay`.

This is a step toward separating simulation from GUI for future network, editor scrubbing and AI systems. The current trigger interpreter still has its own data-driven event system and the editor remains on the existing ImGui presentation adapter. No borrowed RTS engine source is used; this implementation is original C++17. Full deterministic multiplayer/replay viewer and custom native UI remain open.

## Current script-event milestone (November 14)

Authorable integer globals and simple compare/set/add actions now execute headlessly and render through F6. `OnVariableChanged` observes a coalesced first/last value difference from the previous fixed tick, avoiding synchronous recursion and unbounded callback fan-out. The pending event map participates in deterministic replay digests. The live monitor shows current versus initial global values. This is a deliberately limited scripting foundation, not a full parser, lexical scope, typed AST, functions, yielded execution or Galaxy runtime. Progress toward custom actions, native GUI and actor/ability systems remains necessary.


## Recursive document tree and catalog commands (November 14)

`TriggerTree.hpp` defines a stable path made of `(listName,index)`
steps and validates transitions among a rule's root lists,
`IfThenElse.{conditions,then,else}` and `Repeat.actions`. These
paths are resolved against a fresh copy in `EditorSession` during
mutations, not held as pointers across GUI refreshes. Insertion,
deletion, duplicate, reorder, typed scalar property updates, expression
subtree replacement and literal/expression conversion all use
`mutateTrigger`, which runs the same project and trigger-rule validators
as headless tools. Undo/redo remains one shared snapshot history.

`CreatorTriggerTreeWidgets.hpp` is a replaceable ImGui presentation
adapter rendering recursive lists and arithmetic AST controls. It contains
no simulation logic. `CreatorTriggerWidgets.hpp` retains event/global
configuration while delegating ECA lists to the recursive adapter.

`EditorSession::createCatalogObject` and `convertEffectKind` similarly
create fully validated transactional F7 catalog objects and effect-kind
conversions, rather than injecting UI-only state or changing catalog
entries directly. On a rejected candidate both project and undo history
remain unchanged. Linux and Windows execute identical C++ source.


## Typed Ability authoring, references and runtime command cards

The F7 catalog creates Ability objects with a required typed Effect
reference, ownership target policy, cast range, cooldown and costs.
Unit catalog objects store ordered ability ID arrays. DataCatalog
validates every graph and optional Unit loadout. EditorSession's
createCatalogObject and setCatalogField transact through the same
headless project validation and snapshot undo/redo. The F7 inspector
provides typed target/effect selectors and Ability loadout controls.
Catalog rename migrates Unit→Ability and Ability→Effect references,
and inbound reference checks reject invalid deletion.

The independent HUD layer reads the same catalog through RTSSkirmish
and exposes up to three first-page actions for a single selected
caster. Gameplay never relies on GUI-only state: stable caster/target
IDs are serialized in RTSCommandStream and dispatched at fixed ticks.
For screenshot verification, CLI smoke flags --smoke-select-catalog-id
and --smoke-select-ability-caster select actual authored entries rather
than substituting a mock editor. Dedicated Linux Mesa CI artifacts
capture the F7 Ability inspector, F7 Unit loadout and real F9 HUD.


## F7 status modifier inspector and visual Effect DAG

CreatorWorkbench's Behavior panel uses a temporary typed
BehaviorDefinition draft and an Apply button. A new
EditorSession::setBehaviorDefinition clones the project, writes
all seven typed status properties, validates the entire catalog
and trigger rules, then records one undo snapshot. Rejected changes
leave the source map untouched. Templates create original
Fortify/Sharpen/Hinder Behavior categories.

Effect authoring gains ApplyBehavior/RemoveBehavior templates, kind
conversion that fills a valid Behavior reference, typed Status
reference navigation and dropdowns. DataCatalog::rename and the
project's inbound dependency scanner recognize Behavior references
from Effects, preventing silent dangling links. Existing Effect
Sequence editing now supports Up/Down, append, remove and Open
actions with validated transactions.

CreatorEffectGraphWidgets.hpp is a presentation-only adapter that
renders a bounded nested Effect sequence as an interactive tree
with an Edit action on graph nodes. It calls the existing headless
compiler for validity, never executes gameplay, and can be replaced
without changing the document model. Initial F7 inspector width
is responsive (roughly 40% screen width; constrained 400–540px);
custom user docking/resizing remains respected. Browser creation
controls no longer extend beyond the narrow browser panel.
The F6 trigger monitor includes a read-only status watch listing
actual live game state (Unit ID, status, stacks, seconds remaining).


## Ground Ability inspector, input and read-only rendering

EditorSession::createCatalogObject("Ability","Ground") supplies an
original valid preset. EditorSession::setAbilityTargetPolicy changes
between Ground and Self/Ally/Enemy/Any, adding valid nonzero range
and area defaults atomically. EditorSession::setGroundAbilitySettings
clones, validates, and saves grouped radius/team/delay properties
as one undo record; rejected edits leave the project unchanged.
CreatorWorkbench draws a typed F7 Ground inspector and team menu.

AppState::CastAbility mouse input differentiates Ground from Unit
targeting. Ground selection raycasts terrain, validates visibility
and range through RTSSkirmish, and enqueues a standard replayable
CastAbility command with the world point. The native shared
OpenGL renderer reuses its existing ringShader/VAO for cursor
radius (valid/invalid colors) and delayed impact rings. The
F6 diagnostics monitor displays pending impact count, Ability,
center, radius and remaining seconds using read-only watch data.


## F7 typed Projectile editor and F9 model overlay

EditorSession::createCatalogObject creates original Projectile Arc
or Bolt presets. EditorSession::setProjectileDefinition creates a
candidate document, writes all gameplay/visual fields, validates
the full catalog, and records one undo snapshot or rejects the
edit atomically. CreatorWorkbench has bounded numeric inputs,
a typed bool and RGB color picker, plus a Ground Ability
Projectile dropdown and link navigation. Switching an Ability
out of Ground targeting writes an explicit empty Projectile
override in the same transaction, so inherited Ground Ability
children can switch to Self/Ally/Enemy without retaining an
invalid inherited Projectile link. The parent remains unchanged. DataCatalog::rename and RTSProject::catalogReferences
understand Projectile -> Ability dependency edges.

The runtime owns flight positions, contact rules and impact timing.
main_entry_point_render_loop.cpp simply draws a procedural native
Unit mesh at the watch position, and a short tinted GL_LINES trail.
A 24-tick expanding hit ring reads the nonauthoritative impact
flash watch. F6 inspector shows each pending area as
PROJECTILE FLIGHT or IMPACT TIMER. All UI paths compile identically
into Linux and Windows; no duplicated gameplay implementations.

## Shared simulation/search-shape primitive (November 15)

RTSAreaShape.hpp is a renderer-free pure geometry module shared
by fixed-step Area Effect target filtering and F9 visualization.
It introduces Circle, Cone and Line, with a deterministic +X
fallback for coincident caster/ground points. Compiled
AbilityDefinition fields are validated through DataCatalog,
authored in F7 through EditorSession's transaction boundary,
and serialized by the existing project format without a
schema-version bump. The pending impact stores its own caster
origin so the world does not refer to mutable GUI selection or
later Unit facing. No new UI library or external RTS code was
imported. The more elaborate editor remains on its temporary
Dear ImGui adapter; in-house docking is still unfinished.

## Typed Data Navigator

RTSCatalogNavigator.hpp computes deterministic forward and reverse
links for Units, Actors, Models, Weapons, Projectiles, Abilities,
Behaviors and Effects, including parent and inherited references.
CreatorCatalogNavigator.hpp renders a dockable F7 relationship view,
with navigation buttons, source-field labels and searchable ID/type
jump. This replaces guesswork in JSON and does not touch authoritative
map data. The F7 Weapon editor also has typed Projectile delivery
selection, with grouped undoable transactions. Additional references
and the full specialized subeditor catalog remain incomplete.

## Numeric F7 inspector preview polish

RTSCatalogValuePreview.hpp renders human-readable decimal previews
for double-precision fields, avoiding binary serialization noise
(e.g. 0.69999999999999996 reads as 0.7). It affects only the
F7 property summary; raw stored picojson doubles, editable inputs
and serialized project/replay values remain exactly unchanged.
The catalog_value_preview CTest guards this presentation boundary.

## Typed F7 Actor Event authoring controls

Actors now have a specialized Inspector workspace for bounded visual
event rules, with event and style combos, color inputs, duration,
radius, add/remove, grouped Apply and Revert. The Actor | Event-driven
visual creation preset provides an original starter cue. All project
mutations pass through EditorSession::setActorCues, which validates
the entire candidate project and writes ONE undo record on success.

The same headless DataCatalog parser compiles cues during validation,
simulation spawning and per-instance Actor resolution. F6 consumes
a read-only live cue watch, and F9 displays original GL effects,
neither of which can change combat. This is a foundational
subeditor; full animation state/event term/message graph authoring
is still missing.

## Typed F7 Validator and Requirement editing

CreatorWorkbench now includes full typed forms for the new Validator
and Requirement catalogs: predicate kind/subject, numeric threshold,
typed Behavior/Unit references, Boolean operation selection and
editable nested Requirement children with add/reorder/delete.
EditorSession provides setValidatorDefinition and
setRequirementDefinition with complete candidate validation,
transactional Undo/Redo and invalid-edit rollback.
Unit and Ability inspectors offer a typed Required graph selector;
the Data Navigator exposes forward, reverse and inherited links.
Both gameplay and editor share the same headless catalog compiler.
This is a functional Boolean tree foundation, not yet a graphical
drag-wire node canvas or the complete Galaxy Data Editor.

## Typed F7 Upgrade inspector and F9 research cards

CreatorWorkbench now exposes Upgrade max level, cost/time, per-level
stat bonuses, exact Unit match and Requirement links. EditorSession
commits all changes in one validated Undo/Redo transaction. The Unit
inspector edits researchable_upgrades using typed array widgets and
the Validator inspector adds UpgradeLevelAtLeast and a typed Upgrade
picker. The F9 producer HUD shares its 3-slot/page training layout
with Upgrade research commands; F6 reads a nonmutating progress watch.
No duplicate Windows-only GUI implementation was introduced.

## F7/F9 Show/Use integration

Upgrade typed authoring supports two independent Requirement links,
committed atomically through EditorSession. Unit and Ability
inspectors have Show graph selectors plus existing Use selectors.
The Data Navigator indexes the separate graph edges. The F9
command-card layout derives both membership and enabled state
from the authoritative simulation rather than duplicating rules
inside the renderer. Empty-but-capable producers retain empty
production slots, not nonproducer fallback commands.


## Technology graph: editor projection over real engine data

`RTSTechGraph.hpp` composes the existing typed Catalog Navigator edges,
compiled Requirements/Validators and Upgrade definitions into a bounded
headless technology reference graph. It never mutates projects or redefines
simulation admission. `CreatorTechGraph.hpp` is the platform-shared
ImGui adapter providing a dockable graphical map, clickable catalog nodes,
focused one-hop navigation, hover diagnostics, NOT/ANY/Show/Use styling
and clipboard DOT export. `RTSProjectTool --tech-graph` uses the same model.
See `docs/TECH_TREE_MAP.md`. Full independent/native creator widget parity
is still incomplete; no additional UI library was introduced.


## Engine-owned AI research and owner-safe creator policy

The opt-in `RTSAIResearchPolicy.hpp` strictly parses and validates author settings; `RTSAIPlanner` chooses deterministic commands from already-legal research candidates; `RTSSkirmish` applies the source-1 ResearchUpgrade via shared admission/queues and isolated enemy mineral accounting. UI cannot directly mutate the world state. `EditorSession::setAIResearchSetting` commits one validated undoable whole-project transaction; `CreatorAIResearch.hpp` is merely its shared Windows/Linux dockable F7/F9 controller/monitor. Metadata priority references participate in catalog rename/delete safety. The replay format is unchanged and old maps still opt out. See docs/AI_RESEARCH.md.


## Native module separation and creator DPI (November 16)

The native creator uses one validated `EditorSession` and project as before. `CreatorWorkbench::moduleWorkspace` now owns a titled module surface per F5/F6/F7/F8 workspace and a resizable two-column Navigator/Properties table, rather than drawing all root panels directly over the game. Dear ImGui's existing pinned GLFW/OpenGL3 backend has `ViewportsEnable` on desktop so each selected module gets a real native OS window (per-window NoAutoMerge); the game's primary GL context is restored after rendering platform windows. Multiple editor windows are independently addressable but only one primary editing workspace is active at a time. Appearance model `CreatorUISettings.hpp` is headless, strictly validated, user-local JSON and never modifies map/replay state. `CreatorUIRuntime.hpp` supplies a blue/teal theme, focus-sensitive monitor DPI, font atlas reconstruction and absolute-base style scaling. See docs/CREATOR_WINDOWS_ACCESSIBILITY.md.


## First actual gameplay Interface Editor (November 16)

`RTSHUDProfile.hpp` parses/validates optional project `metadata.hud_profile`,
with bounded geometry settings and strict backwards-compatible defaults.
`RTSHUDLayout` now accepts a profile; this ONE headless geometry class
drives F9 native HUD rendering, command-grid hit testing, click interception
and minimap world-coordinate panning. The F10
`CreatorInterfaceEditor.hpp` is an original resizable native module,
a schematic interactive projection of those same geometry calculations,
with resolution presets, selection and movement, sliders, Apply/Discard/
Reset. `EditorSession::setHUDProfile` performs an atomic validated
undo transaction; the game reloads only after an Apply. The sample
and Linux real F10 secondary-window/F9 game screenshots enforce
presentation/use consistency, while `RTSInterfaceEditorTests` covers
metadata bounds, geometric hit regions and undo/redo. See
docs/INTERFACE_EDITOR.md. This is not a complete arbitrary frame/skin
UI scripting engine.


## Original authorable nested UI frames

The dedicated F10 Custom UI frames tab is backed by a headless
`RTSUIFrames.hpp` schema: at most 32 original Panel/Label/Button/Bar
elements with bounded, unique IDs, parent-first hierarchy (depth <=6),
five relative anchors, palette, bounded pixel geometry, live bindings
and whitelisted actions. `RTSUIFrameLayout.hpp` resolves child
rectangles and nested clips exactly once per caller; both
`RTSHUD::drawAuthoredFrames` and `mouseButtonCallback` use the SAME
geometry and topmost hit semantics. The only accepted button actions
reuse existing HUD commands (SelectArmy/SelectWorkers/FocusBase),
preventing direct simulation mutation or arbitrary scripting.
`EditorSession::setUIFrames` is a validated, one-step undo transaction;
default projects are unchanged. `CreatorUIFrameEditor.hpp` supplies
the native draggable, DPI-aware preview/hierarchy and inspectors
without moving editing logic into the GL renderer. See
docs/UI_FRAME_HIERARCHY.md and headless `RTSUIFrameTests`.


## Fixed-tick UI-to-trigger event pipeline

F10 Buttons can select `action="Trigger"` (always consumes world clicks).
F6's actual event dropdown adds `OnUIFrameClicked` with a selectable
`event.frame` reference to such buttons. `RTSCommandKind::UIFrameClick`
is appended to the existing wire enum without renumbering old commands:
valid player-source-only, unitless, coordinate-free, one-frame-ID
commands are enqueued for tick + 1 and captured in replay.
`RTSSkirmish::applyCommand` revalidates the target Button against
the authoritative project, then passes the ID into TriggerEngine's
bounded event queue. The engine dispatches events in command order
at its normal fixed tick, through shared ECA conditions/actions
and trigger tracing. `EditorSession::setUIFrames` and
`TriggerDefinition.cpp` enforce cross-document reference safety.
Existing core gameplay orders, HUD actions and replay serialization
remain backwards-compatible; only original typed command additions.
See docs/UI_TRIGGER_EVENTS.md.


## F6/F10 read-only data bindings and conditional UI

The optional F10 frame fields `variable`, `bar_min`, `bar_max`,
`visible_variable` and `visible_at_least` connect authored frames
to existing F6 typed integer map variables. `RTSProject::validate`
cross-checks every live reference using `parseTriggerVariables`.
`RTSUIFrameLayout` reads `TriggerEngine::variables()` through the
const RTS game context; its geometry/visibility result is shared by
F9 render and input hit-testing. A hidden parent hides all descendants,
including input-consuming Buttons. `EditorSession::renameTriggerVariable`
migrates F10 typed refs transactionally alongside F6 ECA nodes;
Remove rejects dangling references. The F10 UI exposes dropdowns,
bar bounds, integer visibility threshold and hidden preview markers.
See docs/UI_VARIABLE_BINDINGS.md and sample
projects/ui_variable_dashboard_showcase.rtsproject.json.


## Native F10 precision editing helpers

`RTSUIEditorGeometry.hpp` contains unit-tested, headless game-pixel
snap and clamp functions, parent rectangle resolution and
per-axis Start/Middle/End alignment that preserves anchors.
`CreatorUIFrameEditor.hpp` uses these functions for snap grid,
bottom-right resize interaction, arrow nudge and six align buttons.
Drag motion uses the initial press values plus total mouse delta,
avoiding cumulative rounding and DPI drift. The full frame
hierarchy is optional behind a foldout with an always-available
frame selector, so typed F6 data bindings appear near the top
on 720p/high-DPI creator windows. All edits are staged in the F10
draft and validated with the existing single-transaction Apply.
`RTSUIEditorGeometryTests` covers 4 resolutions and malformed
alignment/grid inputs without needing OpenGL.
