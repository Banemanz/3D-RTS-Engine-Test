# Show versus Use Requirements
Original RTS Creator engine/editor | 2026-11-15

## Functional semantics

Catalog entries of type Unit (training), Ability and Upgrade have two
independent optional typed Requirement references:

- show_requirement: determines whether the command appears in the
  native game command card. Empty/absent means always visible.
- requirement: existing gameplay gate, now explicitly called Use.
  A visible command with failed Use stays present but disabled.

| Show | Use | Native command card | Command admission |
| --- | --- | --- | --- |
| False | Either | Hidden, no slot consumed | Rejected |
| True | False | Visible but disabled | Rejected |
| True | True | Visible and potentially enabled | Still checks other costs/rules |

Gameplay checks use the same fixed-tick authoritative Requirement
evaluation as the command HUD, NOT a duplicated UI expression.
Mineral/energy/cooldown fees are never spent on a rejected command.
The command tape stores explicit IDs; no UI page positions are saved.

## Catalog schema

Add optional reference fields on the Upgrade, Ability or trained Unit:

    "show_requirement": "r_tech_available",
    "requirement": "r_tech_unlocked"

Both referenced objects are typed Requirement entries. Requirements can
contain nested Predicate / All / Any / Not Boolean graphs. All normal
limits apply: maximum 8 nested levels, 64 expanded nodes, 8 distinct
children per composite, typed links and cycle rejection.

Show graphs must inspect only the Caster (for Unit training and research,
the producer structure). Target-scoped Show nodes are invalid, even if
nested under a Not or Any composite. This guarantees Show can always
be resolved before a target has been picked.

Ability Use graphs may inspect Target for targeted abilities; Ground
Abilities, Unit training and Upgrade research may not reference a
Target-scoped Use graph. Per-player UpgradeLevelAtLeast Validators
combine with all existing Validator types.

## F7 workflow

1. Create Validators and nested Requirements as usual.
2. On an Upgrade, choose "Show tech when" and "Use tech when" in the
   typed inspector, and Apply Upgrade research. The entire change is
   validated and committed as one Undo/Redo transaction.
3. On a Unit, choose "Show command when" and "Required graph". These
   control whether that Unit appears and can be trained by a producer.
4. On an Ability, choose the same independent Show and Use selectors.
   The caster's visible ability pages update when owner tech changes.
5. Use the F7 Data Navigator to inspect incoming/outgoing links,
   navigate to the referenced graph or see inherited references.
   Renaming/deleting a Requirement follows both link types, including
   supported placed Unit override references.

## F9 production and gameplay details

- Producer command cards pack visible trainable Units first, followed by
  visible Upgrades, three entries per page. Hidden choices do not leave
  reserved slots.
- Disabled choices remain in place for the player to see upcoming tech.
- Ability pages similarly omit hidden Abilities; the same hidden gate
  rejects a direct CastAbility command.
- A production structure with ALL choices concealed retains its producer
  command card and displays disabled EMPTY slots, NOT generic
  Build/Ranger/Worker fallback commands.
- Older maps with implicit base/barracks trainable choices respect the
  same visibility rules. No authored Show references means unchanged
  legacy behavior.

## Technology Forge demonstration

Open projects/technology_forge_showcase.rtsproject.json.

- Level 0: Frontier Tactics research appears in the Command Center.
  Worker training, Layered Plating and Arc Lance are hidden.
- Frontier Tactics level 1: Workers and Layered Plating appear, but
  their Use graphs still require Frontier Tactics level 2, so they are
  visible but disabled. Arc Lance becomes visible to its caster but
  its other Use conditions remain authoritative.
- Frontier Tactics level 2: Worker training and Layered Plating can
  now be used (subject to normal minerals/supply/busy-producer limits).
- Frontier Tactics level 3: the Not(r_max_tactics) Show graph hides
  the exhausted Frontier Tactics research button.

Linux CI captures actual native F9 software-GL frames at level 0 and
after level 1. For the second capture, the screenshot harness submits
a real ResearchUpgrade command and advances its authored fixed duration.
It never assigns technology levels directly or synthesizes GUI images.

## Key implementation areas

- DataCatalog, RTSUpgrades: schema validation of typed Show graphs,
  Caster-only checks and legacy field compatibility.
- RTSSkirmish: dynamic per-producer Unit and Upgrade visibility,
  selected-caster Ability visibility; normal command admission
  enforces the same gates.
- RTSSkirmish::selectedProducerHasCatalog: distinguishes a producer
  with no currently visible options from a nonproducer building.
- CreatorWorkbench, EditorSession: F7 authoring and Undo/Redo.
- DataCatalog::rename, RTSProject::catalogReferences and
  RTSCatalogNavigator: reference indexing, safe rename/delete and
  typed hierarchy navigation.
- tests/tech_visibility_runtime_tests.cpp: genuine hidden/locked/usable
  research, training and Ability commands, rejected raw orders,
  replay roundtrip and tamper detection, scoped validation, all-hidden
  HUD regression and old implicit training fallback.
- tests/tech_visibility_editor_tests.cpp: F7 typed transactions,
  rollback and history, indexed references, rename/deletion,
  graph navigation, Unit placement Show override migration.
- CI: 83 Release CTests on Windows/Linux; Debug replay fingerprint
  byte equality with tech_visibility_fingerprint.txt and native screenshots.
- --features: RTS_FEATURE_TECH_SHOW_USE=1.
  The manual is installed in both release executable bundles.

## Limitations / next work

This is not full Galaxy Editor parity. Still missing:
per-player trigger overrides of tech permissions, alternative button
positions, arbitrary Upgrade field operations, separate scripted
visibility and targeting APIs, custom rejection messages/tooltips,
an independent technology graph overview, multicaster selection
cards, and AI research progression. Runtime placed-producer field
overrides are not yet a separate production palette.

Reference concepts (read-only, no copied implementation):
- https://www.bluetracker.gg/wow/topic/us-en/2674992430-sc-ii-editor-qa-archive/
- https://docs.openra.net/en/release/traits/
- https://mapster.talv.space/galaxy/reference
