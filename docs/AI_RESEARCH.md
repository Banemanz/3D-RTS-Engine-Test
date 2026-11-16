# Opponent Research Planner: deterministic engine and F7 authoring

RTS Creator — November 16, 2026 — original RTS engine technology system

## What is implemented

The enemy AI can now **research authored Upgrade levels**, without invoking
player UI or directly applying an Upgrade. Its policy proposes a
\`ResearchUpgrade\` command stamped with the next simulation tick, a single
enemy-owned producer ID, and a typed Upgrade catalog ID. That command passes
through the **same authoritative command stream and admission logic** as
human research. The replay stores the AI order as \`source: 1\`; replay
verification does not regenerate AI decisions and verifies state digests.

A research attempt requires all of the following:
- Enemy AI and opponent research are enabled in project metadata.
- An alive enemy-owned structure offers the Upgrade via its inherited
  \`researchable_upgrades\` Unit catalog array.
- Its compiled \`show_requirement\` (Show) and \`requirement\` (Use) evaluate
  true for the producer at the current simulation tick.
- The Upgrade is listed in author-specified AI priorities; its next level
  is below maximum and has a valid per-level cost/duration.
- A producer is not simultaneously training/researching, and another
  producer is not already researching that Upgrade for this owner.
- There is sufficient **enemy research budget**. Player minerals cannot
  be spent by AI orders and enemy mineral spending cannot affect player
  ownership.

The planner tries prioritized IDs in author order, **skipping locked
technologies** rather than blocking the entire queue. When several valid
enemy structures can research the same technology, the lowest stable
producer ID wins. The planner has no RNG, dynamic threads, clocks or unordered
container iteration dependencies. Research progress ticks, completion,
stat gains to existing and future enemy Units, and refunds on the destruction
of a researching structure are deterministic and replay-verifiable.

## F7 / F9 native interface

Open **F7 Data → View → Opponent research planner** in the native editor.
Configure the enable flag, starting mineral budget, decision interval, and
ordered list of Upgrades. Click **Inspect** on an Upgrade to jump to its
regular F7 typed inspector. Use **Up**, **Down**, **Remove**, or
**Add technology** to change priorities. Every operation is validated
against the entire project and creates an undo/redo history entry; invalid
edits do not change saved data. The panel warns when no placed opponent
research structure offers any configured Upgrade.

In Play/F9 mode the same **View → Opponent research planner** panel is an
**observer**, showing opponent budget, completed levels and ongoing research
tick progress. This monitoring surface is not included in replay hashes
or gameplay commands.

## Project JSON schema

All fields are optional. Existing projects behave unchanged.

\`\`\`json
{
  "metadata": {
    "enemy_ai_enabled": true,
    "enemy_ai_research_enabled": true,
    "enemy_ai_research_starting_minerals": 75,
    "enemy_ai_research_interval_ticks": 30,
    "enemy_ai_upgrade_priorities": [
      "raider_plating",
      "raider_doctrine"
    ]
  }
}
\`\`\`

- \`enemy_ai_research_enabled\`: Boolean, default **false**.
- \`enemy_ai_research_starting_minerals\`: integer 0..1,000,000,
  default 100; separate from \`player_starting_minerals\`.
- \`enemy_ai_research_interval_ticks\`: integer 2..3600, default 30
  (fixed 60Hz simulation ticks).
- \`enemy_ai_upgrade_priorities\`: unique ordered Upgrade IDs, 0..12.
- \`enemy_ai_enabled\`: existing global opponent AI opt-out. Setting it
  false also disables new research decisions.

Noninteger budgets/ticks, invalid types, duplicate/missing/non-Upgrade
priority IDs, and out-of-bounds values cause project validation errors.
Typed catalog rename migrates research priority references transactionally;
referenced Upgrade deletion is blocked. All author fields round-trip.

**Producer setup:** The authored Unit catalog object for a placed owner-1
structure must include the desired Upgrade IDs in \`researchable_upgrades\`.
A default project does not automatically give the enemy research capabilities.
The sample \`projects/ai_research_showcase.rtsproject.json\` includes
the correctly configured \`rift_bastion\` and two original enemy Upgrades.

## Worked scenario

\`projects/ai_research_showcase.rtsproject.json\` uses an original
\`raider_doctrine\` Upgrade (two different level costs/durations and
incremental attack bonuses), \`raider_plating\` (armor), and a typed
\`UpgradeLevelAtLeast\` Validator / Requirement gating plating at doctrine
level one. The policy prefers plating over doctrine, but initially cannot
see or use plating, so it researches doctrine first. It later researches
plating and the final doctrine level.

Expected sample timing at 60Hz:

| Tick | Engine action | Enemy minerals | Doctrine | Plating |
|---:|---|---:|---:|---:|
| 0 | Initialize | 75 | 0 | 0 |
| 2 | Start doctrine L1 (4 ticks, cost 15) | 60 | 0 | 0 |
| 6 | Complete doctrine L1 | 60 | 1 | 0 |
| 32 | Start plating L1 (5 ticks, cost 20) | 40 | 1 | 0 |
| 37 | Complete plating L1 | 40 | 1 | 1 |
| 62 | Start doctrine L2 (6 ticks, cost 30) | 10 | 1 | 1 |
| 68 | Complete doctrine L2 | 10 | 2 | 1 |

The existing enemy combat behavior and scheduled raider spawns still
operate independently. Tech buffs apply to matching existing/future
owner-1 raiders; opposing player Units never receive those buffs.

## Safety, determinism and limits

The command wire validator only grants the AI **Attack** and narrowly
formed **ResearchUpgrade** orders. Research requires exactly one producer
ID, an Upgrade ID, no attack target/position or append flag; the runtime
rejects forged ownership, unavailable capabilities, bad priorities,
failed Show/Use gates and insufficient funds even if command syntax is valid.
No direct AI commands bypass replay, player selection state or admission.

This is **bounded skirmish opponent research**, not a complete macro AI:
enemy minerals are an authored starting pool, not yet collected via worker
harvesting; there is no automatic AI structure construction, economy
expansion, training policy, adaptive forecasting, alliances or multiplayer
lockstep. No arbitrary strategy-script execution or auto-discovery of
Upgrade dependency graphs happens outside authored priority and
Requirement data. For larger AI design, the next steps are a typed
economy/production planner and trigger-driven high-level objectives.

## Regression / cross-platform checks

\`RTSAIResearchTests\` exercises real command recording, progression
and costs, locked-priority fallback, owner isolation and forged AI command
rejection, refund-on-death, disabled policy, invalid metadata, Undo/Redo,
catalog rename/delete integrity, deterministic candidate selection and
full replay verification. Linux and Windows CTests produce
\`ai_research_fingerprint.txt\`; GitHub Actions checks byte-identical
checkpoint digests between platforms.

The Linux software-OpenGL CI job captures both F7 authoring settings
and F9 live progression via \`--smoke-ai-research\` /
\`--smoke-ai-research-play\` (the latter with \`--smoke-fixed-ticks\`).
Both platform bundles ship the scenario and manual and report
\`RTS_FEATURE_AI_RESEARCH=1\` from the real compiled executable.
No borrowed Blizzard assets, UI, names, or third-party engine source.


## UI screenshot review and default panel placement

The first native F7/F9 screenshots exposed default floating window
overlap, not a gameplay error. The policy panel now reserves the
left browser and right inspector at 1280-wide, and the inspector
offers a narrower default width while the policy is open. In Play
mode, the read-only monitor uses a compact 225px height and no
authoring instructions, leaving the combat map and in-house HUD
visible. The screenshot harness selects `raider_doctrine` so the
policy controls and a related typed Upgrade inspector can be
seen together. User docking and window resizing override defaults.
