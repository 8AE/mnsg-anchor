# Shared Impact boss fights

This branch implements a shared mech with independent native reticles, owner-executed
controls, boss checkpoints and an owner-only stream of native render objects.
The user confirmed Kashiwagi, Taisamba 2 and the post-defeat transition repair.
Balberra and D'Etoile now have separate native AI profiles and reuse the shared
player, rendering and audio paths in story scenes and boss rush. See
[the final-boss implementation notes](impact-last-bosses-sync.md) for native
findings and validation limits. The new bosses have not had a fresh live game run here.

## Corrections from the recordings and native code

The large colored squares were the native full-screen damage-flash model,
`0x480099F0` / `D_8020A2D0_6356B0`, rendered with opaque alpha. They were not
reticles. The first native cursor is constructed in `func_801CCA5C_5F7E3C`:

| Property | Native cursor |
| --- | --- |
| Model | `0x4800A1E0` |
| Material | `D_8020A728_635B08` |
| File | `0x4A8` |
| Scale | `0.2` on all axes |
| Render mode | `9` |
| Aim | Native X/Y rotations, maintained by `func_801CCF74_5F8354` |

Each remote participant gets a gold (`#FFCC40`) copy of that mesh, while the local
reticle keeps its native cyan/red colors. The material overrides RGB but preserves
TEXEL0 alpha in both combiner cycles, keeping the native transparent silhouette.
Remote cursor movement settles over three native ticks; local aiming is unchanged.
Cursors can overlap when players aim at the same point.
Tasks are retained across pauses and hidden when inactive; stale task handles are
rejected using native links, object ownership and callback identity.

The supplied September 11 logs show a shared election and health/ammunition
convergence in stage `608` (`0x260`). The September 12 recording shows the opaque
squares and differing boss/attack presentation. These files are diagnostic
evidence; historical instructions in the previous handoff are not current requests.

## Shared native behavior

| Selector | Boss | Root ID | Root initializer |
| --- | --- | --- | --- |
| 1 | Kashiwagi | `0x50` | `func_801E4800_60FBE0` |
| 2 | Taisamba 2 | `0x5A` | `func_801EF2E0_61A6C0` |
| 3 | Balberra | `0x78` | `func_80200200_62B5E0` |
| 4 | D'Etoile | `0x64` | `func_801FAEB0_626290` |

`D_8020EED0_63A2B0` is the dedicated file_13 battle-state pointer. The boss selector
is the halfword at system `+0x3ADF4`. Valid native stages are `0x21C..0x223`,
`0x239..0x23C`, and title-menu boss-rush stages `0x260..0x263`.
Every selector has a complete profile. In boss rush the live stage must match
the selector exactly (`0x260`/1, `0x261`/2, `0x262`/3, `0x263`/4). Story handoffs
also fence by selector and root ID, even when the scene and task address stay the same.

The version-8 checkpoint contains **293 u32 words**, including:

- Boss HP, shared ammunition, mech HP, combat pause and battle clock.
- Boss pose, animation/phase IDs, collision dimensions and masked private scalars.
- Ten cockpit/mech poses, with camera position, target, FOV and aim kept local.
- Arena displacement/origin, ascent, collision mode, grapple meter, action/timer,
  latch and defeat state where used by the boss.
- Scalar carry references for Taisamba and D'Etoile; native pointers stay local.
- Balberra's 12 weapon/pod records, including separate HP, attack/death callbacks,
  timers, sliding offsets and model/frame state.
- D'Etoile's shield deflection state and afterimage/glow controls. Its 64-entry
  native pose-history ring and write index stay local.

Callbacks, clips and asset recipes use fixed IDs resolved against the locally
loaded USA overlay. No foreign task pointer, object pointer or callback address is
accepted from the network. Phase and private-state adoption waits until both
native introductions have cleared the combat-pause gate. Root/auxiliary state is
applied before the native scheduler. Each player's native camera follows their own
aim. Visible mech poses settle over three native ticks after the update; the local
simulation pose is restored before the next AI/collision update and on promotion.

All participants can submit combat buttons through `func_801D7670_602A50`. The
owner merges held buttons and consumes one participant's ordered press per native
tick. Local presses take priority when there is no pending native action. A pending
action retains its initiator; competing remote edges stay queued, and physical
edges arriving during a pending remote action are queued with local identity. Each
remote press keeps its own aim through release heartbeats and deferred native
action dispatch. Guard holds, guided-fist axes and follow-up R presses are supplied
to their separate native readers. Physical pads and camera aim are restored after
each scoped call. Both dispatchers bind the initiator to every new native attack
task, including task-only laser/barrage attacks. Children inherit that identity
through native allocation at `80034B58`; task reset at `80034A10` removes stale
bindings before address reuse. The scheduler's `8001481C` boundary supplies the
initiator's aim for that task's pre/update/post callbacks, then restores local aim
before another task or scheduler return. This covers repeated punch/guard aim reads
as well as delayed hook launch. Fresh cursor controls update ongoing aim; when stale,
the accepted aim is retained. Pausing clears inputs but preserves live attack
identity; authority/context changes clear it. At most 128 attack tasks are tracked.
A separate bounded FIFO delivers A/B mashes to the native boss reeling reader,
where it can advance the meter after the mech interpreter has restored the pads.
Remote-only mashes use the native neutral-stick increment; simultaneous local
input retains its native stick bonus. Latch loss and expiry discard old mashes.
Input packets name the intended owner and term; old terms,
reconnects and stale controls are discarded.

Only owner-side collisions change shared HP. The native health leaf patches gate
before subtraction, preserving the return value as well as stored HP; restoring
HP in a return hook allowed callers to observe a false lethal result. Balberra
has a separate collision/HP path at `func_8020451C_62F8FC`, and root `+0xAC` must
also be updated because `func_8020407C_62F45C` republishes it to the HUD pool.
Follower collisions do not enqueue a second damage delta. Remote on-foot models
are suppressed while a live Impact root is bound.

## Boss and attack rendering

The native task at shared state `+0x1C4` owns the world/HUD subtree. Its dynamic
HUD display lists stay local. The renderer traverses the scheduler's flat,
depth-first task links and model chains, capturing visible objects with verified
asset/material recipes. Display-only replicas are children of the battle manager
at `+0x1BC`, outside that source subtree. The list head at `D_8006D328_6DF28`
is valid with a self/tail backlink; ordinary nodes require reciprocal links.
Rejecting the manager when it is the head disables the entire visual channel.

[impact-visual-recipes.json](impact-visual-recipes.json) records 130 model/file
recipes and 38 material IDs from native constructors, clip records and bounded
missile/body tables. Each 30-word row carries a generation-and-slot handle, recipe/material IDs,
primitive and environment colors, renderer tags, mode, visibility, pose/frame and six
file-relative segment bindings. Bindings must resolve within resident native
assets; animated texture offsets and color alpha are preserved. File-zero slots
may contain valid animated textures written by native code; capture retains
resolvable asset-relative bases and ignores stale, unresolvable unbound slots. Unknown recipes,
unsupported bindings and unloaded assets do not become arbitrary native pointers.

A follower uses only complete owner frames. Six bounded snapshots supply a
50–150 ms adaptive interpolation buffer for position, scale, wrapped native angles
and forward animation frames. Small frames can arrive at up to 30 Hz; full
four-page frames keep the 8 Hz rate. Rendering advances each native tick within
the same sustained page budget. The render clock never moves backward. Native task/object lifetimes keep stable
handles when traversal order changes. Spawn, despawn, asset changes and teleports
snap; packet gaps hold the latest pose, then expire at 750 ms. No attack path is
extrapolated. Pause, owner/session/visit changes and reconnect clear history.

Replicas are prepared at entry to `func_8000AA00_B600`, immediately before native
draw-list collection. The Impact handler runs simulation **inside** `80002040`;
its return hooks exchange snapshots afterward. Preparing replicas at scheduler
return made `visuals_tick` hide them again before drawing. Preparation must also
precede `80016950`, because model births and render-mode changes must be present
when `8000AA00` collects objects into its mode buckets.

Native matching objects are hidden
for rendering and restored before the next native update/capture. Replica tasks
have no native AI or collision callbacks. When the authoritative graph is active,
the old ballistic Ryo replay is hidden to avoid a second visual for the same shot.
On expiry, pause, owner change or unavailable assets, replicas are hidden and the
native presentation is restored. Pools retain live tasks rather than allocating
again on every pause. Traversal and object counts are bounded; overflow falls
back to native presentation instead of publishing or hiding a partial graph.

Both players need this build. Protocol v7 adds Taisamba arena and attachment state;
the shared implementation retains all eight native grappling-chain
models, delayed hook launch aim and the separate boss reeling input readers.
It retains protocol v5's guided-fist input, uppercut model and owner-driven audio,
local cameras and generation handles. Older Impact packets are rejected.

The September 13 follow-up also fixes unused native texture bindings and adds
74 verified combat cues plus six loop-stop pairs. Taisamba adds nine encounter-specific intro/victory cues. See the full recording/native
audit and limitations in [impact-attacks-analysis-2026-09-13.md](impact-attacks-analysis-2026-09-13.md).

## Network contract

Identity is `(clientId, interactionSession, impactVisit, stage, boss)`. Ordinary
on-foot `playerEpoch` is not required by Impact. The generic boss core receives an
internal visit-to-epoch mapping; there is no on-foot epoch on Impact wire packets.
Native binding supplies a positive visit even when boss rush starts without a
loaded save. Python does not invent a fallback visit.

| Channel | Route and durability | Cadence and bound |
| --- | --- | --- |
| `MNSG_IMPACT` | Fixed `targetTeamId: "default"`; transient coordinator/checkpoints | Shared boss-core coalescing; state <=4096 bytes, packet limit 8 KiB |
| `MNSG_IMPACT_PLAYER` | Same team; transient cursors and control/shot edges | Cursor 10 Hz; event ceiling 30 Hz; <=1024 bytes including NUL; 16 peers; 32 queued edges |
| `MNSG_IMPACT_VISUAL` | Same team, current owner only; latest render frame | Up to 30 frames/s for small frames; 32 pages/s sustained, 64 objects, 4 pages/frame, <=6144 bytes/page including NUL |
| `MNSG_IMPACT_SOUND` | Same team, current owner only; transient combat cues and refreshing loop state | At most 30 packets/s, 8 cues/packet, <=1024 bytes including NUL; 32 queued cues, 750 ms transport expiry |

All packets use compact JSON with one trailing NUL and authoritative root
`clientId`. Hot channels bypass the generic gameplay FIFO and are never placed in
Anchor's durable queue. Render assembly retains one incomplete frame and one
complete frame, expires after 0.75 seconds, and fences stage, boss, owner session,
owner visit, authority term, encounter tuple and sequence. A failed send abandons
that frame; the next cadence slot sends the newest full frame.

The theoretical visual ceiling is 192 KiB/s from the single owner, amplified by
`(team participants - 1)` on server egress: 192 KiB/s for two clients, 576 KiB/s for
four, or 2.81 MiB/s for sixteen, excluding other channels. It is not a statement
of measured server capacity. A representative 64-object localhost frame used
6,759 bytes total over four pages. No public Anchor service was load-tested.

The sound channel adds at most 30 KiB/s owner traffic, with the same team fan-out.
All participants need protocol 8. `mod.toml` packages the coordinator, player,
visual and sound modules.

## Verification and remaining limits

Current results are recorded in [impact-last-bosses-validation-2026-09-13.json](impact-last-bosses-validation-2026-09-13.json).

- `UBSAN=1 tests/run_impact_sync.sh`: 20 native fixture runs and 47 Python tests,
  covering all four profiles, shared controls, native render recipes, two-color
  materials, story/rush transitions, defeat, carry promotion and malformed inputs.
- Full Python suite: 277 tests run, 276 passed and one skipped.
- Seven isolated three-client TCP runs against local Anchor cover all four rush
  fights and Balberra/D'Etoile story stages. They verify real framing/routing,
  owner election, checkpoints, controls, native asset rows, dual colors and audio.
- Release/debug MIPS builds, package contents and installed package hashes are
  recorded with the validation evidence. Static packet audit reports no findings.

These checks do not execute the actual two-client native boss fights. The user
confirmed the preceding Kashiwagi/Taisamba build; Balberra and D'Etoile still need
live gameplay verification. Historical recordings and earlier validation files
refer to their respective earlier builds.
