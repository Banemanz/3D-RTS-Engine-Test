# Custom UI Frames — original nested RTS interface system

RTS Creator — November 16, 2026

## What this milestone adds

The **F10 Interface Editor → Custom UI frames** tab now creates and edits
screen-anchored, parent/child UI content beyond the five standard RTS HUD
panels. Up to **32 authored frames** can be part of a project. They appear
in the actual playable game, not just a designer-only preview.

Four frame types are currently implemented:

| Type | Presentation | Interaction |
|---|---|---|
| Panel | Blue-toned container, optional caption, border | Can block world clicks; may parent nested frames |
| Label | Text with optional live game binding | Non-interactive |
| Button | Colored rectangle, text, optional binding | Issues one allowed existing RTS action on left click |
| Bar | Colored health/supply fill with live binding | Non-interactive |

A frame is anchored to either the screen or an **earlier** Panel by ID,
using TopLeft / TopRight / BottomLeft / BottomRight / Center. Position
offsets are measured in logical game HUD pixels. Children inherit their
parent's coordinate space, are displayed after the parent, and **clip
both drawing and mouse interactions to all ancestors**. The hierarchy
has a bounded depth of six.

These concepts are informed by public Galaxy dialog/frame/anchor
documentation, but all code, naming, palettes, assets and gameplay
behavior are original. No proprietary XML layouts or Blizzard unit
names are included.

## Using the editor

1. Start `RTSCameraTerrain --interface-editor --project projects/ui_frame_studio_showcase.rtsproject.json` or press **F10**.
2. Choose **Custom UI frames**. The left pane shows the game-space frame
   positions at 800×600, 1280×720 or 1920×1080. The right pane contains
   the actual hierarchy tree and properties.
3. Add a Panel, Label, Button or Bar. Select a Panel first to create
   a child (or change its Parent dropdown later). Parents must appear
   earlier in the hierarchy, preventing cyclic dependencies.
4. Click frames in the preview, drag to reposition, or edit numeric
   X/Y offsets and Width/Height. Anchor, parent, palette, text and
   optional live-value binding are editable. Buttons have a separate
   **Click action** field.
5. Click **Apply frames** to record one validated undo step affecting
   the whole hierarchy; **Discard changes** restores the saved version.
   Deleting a parent also deletes all descendants. **Clear custom UI**
   prepares a removal; Apply is required to actually remove it.
6. Save, switch to F9, and use the real widgets. The native HUD
   renderer and mouse router share the same resolved geometry.
   Existing gameplay command handlers are reused; no invented
   direct simulation shortcuts or local scripts.

The complete native-authoring preview is scaled to fit the F10 module;
the underlying authored values remain **game HUD pixel coordinates**,
independent of personal editor View → Appearance settings.

## Authored schema

The project metadata contains an optional `ui_frames` array:

~~~json
{
  "metadata": {
    "ui_frames": [
      {
        "id": "mission_console",
        "parent": "",
        "kind": "Panel",
        "anchor": "TopRight",
        "palette": "Navy",
        "text": "MISSION CONSOLE",
        "binding": "None",
        "action": "None",
        "x": -25,
        "y": 110,
        "width": 304,
        "height": 238,
        "consume_input": true
      },
      {
        "id": "select_army",
        "parent": "mission_console",
        "kind": "Button",
        "anchor": "TopLeft",
        "palette": "Cobalt",
        "text": "ARMY",
        "binding": "None",
        "action": "SelectArmy",
        "x": 15,
        "y": 154,
        "width": 124,
        "height": 42,
        "consume_input": true
      }
    ]
  }
}
~~~

**Bounds and validity:** max 32 frames, unique ASCII IDs (1–48 chars,
letters, digits, hyphen/underscore), text up to 128 printable ASCII
characters, depth at most six, X/Y −2048..2048, width 24..650,
height 20..400. Parents must reference earlier Panel frames and cannot
be self/cyclic. Project loading and every Apply validate all entries;
malformed project data is rejected rather than silently ignored.

Supported palettes: Navy, Teal, Cobalt, Amber, Slate, retaining the
original blue/teal editor and game visual language.

Live text binding choices:
`None`, `Minerals`, `Supply`, `Workers`, `Selected`,
`SelectedHealth`, `Message`, `Ticks`.
The author's text is used as an optional prefix, followed by the
resolved current live value. Bars support `Supply` and
`SelectedHealth` only and use a clamped 0..1 fill fraction.
All values come from the current RTS skirmish, not stale editor mocks.

Button actions: `None`, `SelectArmy`, `SelectWorkers`,
`FocusBase`. Only Button frames can bind an action. The input
router selects the topmost clickable frame, blocks the gameplay
world click, and calls the existing HUD action dispatcher on left press.
A parent Panel can optionally consume clicks in its empty background,
while Label and Bar elements normally pass through.

## Engine implementation and regression suite

- `RTSUIFrames.hpp`: validated, headless typed project schema,
  bounded JSON import/export and hierarchy constraints.
- `RTSUIFrameLayout.hpp`: pure anchor math, resolved rectangles,
  nested clipping, topmost hit-testing and live text/progress bindings.
- `RTSHUD.hpp`: actual native OpenGL overlay rendering, colors,
  borders, text and progress bars; no external textures.
- `main_entry_point_render_loop.cpp`: shared hit model/allowed
  command routing above the classic HUD, noninteractive passthrough.
- `EditorSession::setUIFrames`: one atomic validated and undoable
  authoring transaction; no UI changes are recorded as simulation
  commands or arbitrary script execution.
- `CreatorUIFrameEditor.hpp`: real native F10 design tab,
  preview, visual selection/repositioning, hierarchy management,
  text bindings, palette and action inputs.
- `RTSUIFrameTests`: legacy-map opt out, roundtrip, nested clipping,
  multi-resolution anchors, live bindings, command admission, invalid
  schemas and real EditorSession Apply/Undo/Redo/reset.

The original project fixture
`projects/ui_frame_studio_showcase.rtsproject.json` contains a nested
mission console, live minerals/supply, SelectedHealth bar, two
clickable actions, a nested status footer and a base-focus button.

Windows/Linux native Release CI packages and validates the fixture,
manual and compiled feature marker
`RTS_FEATURE_UI_FRAME_HIERARCHY=1`. Linux Mesa/Xvfb captures the
**actual F10 native OS window** (custom tab selected at 125% UI scaling)
and the **actual F9 game rendering** with selected units and nested
UI, rejecting blank secondary-window captures. Existing strict
cross-OS replay parity is unchanged.

## Limits and sensible next steps

This is a functional **first hierarchical frame system**, but is not
the full Galaxy Editor XML/dialog widget engine. It does not yet
provide user-authored font assets, images, text localization, scroll
lists, independent focus navigation, drag-resize handles, arbitrary
trigger callbacks, textures/skins or event scripts. Custom UI button
actions are deliberately restricted to supported existing game
commands; allowing arbitrary execution would need a formally typed,
replay-safe trigger event layer and permission model.

Nested children are clipped to their parent rectangles; the current
renderer still uses the original bitmap game font (printable ASCII)
and pixel coordinates. Personal editor DPI scaling is fully separate.
The next work should add typed UI-click trigger events and eventually
an asset-backed skin/style pipeline, **without borrowing Blizzard
content or GPL engine source**.

## Conceptual references (not code dependencies)

- [Galaxy native dialog and anchor API](https://mapster.talv.space/galaxy/reference)
- [Galaxy dialogs tutorial](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/043_Dialogs/)
- [Galaxy UI events overview](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/049_UI_Events/)
- [Dear ImGui docking 1.91.9b API](https://github.com/ocornut/imgui)
