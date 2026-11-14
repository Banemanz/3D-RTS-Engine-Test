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
