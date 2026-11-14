# Authoring tutorial: edit maps, actors, terrain and triggers

The current code is a **first editor implementation**, not yet feature-complete
with SC2's Galaxy Editor. It has one in-window procedural terrain workspace,
a generic JSON-property inspector, a standalone CLI and a small declarative
trigger engine.

## Open the game or the editor

From the repository root, after building (see root README):

```sh
./build/RTSCameraTerrain --project projects/default.rtsproject.json
./build/RTSCameraTerrain --editor --project projects/terrain_showcase.rtsproject.json
./build/RTSCameraTerrain --trigger-editor --project projects/terrain_showcase.rtsproject.json
./build/RTSCameraTerrain --scene-editor --project projects/terrain_showcase.rtsproject.json
```

Windows multi-config generators: `build/Debug/RTSCameraTerrain.exe`.

The `--editor` flag enters terrain editing with gameplay paused. Editing a
project loaded with `--project` saves to that exact path with **Ctrl+S**.
When launching without a project, the built-in/default map loads and the Save
target is `projects/working.rtsproject.json`.

| Input | In current prototype |
| --- | --- |
| F5 | Terrain editor; F5 again to play |
| F6 | Trigger inspector with JSON rule editing; F6 again to play |
| F7 | Generic catalog Data Editor; F7 again to play |
| F8 | Scene object and region editor; F8 again to play |
| F9 | Test/play the currently authored map |
| 1–7 (terrain) | Raise, lower, smooth, flatten, paint, block path, clear path |
| 8, 9, 0 (terrain) | Place mineral, catalog Unit, catalog Actor doodad |
| Left mouse | Paint or place; right mouse deletes nearest placed object |
| [ / ] and - / = | Brush radius and strength |
| M | Cycle terrain material 0–7 |
| P / Shift+P | Cycle placed unit type / doodad actor type |
| O | Change placement owner |
| Ctrl+Z / Ctrl+Y | Undo/redo across terrain, placements, catalog edits and trigger edits (32 snapshots) |
| Tab / Shift+Tab (data) | Next / previous catalog object |
| Up / Down (data) | Select a field |
| Enter (data) | Edit selected field as JSON, or bare text |
| Ctrl+N (data) | Add a new arbitrary field |
| Ctrl+D (data) | Create an inherited/derived catalog object |
| Ctrl+R (data) | Rename catalog ID and update known references (one undo action) |
| Ctrl+Backspace (data) | Remove the local override of the selected field |
| Trigger panel: Up/Down | Choose event-condition-action rule |
| Trigger panel: Ctrl+N / Ctrl+D / Delete | Create, duplicate or remove a rule |
| Trigger panel: Enter | Edit selected rule as JSON; Ctrl+V pastes JSON, Ctrl+C copies selected rule |
| Scene: Left-click / Shift+left-click | Select nearest placed object / move selected object onto terrain |
| Scene: Tab / Shift+Tab | Select next/previous scene object |
| Scene: P / R | Place Point / Region at terrain cursor |
| Scene: G / J / K | Move selected to cursor / rotate left / rotate right |
| Scene: , / . / O | Uniformly scale down/up / cycle owner |
| Scene: [ / ] | Shrink/grow region radius |
| Scene: Ctrl+D / Delete | Duplicate/delete selected scene object (referenced regions cannot be deleted) |
| Scene: Enter | Edit JSON per-instance overrides on the selected object |
| Ctrl+S | Save project |
| Esc | Cancel a field edit / leave editing for play |

The renderer currently uses a custom bitmap HUD, **not** a dockable Windows
property inspector. Scene editor provides viewport selection and simple keyboard-driven transforms
of placed units, doodads, Points and circular Regions. Region extents are
drawn as tinted ground overlays. There are no 3D move/rotate/scale gizmos yet.
Some editor interactions are still rough; notably
cross-document undo/redo and placing/editing point and region shapes are not
complete. The runtime renders procedural models even when an actor has a named
mesh file string.

## Headless map authoring / scripting

The same `.rtsproject.json` is a self-contained machine-readable project;
there is no requirement to open a GUI.

```sh
./build/RTSProjectTool --create projects/new.rtsproject.json
./build/RTSProjectTool --validate projects/new.rtsproject.json
./build/RTSProjectTool --list projects/new.rtsproject.json Unit
./build/RTSProjectTool --get projects/new.rtsproject.json ranger damage
./build/RTSProjectTool --set projects/new.rtsproject.json ranger damage 17
./build/RTSProjectTool --derive projects/new.rtsproject.json ranger super_ranger
./build/RTSProjectTool --rename projects/new.rtsproject.json super_ranger ranger
./build/RTSProjectTool --set projects/new.rtsproject.json super_ranger max_hp 300
./build/RTSProjectTool --paint-cell projects/new.rtsproject.json 24 24 4.5 2 1
./build/RTSProjectTool --place projects/new.rtsproject.json Unit super_ranger -6 4 0
./build/RTSProjectTool --placements projects/new.rtsproject.json
./build/RTSProjectTool --set-instance projects/new.rtsproject.json ranger_1 scale '[1.5,1,1.5]'
```

`--set-root` accepts a JSON literal and can replace root-level `triggers`
or arbitrary custom extension fields. Use a script/JSON editor for complex
trigger documents; single-shell-argument escaping varies by OS. Project validation
rejects missing catalog references, invalid terrain, invalid unit stats,
invalid trigger actions and out-of-bounds spawn positions.

## Data organization

A project contains: `schema_version`, `name`, `description`,
`terrain` (dimensions/spacing plus sparse painted-height cells),
`catalog` (typed objects with `id`, `type`, `parent`, `fields`),
`placements` (instances with `id`, `kind`, `catalog`, owner,
position/rotation/scale and per-instance overrides), `metadata` and
preserved arbitrary extension fields.

The `Actor` catalog controls *visuals* and links a `Model` catalog.
The `Unit` catalog controls gameplay stats and points at an `Actor`.
This allows a unit to inherit another unit's gameplay while selecting a
different presentation actor, and allows multiple objects to share a model.

**Per-instance overrides:** each placed Unit or Doodad can override fields
without affecting its shared catalog. A placed Unit accepts three special
objects inside `overrides`: `unit` for gameplay fields, `actor` for
presentation fields and `model` for linked model fields. Example:

```json
{
  "id": "custom_elite", "kind": "Unit", "catalog": "ranger",
  "owner": 0, "name": "Unique elite Ranger",
  "position": [-8, 0, 7], "rotation": [0, 35, 0], "scale": [1.2, 1, 1],
  "amount": 0,
  "overrides": {
    "unit": {"damage": 23, "max_hp": 150},
    "actor": {"tint": [1.0, 0.45, 0.12], "opacity": 0.85},
    "model": {"scale": [1.4, 1.0, 1.2]}
  }
}
```

This actually changes the unique instance's simulation stats, procedural color,
transparency and scale. Other Rangers are unaffected. The `mesh` asset
string also resolves independently but still draws a procedural placeholder
until the model importer is implemented.

**Implemented effect:** Unit health, armor, damage, attack range, cooldown,
speed, costs, basic worker/structure classification; actor tint, scale and
opacity; procedural terrain materials; scene positions; bounded triggers.
**Persisted but not yet used:** most arbitrary fields, real mesh/texture path,
animation/sound property strings, unknown instance-specific arbitrary overrides and advanced gameplay catalog
categories. The recognized `unit`, `actor` and `model` sections do apply.

## Declarative trigger code (first iteration)

A trigger has a unique ID, an `event` object, optional `conditions`, a
list of `actions` and optional `once`. Example:

```json
{
  "triggers": [
    {
      "id": "begin_mission",
      "event": {"type": "OnMapStart"},
      "once": true,
      "actions": [
        {"type": "ShowMessage", "text": "Welcome to your custom map"},
        {"type": "GrantMinerals", "amount": 100}
      ]
    },
    {
      "id": "enemy_wave",
      "event": {"type": "After", "seconds": 20},
      "once": true,
      "conditions": [
        {"type": "UnitCountAtLeast", "owner": 1, "count": 1}
      ],
      "actions": [
        {"type": "SpawnUnit", "catalog": "rift_raider", "owner": 1, "x": 20, "z": -10}
      ]
    }
  ]
}
```

Supported events: `OnMapStart`, `After`, `Every`, `OnUnitDeath`,
`OnRegionEnter` (circular region with radius in placement overrides),
`MineralsBelow`. Conditions: `PlayerMineralsAtLeast`,
`UnitCountAtLeast`, `UnitTypeCountAtLeast`, `SubjectOwnerIs`.
Actions: `GrantMinerals`, `SpawnUnit`, `EndMatch`, `ShowMessage`.

The **F6 trigger inspector** supports browsing, adding, duplicating, removing
and editing the serialized rule JSON. It is a text-based inspector, not
a drag-and-drop trigger node graph. The inline text buffer is limited
to 768 ASCII characters: use project JSON files/CLI for larger rule graphs.
Project reload validates triggers and rejects malformed actions.

The engine **does not execute raw C++, arbitrary OS commands, Lua, or Galaxy**
from project files. This first trigger runner is intentionally an explicitly
validated ECA interpreter. The eventual open modding goal needs a sandboxed VM,
a designer-facing trigger graph editor, typed events, localization, authoring
autocomplete, profiling and debugger.

## Known limitations

- Navigation now uses cell-by-cell terrain gradients to block slopes above
  the maximum walkable rise/run, and adds slope costs to weighted A*.
  Configure `metadata.max_walkable_slope` (default 1.25) and
  `metadata.slope_cost_multiplier` (default 2). The terrain editor's red
  blocked-path overlay highlights both manually blocked cells and slopes
  blocked by the same gameplay threshold. These are global values;
  per-unit climb ability, ramps/cliffs, physics and water pathing are TBD.
- Terrain size and material count are currently bounded for simplicity.
- Arbitrary actor mesh references are preserved but not imported/rendered.
- The current terrain editor may reload the match after editing placed objects;
  a future play-in-editor system should preserve selections and state.
- Editing now uses a single chronological 32-step snapshot history across
  terrain strokes, placed objects, catalog properties and trigger JSON.
  Undo/redo applies to all three editor panels. Autosave, transaction
  coalescing for held keys and recovery after application crashes are TBD.
- End-game hardcoded enemy waves still coexist with trigger-authored events.
- Custom triggers are not yet directly editable through a visual graph.

## Editor reliability and crash recovery

Editor input callbacks now catch C++ exceptions locally instead of allowing
exceptions to escape GLFW's C callback boundary. Errors appear as in-game
notices and are appended to `rts-editor-error.log` in the current working
directory. The first run with these guards cannot prevent native graphics
faults or out-of-memory conditions, so real-machine crash logs still matter.

Edited projects autosave every **25 seconds** to `<project>.autosave`
when an editor panel is active and the document has unsaved changes. This
never overwrites the saved project. When a newer autosave exists, the editor
loads it at startup, marks the document modified, and offers **Ctrl+S** to
keep the recovery. An ordinary successful Ctrl+S removes the stale autosave.
Use the launch scripts or open `--project <file>` explicitly to keep the
correct per-project recovery location. The software OpenGL / CI smoke tests
are not substitutes for a Windows real-machine crash reproduction.

### Windows native crash dumps

For Windows crashes that bypass the protected GLFW callbacks (for example,
access violations in the graphics driver or renderer), the application
installs a last-chance exception handler that writes a **local**
`rts-crash-YYYYMMDD-HHMMSS.dmp` file in its working directory. The
application does not upload dumps. Send the matching dump and
`rts-editor-error.log` privately when reporting a crash, along with
what editor mode/operation triggered it. Minidumps may contain portions
of process memory; do not post them publicly without reviewing privacy.

This exception handler records evidence; it is not a fix for arbitrary
native faults. Other crash handlers or an attached debugger can take
precedence, so dump generation is best-effort.

### Editable unit sight range

A Unit catalog's `sight_range` controls how many world units it reveals
around a living player-owned unit or building. Bounds: **0–128**, default
14 for the ranger. In the Data Editor (F7), select a Unit and edit its
`sight_range` numeric property. Individual placed instances can override
it in F8 Scene Editor using `{"unit":{"sight_range":24}}`.
Heightfield crests interrupt line of sight; explored but not currently
visible areas remain darkened. F5–F8 editor panels intentionally suppress
fog. This is the first LOS layer, not yet Spring-style radar/jamming.

### Data Editor object search (F7)

Press **Ctrl+F** while in the Data Editor to filter the catalog by case-
insensitive **object ID, type, or directly authored name**. Enter applies
the filter; Tab/Shift+Tab now cycle only matching objects. Press
**Ctrl+Shift+F** to clear the filter and return to the entire catalog.
The inspector displays match count and the active search string, and
retains the previous filter when a new query has no matches.

Filtering is a view-only operation; it never edits serialized data and
does not create an undo entry. This is groundwork for an eventual
tree/list property browser, rather than a full SC2 Data Editor UI.

### Formation spacing property

In a project file's top-level `metadata` object, set
`"formation_spacing_multiplier": 3.2` to widen the spacing of grouped
move/attack-move/patrol destinations. The current implementation clamps the
multiplier to **1.5–6.0**. The minimum unit spacing also accounts for the
largest selected mobile unit's radius. This is a map-wide gameplay setting;
per-unit formations, selectable squad geometries and a visual formation
authoring panel remain TBD.

### Catalog object identity

The catalog now indexes object IDs for responsive search and editing as
projects grow. Use **Data Editor duplicate/derive actions** or the
`DataCatalog::add/cloneDerived` API to create entries and `setField` to
author fields. The `entries()` collection is intentionally read-only:
mutating entry IDs directly would corrupt object references. Safe object
renaming with reference migration is planned but not yet implemented.

## Catalog-driven production (creator suite foundation)

A producer is any Unit catalog object with `is_structure: true` and an
inherited or local `trainable_units` list of at most 12 **non-structure**
Unit catalog IDs. Unit mineral cost, supply, build time, name, actor and
worker role are resolved from the trained unit's own catalog definition.
Modify this property in F7 Data Editor as a JSON array, for example:

```json
"trainable_units": ["ranger", "worker", "field_medic", "raider"]
```

In play mode, select exactly one friendly producer. The B/T/C command cells
become its three contextual training slots. **Tab** cycles sets of three when
there are more units. The selected building owns its individual queue
(maximum five reserved jobs), rally point, resource costs and cancellation.
Without a producer selected, legacy B/T/C build/ranger/worker commands
continue to work. Old project files lacking `trainable_units` retain the
built-in base/worker and barracks/ranger training roles.

The validator checks every typed reference (existing Unit, no duplicates,
no structures), maximum 12 slots, and JSON array shape. Catalog renaming
migrates these typed links; arbitrary custom fields still round-trip.
This is a real runtime training mechanic, not merely an editable string field.
Tech prerequisites, upgrades, ability graphs and multiple queue lanes are
not implemented yet.

### Shipped production lab

Use `run-production-lab.bat` or `run-production-lab.sh` from an installed
build. `projects/production_showcase.rtsproject.json` demonstrates Unit
inheritance, actor tint, two distinct training lists, six units (including
catalog-derived Medic/Guard/Scout/Sniper), pages, rally points and per-building
queues without editing source code. F7 edits those fields; F9 play-tests.

## Native creator GUI / docking workspace

The editor now has a **real mouse-driven GUI** rather than a text-only HUD.
Click the **Workspace** menu or the Play/Terrain/Data/Triggers/Scene tabs
at the top to switch modes. Dock, move and resize the **Creator | Browser**
and **Creator | Inspector** windows; the central world view remains an
interactive 3D viewport.

- **Terrain:** click the tool palette to sculpt, smooth, flatten, paint
  textures, edit blockers, or place Units/Doodads/Minerals. Use brush
  radius/strength and catalog dropdowns; paint the 3D viewport with LMB.
- **Data:** search object IDs/types, select definitions in the list, inspect
  resolved properties, rename/derive, and edit or add JSON values.
- **Scene:** select placed instances in the hierarchy, change owners, apply
  numeric position/yaw, scale, duplicate or delete, and edit instance JSON.
- **Triggers:** select, create, duplicate or remove ECA rules and edit JSON
  using a multiline input field.
- **Project:** save, undo or redo from the menu. Ctrl+S, Ctrl+Z and Ctrl+Y
  remain keyboard accelerators.

These controls invoke the existing **EditorSession** authoring methods. This
pass is a functional GUI foundation, not yet the full Galaxy Editor's object
model, trigger wizard, terrain paint layers or visual actor/action graphs.

### Confirming Windows/Linux binary parity

Run `RTSCameraTerrain.exe --version` (Windows), or
`./RTSCameraTerrain --version` (Linux), from the **extracted release bundle**.
It reports `RTS_BUILD_COMMIT` and `RTS_FEATURE_*` entries, including
`FOG_OF_WAR`, `GAMEPLAY_HUD`, and `CREATOR_GUI`. Compare the commit to
`build-info.json`. The build workflow now executes the **bundled native
executable** and refuses to upload when commit or required features do not
match; simply producing a successful compiler exit no longer suffices.


## Scene/catalog links and typed fields
In **F7 Data Editor**, the inspector follows Parent, Unit Actor, Actor Model and trainable-unit references by clicking their links. The incoming-reference section lists recognized references to the selected ID. Inherited fields are shown separately from local overrides. Boolean, number and string fields have direct controls; each accepted change is validated and recorded as one undo transaction. Complex JSON types still use the advanced text editor. In **F8 Scene Editor**, the selected placement's linked catalog ID opens the corresponding F7 entry. Raw extension fields are round-tripped, not silently dropped.

During play, **Esc** opens the engine's custom pause menu. No additional UI dependency is needed for its layout or rendering. The editor itself still uses ImGui pending the native replacement described in `docs/CREATOR_ARCHITECTURE.md`.


## Original starter content, legacy projects, and type filtering

The built-in demo is a self-contained original sandbox, **not a StarCraft II map or asset pack**. New project templates use `ranger`, `rift_raider` and `rift_bastion` with authored display names and actor IDs. Prior project JSON files that embed their own older catalog names remain supported through legacy role lookups. The `player_ranger` metadata role is optional for new projects; `player_marine` is accepted only as a compatibility fallback. The Data Editor's type selector lists types from the active catalog, not a hardcoded list, so creators can add new object categories. Object browsing uses clipped visible rows for scalability. References to SC2 Galaxy Editor describe architecture and UX only, never imported content.


## Typed Effect graphs (Damage, Heal, Sequence)

The shared project catalog supports `type: "Effect"` entries with:
- `{"kind":"Damage","amount":12}` — deals damage to a target unit, using normal armor mitigation.
- `{"kind":"Heal","amount":8}` — heals a living target, clamped to maximum health.
- `{"kind":"Sequence","children":["kinetic_pulse","restorative_pulse"]}` — executes a bounded list of other Effect IDs in order.

All effect nodes are validated before map load/save: maximum 16 stack depth, at most 64 total visited nodes, 1–8 children per Sequence and a numeric amount in the range 0–100000. Cycles, missing or mismatched referenced Effect IDs, and invalid effect kinds fail validation. Renaming/deleting effects observes graph references, inherited values and typed trigger references.

Trigger ECA gains `ApplyEffect` for **OnRegionEnter** events, operating on the entering subject unit and optionally filtered with `SubjectOwnerIs`. For example, an action can be `{"type":"ApplyEffect","effect":"kinetic_pulse"}`. This is the first concrete data-driven effect executor, not yet a general targeting/ability/weapons system.

Run `RTSCameraTerrain --project projects/effects_showcase.rtsproject.json` (Windows: `RTSCameraTerrain.exe`) to load the included playable example, or `--editor --project projects/effects_showcase.rtsproject.json` to inspect the region, event rule and linked Effect objects through F7/F6/F8. Its region applies a short damage/heal sequence to an entering friendly unit once. Existing custom maps do not require adding these entries.
