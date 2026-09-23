# Super Pass Bridge native graph

The protocol11 adapter is implemented in `src/world/anchor_world_bridge.c` and
integrated through the placed root in `src/world/anchor_world.c`. This audit records
its native boundaries; it is not a live multiplayer certificate. Raw evidence
is retained locally in `/tmp/mnsg-world-sync/file51-native-audit-2026-09-19.txt`
and `file51-restore-audit-2026-09-19.txt` in the same directory.

File_51 maps ROM `0x70D740..0x70DFF0` at `0x08000000`. Its placed room is
`0x15E`, Oedo's bridge to Musashi. ROM hash matches the resident/shutter audits.
Actor-data file `0x408` begins at ROM `0x12D9DE0`.

| Entity | Native initializer | Placed definition |
| --- | --- | --- |
| `0x2D0` guard | `func_080002D8_70DA18` | Two actors at (-431,0,29)/(-451,0,29), yaw0x200; task D4 low halfword is dialogue0x10E/0x110; H+D8 is0/1. Model0x2CC, files0x280/0x158, clips0–2. |
| `0x240` opening presentation | `func_08000600_70DD40` | (-441,0,97), yaw0x200; H+D8=1, H+DA=0. Model0x240, files0x1FA/0x16F, clip0, scale0.1010000035. |
| `0x311` blocker geometry | `func_080006F8_70DE38` | Source(0,0,0); initializer sets object(0,0,-9), scale1. Model0x311, file0x2AE, slot0. |

All are placed. File_51 creates no children, rewards, loot or damage tasks.

Save0 is the Lord's Super Pass; save1 records the guards opening the bridge.
File_51 reads these flags but does not write them. Guard dialogue resources
0x10E and0x110, file0x60, contain the native dialog opcode0x8020 with argument1
at ROM0x756FE4/0x75719C. The dialog VM `func_8003D68C_3E28C` dispatches that
opcode to `func_80024038_24C38`. Share save1 only after its native local commit;
never remote-start either dialogue.

Temporary bit0 starts both guards; temporary bit1 opens the presentation and
deletes the blocker. File_51 never clears them. Their numeric indices are scoped
to this room; the global temporary bank has unrelated consumers elsewhere.

The guards' pre-open callback `func_08000174_70D8B4` uses local talk helper
`func_80220F70_5DC440` until save0 is set. With the pass it uses
`func_8022125C_5DC72C`, which also acquires local sequence lock D_800C7AE2.
Accepted local interaction consumes status+68 bit0x4000 and sets temp0.
Both guards then clear task flag0x10000000 and start route0x43 (D8!=0) or
route0x44 (D8==0), continuing at `func_08000088_70D7C8`.

These two route scripts are movement, animation, waits and wrapper return codes
only. Return3 plays sound0x162; return2 sets temp1. Return1 **unconditionally
clears local D_800C7AE2**, sets yaw0x200, reinitializes common NPC state and
returns to ordinary post-open talk at `func_08000000_70D740`. A replica must
not replay that lock clear against an unrelated local interaction.

Portable guard checkpoint fields are callback phase, route C4/C6/C8/CA/CC,
CE/CF/AA, object transform/clip/frame/rate, task playback bit0 and status bit0x400.
Route pointer+9C is the local actor itself. Talk pointers+B4/+BC/+EC, face and
shadow resources, contacts and blink byte+76 stay local. The initializer's
save1 branch chooses post-open guard poses by adding Z20 and X+15/-15; this is
a fresh constructor outcome, distinct from an already-running route checkpoint.

The `0x240` wait callback `func_08000598_70DCD8` observes temp1, enables native
animation and plays sound0x15E, then schedules `func_08000530_70DC70`. Because
placed H+DA=0, the latter is inert. Save1-at-entry instead schedules the also
inert `func_080004AC_70DBEC`, without replaying the opening sound or clip.
The alternate DA!=0 callbacks perform local player/transition work and must not
be included in this placed adapter.

The `0x311` initializer deletes its current task immediately when save1 is set.
Otherwise `func_080006B4_70DDF4` waits temp1 and deletes it. Its removal is
semantic shared state; neither a task pointer nor a blanket temporary-bank copy
is an appropriate checkpoint.

The adapter claims this cohort only when the roster contains exactly one typed
gate, one blocker and each of the two expected guard definitions. Missing
members are reconstructed with resident resources and local task generations;
those members are excluded from generic dynamic NPC registration so that only
the atomic bridge controller can simulate and restore them. The native birth
hook is guarded before the child allocator returns, as well as at NPC setup.
The blocker binder bypasses the constructor's save1 early deletion when the
incumbent's live checkpoint still needs collision before temp1. Nothing imports
task pointers or replays the dialogue that committed save1.

Restore requires a stable pre/route/post callback with both status0x100 and
accepted-result0x4000 clear. Native dialogue helpers see the real File_51 task
callback before saving it into+B4. A pending local result is consumed before
an imported movement latch can replace the pre-open callback. A failed apply
does not acknowledge its receipt, and the whole four-actor checkpoint retries.

Common NPC reinitialization is not idempotent: it probes ground Y and appends a
shadow. Direct completion instead restores verified scalar flags/collision
parameters and preserves the receiver's local shadow-enable0x08000000. With a
shadow, full post flags are0x1A800761 and midroute flags0x0A800760/61; the base
without a shadow is0x12800761 and0x02800760/61. Native live route endpoints differ
from the save1 reload poses, so the established incumbent wins over a fresh
constructor even if the fresh phase appears further along.

Local lock ownership is recorded only after a successful guard acceptance.
Other verified local sequence writers invalidate that ownership. Remote route
completion never calls the native wrapper's unconditional lock clear. Mid-frame
replacement unwraps the former actor before its binding is discarded; a stale
parent does not prevent cleanup of a child that still owns its native slot.

No game callbacks were executed by the native research tools and no fresh
two-client game run has been completed. Host harnesses substitute native calls;
the loopback probe exercises the real Anchor relay with simulated native receipts.
Detailed checks and package hashes are in
[the validation record](world-bridge-validation-2026-09-19.json).
