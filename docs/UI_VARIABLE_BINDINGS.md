# F6 ↔ F10 live variables, conditional frames and replay-safe dashboards

RTS Creator — November 16, 2026

## What changed

The F10 **Custom UI frames** editor now reads authoritative F6 map
variables. Previously, a custom Label or Bar could only show built-in
game values (minerals, supply, health, simulation ticks). Now it can
display any named, validated integer from **F6 → Map variables**, and
a parent or child frame can be hidden until a named variable reaches
an author-specified threshold.

This is a two-way integration: F10 Button → authoritative
\`UIFrameClick\` command → F6 \`OnUIFrameClicked\` ECA actions →
F6 \`AddVariable\` or \`SetVariable\` → F10 read-only live binding,
bar and conditional visibility. Neither the renderer nor F10
directly mutates the simulation.

## Authoring workflow

1. Open F6 Triggers and create the integer Map variable
   \`signal_count\`, initially 0.
2. Create a Trigger subscribed to \`OnUIFrameClicked\` and choose a
   custom F10 Button whose Click action is \`Trigger\`. Add an ECA
   \`AddVariable\` action referring to \`signal_count\` with value 1.
3. Open F10 Interface Editor → Custom UI frames and create a Label.
   Choose **Live data binding → Variable** and select the named
   \`signal_count\` in the **Bound F6 map variable** dropdown.
   Optionally enter a short label prefix such as \`BEACONS\`.
4. Create a Bar, choose **Progress value → Variable**, pick
   \`signal_count\`, set Progress minimum 0 and maximum 3.
   The fill is clamped to 0–100% visually; the underlying integer
   variable can keep increasing.
5. Select an acknowledgement Label (or a parent Panel). Choose
   **Visible when F6 variable reaches threshold → signal_count**
   and **Visible when variable ≥ 1**. The frame becomes visible
   only after the first accepted click. Children inherit parent
   visibility, so hidden panels cannot capture clicks.
6. Apply the frame changes (one undo entry), save the map and play
   with F9. The game displays actual live values and processes
   mouse clicks through the replay command stream.

These options are part of the project. User-only View → Appearance
font sizes, monitor DPI and contrast remain separate from HUD
geometry and trigger values.

## Schema

Optional additional properties on existing \`metadata.ui_frames\`
objects are:

| Field | Type | Purpose |
|---|---|---|
| \`binding\` | \`"Variable"\` | Get live integer from a named F6 Map variable |
| \`variable\` | string | F6 variable ID when \`binding\` is Variable |
| \`bar_min\` | integer | Bar visual minimum, default 0 |
| \`bar_max\` | integer | Bar visual maximum, default 100 |
| \`visible_variable\` | string | Optional F6 variable ID controlling visibility |
| \`visible_at_least\` | integer | Threshold for visibility, default 1 |

Example:

~~~json
{
  "metadata": {
    "ui_frames": [
      {
        "id": "signal_meter",
        "kind": "Bar",
        "anchor": "TopLeft",
        "text": "SIGNAL",
        "binding": "Variable",
        "variable": "signal_count",
        "bar_min": 0,
        "bar_max": 3,
        "x": 28, "y": 282,
        "width": 350, "height": 38,
        "consume_input": false
      },
      {
        "id": "signal_ack",
        "kind": "Label",
        "anchor": "TopLeft",
        "text": "UPLINK ACKNOWLEDGED",
        "binding": "None",
        "visible_variable": "signal_count",
        "visible_at_least": 1,
        "x": 28, "y": 373,
        "width": 420, "height": 32,
        "consume_input": false
      }
    ]
  },
  "trigger_variables": {
    "signal_count": 0
  }
}
~~~

Variable IDs use the existing F6 typed integer naming convention.
The project loader rejects nonintegers, nonfinite values, bad or
missing variable names, stale bindings and visibility references,
and bars whose maximum is not greater than their minimum. Bar
minimum/maximum and visibility thresholds must be within
−100,000,000 to 100,000,000. Labels and Buttons can show variables;
Bars can display \`Supply\`, \`SelectedHealth\`, or \`Variable\`.
Legacy frames without these new fields serialize unchanged.

The **F6 Rename variable** operation transactionally migrates typed
F10 \`variable\` and \`visible_variable\` references along with
recognized ECA nodes/operands. Removing a variable still in use by
any frame is rejected without modifying the project. Unrelated
free-form text is not rewritten.

## Renderer and input semantics

The headless \`RTSUIFrameLayout.hpp\` accepts an optional read-only
game context when resolving positions. It resolves visibility from
the current F6 variable state, inheriting the parent visibility
through the hierarchy. Both the F9 OpenGL renderer and the gameplay
mouse router use this same result; **hidden frames are neither drawn
nor clickable**. Empty or out-of-range values do not cause division
by zero: Bar fill is bounded via
\`clamp((value-min)/(max-min), 0, 1)\`.

The F10 visual designer distinguishes hidden frames using subdued
outlines and a HIDDEN marker. They remain selectable through the
hierarchy tree for authoring. F10 does not update trigger values
directly or bypass replay recording.

## Original working example

\`projects/ui_variable_dashboard_showcase.rtsproject.json\` extends the
original UI Trigger Studio with a named \`signal_count\` integer,
one \`AddVariable\` trigger action, a live counter, a Bar and a
conditional acknowledgement. Launch with:

~~~sh
./RTSCameraTerrain --project projects/ui_variable_dashboard_showcase.rtsproject.json
~~~

Press F10 to inspect Custom UI frames or F6 to inspect the event
graph. In F9 click SEND SIGNAL to increment, fill and reveal
the dashboard.

## Tests and scope

\`RTSUIVariableBindingTests\` exercises real F6→F10 click progression,
variable text/normalized Bar fill, nested visibility and hit masking,
strict schema and invalid variable references, F6 variable rename /
removal / Undo/Redo integrity, projection across four screen sizes,
real authoritative replay roundtrip and visual-only state-hash
isolation. Windows/Linux CTests run assertions in Release. Debug
CI emits and byte-diffs \`ui_variable_fingerprint.txt\` between
platforms.

Linux Xvfb/Mesa records real F10 secondary-native-window and F9
gameplay screenshots using the authored sample. Both native bundles
include the source-free executable, this manual and the new project,
and expose \`RTS_FEATURE_UI_VARIABLE_BINDINGS=1\`.

Still missing are generalized binding expressions, localized or
asset-backed text, client-local per-player UI state, arbitrary
trigger callbacks and a full Galaxy Editor-scale widget framework.
Those should remain explicit future milestones, not implied
complete features.
