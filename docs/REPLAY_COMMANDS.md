# Simulation commands and replay verification

Updated November 14, 2026. Authoring tool and fixed-timestep replay milestone for the original RTS creator. Shared C++17 implementation and tests are compiled identically on Windows x64 and Linux x86_64.

## Playtest workflow

1. Open a project and enter **Play mode** (F9).
2. Issue ordinary unit movements, attack-moves, patrols, attacks, right-click context actions, worker gathering, production, structure building, hold, stop and rally commands. The input is recorded as immutable commands scheduled for the next fixed simulation tick, **with the unit IDs selected when the input occurred**.
3. Select **View → Export playtest replay** or press **Ctrl+Shift+L** while playing. The creator saves `<project-path>.rtsreplay.json`. The replay embeds a snapshot of that project's authoring data; it does not need the original map as a separate file.
4. To verify, use `RTSProjectTool --verify-replay your-map.rtsproject.json.rtsreplay.json` (Windows: `RTSProjectTool.exe`). The tool runs headlessly, reconstructs the map, reissues commands at their recorded ticks, and compares checkpoints. A successful verification prints the number of simulated ticks; any divergence reports the first failing tick, expected digest and actual digest and exits nonzero.
5. To inspect a recording, use a normal text editor: it is JSON containing the map snapshot, a versioned command timeline and checksums. It does **not** contain rendered frames, licensed character assets or a video.

### Command order

All authored gameplay inputs are placed in `RTSCommandStream`, indexed by integer simulation ticks (60 per second). For each tick, commands are sorted by source and unique sequence. Unit target IDs are sorted, de-duplicated at local submission and independently validated on replay loading. The input queue has bounded pending commands (512), future tick horizon (1200), per-tick fan-in (64) and unit-ID count (128). Invalid, duplicate, late, oversized or wildly future commands are refused.

Commands include `Move`, `AttackMove`, `Patrol`, `Gather`, `Attack`, `Stop`, `Hold`, `Build`, `Rally`, `TrainCatalog`, `TrainRanger`, `TrainWorker`, `CancelTraining` and `Context`. The `TrainCatalog` command resolves the specific catalog unit ID **when issued**, not against a possibly changed training page later. A context click preserves its identified enemy target ID; if no enemy was targeted, the original rally/gather/move priority is evaluated at the command's simulation tick.

## Replay format

`RTSReplayTape` now writes `version: 2` (and still reads legacy version 1), `tick_hz: 60`, `ticks`, `project`, `commands`, and `checkpoints`. Each command records `tick`, `sequence`, `source`, `kind`, `units`, `point` (XZ), `target`, `append`, `catalog`. Every checkpoint contains an integer `tick` and a **16-digit hex state digest**. The replay is limited to 32 MiB, at most 24 hours of simulated play and 32,768 recorded commands. These are recording limits, not a reason to stop or disable live gameplay after the recording horizon.

Digests use explicit FNV-1a byte-order handling with quantized floating values (1/4096 resolution), stable unit-ID iteration, navigation state and occupancy, production, resources, queued orders, movement, health and fog-of-war samples. The hash is **not a cryptographic signature** and omits purely visual UI selection. It is intended for detecting reproducibility errors, not preventing malicious tampering.

### Correctness gates

- `RTSCommandReplayTests` checks command unit identity capture despite later UI selection changes, fixed-tick ordering, queued worker production, deterministic state checkpoints, JSON save/load, deliberate checksum and command tampering, future/late/duplicate command validation, and rejection of an unrecorded direct gameplay mutation.
- `replay_cli` CTest verifies an actual on-disk replay produced by the command test through `RTSProjectTool`, rather than calling the verifier only as a library.
- Each Windows and Linux Debug CI job emits `command_replay_fingerprint.txt` from the same scripted scenario. A mandatory **replay-parity** CI job compares *every* checkpoint with `diff`, detecting divergence between MSVC/Windows and GCC/Linux beyond simple successful independent tests.
- Windows/Linux Release suites retain assertions, run the new checks, and verify `RTS_FEATURE_COMMAND_STREAM=1` and `RTS_FEATURE_REPLAY_VERIFY=1` in the **actual** packaged executable.

## Boundaries and remaining work

- This records the original **Play mode** game commands and scripted simulation, not live editor terrain painting or arbitrary external C++ mutations. Such unrecorded changes invalidate replay export rather than yielding a false recording.
- This is a foundation for deterministic multiplayer; it is **not** a network protocol, rollback system, snapshot/restore, full lockstep transport, authoritative server, cryptographically secure anti-cheat or a guarantee of platform-identical results for every possible map and workload.
- Real-time mouse/HUD input now schedules commands at the *next fixed tick*. Commands can fail gameplay checks at execution time (insufficient resources or blocked targets), rather than always resolving synchronously inside the input callback. A future UI pass should show detailed queued/executed/rejected command feedback.
- The replay verifier runs in the headless project tool. Visual interactive playback and timeline scrubbing remain future creator features. Standard unit selection and camera movement remain presentation state, not simulation commands.
- Opponent combat AI Attack orders now use the command queue. The next engineering work includes AI economy/production orders, replay recording diagnostics, replay state snapshots, movement classes, and deeper data-driven ability/actor authoring.


## Replay schema v2 — recorded AI input and v1 playback

Replay version **2** includes `source=1` commands for opponent Attack decisions, in addition to the existing `source=0` player order stream. AI command authorization currently limits source 1 to `Attack` with an explicit target ID and at least one sorted unit ID; the world checks the attacking units belong to team 1 and the target belongs to the player. AI orders are planned after a fixed tick, scheduled for the next tick, and recorded when actually consumed (including unsuccessful orders).

On verification, v2 playback **disables automatic AI planning** to avoid generating duplicate orders and consumes exactly the AI inputs recorded in the tape. Existing v1 JSON replay files remain supported: they contain source-0 commands only, and the verifier reinstates the old direct-AI base-target behavior, preserving their historical simulation. The replay-version field is validated and v1 rejects injected AI inputs.

The AI policy is a pure headless source file, not a replay input parser, and has validated map metadata `enemy_ai_enabled` and `enemy_ai_guard_radius`. New sample: `projects/ai_defense_showcase.rtsproject.json`. The version upgrade does not add interactive replay playback or real multiplayer transport; pending commands are still bounded. See `RTS_ENGINE_AUDIT.md` for larger AI gaps.


## Cross-platform AI planning parity fixture

The replay CI compares two independent scripted test scenarios: the normal combat/production command replay, and a full 180-tick **AI Defense Combat Lab** simulation from `projects/ai_defense_showcase.rtsproject.json`. This second map deliberately places two player scouts near the enemy home base; the test checks that recorded AI Attack decisions target the nearby scouts. The headless runner exports and verifies the tape, and the Windows/Linux parity job compares its digest checkpoints byte-for-byte. This is significantly stronger than comparing a noninteractive or base-only match, but still not a guarantee of universal multiplayer determinism.


### Pre-movement AI scheduling guarantee

AI command proposals are computed **after the current tick's pending commands are applied, but before any `Unit::update` tactical acquisition or movement runs**. This makes the first defensive command against nearby attackers observable on the replay input stream, rather than letting the automatic combat state machine consume the unit's idle state first. Orders still execute on the **next** fixed tick and are sorted by source/sequence, so selection and UI state cannot affect them. Legacy v1 replay AI runs its original post-unit-update direct policy unchanged.


## CastAbility replay, energy, and cooldown identity

CastAbility is appended to the preexisting RTSCommandKind enumeration,
preserving previous indices and JSON name strings. Source-0 player
commands require a nonempty Ability catalog ID and stable caster IDs.
Self targets pass -1; unit-target actions pass a stable target ID.
Source-1 AI is still restricted to its Attack command. At the
authoritative tick, casts recheck the catalog, owner, enemy visibility,
range, minerals, per-Unit energy and cooldown. Rejected inputs are
recorded as rejections, without spending resources or arming clocks.

The ordered map from (caster unit ID, Ability catalog ID) to next-ready
tick participates in stateDigest when nonempty. Energy participates
when the Unit actually has a pool. RTSAbilityCastTests verifies
serializer roundtrips, rejection, cooldown timing, energy/regeneration,
target damage and tamper detection. The mandatory replay-parity job
compares ability_cast_fingerprint.txt from Windows/MSVC and Linux/GCC.
The replay file schema remains v2.


## Ability-driven status modifier replay and exact tick semantics

An Ability's Effect may emit ApplyBehavior/RemoveBehavior. The
CastAbility command remains an authoritative source-0 next-tick
input with stable caster and target IDs; its compiled Effect
operations can now create, stack or remove typed Behavior instances.
Their expiry deadlines use absolute simulation ticks and their
ordering is based only on Unit/Behavior catalog IDs.

Status state is hashed only when nonempty, preserving default
legacy match fingerprints. RTSSimulation behavior runtime tests
record a reproducible behavior_fingerprint.txt with casts, cooldown
rejection, status application and expiration; the strict CI parity
job byte-compares Windows/MSVC versus Linux/GCC fingerprints.
Data validation also rejects corrupt/missing Behavior references,
bounded-overflow fields and invalid status dependencies.
The replay format remains v2; no UI cursor/page or render state is
serialized as a gameplay command.


## Ground CastAbility replay semantics

The existing CastAbility command uses its point XZ field for
Ground targets and targetId=-1; no new command enum or tape
version. Ground target validation happens at the command tick.
Delayed AreaImpact instances store a stable center, caster owner,
area filter, Ability and Effect IDs and absolute impact tick.
The impact examines living Unit positions at arrival, independent
of the game's frame rate or mouse cursor after casting.

Pending impacts are included in the state digest while present,
and their sorted application deterministically updates health,
status and periodic-effect queues. Windows and Linux CI upload
and byte-compare ground_area_fingerprint.txt. CTests verify
successful and rejected commands, JSON tape roundtrip,
and rejection of tampered area center coordinates.


## Projectile-delivered CastAbility commands

Projectile launch uses the existing source-0 CastAbility input:
selected caster IDs, world Ground point, Ability ID and targetId
=-1. Neither the command kind nor replay tape version changes.
At applyCommand time the Projectile catalog is compiled and
its speed, arc, collision rules, launch/start/end and arrival
ticks are snapshotted into the bounded pending-impact job.

Nonempty jobs contribute these fields to the canonical
stateDigest. The projectile's 3D position is derived from
the fixed simulation tick rather than generated by a renderer.
Swept interception is resolved after Unit movement and before
the affected Effect is applied. Purely decorative hit flashes
and trail vertices are excluded from gameplay state digests.
Older nonprojectile area jobs hash their previous fields only.

New projectile_fingerprint.txt is generated by the projectile
CTest suite and byte-compared between Windows/MSVC and Linux/GCC.
Tests verify launch, midflight trajectory, delayed arrival,
contact interception, moving victims, replay JSON roundtrip
and coordinate-tamper rejection.

## Replayable cone and line ground impacts

Ground CastAbility commands remain identical in replay format:
they capture a world point, explicit caster ID and Ability ID.
Shape parameters resolve from the versioned project snapshot.
For future pending Cone and Line impacts the simulation state
digest now includes shape type, cast-time caster origin, cone
angle and line width. Legacy Circle hashes remain unchanged.
The target set is chosen at impact, with stable numeric Unit-ID
sorting; the target orientation does not recompute from any
Unit movement after casting. The dedicated area_shapes_fingerprint
fixture is compared between the Windows and Linux CTest runners.

## Tagged Weapon Projectile replay state

Existing Attack command JSON is unchanged. When a Weapon has a
Projectile reference, a tagged PendingAreaImpact holds attacker,
aimed target and intercept target IDs, damage, armor penetration,
flight endpoints/ticks and collision settings. All pending shot
fields are included in authoritative state digest calculation
only when such shots exist. A moving target can dodge a flight.
Windows/Linux compare weapon_projectile_fingerprint.txt byte-for-byte,
including a serialized replay roundtrip and tamper rejection.
Old Weapon and other map fingerprints are preserved.
