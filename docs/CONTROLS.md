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
