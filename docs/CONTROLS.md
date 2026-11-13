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

**C** queues a Worker at the Command Center (40 minerals); **T** queues a Marine
at the first complete Barracks (50 minerals). Production requires available
supply and is sequential per structure. **X** stops selected mobile units.
**Ctrl + 1-9** saves currently selected friendly unit IDs into a control group;
press the corresponding **1-9** key to recall the surviving members.
**R** restarts the skirmish (and clears control groups); **Esc** exits.

This skirmish is a prototype: no construction preview, minimap, fog, control
groups, production selection UI or building placement feedback beyond console.
