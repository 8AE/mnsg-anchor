# Taisamba 2 synchronization

The September 13 implementation extends the user-confirmed Kashiwagi baseline.
Boss behavior is separated from the reusable multiplayer machinery:

| File | Responsibility |
| --- | --- |
| `src/anchor_impact_kashiwagi.c` | Kashiwagi root binding, 79 phase IDs, 13 animation clips, private scalar mask, reeling hook |
| `src/anchor_impact_taisamba.c` | Taisamba root binding, 129 phase IDs, 16 animation clips, scalar mask, arena/ascent state, local attachment dependencies, reeling hooks and additional sounds |
| `src/anchor_impact_boss.c` | Common boss-profile dispatch and native pointer-range checks |
| `src/anchor_impact_native.c` | Shared root checkpoint capture, validation, scheduler application and mech-pose smoothing |
| `src/anchor_impact_players.c` | Shared input arbitration, per-attack initiator/aim, guided attacks, hook mashes and native reticles |
| `src/anchor_impact_visuals.c` / `src/anchor_impact_sounds.c` | Shared authoritative render/audio streams |
| `src/utils/anchor_impact_catalog.c` | Common ID lookup plus the pre-existing Balberra/D'Etoile catalogs |

No second copy of player input, cursor, rendering or transport logic is introduced.
Kashiwagi's existing phase IDs, clip IDs and scalar mask are preserved. Both clients
must use protocol **7** because the checkpoint grew from 165 to **197 words**.

## Native findings and changes

The Extra Options implementation at `mnsg-extra-options/src/hyper_taisamba.c`
provided a bounded map of 78 combat callbacks and 11 projectile/weapon callbacks.
It was used as a research map; its repeated Hyper AI execution was not copied.
Native evidence targets the USA decompressed ROM SHA1
`6ea0ed71032ce08fc2745f412d84936382197494`, overlay `file_13_raw.bin`.
The main ROM's overlapping `801Fxxxx` analysis is another overlay and must not be
used as evidence for these boss functions.

Five missing root callbacks are appended without renumbering existing IDs:
`801F05BC`, `801F0A0C`, `801F1690`, `801F16E0`, and `801F38DC`.
The last launches the returning weapon and changes to `801F3950`.
The shared phase lookup now supports IDs above 127.

Taisamba's owner executes native ground/air decisions, movement, attacks,
projectile interception, incoming-hit reactions and damage. Followers adopt its
phase, animation, velocity, timers, attack counters, collision dimensions/flags,
collision mode/mask/type, health and shared ammunition at checkpoint boundaries.
Native descendant objects use the existing owner visual stream, including
ballistic/staged projectiles, the returning weapon, whirlwind and bubble trails.
All 16 Taisamba animation clips and its weapon/whirlwind models resolve in the
existing native asset allowlist; no replacement artwork is introduced.

The extended state includes:

- Arena displacement `state+178..180` and origin `state+184..18C`. `801F1788`
  requests upward motion; `801D0C70` integrates and clears it; `801F71DC` uses
  arena height to choose ground/air behavior. The `+2C5` ascent request is synced.
- Shared hook meter `state+70`, action byte `+14E`, and latch presence. Latch
  presence resolves to the receiver's own root task. Cameras, cursor aim and FOV
  remain local. Aim/cursor positions, all native attacks and A/B reeling input
  use the existing shared player paths.
- The combo-grab timer `+14F` and defeat gate `+2C4`. The defeat gate is applied
  alongside HP even during the outro pause, so adopting the owner's later phase
  cannot skip the native victory signal. Grab/release readers `801F5710/801F58AC`
  borrow aim from the tracked collision limb's initiator, then restore local aim.
- Auxiliary `EF30+4/+8/+C` angle, bubble countdown and gate. The auxiliary task
  list at `+0` stays local.

The old Taisamba scalar mask incorrectly included root `+9C` and `+DC`.
`801F3950` dereferences `+9C` as a returning-weapon task; `801F6660` dereferences
`+DC` as a carrying attack. Those addresses are now excluded and rejected on the
wire. For the weapon wait, the owner publishes an active boolean. For carrying,
it publishes the attachment's position, velocity and yaw. Paired hooks expose
read-only local scalar records during these native readers and restore the local
task pointers afterward. The records are never scheduled, rendered or collided.
The carrier checkpoint requires valid attachment data. On owner promotion with
no local carrying attack, the scheduler boundary resumes native attack selection
before executing attachment math. That interrupted carry is not recreated as a
damage-bearing attack.

Whirlwind sound `0130/8130`, attack/voice effects and bubble voices use the shared
owner audio stream. Nine additional intro/victory cues are allowed only for
Taisamba. Audio task scoping also recognizes the mod's extended native task pool.

## Transport and validation

All four existing channels retain their team route, transient delivery, cadence,
identity fences and bounded queues. Checkpoints remain below 4096 bytes even with
197 maximal u32 values; complete packets retain the 8 KiB ceiling. Visuals retain
64 objects / four pages, up to 32 pages per second; no public server was tested.
The unchanged scope is `(clientId, interactionSession, impactVisit, stage, boss)`.
Native validation rejects invalid phase IDs, nonfinite arena/attachment floats,
foreign pointer words, invalid flags/meters and nonzero reserved extension words
before modifying health or world state.

Checks performed:

- UBSan host fixtures run the production boss profiles, catalog and checkpoint
  implementation. They cover phase/clip round trips, arena adoption, callback
  pointer dependencies, pause/scene boundaries and ownership recovery.
- Shared player fixtures run under selectors 1 and 2, including remote attack
  aim, guided controls and ordered hook mashes. Native rendering fixtures cover
  Taisamba root, returning-weapon and whirlwind file/model combinations, alpha,
  segment bounds, draw ordering and retained allocation. The fixture renderer
  does not render N64 pixels or execute the game's full AI.
- Full Python regression suite and local Anchor tests cover checkpoints,
  inputs, render pages, sounds, protocol incompatibility and scope rejection.
  The initial three-client socket checks exercised Kashiwagi stage `0x260`,
  Taisamba selector 2 on that same stage, and Taisamba story stage `0x221`.
  The selector-2 / stage-`0x260` case did not represent native Taisamba boss rush;
  the correction below covers its actual stage `0x261`.
- Release/debug MIPS packages are built. Exact results and hashes are recorded
  in `impact-taisamba-validation-2026-09-13.json`.

A fresh two-client in-game Taisamba fight has not been run here. Intro/outro
timing, complete visual composition and a whole fight through defeat therefore
remain runtime verification items. Existing visual overflow/expiry behavior
falls back to local presentation; it does not publish a partial owner frame.

## Boss-rush activation correction

Both supplied 17:10 player logs show Kashiwagi syncing on stage 608 (`0x260`),
followed by `bind encounter=2 stage=609` and no further active sync or captures.
The native and Python stage allowlists incorrectly accepted only `0x260` for
boss rush, so the Taisamba root bound but failed the readiness gate. The damage
authority path contained the same stage restriction.

Native `FUN_800370B0` enters stage `0x260`; `FUN_80037000` increments the stage
through `0x263`, then returns to `0x25F`. The shared C predicate in
`include/utils/anchor_impact_stage.h` now governs native readiness, checkpoint
validation, and damage authority. Python accepts the same four-stage range for
native stage recognition. The transition repair below separately restricts active
sync to the completed profiles. Story invitation eligibility stays separate.

Regression fixtures use the actual `0x260` to `0x261` transition with no loaded
save, new fallback visits, stale-checkpoint rejection, owner election and
authoritative HP. The local socket harness defaults to each boss's native
boss-rush stage. Protocol 7 and the packet layout are unchanged. See
`impact-rush-validation-2026-09-13.json` for corrected validation and packages.

## Post-defeat transition repair

The user confirmed that Taisamba now synchronizes. Both supplied 17:23 logs show
its HP reaching zero and its native victory sequence finishing. They then show
`bind encounter=3 stage=610`, `sync active=1`, and the first Balberra waiting
callback (`func_802003E0_62B7C0`). Neither log contains an exception stack. The
available macOS crash reports predate these launches, so they do not establish
the faulting instruction for this incident.

The earlier rush-stage fix unintentionally enabled the legacy Balberra path.
Only Kashiwagi and Taisamba have complete boss profiles. Native binding/readiness
now requires one of those profiles; the Python root, player and visual/sound
channels enforce the same boundary. Later bosses retain native gameplay, with no
shared health gate, remote input or replicas. Recognized stage IDs remain intact.

A native regression also reproduced a queued Taisamba defeat snapshot writing
zero HP after the stage had advanced. The scheduler now revalidates queued state
at application time, cached presentation cannot cross stages, and a rush stage
must match its boss selector even before the next initializer replaces the root.
The root bridge retires all cursor/input, visual and sound queues immediately on
exit or scope change. Late packets cannot restore the outgoing fight's state.

Automated fixtures cover recycled root addresses, staggered exits, native HP
fallback, late packets and re-entry into Kashiwagi/Taisamba. This removes the
unintended next-boss sync path and the reproduced transition-state bug; without a
matching exception stack or a new live run, it does not establish that the crash
itself has been reproduced or conclusively resolved. Current checks and package
hashes are in `impact-defeat-validation-2026-09-13.json`.
