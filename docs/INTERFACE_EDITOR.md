# Interface Editor: authorable native RTS game HUD

RTS Creator — November 16, 2026

## Purpose and architecture

The new **F10 Interface Editor** is a distinct, titled desktop-native creator
module, alongside F5 Terrain, F6 Trigger, F7 Data and F8 Scene. It edits **the
actual game's graphical interface geometry**, not the editor's personal
appearance settings and not merely a screenshot. The OpenGL HUD renderer and
game mouse routing both consume the same `RTSHUDLayout` calculations.

The F10 canvas is a schematic interactive *layout projection* of the bottom
strip, tactical minimap, command grid, selection dossier and resource bar.
It uses the original blue/teal aesthetic, not borrowed Blizzard assets.
The preview can switch among 1280×720, 1920×1080 and 800×600, and all
rectangles come from the **same layout class** used by the actual game.

## Basic authoring workflow

Open **Interface Editor** with F10 or Workspace → Interface, or launch
`RTSCameraTerrain --interface-editor --project ...`.

1. Click a panel in the preview or choose it in the Elements list.
2. Drag the minimap left/right, command grid horizontally, bottom strip
   vertically or resource bar up/down. Alternatively use accessible sliders.
3. Choose compact, desktop or 1080p preview to check multiple resolutions.
4. **Apply layout** makes one validated undoable project change.
   **Discard preview** restores saved values without an undo entry.
   **Reset to classic HUD** prepares the original default geometry;
   apply it to remove the custom metadata entirely.
5. Save (Ctrl+S), switch to Play (F9) and observe the real engine renderer,
   minimap navigation and matching command hit regions.
   Undo/Redo uses the same EditorSession history as all other modules.

Editor View → Appearance → 175% increases editor font/control sizes but
**does not change a player's authored game interface**.

## Project JSON

The optional object lives in the project's existing `metadata`. Old projects
without `hud_profile` retain their legacy HUD defaults.

~~~json
{
  "metadata": {
    "hud_profile": {
      "bottom_extra": 34,
      "minimap_scale": 1.18,
      "minimap_inset": 18,
      "command_width": 360,
      "command_inset": 20,
      "resource_top": 25,
      "resource_height": 52
    }
  }
}
~~~

| Field | Range | Default | Meaning |
|---|---:|---:|---|
| `bottom_extra` | −40 to 65 px | 0 | Added to adaptive 194/160 px bottom strip |
| `minimap_scale` | 0.75 to 1.30 | 1.0 | Size multiplier, clamped for neighboring panels |
| `minimap_inset` | 8 to 48 px | 14 | Inset from left |
| `command_width` | 260 to 390 px | 330 | Command button grid width |
| `command_inset` | 8 to 48 px | 14 | Inset from right |
| `resource_top` | 20 to 70 px | 22 | Resource bar y offset |
| `resource_height` | 36 to 72 px | 46 | Resource bar height |

The bounded settings preserve a safe relationship between the fixed
12-button grid, selection cards and minimap. The selection area is an
automatic fill region, not an independent overlapping draggable panel.
This is the **first parameterized gameplay HUD editor**, not a complete
freeform frame-tree UI scripting system.

Malformed types, nonfinite values and out-of-range settings fail
`RTSProject::validate()` and the bundled project tool. The editor snapshots
once, applies one validated transaction and restores everything on error.
Reset removes `hud_profile` rather than leaving redundant default data.
No HUD profile setting changes combat, economy, world simulation ticks
or replay commands.

## Genuine engine and editor integration

`RTSHUDProfile.hpp` is the headless project model. `RTSHUDLayout.hpp` computes
pixel rectangles shared by the actual input routing and native OpenGL
HUD renderer in `RTSHUD.hpp`. `CreatorInterfaceEditor.hpp` previews and
edits the exact same computed geometry without creating fake gameplay.

The showcase is `projects/interface_studio_showcase.rtsproject.json`.
It ships on Windows and Linux alongside this manual; the compiled
executable reports `RTS_FEATURE_GAMEPLAY_INTERFACE_EDITOR=1`.
No extra native library or proprietary asset was introduced.

### Verification

`RTSInterfaceEditorTests` validates legacy default rectangles, authored
changes at 640×480 / 800×600 / 1280×720 / 1920×1080, project roundtrip,
hit region alignment, invalid metadata, EditorSession Apply/Undo/Redo
and default reset. Assertions are enabled in Windows/Linux Release CI.

Linux Mesa software-OpenGL CI captures two **actual** screenshots:
- F10 in a native **secondary GLFW/OS window** at 125% editor UI
  scale; the platform-window framebuffer must not be blank.
- F9 gameplay with the authored HUD, proving the engine, rather than
  an editor-only visualization, consumes the profile.

## Current limitations and next steps

This is not yet complete Galaxy-style UI Editor parity: future work
includes hierarchical custom widgets/anchors, skins and fonts per
map, new frame types, script- or trigger-connected data bindings,
custom mouse focus traversal, keyboard nudging and drag-resize handles,
per-user HUD preferences, and accessibility beyond the authoring
workbench. See [Creator Windows & Accessibility](CREATOR_WINDOWS_ACCESSIBILITY.md).
