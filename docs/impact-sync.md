# Shared Impact boss fights

This branch implements a shared mech with independent native reticles, owner-executed
controls, boss checkpoints and an owner-only stream of native render objects.
The user's September 12 two-player test confirmed the reticles and exposed a
shared camera and stepped remote movement. The refinements below have automated
coverage; they have not yet been observed in a fresh live game session.

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
| 2 | Thaisamba | `0x5A` | `func_801EF2E0_61A6C0` |
| 3 | Balberra | `0x78` | `func_80200200_62B5E0` |
| 4 | D'Etoile | `0x64` | `func_801FAEB0_626290` |

`D_8020EED0_63A2B0` is the dedicated file_13 battle-state pointer. The boss selector
is the halfword at system `+0x3ADF4`. Valid native stages are `0x21C..0x223`,
`0x239..0x23C`, and title-menu boss-rush stage `0x260`.

The version-4 checkpoint contains **165 u32 words**, including:

- Boss HP, shared ammunition, mech HP, combat pause and battle clock.
- Boss root pose, animation clip ID, phase callback ID, collision dimensions,
  render flags and an explicit per-boss mask of scalar task fields.
- Ten cockpit/mech object poses. Camera position, target and FOV are not sent.
- Thaisamba's auxiliary angle, countdown and bubble gate; Balberra/D'Etoile's
  auxiliary attack flags. Native pointer lists in these blocks remain local.

Callbacks, clips and asset recipes use fixed IDs resolved against the locally
loaded USA overlay. No foreign task pointer, object pointer or callback address is
accepted from the network. Phase and private-state adoption waits until both
native introductions have cleared the combat-pause gate. Root/auxiliary state is
applied before the native scheduler. Each player's native camera follows their own
aim. Visible mech poses settle over three native ticks after the update; the local
simulation pose is restored before the next AI/collision update and on promotion.

All participants can submit combat buttons through `func_801D7670_602A50`. The
owner merges held buttons and once-only press edges and executes the native
interpreter. Local analog aiming remains independent. A local press takes aim
priority; otherwise the lowest client ID with a new press supplies the temporary
shooting-cursor aim. Conflicting simultaneous inputs therefore resolve through
one native mech action, not separate per-player mechs. Input packets name the
intended owner and term; old terms, reconnects and stale held input are discarded.
The pad and temporary aim values are restored after the interpreter.

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
at `+0x1BC`, outside that source subtree.

[impact-visual-recipes.json](impact-visual-recipes.json) records 121 model/file
recipes and 38 material IDs from native constructors, clip records and bounded
missile/body tables. Each 29-word row carries a generation-and-slot handle, recipe/material IDs,
primitive/environment color, renderer tags, mode, visibility, pose/frame and six
file-relative segment bindings. Bindings must resolve within resident native
assets; animated texture offsets and color alpha are preserved. Unknown recipes,
unsupported bindings and unloaded assets do not become arbitrary native pointers.

A follower uses only complete owner frames. Six bounded snapshots supply a
150 ms interpolation buffer for position, scale, wrapped native angles and forward
animation frames. Completed packets still arrive at 8 Hz; rendering advances each
native tick without increasing traffic. Native task/object lifetimes keep stable
handles when traversal order changes. Spawn, despawn, asset changes and teleports
snap; packet gaps hold the latest pose, then expire at 750 ms. No attack path is
extrapolated. Pause, owner/session/visit changes and reconnect clear history.

Native matching objects are hidden
for rendering and restored before the next native update/capture. Replica tasks
have no native AI or collision callbacks. When the authoritative graph is active,
the old ballistic Ryo replay is hidden to avoid a second visual for the same shot.
On expiry, pause, owner change or unavailable assets, replicas are hidden and the
native presentation is restored. Pools retain live tasks rather than allocating
again on every pause. Traversal and object counts are bounded; overflow falls
back to native presentation instead of publishing or hiding a partial graph.

Both players need this build: protocol v4 removes seven camera words and uses
generation handles. Older Impact packets are rejected rather than misread.

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
| `MNSG_IMPACT_VISUAL` | Same team, current owner only; latest render frame | At most 8 frames/s, 64 objects, 4 pages/frame, 16 rows/page, <=6144 bytes/page including NUL |

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

Protocol version 3 is incompatible with the previous Impact checkpoint/player
format; all participants need the updated package. `mod.toml` includes both
`anchor_impact.py` and `anchor_impact_visual.py`.

## Verification and remaining limits

- `UBSAN=1 tests/run_impact_sync.sh`: production C codecs, native checkpoint
  application, damage return semantics, Balberra HP and auxiliary fields,
  native cursor recipe, input arbitration, shot capture, texture bounds, alpha,
  render visibility restoration, retained allocation over 50 pause cycles,
  plus Python transport/election/paging regressions.
- Full Python suite: 264 tests run, one skipped; all executed tests passed.
- `tools/test_impact_anchor_local.py --port 43393`: three real TCP clients against
  disposable loopback Anchor revision `bf7b43c10b19428ceba54772c7bae3abca44a345`;
  four-page/64-object delivery, sender exclusion, cross-team isolation, cursor
  and control transport. The temporary server changes only its listener address.
- Release/debug MIPS builds and package import/integrity checks are recorded in
  [impact-polish-validation-2026-09-12.json](impact-polish-validation-2026-09-12.json). Static packet audit reports zero errors/warnings, but only
  inventories literal sends; the channel-specific tests cover generic sends.

The supplied 16:45 recording is the user's two-player test of the preceding build.
The independent-camera, tint and interpolation refinements above have not had a
fresh live game run. The earlier automation restriction and baseline checks remain
recorded in [impact-validation-2026-09-12.json](impact-validation-2026-09-12.json).

Followers still maintain local native graphs for introductions, scene flow and
future authority takeover. Checkpoints cover root/auxiliary scalar state; they do
not serialize every child's private AI state or reconstruct every in-flight
collision actor during host migration. Unsupported render objects remain local,
and a 64-object overflow falls back to local presentation. Sound timing, all four
bosses' full attack coverage, late entry, defeat/scene transitions and exact
mid-attack host migration remain unverified in live gameplay. These limits must
remain visible when describing the branch; automated build/transport success is
not proof of complete boss synchronization.
