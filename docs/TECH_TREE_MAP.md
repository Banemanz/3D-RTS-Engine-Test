# Technology Map: visual authoring and typed dependency export

RTS Creator | November 15, 2026 | Original editor/engine code

## What this implements

The **Technology Map** is a dockable, read-only F7 companion to the existing
Data Inspector, not a separate implementation of research prerequisites.
It projects actual catalog references plus compiled Requirement/Validator
trees into a navigable graph. Both Linux and Windows use the same C++17
`RTSTechGraph.hpp` projection and `CreatorTechGraph.hpp` panel. Nothing in the
graph modifies the simulation, data files, or replay hashes.

Open **Data Editor (F7)**, then **View → Technology dependency map**.
Its four columns distinguish producers, research Upgrades, other Units, and
Abilities. Nodes are labeled using authored display names and catalog IDs.
Click a node to select it in the regular F7 Inspector, where all real editing
and validated undo/redo still occur. Enable **Focus selected** for a
single-hop graph around the selected catalog entry. Drag/dock/resize the
Technology Map window like other native creator panels. Use the mouse wheel
and horizontal scrollbar to inspect large graphs.

## Typed edges and truthful Requirement semantics

- **Research**: a producer Unit offers an Upgrade through
  `researchable_upgrades`.
- **Train**: a producer offers a Unit through `trainable_units`.
- **Equip**: a Unit loadout references an Ability through `abilities`.
  Equipping an Ability does **not** make the Unit a producer.
- **Modifies**: an Upgrade targets a particular Unit catalog ID for stat gains.
  Upgrades targeting all Unit classes have no single outgoing Modifies edge.
- **Show**: a Requirement graph controls whether a command appears.
- **Use**: a Requirement graph controls whether the action is admitted.

Show/Use edges are from **referenced Unit or Upgrade predicates** to
the gated Unit, Ability, or Upgrade. Each carries the source Requirement ID
and applicable `UpgradeLevelAtLeast` or `OwnerUnitCountAtLeast` threshold.
An odd number of nested **Not** nodes marks a negative reference; **Any**
records Boolean disjunction context. The map **does not** claim that an
edge inside an Any tree is a mandatory prerequisite, or that arbitrary
Requirement graphs reduce to simple AND prerequisite chains. Non-tech
predicates (HP, energy, behaviors, structure checks, untyped owner counts)
are counted as **other gates** on affected nodes and remain editable in
the actual Requirement Inspector. Self-references are visible, including
"hide this Upgrade at its own maximum level."

The graph is intentionally *an explanation and navigation tool*, not a
replacement validator or a hidden technology execution system. Graph colors
distinguish edge categories; hover a node for the complete reference,
threshold, source Requirement and logical provenance.

## Headless export for creators and tools

```
RTSProjectTool --tech-graph projects/technology_forge_showcase.rtsproject.json > technology.dot
```

The CLI validates the project and emits deterministic **Graphviz DOT** to
stdout. It escapes arbitrary author-supplied labels, IDs, control characters,
and quotes so they cannot corrupt the graph syntax. DOT supports use by
external tools; no Graphviz dependency is required for engine builds.
The CLI is also installed in both packaged Windows/Linux creator kits.

`RTSTechGraph.hpp` is a reusable, GPU-independent graph extraction layer.
Nodes, links, and row layout have deterministic ordering independent of
catalog insertion order. For very large projects the projection is bounded
to 512 nodes and 2048 edges, with an explicit truncated warning in F7.
The CLI refuses to export an incomplete graph rather than silently truncating.
All other project data remains intact.

## Engineering verification

`tests/tech_graph_tests.cpp` checks the actual Technology Forge project,
producer/research and stat-target links, Show/Use levels, negated self gates,
nested Any/Not provenance, inheritance/roundtrip, output ordering, escape
safety, graph nonmutation and unknown extension isolation. The headless CLI
has a dedicated CTest. CI compiles and tests Windows/MSVC and Linux/GCC,
packages the authoring manual and captures an **actual Linux OpenGL F7
Technology Map screenshot** using `--smoke-tech-graph`.

The feature does not alter replay schemas, gameplay commands, authoritative
state, the in-house HUD, or the validity of existing projects.

## Boundaries and next milestones

- No arbitrary drag-wiring yet; edit requirements through the validated
  graph inspector and reopen this live projection.
- No boolean SAT solver, automatic guarantee that an Upgrade is reachable,
  or exhaustive production-tree proof. Some valid catalogs can be
  deliberately self-gated or require conditions outside this graph.
- No multiplayer technology synchronization or AI research policy.
- No imported Blizzard assets, proprietary Galaxy code, or GPL engine code.

Relevant **conceptual references only**:
- Galaxy Data Editor catalog categories:
  https://s2editor-guides.readthedocs.io/New_Tutorials/04_Data_Editor/058_Data_Editor_Introduction/
- Galaxy Tech Tree scripting API:
  https://mapster.talv.space/galaxy/reference
- OpenRA trait-based production prerequisites:
  https://docs.openra.net/en/playtest/traits/

Next: optional detailed expression-side tree overlay inside the graph,
user-positioned nodes, structured tech diagnostic explanations, AI research
planning in the authoritative command stream, real asset and animation
importing, stronger terrain/scene tooling and eventual in-house dockable
editor widget parity.
