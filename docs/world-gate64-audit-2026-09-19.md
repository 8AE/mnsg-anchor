# Mt. Fear weapon obstacle

File_64's `0x325` root is the two-piece weapon-activated obstacle in room
`0x14B` (Mt. Fear). The sole placed root is roster index16 at
(-523,-230,-17); its initializer assigns the actual model position
(-515,-230,-49), yaw719 and scale0.15. This adapter is world protocol12,
kind10. It is a local implementation milestone, not universal coverage or
fresh two-client gameplay confirmation.

## Native lifecycle

The US ROM SHA-256 is
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
File_64 occupies ROM `0x724C10` through `0x725C30` (exclusive).
The Ghidra program is `/codex-temp/world_file_64.elf`; runtime addresses
overlap other overlays, so ROM suffixes are required.

| Callback | Verified behavior |
| --- | --- |
| `func_08000000_724C10` | Read save0xA1 and delete if complete; otherwise bind model0x24F slot1, collision and parent-owned child at task+D4. Child allocation uses `func_80220410_5DB8E0`; it does not use the common child-registration helper. |
| `func_080001BC_724DCC` | Test contact bit1 and a local hitter. Types0x16/0x17/0x23/0x24 qualify when one of the four native weapon words at save-base+0xA4/+0xA8/+0xAC/+0xB0 is1 or2. Accepted hits start a60-count wait; other hits start7. |
| `func_0800033C_724F4C` | Decrement the signed halfword timer; old zero returns to idle. |
| `func_0800037C_724F8C` | After the60-count wait, allocate and bind a local camera; cache local camera output at task+DC. |
| `func_0800042C_72503C` | Task byte+D0 order0→2→1→3→4 means camera alignment, shaking, rising, splitting and camera return. The timer increments at the end of the callback. |
| `func_08000AD8_7256E8` | Commit save0xA1, restore local camera/control and delete the root and descendants. |
| `func_08000B9C_7257AC` / `func_08000C0C_72581C` | Initialize the child as model0x24F slot3 at scale0.15, then leave it under parent motion. |
| `func_08000DE4_7259F4` / `func_08000C18_725828` | Local rise and split particles; no loot, health or currency grant. |

Shake alternates X by one unit at timer residues0/2 modulo4 and changes phase
at60. Rise switches root slot2, sets child draw bit0x20, and adds20 Y units
and64 yaw units per update through timer48. Split moves the pieces in opposite
sine/cosine directions: three units at20–30, plus two units from30 onward.
Timer30 therefore moves five units. Roll changes by ±5 from30; timer90 ends
the split. The native split effect allocation can fail at20, skipping that
update's movement while still advancing the timer.

Caller disassembly verifies `func_80220410_5DB8E0`'s fifth float Z argument
at stack+0x10 and sixth unsigned-halfword yaw at+0x14. The task allocator
installs common pre/post callbacks and links the child into the native task
hierarchy. Native root deletion removes descendants through that hierarchy.

## Checkpoint and authority

| Words | Meaning |
| --- | --- |
| 0–3 | Placed index, entity0x325, kind10, actual local camera-sequence ownership. Received busy never acquires a camera. |
| 4–9 /17–22 | Root/child XYZ in hundredths and three angles modulo1024. |
| 10–13 | Root static slot1/2, signed timer, explicit phase1–10, cleared collision/motion bit0x800000. |
| 23–25 | Fixed scale150 thousandths, child draw bit0x20 and completion. |
| 38,48,49 | Pause, local task incarnation and native receipt. |
| All remaining | Reserved zero. |

Wire phases are idle, rejected-hit delay, accepted-hit wait, camera alignment,
shake, rise, split, camera return, local cleanup and completed. Idle and
rejected-hit delay have zero authority progress; subsequent phases increase
monotonically. Timers do not select authority. The row has no input channel.

The adapter consumes the native hit locally but waits for transport ownership
confirmation before acquiring controls or running the native camera sequence.
The native60-update delay remains camera-free, allowing competing hit claims
to settle before the confirmed winner acquires controls at timer zero.
If another local sequence took the native control lock during that delay,
the gate waits at zero until the lock clears. It never overwrites that lock.
The chosen local activator runs its own native camera callbacks. An observer
applies only scalar geometry and predicts within the current motion phase.
It waits at phase boundaries so it cannot overtake the native camera owner.
On owner departure it advances remaining motion and commits completion without
allocating a camera. A live local sequence must finish its own cleanup before
accepting replacement state; simultaneous activation still needs game testing.
An active camera owner retains authority through pause; observers hold its
paused checkpoint until it resumes or leaves. A camera-free owner can hand
off to an unpaused peer. This avoids a second simulation completing while the
paused owner still holds native camera/control resources.

Late application validates native timer/slot combinations, then restores both
pieces atomically. Model0x24F files, slots1–3, File_64 and common wave0x152
must be resident. Child reconstruction uses the verified child initializer,
not the root initializer that also creates collision and unchecked children.
Pending births are not duplicated. Task/object/generation identities protect
against pool reuse; child reuse clears only the parent's owned+D4 link.
Completion is a canonical row handled before any removed-task dereference.
The existing durable item-sync channel now carries save0xA1 as `wl_mt_gate`,
so completion also reaches teammates who were outside the room and future
team entrants. An already-instantiated idle copy retires on that flag; an
active local camera ignores the external completion until its own cleanup.

Reconstruction suppresses effects whose boundary already passed. Local
particles can play at a future boundary; native collision, camera, control,
dialogue pointers and resources are never serialized. Disconnecting an
observer mid-sequence retains a camera-free continuation. Paused/restoring
objects also suppress common-post motion.
The scheduler's initial control-clear preserves a paused session's suspension,
but does not suspend a disconnected observer again after reset released it.
The disconnect regression exercises this control-clear on every subsequent
update; reverting the fix makes its completion assertion fail.

## Networking and validation

The existing `MNSG_WORLD` route stays quiet, team-scoped and transient, with
no offline queue. Receivers enforce room0x14B, session, version and roster
signature. Changed checkpoints use5Hz, steady refresh1Hz, semantic edges a
50ms minimum; the50-word row and existing packet/part limits remain unchanged.
Ownership confirmation is a local Python-to-C reply, not additional fan-out.
Completion uses the existing team `SET_FLAG`/snapshot lifecycle; it is a
one-time monotone progression bit, not an offline stream of motion rows.
The isolated loopback probe observed a606-byte maximum packet and verified
sender exclusion, team isolation, bootstrap receipt, completion and handoff.

The production-C harness exercises hit deferral, local camera ownership,
reconstruction failures, pending child initialization, pool reuse, pause,
camera-free handoff, exact split boundary motion, invalid checkpoint rejection
and removed-root completion. The core harness checks typed room registration,
self ownership confirmation and null-root completion. C/Python schema tests
compare per-field bounds and invalid mutations. See [validation results](world-gate64-validation-2026-09-19.json).

Fresh two-client tests remain necessary: simultaneous hits, pause during the
activator's camera sequence, joins at each motion boundary, owner departure,
collision under both pieces, room reload, and completed-save entry.
