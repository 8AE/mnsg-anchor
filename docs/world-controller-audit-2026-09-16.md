# Remaining room-controller audit

This is an implementation worklist, not a universal-coverage certificate.
The inventory examined 800 US-ROM room metadata slots, 374 metadata records,
333 actor-data waves, 292 nonempty rosters and 255 distinct placed entity types.
The corrected three-list inventory totals 3,888 sources and a maximum of 57
per room. SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.

## Completed in the continuation pass

- Native resident lists at room metadata+0 now append after normal and sorted
  partition sources. This adds the two `0x1FC` platforms in room `0x30` and five
  `0xFC`/`0xFE` enemies in room `0x131` to their existing adapters. These pointers
  are resident; the native manager does not resolve this list through the room
  wave. Source descriptors identify them even though task+0x70 remains null.
  The production roster harness verifies stable ordering, source deduplication,
  missing-wave retries, capacity, registration and dead-spawn suppression.
- File_59: 22 native continuations, including town arrivals and pickpocket
  approach/return/escape. Portable private scalars, local route-pointer binding,
  native face selectors, local blinking, ownership handoff and late reconstruction.
- File_30 entity 0x356: indexed rotor and wait state, integer-valued float cycle
  1 through 8, native two-unit angle step and 30-update wait.
- File_30 entity 0x3B4: rider trigger, outbound motion, endpoint wait, return,
  rearm; native Z endpoints -404 and 396, speeds -4/+4, 60-update waits.
- World protocol 4: 48 placed words / 83 dynamic words, bounded transient team
  packets and continuation-edge cadence. Older clients are ineligible.
- File_50 placed entity 0x226, modes 0/1: native hit, pressed pose, flag-set,
  spark-boundary and settled checkpoints, local spark replay guard, validated
  definition-selected flag outputs. These are all modes in the placed inventory.
- File_30 entity 0x324 variants 0–11 and 0x326 variants 0–7: flag-driven
  activation, native motion and fixed endpoints. One-way phases survive stale
  idle snapshots, pause handoff and culling/reload without replaying triggers.
  A save-aware 0x326 constructor's endpoint yields to a live move; its initial
  flag result is recorded separately from an observed completion.
- Protocol 5 adds a pause-presence bitmap and prefers the furthest one-way
  controller phase. Row lengths and all packet/cadence bounds stay unchanged.
- File_30 0x32A is static tinted geometry: continuation 0800435C only calls
  the local tint helper. It has no moving puzzle state to stream.

## Native findings that constrain the remaining work

The production-classifier audit on 2026-09-19 executes the actual C roster
classifier and regular-enemy predicate against all3,888 big-endian ROM
placements. Its per-family results are in
`world-placement-coverage-2026-09-19.json`; regenerate with
`tools/audit_world_coverage.py`. Initially820 placements matched placed-world
adapters,742 regular enemies and3 coupled bridge members;2,323 placements
across175 families need other evidence. That last group includes runtime NPC
promotion, bosses, static scenery and save-derived pickups as well as actual
gaps, so it is not an unsupported-object count. Conversely, a classifier match
alone is not proof of a complete synchronized lifecycle.

This broader audit identifies gameplay families missing from the earlier
scene-heavy worklist. Protocol14 now covers all46 File24 entity0x3CA spike-floor
placements in rooms0x32/0x34/0x3A, including their timed extension/retraction,
pause, late entry and culling/reload. See [the spike audit](world-spike-audit-2026-09-19.md).
The same protocol adds all six File24 entity0x1AA jump ropes in room0x41,
including full16-bit rotation, pause and initialized restore; see
[the rope audit](world-rope-audit-2026-09-19.md). The current classifier counts
are884 placed-world,742 regular-enemy,3 bridge members and2,259 needing other
evidence after protocol15 adds all12 File40 tops/platforms. See the
[File40 audit](world-file40-audit-2026-09-19.md). The three File30 equipment
idle pickups also reconcile their shared collection flags and local shine;
see [the equipment audit](world-equipment-audit-2026-09-19.md). File30 trap
families, File40 bomb blocks and other mechanisms still require explicit
native lifecycle adapters. These remain in scope alongside the coupled
scenes below. Dynamic births require a separate producer/child inventory.

`func_80023DF0_249F0`, `func_80023E40_24A40` and
`func_80023E94_24A94` set, clear and read the temporary bank at 0x80168E90.
`func_80023EE4_24AE4` clears 100 bytes from actor-manager initialization.
The sibling bank starts at 0x80168EF8 and has separate helpers. Shared room
flags cannot be implemented by copying both banks wholesale: their consumers
also include shop dialogue, minigames and local scripted scenes.

The remaining work needs controller-specific field and callback adapters,
shared child identities, and semantic flag transitions. Local input/camera and
personal rewards must retain their existing boundaries. Save-flag replication
alone does not reconstruct an already-running controller graph.

| File / entities | Native room scope | Work remaining |
| --- | --- | --- |
| File_30 `0x19D` slicer emitters/blades | `0xAB`/`0xAC` | Both root and child expose common HP1 incoming-hit processing and can produce native death loot. Root countdown/emission, stable blade identities, flight checkpoints, shared kill/loot arbitration and cause-specific removal must be handled together. Motion alone is insufficient. |
| File_30 `0x3EF` random spawner | `0x91` | Shared random birth cadence and current child reconstruction. Child model12F moves with velocity and turns toward a native target pointer; that pointer must be rebuilt locally. |
| File_40 `0x365`/`0x366` | `0x3E`/`0x3F` | Protocol15 implemented; all12 placements classified, native/codec/loopback checks passed. Fresh two-client riding/contact validation remains outstanding. |
| File_40 `0x1A9` and separate `0x3CB` graph | `0x65`/`0x66` for1A9 | Bomb-block proximity/fuse and surviving-hit explosions allocate children, while immediately lethal common damage follows a different path. Cause-specific lifecycle and child identities remain unimplemented. |
| File_30 0x1B9/0x1BA/0x1BB/0x1BF and coupled 0x3D3 | 0x31 | Implemented protocol 9 atomic crane/pad/reward checkpoint, per-client pad aggregation and local child reconstruction. Camera, fades and scenes remain local. Native/transport checks are documented in [the crane audit](world-crane-audit-2026-09-19.md); fresh two-client gameplay remains outstanding. |
| File_30 0x354 / File_32 0xFC | 0xB2 | Protocol 10 adds retained shutter phase, timer and emission identity, overlapping route-57 robots, current-state reconstruction and kill arbitration. Native death runs after commit; only the arbiter emits loot. See the [shutter audit](world-shutter-audit-2026-09-19.md). Fresh two-client gameplay remains outstanding. |
| File_30 0x3D0 | 0x35 | Adapter implemented in protocol 8: carry/throw, break/retry, completion and stable reward slots. Fresh two-client game validation remains open. |
| File_44 0x1F4 gated dynamic invocation | Placed records are static | All 33 placed records have the gate disabled. Audit scripted/dynamic invocation of the separate gated branch before classifying that branch. The 0x228/0x1FE placed adapters are implemented below. |
| File_50 unplaced timed/rearming routines | Unreachable through the native entity initializer | Disassembly confirms the initializer selects the one-way path for byte +D4 equal 0/1, then compares the entire big-endian word +D4 against 2/3. Either word necessarily has byte +D4 equal zero and already took the one-way branch. No alternate entry into these subgraphs occurs within File_50; arbitrary direct cross-overlay callback injection is outside this classification. |
| File_51 0x240/0x2D0/0x311 | 0x15E | Protocol11 couples both guard routes, temp0/1, gate animation and blocker lifetime. Established checkpoints override fresh save1 constructor poses; native dialogue, shadows and sequence-lock ownership remain local. See [the bridge audit](world-bridge-audit-2026-09-19.md). Fresh two-client gameplay remains outstanding. |
| File_53 0x316 | 0x153 | Complete native graph and dialogue-owned save0x17 commit traced. All visible children inherit entity0x316 and need distinct role identities; NPC catch-up and private mesh initialization need dedicated handling. Shared visual checkpoints and safe scene handoff remain unimplemented; see [the File53 audit](world-file53-audit-2026-09-19.md). |
| File_58 0x2A0 and File_46 0x1B0 | 0x155 | File_58 is a local camera/scene controller, but its room flags also drive File_46's visible Koryuta body and wave graph. That graph remains a coverage gap. Save0x19F is a local proximity/camera token and must not be copied to bypass its prerequisites. See [the quest audit](world-quest-native-audit-2026-09-19.md). |
| File_62 0x315/0x3D6 | 0x16A, 0x182 | Protocol13 implements the separate hit-reactive0x3D6 container's repeatable opening/closing and nested Silver Doll birth, descent, reconstruction and collection. See [the Doll audit](world-doll-audit-2026-09-19.md). The Kihachi0x315 scene graph still needs its own shared presentation adapter; see [the quest audit](world-quest-native-audit-2026-09-19.md). Fresh two-client gameplay remains outstanding. |
| File_64 0x325 | 0x14B | Protocol12 implements the two-piece weapon obstacle's hit arbitration, shared motion and completion, child reconstruction and camera-free replica/handoff continuation. See [the obstacle audit](world-gate64-audit-2026-09-19.md). Fresh two-client gameplay remains outstanding. |
| File_67 0x335, File_70 0x344 | 0x14C, 0x14D, 0x158 | Native graphs classified as local travel/entrance cinematics, including their finite visual children. File67 requires all four shared Miracle items and retires its controller if any is missing; its verified local departure-choice/cue0x6B/0x6C are now excluded from item replication, including legacy queued input. File70's durable completion0xC4 accompanies existing0xC3; remote completion leaves an active local scene to perform its own cleanup. See [the travel flag audit](world-travel-flags-audit-2026-09-19.md) and [quest audit](world-quest-native-audit-2026-09-19.md). Fresh two-client gameplay remains outstanding. |
| File_74 0x35C, File_75 0x35D | 0xC1 | All61 overlay functions traced: linked Gorgeous Music Castle cinematics, local camera/player/control/fade work and staged visible children. Temporary0/1 and save0x75/0x76 coordinate local scene prerequisites; not a general room puzzle. Shared-visible-state classification remains open; see [the quest audit](world-quest-native-audit-2026-09-19.md). |
| Other placed and dynamic families | All inventoried rooms | Finish per-family classification against the native entrypoints; distinguish static scenery from actors with simulation, interactions, gameplay children or private continuations. |

Useful local research files (not repository inputs):
`/tmp/mnsg-world-sync/room-actors.json`, `inventory.py`, `elf.py`,
`flag-calls.json`, `flag_calls.py`. The flag-call scan is a conservative MIPS
constant lookback aid; branch/delay-slot ambiguity requires native disassembly.
It is not proof that every reader/writer was found or an argument is constant.

Ghidra overlay identity matters: File_30, File_43, File_44 and File_59 all map
at 0x08000000. Use the matching ELF and full ROM-suffixed symbol. The main ROM
program has stale overlapping function boundaries in some regions.

Next native entrypoints verified during the switch pass:

- File_44 0x1F4 enters at 080001B8. Optional gating reads byte +D4 and
  temporary-bit index byte +D5; all examined placed definitions have +D4 zero.
  The common helper 08000000 selects local static geometry. Its Ghidra
  `processEntry` prototype loses argument types; check caller disassembly.
- File_44 0x228 enters at 08002600, waits for temporary bit `(byte +D8)-1`
  when +D8 is nonzero, then 080026D0 caches XYZ at +C8/+CA/+CC and subtracts
  0x8000 from the four +D0/+D2/+D4/+D6 halfwords. Continue at 08002788,
  08002820 and 080028B4; do not decode these parameters twice on reconstruction.
- File_44 0x1FE enters at 08002E28. +D8 bit 2 gates on save flag 0x1A4.
  It decodes the same four halfwords, binds model 0x1F9, creates a child with
  continuation 08002DBC and stores that local child pointer at +9C. The parent
  then continues at 08003074. The child retains its origin as floats +D0/+D4/+D8.
- File_30 0x3D0 enters at 08006B40, binds model 0x1C1, initializes XYZ to
  (170,-100,135), collision mask +96 to 0x1E1, +DC cooldown to zero, and saves
  prior VX/VZ as floats at +D0/+D4. Continue the collision/puzzle trace at 08006C18.

File_44 placed adapters now implement all 19 `0x228` records and 17 `0x1FE`
records. The latter retains and reconstructs its local child collider, shared
origin/movement, shake/melt/wait/reform timers and packed tint. Parent and child
pool reuse detach stale links; culling preserves a paused checkpoint and reset
does not destroy native children. Native imports are guarded by resident model
and code waves, and constructor retry does not decode parameters twice.
Wire phases are 60–63 and 64–68 respectively, under world protocol 6.

The subsequent protocol 7 pass adds established presence and native application
receipts for cyclic actor bootstrap. A lower-ID newcomer must apply the current
published checkpoint before taking ownership, including when the incumbent is
paused. Receipts are tied to the local task incarnation and invalidated by
resource/application failure. The initial server roster gates establishment;
former-owner presence does not make an older cached full row current again.
Host/native and loopback evidence is in `world-bootstrap-validation-2026-09-17.json`.

No fresh two-client in-game validation has been performed for this pass.

The File_50 initializer reachability check was completed on 2026-09-19.
`func_08000000_70C820` executes LBU at `0800006C`; its branch at `0800007C`
executes LW at `08000080` only when that byte differs from zero and one.
The subsequent word comparisons against 2 and 3 cannot succeed on the
big-endian US-ROM. This holds for dynamically constructed definitions too,
not only the four placed definitions. File_50 references to `080003B8` are
the unreachable constructor selection and its own rearm loop at `0800062C`;
`08000644` is referenced only by the unreachable constructor selection.
These routines are not ordinary gameplay coverage gaps for entity `0x226`.
This does not classify the separate File_44 gated branch.

`0x3D0` native findings used by the implemented protocol 8 adapter:

- `08006C18_6C6368` tests `D_8015C5E4`, the current-health word, not the
  character selector. Its zero-health branch changes task flags and skips the
  rolling/puzzle update. Preserve this local native gate when choosing a live
  simulator; do not synchronize player health.
- Its completion region is `-4 < X < 18`, `103 < Z < 120`, `Y == 8`.
  Completion zeroes velocity, sets status bit `0x80000`, and schedules
  `08006F48_6C6698` (shrink). That callback eventually binds animation 1;
  `08007040_6C6790` waits for completion and binds animation 2, and
  `08007094_6C67E4` then starts a 200-count reward phase.
- `080070E4_6C6834` emits a native child when unsigned timer `% 10 == 0`,
  including the zero boundary before decrement. It chooses initializer
  `80214314` for random values below 80 and `802141AC` otherwise, gives each
  child upward velocity 5 plus random horizontal velocity, and eventually
  schedules the empty `080072C4_6C6A14` continuation. Replicas must not replay
  this reward producer; use the existing child identity/claim path.
- The reward helper `8021DDE8` calls `802171A8`, which reaches the already
  tracked `80218C28` parent/child setup. Thus this producer can use the shared
  loot allocator after its parent is given typed world authority.
- `8021DA1C`, called from the rolling phase with fifth argument zero, queries
  local geometry and can reverse X/Z velocity. Its other fifth-argument branch
  has additional contact work. Caller disassembly confirms fifth argument zero. `801E54A8` sets
  status `0x8000` and the local player work +0x8C held pointer; `801E55A0`
  clears both. `8021C518` sets `0x1000/0x2000` from geometry results. The
  countdown is an unsigned halfword, loaded with LHU. `8021A328` clears
  source +0x70 before proximity deletion; an actual throw break retains it.
  Native cooldown tasks wait for leaving a larger proximity box, not a timer.

The shared puzzle deliberately retains completion across native proximity
culling during an occupied room visit. Its stable reward slots represent one
shared completion, regardless of how many local throw attempts preceded it.
The completed-reload native harness now checks that re-placement restores phase
74 and does not produce a second reward sequence. A real scope reset releases
the completion. This is shared-world retention, rather than a claim that the
unmodified native spawner cannot re-place a completed actor. Roster mismatches
are rejected before dynamic identities are merged.

Review also questioned nested common-post calls. The inspected `80218F30`
body calls `80218E7C`, performs deletion and schedules a cooldown post with
`80035244`; it does not recursively invoke that post. No nested execution was
observed. This remains static call-path evidence, not native runtime testing.

The completed-reload review led to a separate reproduced reconnect defect:
disconnecting after a break cleared the retained row but left the broken latch
and attempt number on the roster entry. A later native placement inherited the
latch and never simulated. Sync reset and fresh registration now clear that
state unless a retained checkpoint explicitly supplies it. The native regression
failed before the change and passes with it.
