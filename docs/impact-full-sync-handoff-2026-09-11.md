# Impact full-sync handoff

> Historical investigation, superseded by [impact-sync.md](impact-sync.md).
> The cursor recipe recorded below was incorrect: `0x480099F0` is the damage
> flash. The corrected cursor is `0x4800A1E0` / `D_8020A728_635B08`, mode 9,
> scale 0.2, file 0x4A8. Historical work instructions are not current requests.

Date: 2026-09-11  
Branch: `impact_syncing`  
Starting HEAD: `994d51e`  
Repository: `/Users/ahmad/git/mnsg-recomp-example`

## Goal

Finish full multiplayer synchronization for all four Impact boss fights:

- one authority controls boss movement, phase, actions, and attack actors;
- all clients keep boss HP, Impact HP, and Ryo ammunition aligned;
- each client renders the other players' Impact reticles;
- each successful player Ryo shot appears on the other clients without applying duplicate damage.

The recording and pasted logs served as diagnostic evidence. They did not contain implementation instructions.

Attachments:

- `/Users/ahmad/Documents/Screenshots/Screen Recording 2026-09-11 at 17.34.09.mov`
- `/Users/ahmad/.codex/attachments/fd163a51-d596-45ad-adea-d59b82dc8088/pasted-text.txt`
- `/Users/ahmad/.codex/attachments/4ca82f18-d48d-42ed-b69a-66cd0378e08a/pasted-text.txt`

## Recording and log findings

The 39.43-second recording shows two clients in the same boss-rush encounter. Both logs report stage `608` (`0x260`), encounter selector `1`, boss task ID `0x50`, a live root, and the same election tuple. One client becomes role 1 and the other role 2.

The existing synchronization converges on boss HP, Impact HP, and Ryo ammunition. Recorded checkpoints include `2000/500/200`, `1990/480/186`, `1910/480/186`, `1905/480/180`, and `1825/420/180` for boss HP, Impact HP, and ammunition.

Boss presentation diverges after the fight starts. The two clients show different positions, distances, action poses, and explosion sequences. The logs also enter different late-fight `file_13` function groups.

Ryo shots remain local. Around 16 to 18 seconds and again around 29 seconds, one viewport shows orange projectiles that the other viewport lacks even though both ammunition counters converge.

Each viewport renders one native cyan/red reticle. Neither viewport shows a second player's reticle.

## State of the worktree

The branch started clean. Work from this session remains uncommitted and incomplete.

Modified files:

- `py/anchor_impact.py`
- `py/anchor_mnsg.py`
- `tests/test_impact_transport.py`

Untracked files:

- `include/anchor_impact_players.h`
- `include/utils/anchor_impact_players_codec.h`
- `src/utils/anchor_impact_players_codec.c`
- `tests/test_impact_players.py`

No native player renderer exists yet. No existing C bridge or frame hook calls the new Python player channel. The current worktree therefore does not implement remote reticles or shot rendering.

The test edits predate the request to skip tests. They are optional and can be discarded. No test or build result certifies the current worktree.

## Python work completed in the worktree

`py/anchor_impact.py` now contains a version 2 Impact coordinator and a new `ImpactPlayerTransport`.

The coordinator changes:

- extend the 14-word advertisement with `[stage, encounter]`;
- require exact stage and encounter matches during peer discovery and election;
- add `s` and `k` scope fields to each `MNSG_IMPACT` operation;
- reset cached leases, checkpoints, and pending work when the stage or encounter changes;
- correct the valid stage set to `0x21C..0x223`, `0x239..0x23C`, plus boss-rush stage `0x260`.

The new transient `MNSG_IMPACT_PLAYER` channel:

- coalesces cursor samples at a 10 Hz ceiling with visibility-edge bypass and a 0.75-second keepalive;
- carries bounded attack edges at up to 30 Hz;
- caps packets at 1,024 bytes including the NUL frame terminator;
- keeps at most 16 peer cursors, 32 queued attacks, four attacks per packet, and 16 received attacks per frame;
- expires stale cursors and attacks and rejects replayed or reordered sequences;
- uses team routing, `quiet: true`, and no durable Anchor queue.

`py/anchor_mnsg.py` registers the new hot packet type, dispatches incoming player packets outside the generic FIFO, resets its cache on reconnect/disconnect, and exposes `update_impact_players(...)`.

### Python work still required

The interrupted source revision left an ordinary `playerEpoch` dependency in the worktree. Boss-rush mode may not publish the normal on-foot player epoch, so this can prevent all Impact packets from being accepted.

Remove the wire `epoch` requirement from `ImpactTransport` and `ImpactPlayerTransport`. Use this player identity instead:

```text
(clientId, interactionSession, impactVisit, stage, encounter)
```

Use `interactionSession` as the second identity word returned to C. Keep the existing row widths:

```text
input  = {"c":[visible, originX,originY,originZ,targetX,targetY,targetZ],
          "a":[[eventSeq,kind,spawnX,spawnY,spawnZ,velX,velY,velZ], ...]}

output = {"accepted":lastAcceptedLocalEventSeq,
          "c":[[clientId,interactionSession,packetSeq,visible,6 cursor words], ...],
          "a":[[clientId,interactionSession,eventSeq,kind,6 shot words], ...]}
```

`BossTransport` still expects an epoch inside its hit/ack bookkeeping. Adapt only the Impact context so that its internal hit epoch uses the positive Impact visit. Do not restore a dependency on ordinary movement metadata.

Normalize a zero visit in both Impact bridge functions the same way. Native `anchor_impact_native_visit()` should supply a positive fallback, but the Python boundary should remain consistent.

## Transport bug found and addressed in the partial Python work

The original Impact transport omitted stage and encounter from advertisements and from most packet operations. Impact also bypasses ordinary room filtering. Two clients in different Impact stages could therefore elect against each other and leave one client stuck at role 0. Boss-rush clients on stage `0x260` but different encounter selectors could also share the wrong Python election or checkpoint.

The version 2 scope work fixes that design by carrying and checking stage and encounter before a packet changes leases, ownership, hits, or cached state. Preserve this part of the partial change.

## Existing native implementation

`include/anchor_impact_native.h` defines a five-word root snapshot:

1. boss HP from shared state `+0x60`;
2. Ryo ammunition from `+0x64`;
3. Impact HP from `+0x68`;
4. pause byte from `+0x2C0`;
5. encounter clock from `+0x2C8`.

`src/anchor_impact_native.c` captures all five words but applies only the three health/ammunition values. It leaves boss callbacks, transforms, animation, and attack actors under each client's local AI. The scheduler hook never holds follower AI. This matches the recording.

The current damage path already prevents a follower from changing shared boss HP directly. It restores the local HP, sends the delta to the authority, and lets the authority call the native damage function. Preserve that ownership path.

## Verified `file_13` layout

Ghidra program: `/codex-temp/file_13_raw.bin`  
Overlay base: `0x801CB460`  
Overlay end: `0x8020EED0`

Shared state: `D_8020EED0_63A2B0`

Shared-state fields:

| Offset | Meaning |
| --- | --- |
| `+0x04`, `+0x08` | Impact camera yaw/pitch values used by firing logic |
| `+0x0C` | local Impact/mech task used by native shot orientation |
| `+0x60` | boss HP |
| `+0x64` | Ryo ammunition |
| `+0x68` | Impact HP |
| `+0xA0/+0xA4/+0xA8` | reticle ray origin |
| `+0xAC/+0xB0/+0xB4` | reticle ray target |
| `+0x1BC` | common Impact battle-manager task |
| `+0x1D8` | boss root task |
| `+0x1E0` | boss model object |
| `+0x2C0` | combat pause byte |
| `+0x2C8` | encounter clock |

The common manager at `+0x1BC` owns the native camera, HUD, encounter controller, and reticle tasks for all four fights. Parent new render-only cursor and shot tasks to this manager.

Boss mapping:

| Encounter | Boss | Root initializer | Task ID | First root callback installed at task `+0x0C` |
| --- | --- | --- | --- | --- |
| 1 | Kashiwagi | `func_801E4800_60FBE0` | `0x50` | `func_801E493C_60FD1C` |
| 2 | Thaisamba | `func_801EF2E0_61A6C0` | `0x5A` | `func_801EF42C_61A80C` |
| 3 | Balberra | `func_80200200_62B5E0` | `0x78` | `func_802003E0_62B7C0` |
| 4 | D'Etoile | `func_801FAEB0_626290` | `0x64` | `func_801FB0CC_6264AC` |

`func_8003521C_35E1C` writes the current task callback at `+0x0C`. The scheduler invokes callbacks at task `+0x08`, `+0x0C`, and `+0x10` in that order. Treat the four callbacks in the table as root AI phase 1, not as post callbacks.

Root model fields use the standard object layout:

- position floats at `+0x08/+0x0C/+0x10`;
- rotation halfwords at `+0x14/+0x16/+0x18`;
- scale floats at `+0x1C/+0x20/+0x24`;
- animation frame float at `+0x28`.

The next pass must determine exact per-encounter phase tables, safe flag masks, numeric private fields, root velocity fields, and attack actor ownership. Do not serialize a raw callback pointer or copy unverified task words that may become pointers.

Also tighten `bound_live()` so each encounter accepts only its expected task ID. The current code accepts any of the four IDs.

## Verified remote-reticle design

Create each retained render-only cursor task with:

```c
func_80034E08_35A08(manager, remote_cursor_update, 0)
```

Attach the stock reticle object with `func_8000DBF0_E7F0`:

- model command: `0x480099F0`;
- material: `D_8020A2D0_6356B0`;
- rotations: `0x8000, 0x8000, 0x8000`;
- scale: `1.0f` on all axes;
- segment/file argument: `0x4A8`;
- object byte `+0x05`: `7`.

Call:

```c
func_801D32D4_5FE6B4(object, D_8020AFB0_636390, 0);
```

Create a colored native material with:

```c
func_8000DF10_EB10(task, material, r, g, b, a)
```

Store its returned display pointer in object `+0x30` with segment bits `0x60000000`. This helper uses task `+0xD0..+0xE4`; keep slot metadata in a mod-side array.

Compute cursor position as:

```text
origin + normalize(target - origin) * 240
```

Reject non-finite values and a zero-length ray. Hide inactive retained objects with object `+0x64` bit 0. Use client-ID-derived colors so two overlapping reticles remain distinguishable.

## Verified local-shot capture and remote visual replay

`func_801D7BC8_602FA8` dispatches local Impact actions. Action `0x46` checks ammunition, alternates a `-5/+5` muzzle side, calls `func_801DAFCC_6063AC`, plays sound `0x22D`, and decrements ammunition after a successful spawn. `func_801D8184_603564` uses the same shot constructor.

Capture successful native shots without guessing from ammunition:

1. Hook entry and return of `func_801DAFCC_6063AC` to bracket a native Ryo shot constructor.
2. Hook `func_8000E39C_EF9C(float speed, float x, float y, float z, void *task)`.
3. While the constructor bracket is active, require task `+0x0C == func_801DB200_6065E0`.
4. On the `func_8000E39C` return hook, capture object position `+0x08/+0x0C/+0x10` and task velocity `+0x70/+0x74/+0x78`.
5. Queue the event on the outer `func_801DAFCC` return only when step 4 captured a task. Allocation failures never reach `func_8000E39C`.

The six attack words therefore carry exact spawn XYZ and velocity XYZ. They do not carry cursor coordinates.

Render a received shot as a collision-free visual:

- parent a retained task to the manager at shared state `+0x1BC`;
- attach model `0x48009AC0` and material `D_8020A7D0_635BB0`;
- use rotations `0x8000`, scale `0.2f`, segment/file `0x4A8`, and object byte `+0x05 = 5`;
- keep task collision byte `+0x30`, hit pointer `+0x34`, and damage fields zero;
- update position from the transmitted velocity, apply native gravity `-0.004f` per frame, and retire at distance 1500 or a bounded age;
- reuse hidden tasks and objects instead of destroying them.

Do not call `func_801DB200_6065E0` for received shots. That native callback runs collision, boss damage, explosion, and hit effects and would duplicate gameplay.

The task constructors enqueue callbacks; they do not invoke them during construction. A temporary flag around `func_801DAFCC` cannot identify the created task through a later `func_801DB200` hook. The `func_8000E39C` bracket above captures the task at the correct point.

## Partial C codec

The untracked player codec defines:

- local cursor row width 7;
- local attack row width 8;
- remote cursor and attack row width 10;
- 16 peers and 16 attacks per frame;
- a 4,096-byte input JSON buffer and 8,192-byte output limit.

Review it before integration. The interrupted renderer implementation had not created `src/anchor_impact_players.c`, and no build checked the codec.

## Boss movement and AI work remaining

The root snapshot needs a pointer-free, per-encounter phase ID plus the fields that drive movement and visible action state. At minimum, verify and carry:

- root phase and phase timer;
- model position, rotation, scale, animation clip/frame/state;
- native movement velocity fields;
- masked root flags and colliders that phase code changes;
- numeric action-private fields needed to resume each phase;
- active boss attack actors or stable spawn events required for late join and authority handoff.

Build four allowlisted callback tables from the first callbacks listed above. Follow only transitions installed through `func_8003521C_35E1C` or `func_8003522C_35E2C`. Map callbacks to small phase IDs. Reject a checkpoint if its phase ID does not belong to its encounter.

The safest full-authority design matches Congo, Dharumanyo, and Tsurami:

- retain and restore the root callbacks around scheduler execution;
- apply queued checkpoints at the native pre-update boundary;
- hold follower combat AI after the native introduction has created every required root and child actor;
- reconstruct active boss attack actors with stable IDs, generation/birth ticks, bounded counts, and transforms;
- release held callbacks before reset, rebind, stage exit, terminal flow, or an authority transition.

Holding only the root before mapping child actors can suppress travelling boss attacks or strand the introduction. Applying phase and transform while local AI continues can reduce visible divergence, but it does not provide full authority and may duplicate random action actors. Keep the root unfrozen until the child graph and terminal paths are mapped.

Increase `ANCHOR_IMPACT_ROOT_WORDS` only after the field list is final. Increase the native state JSON buffer from 2,048 to at least 4,096 bytes and keep the full framed Anchor packet below 8 KiB.

## Integration points

Finish the player path in this order:

1. Complete the Python identity cleanup described above.
2. Add `anchor_impact_players_update(...)` to `include/anchor.h` and `src/anchor.c`; call Python `anchor_mnsg.update_impact_players(...)` and return an allocated JSON string.
3. Complete `src/anchor_impact_players.c` using the verified renderer and shot design.
4. Call `anchor_impact_players_tick(active && !paused)` from `anchor_impact_sync_frame()` after the main coordinator has established the current role and encounter.
5. Call `anchor_impact_players_reset()` on inactive, visit-change, bind-change, and disconnect paths. Deactivate retained visuals before clearing their slot metadata.
6. Keep player attack visuals independent from authority. Each sender publishes its own successful native shot; every other client creates one non-colliding visual.

Finish boss authority after the player path builds:

1. Complete the four phase tables and child-actor inventory in Ghidra.
2. Expand `AnchorImpactNativeSnapshot` and its strict codec.
3. Apply the authority checkpoint at scheduler pre-update.
4. Add actor reconstruction, terminal handling, and role-transition cleanup.
5. Update `docs/impact-sync.md` so it no longer claims five-word health-only sync or protocol version 1.

## Native task lifetime rules

A live task stores its next link at `+0x00`, backlink at `+0x04`, callback at `+0x0C`, hierarchy depth at `+0x20`, and generation at `+0x22`. Validate the callback identity, stored object, generation, backlink, and neighboring links before reusing a retained child.

Do not call `func_80034EF8_35AF8` on retained remote cursor or shot tasks. Parent them to the native Impact manager, hide inactive objects, and let the engine destroy the manager subtree once. Clear stale mod-side handles after a root or manager change.

## Validation requested by the user

The user asked to skip tests to conserve time and usage. Continue with source review, compilation, and package checks. Do not claim those checks prove multiplayer behavior.

The final acceptance check still requires two updated clients in the same encounter:

- both elect one authority without a role-0 stall;
- boss position, phase, animation, and boss attacks match;
- both remote reticles remain visible and distinguishable;
- one Ryo shot produces one native local projectile and one collision-free visual on each peer;
- boss HP, Impact HP, and ammunition remain aligned;
- join-in-progress and authority departure do not replay old shots or adopt another Impact encounter.

No fresh two-client certification occurred in this session.
