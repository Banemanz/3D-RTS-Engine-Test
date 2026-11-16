# Responsive UI Frame Layout — Rows, Columns and Live Reflow

RTS Creator, November 16, 2026. Original desktop-native F10 authoring tools,
C++17/OpenGL engine, Linux/Windows parity.

## Capability

The **Custom UI frames** tab of F10 now supports automatic child layout for
author-defined Panels. Previously all frames used independent TopLeft,
TopRight, BottomLeft, BottomRight or Center anchors, which made even simple
vertical menu stacks laborious to build. These existing **Free** layouts
remain the default and their world coordinates do not change.

A parent **Panel** may now set **Arrange direct children**:

- **Free** — existing independent anchor and X/Y offsets, unchanged.
- **StackColumn** — children placed top to bottom in stable authoring order.
- **StackRow** — children placed left to right in stable authoring order.

Each flow parent has adjustable **Inner padding** (0..80 logical game pixels)
and **Gap between visible children** (0..64 pixels). Frame order is the
original topological sequence in the project, not a hash-map order. Every
parent has a separate stack cursor, supporting nested Rows inside Columns
and vice versa up to the existing six-level hierarchy limit.

A direct child may use **Stretch across parent**. In a Column this makes the
child's width the parent's inner width; in a Row it makes the child's height
the parent's inner height. Primary-axis width/height remains authorable;
the child can still nudge its position with X/Y offsets. A flow parent's
placement defines the child origin, so the child's legacy five-position
anchor is retained for later Free mode but ignored during flow. Only
direct children use that parent's cursor; nested grandchildren use their
own parent. Existing F6 conditional visibility is respected:
**invisible siblings consume neither their own size nor a gap**, so later
children automatically move upward/leftward when F6 variables hide frames.

The exact same headless \`rtsResolveUIFrames\` function drives F10's
canvas, F9's native OpenGL draw, and world-input interception.
Parent/viewport clipping applies to both drawn content and clickable
areas, including stretched and dynamically reflowed buttons. No frame
can accept a mouse click outside its clipped visible area.

## Authoring workflow

1. Open F10 → **Custom UI frames**. Select an existing **Panel** or create
   a Panel and select it in the hierarchy.
2. Expand **Responsive child layout**. Choose StackColumn or StackRow,
   edit padding and gap, and select **Reset child nudges** if imported
   Free-layout children have large X/Y offsets.
3. Select individual children. When their parent flows, use
   **Stretch across parent**, primary-axis dimensions, and optional
   X/Y nudges. Anchor buttons are disabled because the parent owns
   automatic placement; switching back to Free restores anchor behavior.
4. Check compact (800×600), desktop (1280×720), and 1080p
   (1920×1080) previews. The inspector shows the **actual resolved
   rectangle** and a clipping warning when appropriate. F10's original
   4/8/16/32px snapping, drag handles, grid, alignment controls,
   per-user 85–250% DPI scaling and native OS windows remain available.
5. Use **Apply** to commit the complete frame hierarchy as one
   validated undoable transaction. Preview-only edits cannot alter
   F9 gameplay before Apply. Undo/Redo and Discard still work.

The original reference sample
\`projects/ui_responsive_layout_showcase.rtsproject.json\` demonstrates
a column-based signal control panel and a row-based army command strip,
with F6 variables driving a visible acknowledgement. SEND SIGNAL issues
the existing replay-safe UIFrameClick command and changes
\`signal_count\`. The **UPLINK ACKNOWLEDGED** label appears, following
items move by the new label's height plus gap, and the same new hit boxes
are used by the playable game. No UI script bypass or unrecorded
simulation changes are used.

## Schema extension

Optional keys apply to existing \`metadata.ui_frames\` objects.
All prior JSON remains valid and behaves identically.

\`\`\`json
{
  "id": "signal_panel",
  "kind": "Panel",
  "parent": "",
  "anchor": "TopRight",
  "x": -25,
  "y": 105,
  "width": 318,
  "height": 345,
  "layout": "StackColumn",
  "layout_padding": 12,
  "layout_gap": 7,
  "consume_input": true
}
\`\`\`

A child may add \`"stretch_cross_axis": true\` when its parent uses
StackColumn or StackRow. The full serialized frame also includes
its previous standard \`kind\`, \`palette\`, \`text\`, \`action\`,
\`binding\` and other fields. Flow-specific keys are only written when
needed; default layouts do not gain extra metadata.

Validation enforces: known layout enum; only Panels own a non-Free
layout; finite padding 0–80 and gap 0–64; stretch only under an
existing flowing Panel; existing parent-first ordering, max 32 frames,
six nesting levels, text/action restrictions, bounded geometry,
typed F6 variables and F6 trigger references. Invalid projects are
rejected at load or Apply, never partially accepted.

## Determinism, rendering and test coverage

\`RTSUIFrames.hpp\` defines/validates/serializes author fields.
\`RTSUIFrameLayout.hpp\` resolves per-panel stable cursors, stretching,
F6 visibility and ancestor clips. \`CreatorUIFrameEditor.hpp\`
exposes F10 flow controls, preview, readouts and normalization.

\`RTSUIResponsiveTests\` loads the original responsive sample and
checks concrete positions across 640×480, 800×600, 1280×720,
1920×1080; column/row ordering, configurable gap/padding, cross-axis
stretching, hidden-child collapse after a real F6-triggered click,
nested viewport clipping and topmost input safety; malformed field
types, illegal layout modes, invalid bounds, no-parent stretch;
roundtrip, EditorSession Apply/Undo/Redo, and replay verification.

It also compares **120 deterministic 60Hz simulation ticks** using
identical F6 triggers and button IDs but different Free vs flow
presentation geometry. The authoritative state digest must remain
identical. The resulting \`ui_responsive_fingerprint.txt\` is compared
byte-for-byte between Windows and Linux in existing CI. Both operating
systems run assertions in Release and bundle/validate the sample,
manual and compiled \`RTS_FEATURE_UI_RESPONSIVE_LAYOUT=1\` marker.

Real Linux software-OpenGL CI captures:
- F10 Custom UI frames in an actual native GLFW/OS window at 150%
  creator UI scaling, with the StackColumn parent selected.
- F9 gameplay after an actual replay-queued SEND SIGNAL click, showing
  the responsive signal column and row action strip. This is an
  engine screenshot, not an editor-only rendering.

## Limits and next steps

Rows/Columns currently use one-dimensional stacks, not wrapping,
flex-grow distribution, per-child alignment constraints, grid,
scroll containers, text-measured autosize, translated fonts or custom
texture assets. Hidden frames collapse their space while fully
clipped children may still occupy a slot (author can resize/rearrange
the parent). Stack layouts use pixel units independent of creator DPI
settings; they do not yet respond to the game's own text accessibility
preferences. Future work can add cross-axis alignment, actual text
measurement, min/max constraints, zoom/scalable native bitmap font,
more widget types, keyboard focus and usable asset spritesheets
without taking proprietary Blizzard code or interface artwork.


## Screenshot validation and final signal-label polish

The first exact-feature Linux native F10 capture displayed the selected StackColumn Panel, the separate StackRow command strip, and visible layout/resize controls at **150% editor UI scale**. The actual F9 screenshot after a recorded F6-triggered SEND SIGNAL click displayed the ACK label in the stack and live BEACON CHARGE/BEACONS values, while the rest of the RTS HUD and world remained functional. QA found that the decorative prefix `STATUS` unnecessarily shortened the dynamically supplied message in the sample. The fixture now leaves the Message-binding prefix empty so it can display the complete `BEACON SIGNAL RECEIVED` text. A regression assertion verifies the actual message binding, not a hardcoded visual placeholder. The real screenshots are captured as artifacts on the latest commit.
