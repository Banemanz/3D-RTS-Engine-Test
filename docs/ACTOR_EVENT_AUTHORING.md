# RTS Actor Event authoring — November 15, 2026

## Design goal and boundaries

Actor Events are presentation ONLY. An event can create an ephemeral
visual cue but cannot modify targeting, damage, collision, pathfinding,
command streams, triggers, unit stats, replay hashes or the simulation
clock. A headless process can compile and produce cue snapshots without
OpenGL; rendering is the responsibility of the native display layer.

This is an independent intermediate foundation based on the idea of
actor/presentation separation discussed in public Galaxy Editor docs.
It is NOT a complete Galaxy Actor event, animation or sound engine.
There is no imported proprietary or GPL implementation or asset.

## F7 authoring workflow

1. Select an Actor in the Data Editor (F7) or create one with
   New catalog object > Actor | Event-driven visual.
2. Open Actor event rules | Presentation only.
3. Add up to 12 entries. Every entry configures the event, procedural
   style, color, size, and duration in fixed 60Hz simulation ticks.
4. Apply actor events commits the entire array as ONE validated,
   undoable project transaction. Revert restores the saved draft.
5. Place/select a Unit with that Actor. Per-instance Actor overrides
   and inherited Actor fields are supported. F9 playtest shows
   procedural cues; F6 shows the recent live Actor event watch.

An Actor catalog object's fields may include this JSON fragment
(no actual game projects or byte-exact replay files are changed):

    "events": [
      {
        "when": "Attack",
        "style": "Pulse",
        "tint": [0.2, 0.85, 1.0],
        "radius": 1.5,
        "duration_ticks": 70
      },
      {
        "when": "Impact",
        "style": "Ring",
        "tint": [1.0, 0.65, 0.22],
        "radius": 2.25,
        "duration_ticks": 46
      }
    ]

Multiple rules can respond to the same event. Children inherit the
parent Actor's event array, and may replace it with a local array
or an empty array. Unknown extension fields still roundtrip untouched.

## Supported event signals

| Event | Source |
| --- | --- |
| Attack | A Unit issues any normal attack |
| Launch | Weapon or Ground Ability creates an authored projectile |
| Impact | Instant hit, Ability effect, projectile arrival or miss |
| Death | The simulation first discovers a newly dead Unit |
| Cast | A Unit successfully casts an Ability |
| Status | A Behavior is applied or refreshed on a Unit |

All events are resolved from the actual fixed tick, including
projectile first-contact interceptions. An Impact cue on a missed
projectile is visual feedback at its endpoint, not damage.
Events on placed Units are a spawn-time snapshot of the effective
Actor and its placement-level actor override.

## Visual effects and validity rules

Pulse is an expanding/fading ground disc. Ring is a fixed-size
fading circle-like ground mark. Beacon combines inner and outer
glowing markers. The renderer shows cue effects only on currently
visible terrain to avoid leaking unseen enemy activity.

Event and style names are strictly case-sensitive. Tint must be
three finite RGB values each from 0 to 1. Radius must be 0.25 to 12
world units. Duration must be an integer from 1 to 180 ticks.
There can be at most 12 rules per Actor. Presentation retains at
most 256 active cue snapshots, evicting the oldest when saturated.
No event data enters authoritative replay hashes. The renderer
alone advances animations between frames.

## Sources of truth

- 3D RTS (v0.3.10)/RTSActorCue.hpp: typed cue event and style names
- DataCatalog.cpp: strict inherited schema compiler
- Unit.hpp and RTSSkirmish.cpp: spawn-time snapshots and fixed-tick
  event producer with bounded ephemeral cue projection
- EditorSession.cpp: atomic, validated undo/redo
- CreatorWorkbench.hpp: specialized F7 editor
- CreatorTriggerMonitor.hpp: live F6 diagnostics
- main_entry_point_render_loop.cpp: procedural native GL display
- projects/actor_events_showcase.rtsproject.json: original sample map

## Automated verification

The actor_event_runtime regression tests live attack and projectile
launch/impact, death and Ability Cast/Impact cues, event lifetime,
strict invalid schema rejection, placement overrides, and replay
roundtrips. It additionally compares TWO otherwise identical matches
one with cues and one without on every tick: all state digests must
match. actor_event_editor tests the F7 preset, grouped mutations,
invalid rollback, inheritance, serialization and undo/redo.
All 75 Windows/Linux Release CTests passed at the functional
validation milestone fef3a687, as did native bundle checks.
The CI workflow writes actor_cues_fingerprint.txt and compares its
bytes across Windows/MSVC and Linux/GCC, alongside other replay
fingerprints.

## Future work

A reusable event action catalog; sound/particle/mesh presentation
actions; animation graphs; event terms/messages; conditional filters;
actor timelines; richer F7 graph editing; and independent asset import
and packaging. Avoid changing game mechanics to drive visuals.
