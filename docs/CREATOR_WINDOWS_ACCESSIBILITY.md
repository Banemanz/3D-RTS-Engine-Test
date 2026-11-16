# Creator module windows and accessible interface scaling

RTS Creator — November 16, 2026. Original cross-platform C++17 editor.

## Why this exists

The previous prototype drew one \`Creator | Browser\` and one
\`Creator | Inspector\` directly over the 3D world, then switched their
contents when the user pressed F5/F6/F7/F8. This kept the engine and
editor coupled visually, even though catalog, scene, terrain and
trigger authoring already lived in independent headless C++ layers.

Public Galaxy Editor screenshots show separate module workspaces
rather than a single giant property sheet. This pass adopts that
**organization**, while keeping the existing blue/teal RTS Creator
theme and avoiding copyrighted assets, code and unit names.

## Distinct editor module workspaces

Each authoring workspace now has its own titled, resizable editor window:

| Shortcut | Module | Existing functionality retained |
|---|---|---|
| F5 | Terrain Editor | Height painting, path blocking, placements |
| F6 | Trigger Editor | Visual ECA/variables, conditional expressions |
| F7 | Data Editor | Catalog types, advanced property inspectors, research graph |
| F8 | Scene Editor | Stable scene entity selection and transform editing |
| F9 | Playtest | Renderer, native command grid, replay and simulation |

The modules **share the same document, editor history and validation**.
Each module has a persistent window identity; switching modules does not
reuse the previous module's saved geometry. Each window has an in-window
Save / Undo / Redo / Back to Playtest toolbar. The Navigator and
Properties areas are independent bordered panels separated by a
**user-resizable splitter**. The large editor window can be moved or
resized without changing gameplay resolution.

On normal Windows/Linux desktop runs, Dear ImGui docking **multi-viewports
are enabled** and each selected module uses the pinned backend's per-window
`NoAutoMerge` flag: it opens as a **real native GLFW/OS window even while
overlapping the game window**. The module can be moved to another
monitor, minimized or resized independently. Its desktop decoration/
taskbar visibility is preserved. Its module selector also switches
directly among the different editors. The original terrain/game viewport remains
in the original native window. Additional technology, relationship,
opponent AI and trigger monitor panels remain independently floating
or dockable.

At present one primary editing module is active at a time (as it was
before). Modules are **separate named GUI workspaces / native
windows**, not four simultaneously active independent project
controllers. We are not yet claiming complete StarCraft II UI Editor,
cutscene editor or importer feature parity. The View → Appearance
menu contains a **Separate editor modules** switch to return to the
older two-floating-window view if preferred.

## Readability / DPI / contrast

**View → Appearance** in the top menu bar offers:
- **UI scale 85%–250%** with quick presets at 100%, 125%, 150%,
  175%, 200% and 250%. The default is a more legible 115%.
- **Follow OS / monitor DPI** enabled by default. On supported
  GLFW platforms, monitor scaling is read from
  \`glfwGetWindowContentScale()\`; a focused detached editor window
  uses the content scale of the monitor it is on. The manual scale
  is multiplied by the OS scale (clamped to 400% effective for safety).
- **Enhanced text contrast**, retaining the blue-on-blue palette
  but providing a darker background, brighter text, stronger borders
  and clearer muted labels.
- **Separate editor modules**, enabled by default.
- **Reset editor appearance** to predictable settings.

Font atlases are reconstructed at the requested pixel size when the
scale changes, and padding, slider handles, scrollbars, borders and
other ImGui style sizes are reconstructed from an immutable base style
each time. Repeated slider adjustments do **not** compound and distort
the layout. When available, the engine uses an installed OS sans-serif
font (Segoe UI, DejaVu Sans or Liberation Sans); if absent, it uses
ImGui's built-in fallback. No font files are bundled or redistributed.
This gives larger, crisp text rather than simply enlarging a
13-pixel font texture. The native game HUD is a separate in-house
rendering path and is **not** silently rescaled by editor settings.

A note about multi-monitor DPI: the current 1.91.9b docking backend
uses a shared ImGui font atlas for this context. The creator tracks
the **focused platform window's** monitor and applies that scale.
This is a practical first step, not fully independent per-window
font atlases for several simultaneously visible windows on monitors
with different pixel density.

## Preferences are private to the user, not the map

The validated \`CreatorUISettings\` JSON file is stored in a
user-writable configuration folder:

- Windows: \`%LOCALAPPDATA%/RTSCreator/ui_settings.json\`
- Linux: \`$XDG_CONFIG_HOME/rts-creator/ui_settings.json\`, or
  \`~/.config/rts-creator/ui_settings.json\`

Invalid JSON, wrong types, unsupported versions, impossible scales
and oversized files fall back to defaults without crashing the editor.
Appearance changes use a temporary file before replacing the
persistent configuration. Map JSON, deterministic simulation state,
saved replays and editor catalog field history do **not** embed your
visual accessibility settings.

## Engineering and native tests

\`CreatorUISettings.hpp\` is a headless, C++17-stdlib settings and
validation layer. \`CreatorUIRuntime.hpp\` owns DPI selection, the
fresh font atlas and blue/teal ImGui style, sharing the official
GLFW/OpenGL3 docking backends on Windows and Linux.
\`CreatorWorkbench.hpp\` owns navigation, resizable module windows
and existing browser/inspector adapter logic; all actual game
editing still uses EditorSession transactions.

\`creator_ui_settings\` CTest covers scale boundaries, effective DPI
clamping, contrast and separate-module flags, JSON round-trip,
user-preference overwrite, unsupported version handling and
malformed/oversized startup recovery. CI also runs actual OpenGL
multi-viewport startup using \`--smoke-module-windows\` at 150% UI
scale on an extended Xvfb desktop, requires at least **two real
GLFW platform windows**, and captures the secondary module's own
OpenGL framebuffer. A separate single-viewport screenshot checks
large-font 175% layout. Both Windows and Linux compile the same
GUI code, with optimized CTests and release bundle verification.

## Reference material (conceptual only)

- [Galaxy Editor module navigation](https://s2editor-guides.readthedocs.io/New_Tutorials/01_Introduction/003_Navigating_the_Interface/)
- [Galaxy terrain module](https://s2editor-guides.readthedocs.io/New_Tutorials/02_Terrain_Editor/019_Terrain_Editor_Interface/)
- [Galaxy trigger module](https://s2editor-guides.readthedocs.io/New_Tutorials/03_Trigger_Editor/033_Trigger_Editor_Interface/)
- [Dear ImGui DPI FAQ](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md)
- [GLFW window DPI and monitor scaling](https://www.glfw.org/docs/latest/window.html)

## Current limits / next steps

- Add a genuinely authored in-game UI layout/style Editor, alongside
  the existing native command grid, once HUD skins and anchor models
  have a schema and replay-safe pipeline. The appearance panel here
  configures the **creator's own GUI**, not the game's authored UI.
- Add detachable simultaneously active independent module controllers
  and document-specific docking trees.
- Add a richer full-screen diagram and high-contrast focus navigation,
  more granular text sizing, keyboard-first workflow and colorblind
  safe palette options.
- Continue game engine, multiplayer-safe economy, asset import,
  animations, and rendering/editor parity on the **same draft PR**.


### Large-type layout ergonomics (post-screenshot review)

The Data relationship navigator is now closed by default in the new
module layout; it remains available from **View → Data relationship
navigator**. This prevents a third floating pane from covering large
property controls. The module fills most of the available primary
window on first use. Navigator/Properties columns keep legible
minimum widths and show a **horizontal scrollbar** when 175–250% text
sizing exceeds one monitor. Top title and action buttons are on
separate rows instead of risking clipped controls at high DPI.
User-sized/docked windows still override first-use defaults.


### Navigator space at high UI scales

At 150–250% zoom, creation presets now live under the discoverable
**Create / derive objects** foldout so the Object Explorer gets the
available height. When a link, technology node or inspector action
selects a different catalog entry, the list scrolls to that object
exactly once; ordinary manual browsing does not repeatedly jump back.
This is an accessibility/layout improvement on top of the resizable
Navigator/Properties splitter.


### Keyboard commands in detached editor windows

The primary game GLFW callback handles shortcut keys when the game is
focused. Detached native editor windows receive keyboard input via
ImGui's own GLFW platform backend instead. The active module therefore
also accepts **F5 Terrain, F6 Triggers, F7 Data, F8 Scene, F9 Play,
Ctrl+S Save, Ctrl+Z Undo and Ctrl+Y Redo** when its OS window is
focused. Text editing keeps its own Ctrl+Z/Y history. Commands share the
same project editor controller, not a second copy of the map.


### Desktop-height window sizing

Detached modules now size their initial height against the **monitor
work area** (approximately 85% of the available desktop), rather than
inheriting the 720px game window height. The first-use height is clamped
to the usable monitor area; user-resized windows still retain their
saved sizes. This gives the large-text F7 object explorer much more
room on 1080p and taller displays without sacrificing the independent
world viewport.


## F10 Interface Editor versus personal GUI accessibility

F10 is now a **new native creator module** for authoring the playable
game HUD itself. Its layout canvas supports compact/desktop/1080p
resolutions and applies profile edits to project JSON only when the
author presses Apply; Undo and Redo use the unified project history.
Separately, View → Appearance remains per-user for accessibility:
175% zoom and higher text contrast do NOT alter the game HUD or
the layout delivered to players. See [Interface Editor](INTERFACE_EDITOR.md).

F7's property browser has also been redesigned as a resizable,
horizontally scrollable Scope/Field/Value table with per-field filtering,
inherited/local tint distinction and wrapped long values. This prevents
a single `[INHERITED] long_field = long JSON` label from disappearing
at larger DPI/text settings.


## Custom UI frame tree and large-text controls

The F10 workspace now includes a separate Custom UI frames tab with
a model-backed visual hierarchy, explicitly labeled high-contrast
properties and resolution previews. UI designer fonts/controls still
use View → Appearance scaling (85–250% plus active monitor DPI),
separate from the game-space frame offsets and dimensions being
authored. Frame properties include exact numeric offsets, parent
reference, anchoring, palette, text/binding and click action; the
hierarchy remains scrollable at large text sizes. No new bundled fonts
or platform-specific UI branch. See docs/UI_FRAME_HIERARCHY.md.
