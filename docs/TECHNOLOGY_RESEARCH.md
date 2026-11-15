# Upgrade technology and research authoring
RTS Creator | original native engine/editor | November 15, 2026

## Why this exists

A reusable Upgrade is a typed gameplay catalog object. Researching it
advances an authoritative, per-owner technology level at a fixed simulation
tick. This level modifies existing and future matching Units. The gameplay
code is independent of renderer/Actor cues and runs equally on Windows and
Linux. Existing maps without Upgrades remain fully backward compatible.

These are original general RTS mechanics, not StarCraft II content,
source or assets. The architecture was informed by public Galaxy Editor
documentation on upgrade-level modifiers, tech-tree requirements and
Show/Use distinctions, and by OpenRA's public production/trait docs.

## F7 authoring, step by step

1. Open Data Editor (F7), choose New catalog object and create either
   Upgrade | Attack research or Upgrade | Armor research.
2. Select that Upgrade. Set maximum research level, cost and fixed tick
   duration per level, damage/armor per level, affected Unit class (or All),
   and optional Caster-only Requirement graph. Apply commits one validated
   undoable edit. Revert discards uncommitted input.
3. Select the Unit catalog object of a player-owned producer structure,
   e.g. Command Center. Enable researchable_upgrades, then add the
   typed Upgrade reference. An ordered maximum of 12 research choices
   is supported; parent Unit inheritance and local empty overrides work.
4. For tech unlocking, create Validator | Research level with kind
   UpgradeLevelAtLeast, subject Caster, integer threshold and linked
   Upgrade ID. Wrap in a Requirement | Single validator. Such graphs
   can gate subsequent Upgrades, trainable Units or Abilities.
5. Press F9, select that producer building and use its normal command
   cards. Training and research share 3 paginated slots per page.
   Research shows a six-character Upgrade label plus nextLevel/maxLevel
   (or MAX once fully researched) to fit narrow command cards.
   F6 Trigger Monitor shows active research, remaining ticks, level
   and a progress bar. Cancel Queue refunds an in-progress job.

Original sample: projects/technology_forge_showcase.rtsproject.json.
Frontier Tactics improves Rangers and unlocks Layered Plating,
Worker training and a chained Arc Lance Ability Requirement.
The Data Navigator displays typed outgoing/incoming references.

## Upgrade fields (inside catalog entry fields object)

    {
      "name": "Frontier Tactics",
      "unit": "ranger",
      "max_level": 3,
      "mineral_cost": 45,
      "research_ticks": 180,
      "damage_per_level": 3,
      "armor_per_level": 1,
      "requirement": ""
    }

Fields:
- unit: optional exact Unit catalog ID. Empty means all owned Unit
  classes. The ID match is exact, not automatically all descendants.
- max_level: integer 1..5.
- mineral_cost: integer 0..100000, the same cost for EACH level.
- research_ticks: integer 1..36000 at the game's fixed 60Hz tick;
  constant duration for each level.
- damage_per_level: finite float 0..100, additive to normal Unit
  attacks (including Weapon-linked and projectile-delivered hits).
- armor_per_level: finite float 0..100, additive to Unit mitigation
  in takeDamage (including Effect damage using that method).
- requirement: optional Caster-only reusable Requirement ID.
- Total per-Unit damage and armor bonuses cap at 1000 each.
- Unknown extension fields are preserved in catalog JSON.

Upgrades do NOT rewrite UnitData, Weapon fields, temporary Behaviors,
cooldowns, resource production, max HP, or healing amounts.

## Producer and Validator references

Example producer Unit field:

    "researchable_upgrades": ["frontier_tactics", "layered_plating"]

These are strictly typed, unique Upgrade IDs, at most 12, in
author-controlled order. Derived Units inherit the list unless
they author a replacement (including an empty list).
Normal Unit training uses trainable_units and is unchanged
on maps without research.

Example technology-level Validator:

    {
      "kind": "UpgradeLevelAtLeast",
      "subject": "Caster",
      "value": 1,
      "upgrade": "frontier_tactics"
    }

Its value must be an integer from 1..5. The owning Caster's completed
tech levels are checked, not pending or queued technology. Use
All/Any/Not Requirement nodes to combine this with Health, Energy,
active Behavior, owned Unit count or other Upgrade predicates.
Research prerequisites cannot inspect a Target Unit: the only
context is the producer building; nested target scope is rejected
during catalog validation. Self-gated tech can be authored but
cannot research itself from level zero without future scripting.

## Authoritative research lifecycle

- The player queues ResearchUpgrade with an exact Upgrade catalog ID
  and explicit producer entity ID. The command is validated and stored
  in the existing fixed-tick command/replay stream. The enum entry
  was appended to preserve older command IDs.
- The producer must be alive, player-owned, completed and explicitly
  list the Upgrade; prerequisites and mineral costs must be satisfied.
  Max-level research cannot be issued. Each producer supports at most
  one concurrent research job, research/training are mutually exclusive,
  and an owner cannot research the same Upgrade concurrently on two
  producers. At most 64 research jobs are active.
- Research deducts minerals at admission, then completes exactly
  after research_ticks simulation ticks. Rejection consumes nothing.
  Canceling via the existing command or destruction/loss of the
  producer refunds the research cost and grants no technology level.
- Completed levels are stored by (owner, Upgrade ID) in stable sorted
  order. All already-present matching owner Units immediately get
  new calculated damage/armor bonuses; future placed/trigger/trained
  spawns get the same levels. Reset clears runtime progress.
- Research jobs, completed owner levels and nonzero unit modifiers
  are in authoritative stateDigest. No clock, GPU, pointer or UI
  data participates. Empty tech state contributes no extra hash,
  preserving legacy no-tech replay fingerprints.

## Source map and testing

- RTSUpgrades.hpp/.cpp: strict typed catalog compile and lists.
- RTSRequirements.cpp: UpgradeLevelAtLeast predicate.
- DataCatalog, RTSProject, RTSCatalogNavigator: typed refs, safe
  rename/deletion, inherited fields, scene research-list overrides.
- RTSSkirmish.cpp: live research scheduler, cancellation/refund,
  per-owner levels, stat propagation and canonical state hash.
- RTSCommandStream: ResearchUpgrade command and JSON replay roundtrip.
- CreatorWorkbench, EditorSession: atomic F7 creation/editing and
  typed reference cards/Validator research-level selector.
- RTSHUDLayout and main loop: F9 producer tech cards with normal
  page/keyboard commands. CreatorTriggerMonitor: read-only F6 watch.
- tests/upgrade_runtime_tests.cpp: actual research/cancel/replay,
  costs/ticks, max level, future spawns, Unit mitigation, tech gating,
  invalid project/enum/tamper and deterministic fingerprints.
- tests/upgrade_editor_tests.cpp: presets, grouped edits, invalid
  rollback, Undo/Redo, inheritance, rename/deletion protection,
  scene placement research list migration and roundtrip JSON.
- projects/technology_forge_showcase.rtsproject.json: original map.
  Both platform Release bundles validate it with packaged ProjectTool.

CI requires 81 Release CTests on Windows and Linux, Debug tests and
byte-identical upgrade_research_fingerprint.txt. Native Linux Mesa
captures a real F7 Upgrade inspector and F9 producer research card
screenshot. Bundle verifier checks RTS_FEATURE_UPGRADE_RESEARCH=1
and ensures this manual is packaged.

## Known limitations and next steps

Not yet included: arbitrary field operation tables like Galaxy's
Upgrade effects, per-level custom costs/effects, research speed
modifiers, AI research orders, enemy researcher UI, research queue
stacking, tech Show versus Use distinction, multiplayer syncing,
animations/sounds/particles for research, or a standalone technology
tree graph/overview panel. The foundation is intentionally generic,
bounded and testable rather than claiming Galaxy Editor parity.

Relevant public documentation (research only, no imported source):
- https://staredit.net/topic/13227/
- https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/
- https://mapster.talv.space/galaxy/reference
- https://docs.openra.net/en/release/traits/

## Extension: separate technology Show and Use graphs

Upgrade entries now have optional show_requirement (Caster-only
visibility) in addition to requirement (existing Use/admission).
Unit training and Ability catalogs support both fields too. A
false Show graph removes a command from native F9 pages; a failed
Use graph leaves it displayed but disabled. Backend commands
enforce both. Technology Forge now demonstrates multi-level
reveal, unlock and max-level concealment. See docs/TECH_SHOW_USE.md.
