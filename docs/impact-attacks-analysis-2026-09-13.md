# Impact attack and sound follow-up — September 13, 2026

Implemented on `impact_syncing`, based on the supplied solo fight recording/log and
USA native code. Automated checks and local transport checks pass. This is not
certification of a fresh two-player game run.

## What the attachments establish

The 171.64-second recording shows a complete Kashiwagi fight: introduction,
repeated punches/guard and leg attacks, boss movement and missile phases,
a special attack sequence, and defeat. The approximate missile/special/defeat
segments are 90–110, 114–130, and 140–165 seconds. These are visual observations,
not frame-accurate input traces. The MOV contains a video stream and **no audio
stream**. The 1,083-line log shows stage 608, boss selector 1, boss HP 2000 to 0,
mech HP 500 to 260, and ammunition 200 to 182. Its startup/preload warnings do not
identify a gameplay crash; it contains no per-button or per-sound trace.

A solo recording cannot demonstrate which network operation was missing at a
remote client. The reported failures were traced through the existing transport,
native input dispatcher, native projectile collision readers, model constructors
and final sound queue. Attachment contents were treated as evidence, not as task
instructions.

## Missing or broken paths and the changes

| Path | Code finding | Implemented change |
| --- | --- | --- |
| Attacking boss projectiles | Remote presses were ORed together, all were consumed even when a local press won aim priority, and release/aim heartbeats overwrote the aim associated with an unconsumed press. The native dispatcher can also defer a requested attack to another tick. | Bounded FIFO preserves each press and its aim, consumes one participant's edge per native tick, and retains aim for deferred native actions. Both controller roles receive the selected edge. The authority still creates the real native attack and collider, which can trigger the boss projectile's native contact handling. |
| Punches, uppercuts, kicks and guard visuals | Capture attempted to resolve stale pointers in native texture slots whose file ID was zero. Native binding intentionally skips such slots. The uppercut's extended model `0x18000248` was absent from the recipe list. | Skip inactive bindings, keep validation for active bindings, and append recipe 122 for the verified uppercut phase. Existing native limb recipes remain in use. |
| Short attack visibility and projectile response | All render frames waited for the full 8 Hz budget, even when they contained only a few objects, with a fixed 150 ms render delay. | Small frames can publish up to 30 Hz. Cadence is charged by page count against the same sustained 32-page/s budget. Interpolation delay adapts from about 50 to 150 ms; the render clock never rewinds. Large frames retain the 8 Hz budget. |
| Holding guard | The native release reader runs after the interpreter restores the physical controller state. It therefore could not see remote holds. | Merge remote guard holds only around that native reader, then restore the physical pads. |
| Guided fist and follow-up R press | Special-fist callbacks independently read analog axes and R-button presses outside the main interpreter. | Add bounded kind-3 axis samples; the guided fist uses its initiating participant's axes in its native callback. Forward the selected remote R edge through the separate native follow-up reader. Restore axes/buttons afterward. |
| Boss and Impact sounds | The existing on-foot sound capture excludes the dedicated Impact task subtree. Followers therefore depended on speculative local AI producing the same cues. | Owner-only, encounter-scoped sound channel for 74 verified native effect IDs and six matching loop stops. Follower speculative copies of those cues are suppressed before the native sound queue. Replay retains the native pan/volume command. |
| Loop cleanup | Replaying only a looping effect's start, or losing its stop, can leave it playing. | Carry final loop state with each sound packet and repeat it every 250 ms. Reconcile starts/stops, deduplicate sequences, and stop remote loops on expiry, pause, exit or authority changes. |

The native interception readers use real contacts at task `+0x34/+0x38`.
`FUN_801D36CC` reads incoming attack type/damage; Kashiwagi missile callbacks such
as `FUN_801EA534` and `FUN_801EA900` react to contacts through the native path.
No damage is fabricated from a remote visual, and no follower replica receives
collision or AI callbacks. This fixes identified input delivery/aim defects; it
does not implement historical collision rewind for high network latency.

## Native evidence

- `FUN_801D0260`: world/HUD and native input task creation.
- `FUN_801D7670`, `FUN_801D7BC8`: separate held/pressed reads and deferred action
  fields `+0x141/+0x145`; actual native attack dispatch.
- `FUN_801D8878`, `FUN_801D9474`, `FUN_801DD5D8`: punch, uppercut and leg models.
- `FUN_801D9730`: uppercut transition to `0x18000248`, file `0x4A8`.
- `FUN_80035964`, `FUN_80014218`: reused model initialization and file-zero
  segment-binding behavior.
- `FUN_801DD394`, `FUN_801D9940`, `FUN_801DAEC8`: separate guard, analog and
  follow-up button readers. Disassembly confirms 9940's `(task, object)` ABI;
  its decompiler's extra double argument is not used as a hook signature.
- `FUN_80038C30`: disassembly confirms cue u16, pan/volume u8, eight queue slots,
  low-16-bit duplicate suppression and `D_801C09C9` mutation guard. The patch
  preserves native behavior outside the scoped Impact effect stream.

Verified loop pairs are `0122/8122`, `0130/8130`, `0152/8152`, `023E/823E`,
`0240/8240`, and `0241/8241`. Music/sequence commands remain local.

## Protocol and limits

All participants need **protocol 5** packages. The root checkpoint stays at 165
words; cameras remain local. The new analog event and appended model recipe are
version-gated together with audio.

| Channel | Route | Durability | Cadence and storage |
| --- | --- | --- | --- |
| `MNSG_IMPACT_PLAYER` | Current team | Transient ordered events and latest cursor | Existing 30 Hz event ceiling, four events/packet, 1 KiB including NUL, 32 queued events; axes use the same channel. |
| `MNSG_IMPACT_VISUAL` | Current team, owner only | Latest atomic frames | Up to 30 Hz for one page, 8 Hz for four; 32 pages/s sustained, at most four 6 KiB pages per burst; six-frame history, 750 ms expiry. |
| `MNSG_IMPACT_SOUND` | Current team, owner only | Transient cues plus refreshing loop state | Up to 30 packets/s, eight cues/packet, 1 KiB including NUL, 32 queued cues, 750 ms transport expiry, four native replays/tick. |

All receivers fence owner, interaction session, Impact visit, stage, boss,
encounter tuple, term and sequence. Nothing uses Anchor's durable/offline queue.
The visual ceiling is 192 KiB/s sustained plus a bounded frame burst; audio adds
at most 30 KiB/s. Server egress multiplies those rates by the other participants
on the team. No public server was tested or stressed.

## Validation and remaining limits

- Six native Impact fixture groups pass under UBSan, plus 40 focused Python tests.
  Cases include preserved press aim after release, simultaneous inputs, deferred
  native actions, guard holds, analog steering, R follow-up, stale texture slots,
  native queue limits/deduplication, follower suppression and loop cleanup.
- Full Python suite: 270 tests, 269 passed and one pre-existing skip.
- Existing on-foot native sound regression passes under UBSan.
- Three real TCP clients against disposable loopback Anchor revision
  `bf7b43c10b19428ceba54772c7bae3abca44a345`: atomic 64-object frame delivery,
  cursor/buttons/analog input, audio start/stop, deduplication, sender exclusion
  and different-team isolation pass. Server stopped afterward.
- Release/debug builds and archive/source checks pass. Existing unrelated
  `notif_ensure_init` unused-function warning remains.
- Native reference corrections were applied in the sibling documentation
  checkout, regenerated, built, tested and checked through local browser search
  and the changed pages. They were not published.

Fresh two-player gameplay, audible timing/balance, every boss's full encounter,
late entry, and mid-attack authority migration remain unverified. Audio currently
retains the authority's computed pan/volume; it does not recompute spatial sound
for each independent camera. Checkpoints do not reconstruct every private child
AI/collision state on host migration. Unknown render objects and over-capacity
frames retain the existing native fallback.

Package hashes and machine-readable results:
[impact-attacks-validation-2026-09-13.json](impact-attacks-validation-2026-09-13.json).
