# File_30 timed shutter and dynamic enemy audit

Native audit and protocol-10 implementation. Explicit Ghidra programs:
`world_file_30.elf`, `world_file_32.elf` and `mnsg.us.0.z64`. ROM SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.

Room `0xB2` contains two placed entity `0x354` definitions, category 0. Their
12-byte payloads are `000000000000000000000000` and
`000000010000000000000000`. Native placement copies definition words +4/+8/+C
to task +D0/+D4/+D8. Only the second shutter emits enemies.

| Native callback | Verified behavior |
| --- | --- |
| `func_08005590_6C4CE0` | Set model +5E to 23F, flags +60 to 802006E0, bind animation E at 0.02, mesh byte +6C=5, all scales 0.1 and signed timer +8A=60. Entity +5C remains 354. |
| `func_08005624_6C4D74` | Post-decrement timer; old zero enables playback, plays sound124 and advances. Nominal 60 covers old values60 through0. |
| `func_08005684_6C4DD4` | Wait for object +7C mask0x02; timer90, flags `(old & ~1) | 01000000`, rebind animation E at0.05, advance. |
| `func_080056FC_6C4E4C` | At old timer30 and task+D0==1, emit the native effect and enemyFC. Post-decrement; old zero plays sound124, enables playback and advances. |
| `func_080057D0_6C4F20` | Wait for animation completion; clear playback/reverse bits, bind animation E at0.05, timer60 and return to05624. |

These continuations have only the stated scheduler-pointer xrefs in File_30;
the initializer's external xref is its entity table entry. No temporary/save
flag helper occurs in this graph. D0 is immutable placement configuration.

The emission callback calls `func_8021A4E4_5D59B4(actor,1.0f)`, which creates a
randomized category8 effect, then `func_80217360_5D2830(actor,0xFC,6)`. A successful
enemy receives D0=39, D4=1 and object XYZ=0. Replica replay of this callback
duplicates gameplay births; it cannot serve as a presentation-only update.

The recovered spawn ABI is
`func_80217360_5D2830(parent, unsigned short entity, unsigned char category)`.
It uses `80219CA0` category6, allocates the entry from `PTR_ARRAY_802287BC[FC]`,
copies the parent transform, calls `80218C28`, sets +5C/+5E=FC and increments the
live count. The common child setup explicitly sets +70=0, category +77=6 and
health +8D=1. It installs common pre/post and copies generation/presentation
fields. Neither the shutter nor its child stores a durable cross-client identity.

The gaps found before this implementation:

- `enemy_sync.c` recognizes FC as a regular enemy, but its registration uses
  exact placed-source identity through `80218A54`; finalizer recovery uses +70.
  This dynamic child bypasses that registration and has +70=0.
- `anchor_world.c` classifies the shutter by entity354, not its model23F.
  Its roster kind remains zero; door synchronization for entity23F does not apply.
- `anchor_world_dynamic.c` observes the child birth and parent index, but its
  current classes are NPC/loot/hazard. FC remains unclassified and is discarded.

The implementation must pair a typed, retained shutter phase/timer/animation
checkpoint with dynamic enemy identity `(room visit, shutter roster index,
emission generation)`. Only the simulator executes056FC. Death/removal belongs
to each emission and must not use the shutter's or any static enemy's death bit.
Late entrants need the live child set; repeated cycles cannot reuse one identity.
Existing enemy action/health handling should be reused only after this lifecycle
is represented and reconstruction is verified against File_32.

## File_32 robot lifecycle

`func_08001EA4_6D0F84` binds model FB while retaining entity FC. D0=39 means
route 57 through unsigned halfword D2. D4=1 skips ground placement and chooses
flags 002E8261. `80219E70` adds local shadow bit 08000000 only if allocation
succeeds. The initializer binds animation 0, attack geometry (4,40,0), death
selectors 3/4, and local display-list selectors before scheduling the sole AI
`func_08001FF4_6D10D4`. With source pointer +70 zero, the initial placement-effect
helper does nothing. This setup can safely initialize a new proxy while it is
the current native scheduler task; the route checkpoint must be applied before
the first AI invocation.

Route 57 teleports to origin+(70,-15,180), then uses opcode23 to walk through
(70,-15,140), (280,-15,140), (280,-15,0), (310,-15,0). Opcode0D returns one and
the AI marks removal. There is no timeout and no shutter child-count guard;
overlapping generations are valid. Restore route C4, timer C6, origin C8/CA/CC,
substate CE, PC CF, facing AA, status bit400, transform and velocity. Pointer
9C must refer to the local actor. Target scratch is recomputed by the route.
Shadow, hitter, callback and display-list pointers never cross the wire.

Common child setup gives one HP at byte8D. The initializer never changes it.
`80218350` calls death function `80218548` directly at PC80218524 when health
reaches zero. A damage return hook is too late to prevent its side effects.
For this exact route actor, accepted ordinary damage has status80, a non-null
local hitter at38 and clear status1. The fast path uses status40000. The entry
hook defers these hits into a shared claim, retaining one HP, clearing pending
damage and applying native surviving-hit cooldown (status1, byte8C=60).

After the arbiter commits, fast native death runs once per local copy. Native
word64 bit8000 suppresses loot on non-committing peers. Only the arbiter rolls
the native coin/health drop; at most one child is created. Its shared parent
and ordinal follow the robot's shutter/emission identity, independent of kill
order. A route-ending removal does not execute damage or generate loot.

Fast death skips the ordinary fragment/sound branch. Committed kills therefore
play sound222 and allocate its two safe category8 fragment initializers
`802130C8`/`8021332C` through `802171A8`, copying scale and writing selector
bytes8E=3/8F=4 before their scheduled initialization. These callbacks take task
and object pointers, not selector arguments. Their non-attacking fragments
and short-lived smoke remain local presentation. Model FB clips3/4 must be
resident before creating them; a missing resource never triggers a binder call.
Coin/health children inherit entityFC and use model1 with clip4/3. High emission
ordinals use a separate parent/kind/room key plus full ordinal, preserving unique
drop identities beyond the original 16-bit packed loot window.

## Checkpoint contract

Placed kind8 is confined to entity354 in roomB2. Its 50 words retain pose,
animation E, signed timer17, phase18 (1–4), immutable emitter19, monotone
emission ordinal20, playback/reverse29 and pause38. Retained state restores
after local culling; only the simulator executes the emitting callback.

Dynamic kind6 retains each FC generation in the existing bounded 83-word
stream. Its canonical identity contains roster signature, room, parent and
emission ordinal. Phase42=15 selects the one allowed route callback; route32
must be57. Word43 carries the immunity byte, 44 distinguishes a committed kill
from route completion, and45 carries route status400. Claims use the existing
single arbiter lease and send-success gate. Reconstructed copies bind native
resources locally before restoring the route.

Host/native, transport and package checks are recorded separately. Fresh
two-client gameplay validation remains outstanding, including overlapping
robots, simultaneous hits, late entry, room culling and handoff during a kill.
