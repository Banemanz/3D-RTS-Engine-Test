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
| `OnVariableChanged` | A named script global changed during the preceding fixed tick | Variable selector |

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

### Map variables (shared trigger state)

F6 now has **Map variables (shared trigger state)** above the ECA inspector. Add a variable by ID, edit its initial integer, or remove it using the mouse. IDs use ASCII letters/digits/underscores and must begin with a letter or underscore. Project JSON stores these in the optional top-level `trigger_variables` object (for example `{"wave":0,"score":0}`).

All triggers in one map share the values; restarting the match restores the authored initial values. Map variables are signed **integers limited to -100000000..100000000**, with a maximum of 64 per map. Addition saturates at the bounds. References are validated during authoring/loading: deleting a variable still used by a rule is rejected, preserving history and project data.

- **VariableCompare** condition: a named variable, one of `==`, `!=`, `<`, `<=`, `>` or `>=`, and a literal integer.
- **SetVariable** action: assign an integer literal.
- **AddVariable** action: increment or decrement by an integer, clamped to the permitted range.

Example: OnMapStart → SetVariable `wave` to 1; Every 15 seconds → AddVariable `wave` by 1; a second rule with VariableCompare `wave >= 4` → SpawnUnit using an original map Unit catalog. A **OnVariableChanged** event can subscribe to a map variable and react on the following 60 Hz simulation tick. Repeated writes to one variable within a tick are coalesced; a change that returns to the starting value does not emit an event. This avoids recursive calls and bounds the number of pending variables to 64. The changed event has no old/new-value payload yet: use VariableCompare to test its current value. These are bounded sandboxed nodes, **not** a Galaxy-language compiler, local scopes, branching, loops or coroutines.

Replay state digests include nonempty trigger globals; maps without globals retain the older digest behavior for v1/v2 compatibility. `RTSTriggerVariableTests` produces `trigger_globals_fingerprint.txt` and CI compares Windows and Linux digests.
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


## Live playtest trigger monitor (November 14)

Select **View → Playtest trigger monitor**, or **Open playtest monitor** in the F6 rule inspector. You can leave the window visible during F9 playtesting and then return to F6. The monitor reads actual fixed-step `TriggerEngine` executions, not a simulation of their JSON. It provides a search field, per-rule **Tried / Fired / Blocked** counters, and time-stamped rows showing event type, rule ID, subject-unit ID (where present), action names or the first failing condition. Click **Open rule** to navigate to that ECA definition.

**Capture trigger events** toggles collection, not script execution. **Clear event log** discards recent rows without resetting rule counters. Project edits, reload and match restart reset all counters and history. The bounded ring retains the most recent **192 events**, lives only in memory and is never written into map files. A zero attempt count means no matching event occurred; a blocked count means conditions were checked and failed.

The original `projects/trigger_diagnostics_showcase.rtsproject.json` demonstrates one successful OnMapStart initialization, one blocked high-mineral condition and one region-triggered Effect. Run `RTSCameraTerrain --project projects/trigger_diagnostics_showcase.rtsproject.json` and open the monitor. CI also captures the real OpenGL window with `--trigger-editor --smoke-trigger-monitor --project projects/trigger_diagnostics_showcase.rtsproject.json --smoke-frames 120 --screenshot rts-trigger-monitor.ppm`.

This is **execution logging, not yet breakpoints, single-stepping, a watch debugger or a full scripting profiler**. The monitor is currently rendered by the existing temporary editor GUI adapter; its trigger parsing and logging are headless C++ shared between Windows and Linux. The design draws on documented Galaxy editor functionality, but includes no proprietary implementation or assets.

### Play the included event-driven scripted map

Load `projects/script_relay_showcase.rtsproject.json` from the bundled `projects/` folder and open F6. Two map globals (`alarm_level` and `raid_count`) are initialized by project data. A 2-second timer increments the alarm; a variable-change event reacts by spawning a creator-catalog Raider and incrementing the raid count; another variable-change listener rewards resources after multiple raids. Open the Playtest trigger monitor to watch the initial/current values and executed rule history. Automated tests verify the same sample through the headless interpreter, a 330-tick scenario and replay verification on Windows and Linux. The Linux screenshot artifact is `rts-scripted-relay-screenshot` from the **Tests + screenshots (NOT executables)** workflow; the playable ZIP is in **Download Windows EXE and Linux RTS Builds**.

Variable IDs also have an **F6 → Rename** control. Renaming is one validated, undoable operation: it migrates precisely typed `OnVariableChanged`, `VariableCompare`, `SetVariable`, and `AddVariable` references in every trigger, while leaving unrelated text and custom-extension fields untouched. Invalid names and duplicate IDs are rejected without changing the project.

### Typed integer expressions (November 14 continuation)

Actions and conditions that previously required fixed numeric operands may now
use **bounded expression trees** with map-global references. This is
independent, original engine behavior, not a Galaxy source-code parser.

- For a VariableCompare condition or SetVariable/AddVariable action, replace
  `"value": 5` with `"value_expr": {"op":"Variable","name":"raid_count"}`.
  For a GrantMinerals action, replace `"amount": 5` with
  `"amount_expr": {"op":"Variable","name":"raid_count"}`.
- Supported operations: Literal, Variable, Add, Subtract, Multiply, Divide,
  Modulo, Min, Max, Negate, Absolute, Clamp (value/minimum/maximum).
  Binary nodes use `left` and `right`; unary nodes use `operand`.
- A nested example:
  `{"op":"Multiply","left":{"op":"Variable","name":"raid_count"},"right":{"op":"Literal","value":10}}`.
- The F6 trigger inspector can switch these operands between literal and a
  direct map-variable reference using validated undoable widgets. More complex
  expressions are edited through **Advanced JSON**; the shared headless parser
  validates them before committing.
- The engine enforces at most **8 expression levels and 64 nodes per expression**.
  Every intermediate result is clamped to the integer range -100000000 to
  100000000; division truncates toward zero. Division and modulo by zero
  deterministically produce 0. References must resolve to declared globals.
  Ambiguous simultaneous literal/expression fields and unexpected AST keys fail
  validation. Expression references migrate during safe global rename.

The runnable `script_relay_showcase.rtsproject.json` now uses an expression for
its reward size. The new `RTSScriptExpressionTests` checks math boundaries,
validation, editor transactions, replay verification and generates the
Windows/Linux strict parity file `script_expression_fingerprint.txt`.

### Safe branching and bounded loops

The ECA runtime now executes nested **IfThenElse** and **Repeat** action
nodes as real simulation logic. These are declarative script objects, not raw
C++ or arbitrary Galaxy-language text.

- `IfThenElse` has `conditions` (1..16 normal ECA conditions), `then`
  (1..32 ordered actions) and optional `else` (0..32 actions). All conditions
  must pass; only one branch executes.
- `Repeat` has `actions` (1..32 actions) plus either `times` (0..16) or
  `times_expr` (a validated integer expression). A dynamic count is clamped
  to 0..16, evaluated once when the loop begins.
- Nested control-flow depth is bounded to 4, with at most 256 total authorable
  action nodes per trigger and a validated **512-step worst-case execution
  budget** (counting even the maximum possible variable-driven repeats).
  Invalid graphs are rejected before saving. Repetition, branches and variable
  writes occur within the authoritative fixed-step simulation and participate
  in replay verification.

The F6 action menu offers both node types with valid initial templates.
Expandable summaries show their nested branches/conditions; edit nested lists
through the existing full-rule Advanced JSON inspector until the recursive
property widget milestone. This is a partial Galaxy-style control-flow
foundation, not lexical scopes, custom action definitions, unbounded loops,
coroutines or a Galaxy source compiler. The scripted relay showcase now uses
IfThenElse + Repeat + a map-variable expression to calculate its resource
reward. Run `RTSScriptFlowTests` to exercise real execution, validation,
serialization, undo/redo, variable-reference migration and replay parity.
