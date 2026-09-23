# Balberra and D'Etoile synchronization

The September 13 18:25 logs show the working Kashiwagi/Taisamba implementation
followed by native-only Balberra on stage 610 (`0x262`) and D'Etoile on 611
(`0x263`). This change replaces that temporary restriction with complete boss
profiles. The logs establish the baseline; they do not test this new build.

## Organization and behavior

| File | Responsibility |
| --- | --- |
| `src/bosses/impact/anchor_impact_balberra.c` | Binding, 33 root phases, root HP, 12 weapon/pod checkpoints, part destruction/attack timers and boss cues |
| `src/bosses/impact/anchor_impact_detoile.c` | Binding, 76 root phases, 14 animation clips, shield state, carry dependencies, grapple/release hooks, defeat and boss cues |
| `src/bosses/impact/anchor_impact_boss.c` | Common arena/action/lifecycle fields, auxiliary scalar packing, local child traversal and scalar carry views; carry helpers also serve Taisamba |
| `src/bosses/impact/anchor_impact_native.c` | Checkpoint timing, root readiness, scene/selector/task fences, local-camera separation and mech smoothing |
| `src/bosses/impact/anchor_impact_players.c` | Shared owner-executed controls, native reticles, per-attack aim, initiating-player stick input and ordered hook mashes |
| `src/bosses/impact/anchor_impact_visuals.c` / `src/bosses/impact/anchor_impact_sounds.c` | Native render/audio streams, including two-color glow materials and boss-scoped sound reset |

Kashiwagi and Taisamba retain their phase/clip IDs and gameplay behavior. Each
boss has its own C profile; the old Balberra/D'Etoile catalog fallbacks were moved
into those profiles. Shared player actions, remote gold cursors, independent
cameras and presentation smoothing are available for every fight.

Balberra's root HP lives at task `+0xAC`, and the native updater republishes it
to the shared HUD pool. Its gun, beam, deck cannons and six indexed rocket tubes
also have independent health and persistent destruction callbacks. Checkpoints
reconcile those tasks by semantic slot, callback and local graph membership,
including timers, flash, sliding-pod offsets and animation. Three tubes share
ID `0x71`, so their native `+0x90` index is part of identity. Native parent task
and model pointers at `+0xB4/+0xB8` are preserved. Drone deploy/return, projectile
movement, explosions and surviving parts use the shared owner render stream.

D'Etoile's shield consumes its command pulse during the native update. The
checkpoint therefore includes the shield's resulting normal/deflected state,
timer, collision mode, rotation and attachment offsets. Its afterimage and glow
controls are captured, while the local 64-entry pose-history ring and write
index stay local. Root `+0xDC` is excluded from the scalar mask: carry callbacks
receive a temporary local view containing only the carrying attack's pose and
velocity. A promoted owner without that attack resumes the native idle setup.
Release aiming uses the attack initiator; the opening grapple also uses that
player's stick, and subsequent reeling consumes the shared ordered A/B presses.
Balberra has no native A/B reeling callback; `801FED3C` belongs to D'Etoile.

Native effects made with `func_8000E030_EC30` use both primitive and environment
RGBA. Their two alpha values now survive capture, interpolation and native
reconstruction. The existing 130 model/file recipes remain native game assets;
no replacement sprites or external artwork were introduced.

## Story, boss rush and lifecycle

Rush selectors 1..4 must match stages `0x260..0x263` exactly. Story battle stages
use the existing `0x21C..0x223` and `0x239..0x23C` recognition, with live selector,
root ID and visit checks. They do not depend on ordinary on-foot room epochs.

Story Balberra calls `802085B0` during its defeat sequence. That routine increments
the selector and clears the completion flag before the existing root becomes
D'Etoile through `801FAEB0`. Tests cover the same scene, same root address and
same advertised visit, including the interval where selector 4 is live but the
old task ID is still `0x78`. Late Balberra checkpoints and all old presentation
channels are rejected. D'Etoile's final defeat also retires safely when returning
to the rush menu or leaving the story fight. Combat phase changes still wait for
the local native introduction; defeat signaling crosses that pause separately.

## Wire contract and verification

All clients need **protocol 8**. Root snapshots are **293 u32 words**, including a
128-word per-boss extension. Visual rows are **30 words**, with the environment
color appended after the existing segment pairs. The previous 197-word root and
29-word row layouts are rejected. Checkpoints remain below 4096 bytes; root
packets retain the 8 KiB bound. Visuals retain 64 objects, four pages, 6 KiB/page
and 32 pages/second. For a team of N players, owner render egress is bounded by
192 KiB/s times N-1 recipients. All channels retain transient team routing,
root sender identity, ordering, expiration and bounded queues; nothing is queued
offline. The tests use only a disposable server bound to loopback.

[Validation evidence](impact-last-bosses-validation-2026-09-13.json) records:

- 20 native fixture runs under undefined-behavior sanitizer and 47 focused Python tests.
- 277 full Python tests: 276 passed, one skipped.
- Seven real TCP runs with three clients, covering all four rush stages,
  Balberra story stage `0x222`, and D'Etoile on `0x222` and `0x223`.
- Release/debug MIPS builds, packaged Python equality, archive CRC and hashes.
- Local native API documentation generation, build and 34 tests.

The host fixtures execute the production synchronization code with native imports
stubbed; they are not a game emulator. TCP checks verify transport with synthetic
native records. No fresh live two-client gameplay run of these new bosses was
performed. Native afterimage/projectile population and cinematic timing still
need that runtime verification; the renderer retains its existing 64-object cap.
