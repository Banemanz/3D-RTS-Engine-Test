# Player controls and scenario

The camera pans with **W/S/D and the Arrow keys**. A is intentionally reserved for
attack-move commands; use **Left Arrow** to pan left. The mouse wheel zooms and
Q/E rotates the camera.

**Left-click** selects one friendly unit/building, **Shift + click** toggles a
unit in the selection, and drag-box selects friendly mobile units. Enemy click
selection is inspection-only (enemies are never controllable). **Right-click**
normally moves selected units, attacks an enemy under the cursor or, when workers
are selected, assigns the nearest clicked mineral patch for repeated collection.

**A** arms attack-move; **right-click** the target location to commit. Units
continue their routes after tactical engagements. **B** arms Barracks placement;
**right-click** a clear location within 18 world units of a selected Worker.
Successful placement spends 150 minerals and takes 9 seconds to finish.
**Shift + right-click ground** appends movement/attack-move destinations to the
current route for units already moving. Queued terrain commands do not currently
support queuing targeted attacks or worker gather jobs.

**C** queues a Worker at the Command Center (40 minerals); **T** queues a Ranger
at the first complete Barracks (50 minerals). Production requires available
supply and is sequential per structure. **X** stops selected mobile units.
**Ctrl + 1-9** saves currently selected friendly unit IDs into a control group;
press the corresponding **1-9** key to recall the surviving members.
**R** restarts the skirmish (and clears control groups); **Esc** exits.

This skirmish is a prototype: no construction preview, minimap, fog, control
groups, production selection UI or building placement feedback beyond console.

## RTS Gameplay HUD (new)

The gameplay screen now uses a top resource strip, a bottom-left tactical
minimap (click to center the camera), a selection/health panel and a clickable
3×3 command grid instead of a debug-text overlay. HUD clicks are consumed
before world raycasting, so buttons do not accidentally issue game orders.
Commands include move **M**, attack-move **A**, stop **X**, build **B**,
train Ranger **T**, train Worker **C**, select all army **F2**, focus/select
Command Center **H**, and patrol **P**. Patrol orders move selected units
between their original position and the clicked destination indefinitely
until stopped or replaced. **Esc** cancels an armed targeting command before
exiting on a second press. **Shift+RMB** queues waypoints and exits patrol
after its current leg.

Visuals are procedural (no imported command icons, portraits or art assets
yet). The HUD is rendered with the existing OpenGL pipeline and currently
has no user-configurable layout system or Unicode font atlas.

**Building rally points:** Select a completed Command Center or Barracks
and right-click reachable terrain to set its rally point. Trained workers and
Rangers automatically path toward that destination. Select the structure to
see its rally marker on the battlefield and current training queue count
in the selection panel. A mixed mobile+structure selection issues ordinary
unit orders instead of accidentally changing the rally point. Rally points
are current-match state and reset on restarting the skirmish.

**Production cancel and progress:** Selecting a producing building displays
its queue length and a small progress indicator. The final command-grid
slot becomes **CANCEL Q (V)** when a production structure is selected; it
removes the newest pending order for that building and refunds its mineral
and supply reservation. Production still occurs one item at a time per
building; ordering remains prototype-only, not a full production planner.

### Multi-unit selection card grid

Select several friendly units (box-select or press **F2**). Their details now
appear as a card grid in the bottom selection panel, with individual HP
stripes and compact class abbreviations. **Left-click a card** to isolate
that unit; **Shift+left-click** removes/adds it from the current selection.
Cards are laid out from the same pure `RTSHUDLayout` geometry used for
mouse hit-testing. Groups too large for the panel show a `+N MORE UNITS`
indicator. Empty selection-panel clicks are consumed instead of causing
world selection or movement.

## Command-grid expansion: hold position and worker selection

The RTS game command grid now contains **12 clickable actions** in four rows,
including **Hold Position (G)** and **Select Workers (F3)**. The last button
cancels an armed targeting mode. The other actions still use their original
shortcuts: M move, A attack-move, X stop, B build, T Ranger, C Worker,
F2 select army, H focus the home base, P patrol, and V cancel production.

**Hold Position (G)** immediately stops all selected mobile units and prevents
separation steering or existing momentum from sliding them around. They can
automatically attack enemies already inside their weapon range but do **not**
chase, even if those enemies withdraw. X restores normal idle/aggro behavior,
and a new movement or explicit attack order also clears Hold Position.

**Attack-move resumes** its authored destination after an interrupted fight,
replanning from the unit's present position instead of silently skipping the
interrupted waypoint. Patrol likewise resumes the interrupted leg.

**F3** selects all living player-owned workers; this is distinct from F2's
combat army selection, and complements building rally points and harvesting.
The HUD highlights active Hold Position orders and shows a gold stripe on
held unit cards.

### Contextual training (data-driven producer)

Select one friendly structure with a non-empty `trainable_units` list:
**B**, **T**, and **C** become training slots 1, 2, and 3. The bottom-right
HUD displays the actual catalog unit IDs. **Tab** pages through slots 4–12,
three at a time; the bottom-right command is the page switch. **V** cancels
the most recent queued production on the selected structure, refunding its
reserved minerals and supply. Right-click sets its rally point.
When no producer is selected, the older B build / T ranger / C worker shortcuts
continue to work.


## Engine-native pause menu
During a match, press **Esc** with no active command to pause. Esc again resumes. Click **Resume Match**, **Restart Match**, **Open World Editor**, or **Exit Game**. Simulation, tactical orders, and camera input are suspended while paused. An active move/build/attack command is cancelled by Esc first. In editor mode, Esc returns to play without closing the process.


## Visual ECA shortcuts and workflow

Press **F6** in the game to open Trigger Editor. Use **New trigger** in the Browser; select a trigger and the Inspector shows its event, conditions and ordered actions. Mouse menus can add event types, conditions and actions, assign their typed fields or references, and move/remove individual nodes. **Ctrl+Z** undoes the last committed authoring action and **Ctrl+Y** redoes it. Save with **Ctrl+S**, and use **F9** to playtest. Raw JSON is available under Advanced. `docs/TRIGGER_EDITOR.md` describes the supported operations and restrictions.


## Native pause-menu keyboard controls

During play, **Esc** opens the engine-rendered pause menu (after cancelling a pending tactical command). **Up/Down** or **W/S** move the highlighted choice, **Enter** or **Space** activate it, and **Esc** resumes. Mouse clicks use the same action dispatcher. Both operating systems use `NativeFocus.hpp` for focus navigation; no additional GUI dependency is involved.

## Inspecting live triggers

In F6 use **Open playtest monitor**, or select **View → Playtest trigger monitor**. F9 begins a playtest; inspect rule attempt/fired/blocked counters and search recent recorded events. **Open rule** returns to the corresponding definition. The monitor continues to display live events in Play mode when left open. Reopening/reloading the project resets history. See `docs/TRIGGER_EDITOR.md` for constraints and the debugging example.


## Creator replay controls

- **F9:** start a new Play session and its recording baseline
- **Ctrl+Shift+L:** export the in-memory Play mode command tape and state checkpoints to `<project-path>.rtsreplay.json`
- **View → Export playtest replay:** same function via the editor menu, enabled only in Play
- **RTSProjectTool --verify-replay PATH:** headless 60 Hz replay verification, on both Windows and Linux; returns a nonzero error with the first mismatching tick if simulation diverges

Selection, camera and editor layout are not recorded game commands. The former immediate mouse/HUD gameplay orders run at the next fixed simulation tick, and a rejected order now produces a deferred gameplay message. Detailed command debugging and interactive replay playback remain forthcoming.


## Playtest Ability command controls

Press F9 to enter gameplay. Select one allied Unit with an authored
Ability loadout. The first three assigned skills replace unavailable
Build/Train slots in the command grid, while movement and other orders
remain accessible. Press F for slot 1, J for slot 2 or K for slot 3.
Self-target skills queue immediately; other policies enter targeting
mode. Left-click a valid visible unit in range to queue the ability.
Right-click cancels targeting. Disabled buttons indicate insufficient
minerals/energy or an active cooldown; cooldown seconds and per-Unit
energy appear in the existing native HUD. Select F7 to edit the
catalog and loadouts. Up to eight skills may be authored per Unit;
HUD pagination beyond three skills is not yet implemented.

### Eight-skill command-card paging

Unit loadouts can contain up to eight Ability IDs. For a single selected caster with more than three skills, the bottom-right command-grid button shows SKILLS page/count. Click it or press TAB to cycle the 3-slot Ability command pages (F/J/K remain the cast hotkeys on each page). This card page is UI-only state: it cannot change the fixed simulation or recorded replay hash. Producer training pages retain priority when a structure is selected.


## Status gameplay and editor workflow

In F7, select a Behavior to edit stack cap, armor/damage/speed
bonuses, duration refresh and dispellability as one grouped action.
Select an ApplyBehavior/RemoveBehavior Effect to choose its linked
Behavior. Select a Sequence Effect for the visual node tree;
click Edit on a node to inspect it, or use Up/Down/Remove/Append
on ordered child references.

In F9 with the Behavior Workshop map, select a Ranger.
F casts the self-targeted Brace Stance (temporary armor stacking),
J enters unit target mode for Tether Bolt (movement/attack debuff),
K enters unit target mode for Break Tether (dispel).
Left-click a valid visible target within range, right-click to
cancel. View the selected Unit's effective attack/armor plus
status count and remaining energy in the native HUD.
F6 trigger monitor shows real active status instances when
simulation is running; it does not replace F7 authoring.


## Ground-targeted Abilities in F7 and F9

F7: New catalog object > Ability | Ground area impact.
Select a Ground Ability and edit Area radius, Affected team,
and Impact delay (ticks) as one Apply ground settings action.
Changing target policy from Self to Ground sets a valid
nonzero range and default Ground parameters automatically.
Assign the Ability in a Unit loadout and save the project.

F9: select exactly one friendly caster and activate its Ground
skill with F/J/K or the command card. Hover over visible terrain:
teal ring = valid center/range; red ring = invalid. Left-click
submits a Ground CastAbility command; right-click cancels.
The orange radius circle remains while its authored impact
delay counts down. Units entering/leaving the area before
impact are affected/avoided accordingly. TAB pages to more
skills. Try Ember Arc (F) or allied Repair Circle (J)
in ground_impact_showcase.rtsproject.json.


## Projectile Forge controls

F7: create a Projectile Arc or Bolt entry, edit speed, arc,
size, contact radius, collision and RGB color, then Apply
projectile settings. Select a Ground Ability; its Projectile
delivery dropdown chooses an authored Projectile ID or None.
Use Edit projectile to jump to its catalog entry. Settings,
reference selection and target-kind conversion are undoable.

F9 in projects/projectile_forge_showcase.rtsproject.json:
select a Ranger, press F to target arcing Aether Arc, J for
the intercepting Pulse Bolt, or K for Repair Circle; click
visible ground within range to fire. Colored projectiles fly
through the 3D world with short trails and the orange target
ring persists. Collision-enabled bolts can detonate when
touching an eligible Unit before their destination. A teal
ring indicates valid ground aim, red indicates invalid aim;
RMB cancels. TAB cycles through additional Ability pages.

## Area Shape Laboratory — F7 and F9

Open projects/area_shapes_showcase.rtsproject.json.
In F7, select fan_surge or rail_line to edit Search shape,
Cone angle or Line width, Area radius, team and impact delay.
Press Apply ground settings to commit one undoable edit.
In F9, select a Ranger. F aims the Fan Surge cone, J aims
the Rail Line rectangle; K aims the inherited Repair Circle.
The colored reticle previews the active area search and
any delayed pending impacts stay visible as outlines.
Aim direction is from the caster toward the clicked point,
with the wedge or strip extending farther from that point.
RMB cancels a targeting action. F6 impact watch labels shapes.
