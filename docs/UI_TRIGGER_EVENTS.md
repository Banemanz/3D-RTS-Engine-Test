# Replay-safe UI trigger events

RTS Creator, November 16, 2026.

## Editing a scriptable UI button

This pass connects F10 Custom UI frames to F6 visual Trigger Editor.
In F10, select or create a Button and set **Click action → Trigger**.
Scripted buttons always consume world clicks, preventing unintended
unit orders behind the UI. Apply and save the project.

In F6, create a Trigger and select **OnUIFrameClicked** as its event.
Select the ID of the intended script-enabled Button from the **frame**
dropdown. Attach the usual typed conditions and ECA actions, including
GrantMinerals, ShowMessage, SetVariable, Repeat or IfThenElse. The
ordinary Run only once option remains supported; otherwise each
click fires the rule. F6's trace monitor reports executions/blocks.

The original demonstration at
`projects/ui_trigger_showcase.rtsproject.json` provides an amber
**SEND SIGNAL** button, the event `OnUIFrameClicked` for
`event_button`, a +75 minerals GrantMinerals action, a
"BEACON SIGNAL RECEIVED" ShowMessage action and a status label bound
to the live Message variable.

## Real authoritative command and replay path

The F9 native mouse callback hit-tests with the same resolved
RTSUIFrameLayout geometry used for rendering, honoring topmost and
ancestor clipping. A Trigger button queues a typed, validated
`RTSCommandKind::UIFrameClick` at the *next* fixed simulation tick.
The command stores only its authored Button ID, never GUI pixel
coordinates, local monitor DPI, selected unit list or an executable
script. RTSCommandStream records/replays it through the same
deterministic input channel as existing player orders.

The game command interpreter validates that the ID actually belongs
to an input-consuming authored Button with action Trigger, then
delivers the event into TriggerEngine. The trigger runs through
the same bounded declarative condition/action interpreter and
produces real authoritative simulation state (minerals, variables,
messages, unit spawns, etc.). A full replay checks its resulting
world-state digest; no local-only rewards or unrecorded shortcuts.

**Strict constraints:** player source 0 only; no selected units,
world point or target, no queue append; 1–48-character ASCII frame ID,
letters/digits/underscore/hyphen only; and a valid script-enabled
Button present in the loaded project. AI source forgery and invalid
payloads are rejected. At most 16 UI events per tick are admitted,
preventing unbounded rapid-click trigger work. Multiple accepted
events fire in command order. Deferred OnVariableChanged notifications
remain next-tick, as before.

## Authored JSON

Button object within metadata.ui_frames:

~~~json
{
  "id": "event_button", "parent": "",
  "kind": "Button", "anchor": "TopLeft",
  "palette": "Amber", "text": "SEND SIGNAL",
  "binding": "None", "action": "Trigger",
  "x": 28, "y": 174,
  "width": 192, "height": 42,
  "consume_input": true
}
~~~

Rule object within the project's top-level triggers array:

~~~json
{
  "id": "on_beacon_signal",
  "event": {"type": "OnUIFrameClicked", "frame": "event_button"},
  "once": false,
  "actions": [
    {"type": "GrantMinerals", "amount": 75},
    {"type": "ShowMessage", "text": "BEACON SIGNAL RECEIVED"}
  ]
}
~~~

The trigger definition parser rejects missing IDs, non-button
references, buttons without Trigger action and non-consuming buttons.
Project validation checks cross-links when a project contains authored
UI frames and triggers. F10 Apply likewise refuses to delete or
repurpose an F6-referenced Button, leaving the original project
unchanged and undo history intact. Existing maps without authored
UI remain backwards-compatible.

## Native builds, tests and limits

RTSUITriggerTests checks the real bundled sample, parser references,
invalid changes with atomic rollback, F6 event editing, repeated
clicks, 16-per-tick admission, command schema forgery, fixed-step
execution, trace, 120-tick replay verification and serialized replay
roundtrip. It writes `ui_trigger_fingerprint.txt`, byte-compared
between Windows and Linux Debug CI; Release CTests run with asserts
enabled. Both native bundles include the manual/sample and require
the executable feature marker `RTS_FEATURE_UI_TRIGGER_EVENTS=1`.

Real Linux Mesa screenshots show the F10 script-enabled Button
inspector at 150% editor UI scaling and F9 gameplay after the
recorded input has executed. This is more than visual mockup.

Current limits: no arbitrary native scripting, custom imported UI
assets, mouse-to-world targeting from UI, multi-client player-scoped
events, or full Galaxy dialog API parity. The engine uses original
code/content and existing typed trigger actions, not copied Blizzard
or third-party source.

See [UI Frame Hierarchy](UI_FRAME_HIERARCHY.md) and
[Interface Editor](INTERFACE_EDITOR.md).


## High-DPI F10 inspector screenshot QA

Real Linux OpenGL F10 screenshot at 150% creator UI scaling
confirmed the selected script Button rendered correctly but its
properties were pushed below a tall hierarchy list. The native
F10 right inspector now reserves space for a compact, vertically
scrollable hierarchy (about 28% of the available panel), scrolls
the selected item into view and displays the selected Button's
ID/type/Trigger event status above the inspector. The main
**Click action** selector is now the first editable property,
rather than below offsets, dimensions and bindings. Apply/Discard/
Clear remain pinned at the top. This avoids tiny controls and
excessive nested scrolling at 150-250% DPI scaling.

Real F9 screenshot showed the intended mineral reward and
"BEACON SIGNAL RECEIVED" event message but truncated the linked
left-hand status Label. The original sample's status display is
widened to show the complete message without compromising the
native resource strip or RTS command grid.
