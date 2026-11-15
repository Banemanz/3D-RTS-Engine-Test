# Validator and Requirement graph authoring
Original RTS engine and editor — November 15, 2026

## Purpose

Validators describe one reusable live-gameplay question. Requirements
combine Validators with deterministic Boolean logic. The engine evaluates
these objects on the authoritative simulation state, so they can gate
an Ability cast or admission to a Unit production queue. This system
is separate from Actor presentation events, trigger monitoring, and
GUI availability. Legacy objects without a requirement field are
unrestricted and behave exactly as before.

## F7 authoring walkthrough

1. Open Data Editor (F7), choose New catalog object, and create a
   Validator: Health threshold, Energy threshold, Status presence,
   Is structure, or Owned unit count.
2. Select the Validator in the object browser. Edit the predicate kind,
   which actor to read (Caster or Target), and the kind-specific
   threshold or linked Behavior/Unit. Apply Validator commits one
   validated, undoable project edit.
3. Create a Requirement | Single validator. Pick the Validator in
   the Requirement graph inspector.
4. For multiple conditions, create Requirement | All children,
   Any child, or Negate child after you have leaf Requirements.
   Add child Requirement references, reorder them or remove them,
   then apply the entire graph in one undo transaction.
5. Select an Ability or Unit. In the typed Requirement selector
   choose the authored graph. Select None to clear the restriction.
   The F7 Data Navigator exposes forward/reverse dependencies,
   inheritance and usable jump links.
6. In F9 try the ability or production order. A failed gate does not
   consume energy/minerals or set a cooldown; rejected commands
   are still recorded and replayed deterministically.

Original sample: projects/requirements_workshop_showcase.rtsproject.json.
Arc Lance requires an injured non-structure target and enough caster
energy; Worker training is locked until enough Rangers exist.

## Validator catalog fields

Every Validator has a kind and a subject. Subject=Caster refers to
the issuing Unit (for production, the producer structure).
Subject=Target refers to a selected ability target. A Self Ability
uses the caster for both subjects.

| kind | supported fields and semantics |
| --- | --- |
| HealthBelowPercent | value 0..100, strict current HP percentage below threshold |
| EnergyAtLeast | value 0..10000, current energy at least value |
| HasBehavior | behavior: typed Behavior ID, active applied behavior |
| IsStructure | truth value of Unit isStructure flag |
| OwnerUnitCountAtLeast | value integer 1..4000, optional unit: exact Unit catalog ID; counts currently living Units owned by subject owner |

Unknown extension fields remain untouched. For HasBehavior the
behavior reference is required, including when no Behavior is
currently active. For OwnerUnitCountAtLeast an empty unit reference
counts all living owner Units including structures, while a named
unit matches that exact catalog ID.

Example Validator fields (within a catalog entry):

    {
      "kind": "HealthBelowPercent",
      "subject": "Target",
      "value": 50
    }

Another useful Validator:

    {
      "kind": "OwnerUnitCountAtLeast",
      "subject": "Caster",
      "value": 5,
      "unit": "ranger"
    }

## Requirement catalog fields

A leaf uses kind=Predicate and a typed validator ID:

    { "kind": "Predicate", "validator": "target_wounded" }

Composite nodes use kind All, Any or Not, with a children array
of referenced Requirement IDs, for example:

    {
      "kind": "All",
      "children": ["has_energy", "target_wounded", "not_structure"]
    }

All means every child is true; Any means at least one is true;
Not logically inverts exactly one child. Composite nodes require
1..8 unique children; Not requires exactly one. Cyclic references
are rejected; expanded graphs are limited to 8 levels and 64
nodes to bound compile and evaluation. Each node and reference
is validated before a map can be saved.

On an Ability or Unit catalog entry, attach the graph with:

    "requirement": "your_requirement_id"

Empty or absent requirement means no restriction. All typed links
are protected by project rename/deletion checks and can be inherited
by derived catalogs. Placed Unit overrides of requirement references
also participate in rename safety.

## Gameplay admission

- Self Ability: evaluation is available before target selection;
  Caster and Target both name the caster.
- Enemy/Ally/Any Ability: evaluation occurs against the actual
  target chosen by the player; no speculative target is invented.
- Ground Ability: only Caster-scoped Validators are permitted,
  since it has a ground point but not a Unit target. Such gates can
  be evaluated before drawing an enabled cast cursor.
- Unit production: the Requirement on the *trained Unit* is evaluated
  against the producer building when its queue order is submitted.
  Only Caster-scoped predicates are permitted. This does not block
  placed Units, trigger-spawned Units, or existing starting Units.

Target-only graph links on Ground Abilities or trainable Units are
rejected at map validation, even when hidden under Not or Any.
Requirements are evaluated on submission, not again after a
successful queued production job has started.

## Boundaries and regression coverage

- DataCatalog/RTSRequirements.cpp: strict typed graph/predicate
  compilers with inheritance and bounded cycle traversal.
- RTSSkirmish: no rendering dependencies; checks live Unit HP,
  energy, structure flag, active Behaviors, and owned Unit counts.
- EditorSession: grouped validated authoring with undo/redo.
- CreatorWorkbench: specialized F7 editor, reference selectors.
- RTSCatalogNavigator: dependency and inherited link browsing.
- tests/requirement_runtime_tests.cpp: actual positive/negative
  Ability commands, training gates, Boolean graphs, strict invalid
  project rejection, status expiry and replay/tamper checking.
- tests/requirement_editor_tests.cpp: create/edit, invalid rollback,
  undo/redo, rename/delete, inheritance, placement override
  migration, JSON roundtrip and reverse graph links.

Windows/Linux CTests and exact replay state fingerprints are
required before the feature is considered complete. Native F7
screenshots are captured in Linux CI under software OpenGL and
the packaged executable reports RTS_FEATURE_REQUIREMENT_GRAPHS=1.
The current feature set is not a full Galaxy Editor validator,
tech-upgrade, requirement-node or trigger-library implementation.

## Follow-on work

Expose richer comparisons and custom variable predicates; create
Upgrade and Research catalogs that can supply tech unlocks to these
Requirements; add explicit trigger conditions and scripted ability
query results; generalize to targeting filters, production item
visibility and richer failure diagnostics; add searchable node graph
and instance-data provenance views.
