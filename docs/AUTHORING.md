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
./build/RTSProjectTool --get projects/new.rtsproject.json marine damage
./build/RTSProjectTool --set projects/new.rtsproject.json marine damage 17
./build/RTSProjectTool --derive projects/new.rtsproject.json marine super_marine
./build/RTSProjectTool --set projects/new.rtsproject.json super_marine max_hp 300
./build/RTSProjectTool --paint-cell projects/new.rtsproject.json 24 24 4.5 2 1
./build/RTSProjectTool --place projects/new.rtsproject.json Unit super_marine -6 4 0
./build/RTSProjectTool --placements projects/new.rtsproject.json
./build/RTSProjectTool --set-instance projects/new.rtsproject.json marine_1 scale '[1.5,1,1.5]'
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
  "id": "custom_elite", "kind": "Unit", "catalog": "marine",
  "owner": 0, "name": "Unique elite Marine",
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
transparency and scale. Other Marines are unaffected. The `mesh` asset
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
        {"type": "SpawnUnit", "catalog": "zealot", "owner": 1, "x": 20, "z": -10}
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

- Heightfield movement follows terrain height visually but uses a flat XZ
  navigation planner; slopes/cliffs/water do not yet have physical/path costs.
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
