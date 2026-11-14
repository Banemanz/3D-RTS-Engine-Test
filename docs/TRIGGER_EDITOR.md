# Visual Trigger Editor: event–condition–action authoring

This project is an independent RTS engine and creator toolkit. Its editor workflow is informed by documented RTS authoring patterns, not copied game code, art, story, sounds or UI assets.

## Open and build

Open a project using `run-editor.bat` on Windows or `./run-editor.sh` on Linux, then select **TRIGGERS** (F6). The left **Creator | Browser** panel lists all map rules and has New Trigger, Duplicate, and Delete. Selecting a rule opens its structured inspector on the right. Changes operate on the project JSON through **EditorSession**; there is no separate format or platform implementation.

Every rule consists of an event, zero or more conditions, and at least one ordered action. The inspector lets you select an event type, set its typed fields, choose whether it runs once, add conditions/actions from menus, edit their typed values, reorder nodes, and remove nodes. The same project can still be opened in the advanced full-rule JSON editor for extensions not yet covered by the visual controls.

### Events

| Event | Meaning | Inspector properties |
|---|---|---|
| `OnMapStart` | Project begins | Run once |
| `After` | Elapsed simulation time reaches a threshold | Seconds (0–360000) |
| `Every` | A repeating simulation-time interval | Seconds (0.1–360000) |
| `OnUnitDeath` | A unit dies | Optional owner condition |
| `OnRegionEnter` | Living mobile unit enters a named scene Region | Region selector; receives a subject unit |
| `MineralsBelow` | Player minerals fall below a threshold | Amount |

To use **OnRegionEnter**, first create a Region in the **SCENE** editor and save it. Then choose the region by ID in the Trigger Editor.

### Conditions

`PlayerMineralsAtLeast`, `UnitCountAtLeast`, `UnitTypeCountAtLeast`, and `SubjectOwnerIs` are supported. All conditions on a rule must pass. Unit references are chosen from the map's **actual Unit catalog**, including custom modded types.

### Actions

| Action | Effect |
|---|---|
| `ShowMessage` | Displays a short message on the gameplay UI |
| `GrantMinerals` | Adds or removes player mineral resources |
| `SpawnUnit` | Spawns an authored Unit catalog entry at map coordinates |
| `ApplyEffect` | Applies a typed Effect catalog entry to the unit entering a region |
| `EndMatch` | Ends the skirmish in victory or defeat |

**ApplyEffect** only works with `OnRegionEnter` because it requires a subject unit; the visual menu disables it for other events. Create or derive an `Effect` catalog entry using F7. Current Effect operations are Damage, Heal, and Sequence; validation bounds depth, execution length, and child references. The `effects_showcase.rtsproject.json` map demonstrates this.

### Example: announce a map objective after a delay

1. Create a trigger; give it a name in Advanced JSON if needed.
2. Change Event kind to `After`, set seconds to `3`.
3. Leave Run only once enabled.
4. Use Add action... → `ShowMessage` and edit the text. Remove the original placeholder action if you prefer.
5. Save the project with Ctrl+S. F9 enters Play; restart to run OnMapStart/After events from the beginning.

The engine's behavior is driven by the actual saved project and its catalog; the editor does not synthesize separate runtime logic.

## Safety, history and performance

- All structured edits are atomic: copy, apply, validate the same ECA parser as the simulation, commit and record one editor undo action. Invalid edits leave the project and existing history unchanged and display a validation message.
- Full-rule JSON edits also validate *before* changing the project. Advanced mod fields are retained when not edited.
- Each rule is limited to 32 conditions and 64 actions, and maps to 256 triggers; event intervals, strings, IDs, coordinates, references, and effect execution are bounded.
- The visual UI is currently implemented by the existing pinned ImGui editor presentation adapter. It is not a new engine dependency. The simulation, project storage, trigger parser, and undoable API are headless C++. The in-house retained UI migration is documented separately in `CREATOR_ARCHITECTURE.md`.
- Both Windows x64 and Linux x86_64 run the same authoring tests and bundled feature verifier; Linux additionally renders the actual editor under Mesa/Xvfb.

## More research

- [Galaxy Trigger Editor interface guide](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/033_Trigger_Editor_Interface/)
- [Galaxy Events overview](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/035_Events/)
- [Galaxy Actions overview](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/034_Actions/)
- [Recoil synced simulation vs unsynced UI architecture](https://recoilengine.org/docs/guides/getting-started/widgets-and-gadgets/)

These are UX and architecture references only; no proprietary implementation or content is included.
