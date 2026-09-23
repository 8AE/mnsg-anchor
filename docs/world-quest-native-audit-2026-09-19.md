# Remaining quest controller graphs

These are native findings for the universal-coverage worklist. Each section
records its implementation or classification boundary. Local camera work does
not make visible children static or automatically synchronized.

US ROM SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
Use explicit Ghidra overlay programs; identical RAM addresses in different
files are unrelated callbacks.

## File_53, Gateway Viewpoint

Entity0x316, room0x153, placed index4. File_53 ROM0x70E0B0, size0x25B0.
The root reads save0x40 (Tsurami defeated) and save0x17 (Kyushu disappeared).
Before the first flag it creates static decoration and two visible children.
Between those flags, the root uses the native proximity trigger with range10000
and schedules the quest graph. After save0x17 it deletes immediately.

`080001BC_70E26C` coordinates a local player/dialogue task, local camera,
moving NPC, object motion, mesh-grid effect and fade. The graph's temporary
bits0–17 coordinate both presentation and local player control; copying this
bank would bypass local prerequisites.

- Local player task `0800037C_70E42C` owns control and scripts68/69, with
  dialogue2F2/2F3/2F4. The follow-up verified dialogue2F4's opcode8020
  argument17 at ROM0x7A7BDC as the durable save0x17 commit.
- NPC child `080007F4_70E8A4` uses model0x31B, dialogue2EE/2EF/2F1,
  route41 then route42. Its later callback `080009DC_70EA8C` moves through
  (478,105,300), (478,105,658), (571,105,706) at speed0.8, hides its own
  shadow and deletes. Caller disassembly confirms the fifth float argument
  to the movement helper; the follow-up decoded both native routes and
  identified their pointer-free working fields.
- Child `08001680_70F730` binds model0x24E static slot1 at scale1. It waits
  for temporary bit9, shakes X±2, then binds animated clip0 at rate0.05 on
  bit10; bit11 deletes it.
- Child `08001868_70F918` binds model0x24E static slot2 at(-3,153,-3).
  Bits7 and6 start the mesh effect; bit13 starts a60-count shake. It then
  sets bit10 and rises0.74 units for a270-count stage, sets bit11 and rises
 2.5 units for a900-count stage, sets bit14 and deletes.
- `08001BD0_70FC80` allocates local heap state and native mesh/vertex/display
  resources. `08001E60_70FF10` drives alpha, timer and bits12/13/15/16.
  These allocations and pointers are local. Native cleanup is delayed before
  freeing resource arrays; the exact common-path delay remains unresolved.
- `08002018_7100C8` owns a local full-screen fade. `0800220C_7102BC`
  creates decoration model0x33F slot0 at(480,110,91), yaw200, scale0.7.

The next implementation needs separate shared checkpoints for the NPC and
visible motion/effect graph, while retaining local player, dialogue and camera
lifecycles. Root flag snapshots alone cannot reconstruct it.
The [complete File53 audit](world-file53-audit-2026-09-19.md) records the
inherited child identity, safe construction requirements, verified dialogue
commit and remaining scene-ownership boundary.

## File_58 and the File_46 Koryuta graph

Entity0x2A0 in room0x155 is File_58's local camera/scene controller
(ROM0x71A510, size0x1570). It has no model, collider, reward or gameplay
children. Its task+D0 points to12 bytes: phase byte0, camera subphase byte1,
camera handle+4 and saved camera output pointer+8. Task+84 is a local target
object; E0/E4/E8 are camera/ascent/color scalars, and8A is the timer.

The visible placed entity0x1B0 belongs to **File_46**, not File_57.
`08001A28_702B28` creates eleven0x1B4 body parts plus additional child
controllers. Generic normal-enemy placement currently excludes0x1B0, so
the existing enemy adapter is not proof that this visible graph is covered.

| State | Native meaning and boundary |
| --- | --- |
| Save0x14 (`fl_koryuta`) | Suppresses File_58 initialization; existing item sync carries this durable flag. An active local controller still needs its own cleanup. |
| Bank0 temporary bit0 | Terminal state written by File_46's health callback `08002540_703640`, also `0800140C_70250C` when D_8015CDB6 is3. File_58 reads it for the return scene. |
| Bank1 temporary bit0 | File_58 phase13 writes intro completion. File_46's wave producer reads it to choose delay512 or1, then emits at80-global-tick boundaries. It has a visible world consequence. |
| Save0x19F | Local proximity/camera token. File_46's sensor `08002EB4_703FB4` checks local Z within200 and creates the local camera handle required by the File_58 return sequence. Do not share this flag to bypass the sensor. |
| Save0xC2 | Written by File_46 timed departure and File_58 terminal sequence; durable terminal meaning verified, descriptive name unresolved. |

Shared terminal/intro checkpoints must wait for the appropriate local scene
prerequisites. Replaying File_58 `08000B8C_71B09C` to catch up would move the
local player, manipulate controls/camera, fade and warp. Late entry needs
File_46's current body/wave graph plus safe local scene completion, not direct
replay of that terminal callback.

Raw research is saved outside repository inputs in
`/tmp/mnsg-world-sync/file53-*-decompiled.txt` and
`/tmp/mnsg-world-sync/file58-native-audit-2026-09-19.txt`. The File_58 audit
SHA-256 is `ad6c2597b81857207cb9bd7d34ec22ef5eca5c2b5ad9b801407d007a917591c7`.
These findings are static native evidence, not live gameplay validation.

## File_62: Kihachi scene and the separate Doll container

All26 overlay functions were decompiled with explicit program
`/codex-temp/world_file_62.elf` (ROM0x721620, size0x2EE0). Entities0x315
and0x3D6 each occur in rooms0x16A and0x182. Room0x16A is Zazen Town
Watering Hole. These are separate entrypoints, not one private-state layout.

Entity0x315 enters at `08000000_721620`. Save0x23 enables the controller;
initialization clears the reversible dialogue-cue bits3,4,0x5E and allocates
28 bytes of local scene state. Proximity starts player positioning, control
lock, camera and lighting. Branches inspect save0x35/0x0F/0x37; helpers create
model0x315, water/effect models and local dialogue targets. Temporary bit0
drives the visual model's bobbing. Cleanup `08001AA8_7230C8` releases its own
camera/control, waits outside a larger proximity range and rearms. Neither
that cleanup nor its scene pointers may be replayed to restore a remote NPC.
Its dialogue-dependent graph and visible child state still need an adapter.

Entity0x3D6 enters at `08002514_723B34`, category11. It binds model0x3D6,
clip0 at rate1/30, a collider and100 HP. Its lifecycle is:

| Callback | Behavior |
| --- | --- |
| `080025A8_723BC8` | Wait for hit bit0x80 and a non-null local hitter, play0x24F, restore100 HP and start opening. |
| `08002618_723C38` | Add8 pitch units per update, clamp at70 and enter the spawn check. |
| `0800266C_723C8C` | If save0xEE and temporary bit2 are both clear, allocate helper `080027AC_723DCC` at(-6,125,-102), then start closing. |
| `080027AC_723DCC` | Create category9 child using File_26 initializer08000514, record file0x1A, XYZ(-6,125,-102) and packed definition word0x00EE0000. Only successful child allocation sets temporary bit2. Store the local child at+EC. |
| `080028D8_723EF8` | Move the reward's Y down by1 each update; terminate the mover once Y<35. It does not delete the reward. |
| `08002740_723D60` | Subtract16 pitch units modulo1024; wrap past0x300 clamps to0, plays0x273 and returns to hit wait. |

The existing durable item table names save0xEE `sd_zz_wh`. That flag handles
collection; it does not reconstruct a live uncollected Doll or its descending
motion. File_26 initializer `08000514_6AECF4` reads the high halfword at+D0,
temporarily binds model1 static slot2, restores the actor's model identity,
allocates a local shadow and enters `080005F8_6AEDD8`. Pickup bit0x200 locks
local controls, sets that save flag and starts dialogue0x66 or0x312 depending
on save0xE2. Cleanup `080006E0_6AEEC0` waits for local dialogue, performs the
native save/count update, releases controls and deletes. Remote removal must
not replay that local pickup or cleanup. Protocol13 now provides stable birth
identity, reconstruction and opening-cycle authority using the verified helper
deletion semantics. A missing allocation is not a successful birth.

The follow-up resolves the helper lifetime and actual Doll identity. The placed
root is roster index7 in room0x16A and index2 in0x182, both at(-10,21,-126).
`func_80220410_5DB8E0` inserts the mover below that root without copying its
entity/model. The mover then allocates the Doll on the separate category9 list
through `func_802171A8_5D2678`. Both inherited identities remain zero; the
mover's+EC is the only explicit reward link. The current dynamic tracker sees
the birth but cannot classify that zero-identity File26 task.

The mover subtracts1 from Y each update and stops at Y=34 after91 updates.
`func_80035020_35C20` removes only the implicit current task; it may defer
unlinking behind deeper scheduler nodes but never runs the descendant-deletion
loop. The Doll survives. This routine cannot be called from another task to
remove a remote mover.

Reconstruction must write File26 residency, XYZ and packed0x00EE0000 before
the scheduled native initializer. It binds model1/slot2, restores model zero,
sets rotations to the native0x8000 sentinel and retains a locally allocated
shadow. Valid final flags are0x084006E0 with that shadow or0x004006E0 when
shadow allocation fails. Shared collection needs a tombstone before pickup
contact; a Doll already waiting in its local dialogue cleanup must finish that
cleanup and release controls. The dedicated controller/birth adapter is now
implemented and locally validated; see the [Doll audit](world-doll-audit-2026-09-19.md).
Fresh two-client gameplay remains unverified. This does not implement the
separate Kihachi scene graph.

Raw evidence: `/tmp/mnsg-world-sync/file62-helpers-decompiled.txt` and
`file62-main-decompiled.txt`. New ABI-sensitive calls still require caller
disassembly before implementation. The completed allocation/termination ABI
audit is `file62-doll-native-audit-2026-09-19.txt`; the earlier Doll trace is
`/tmp/mnsg-world-sync/file26-doll-decompiled.txt`.

## Files_67/70: travel and entrance cinematics

Every function in File_67 (ROM0x7295A0, size0x1490) and File_70
(ROM0x72E260, size0x1630) was decompiled; allocation/stack arguments and
terminal cleanup were checked against disassembly. These controllers own
local player/camera sequences and finite visual children rather than a room
mechanism with persistent geometry. No world-motion row is added for them.

File_67 entity0x335 appears in rooms0x14C/0x158. Its24-byte state contains
phase/timer plus local helper, prop, camera-track and camera-output pointers.
Phase0 requires all four Miracle inventory words, which already share. If
any is zero it frees its private state and retires the current controller;
it does not remain waiting for a later inventory update.
Later phases run local dialogues0x1F6/0x1F9, shake the camera, position the
player, and eventually warp this client to stage0xA8. Its0x336 child follows
only this cinematic: jitter, velocity, scale decay, animation, then explicit
retirement. Entity0x289 fades and the terminal two-plane visual are finite
presentation with no reward/contact/damage callback. Replaying any camera or
warp phase to catch up another client would be incorrect.

Save0x6C is read/cleared,0x6B selects departure and0xA2 is cleared at the end;
temporary bit0 marks local scene entry. Do not add these to a world checkpoint.
The legacy item table carried0x6B/0x6C as `fl_outerspace`/`fl_to_space`.
The bounded scenario follow-up located0x6B's set in File102's dialogue0x1F9:
the "Yes, we will!" branch reaches ROM0x77E140; the "No, we're not going..."
branch does not set it. Native phase5 then reads the flag to decide departure.
File102 also sets0x6C in four dialogue branches before File67 clears it.
These are concrete local-choice interactions. The completed transport review
removes both keys from the native sync table and debug sync catalog. Incoming
legacy queued deltas and snapshots therefore ignore them; outgoing snapshots
and gain detection no longer include them. Native local bits and the four
shared Miracle words are preserved. There is no retrospective save migration:
an already-set local6B cannot be attributed safely to native choice versus old
remote input. An aligned opcode scan does not prove all indirect accesses
absent. See [the travel flag audit](world-travel-flags-audit-2026-09-19.md).
Evidence: `file67-scenario-followup-2026-09-19.txt` under the same scratch directory.

File_70 entity0x344 has one placed variant in room0x14D, task+D0=0. Entry
requires local destination marker D_8015C848 and exits immediately when
save0xC4 is set. Variant1 instead checks D_8015C844 and save0xC3; its external
invoker is unresolved. Accepted entry acquires local control/camera and
allocates24-byte private state. Both variants create up to21 randomized
category8 particles, each a local finite alpha fade, before moving the local
player and camera. Variant0 starts dialogue500 and sets0xC4 at phase3 timer10;
variant1 similarly sets0xC3 with dialogue0x1D4. Camera/dialogue cleanup then
releases its own controls, frees state and retires the root.

The missing0xC4 completion now uses the existing durable OR-applied team flag
channel as `fl_shore_entry`;0xC3 was already `fl_mtfuji`. Native active bodies
never read0xC4, so receiving it cannot interrupt their cleanup. A future
constructor observes the shared bit before acquiring any local controls.
Temporary bit0, entrance markers, camera pointers, particle births and random
state stay local. The internal0xC4 label describes its verified entry-complete
behavior; a canonical game-facing location name has not been established.

Raw evidence is `/tmp/mnsg-world-sync/file67-70-native-audit-2026-09-19.txt`
and the corresponding `file67-disassembly-2026-09-19.txt` /
`file70-disassembly-2026-09-19.txt`. No fresh two-client game run was performed.

## Files_74/75: Gorgeous Music Castle cinematic graph

All34 File_74 functions (ROM0x734790, size0x3F40) and27 File_75 functions
(ROM0x7386D0, size0x1780) were decompiled in their explicit overlay programs.
The placed roots are0x35C/category0 and0x35D/category1 in room0xC1.
The earlier worklist's generic puzzle description was misleading.

File_75 waits for local player position and native standing/action state,
creates its local camera and player-script task, hides the HUD and runs
dialogue0x105. It sets temporary bit0 only after that local dialogue ends.
File_74 waits for that bit, checks local player state and acquires controls.
It owns104 bytes of heap state containing phase, child pointers, lighting,
camera output and saved render callback. Its staged children include
models0x351–0x353,0x318,0xFB,0x359–0x35B,0x367 and0x36A. Some use shadows, collision
setup and texture resources, so a generic NPC or raw-memory copier is unsafe.

File_74's19 phases reposition, animate and hide those children while following
local camera tracks. Terminal phase0x12 deletes the child graph, releases
texture/render/lighting/heap resources, restores camera viewport and sets
temporary bit1. File_75 waits for this bit before continuing its own resource
load, local character positioning/scale, companion clones, dialogue, camera
shake and screen flash. Its last phase invokes the native scene transition.
Save bits0x76/0x75 are animation/dialogue cues read only at the matching local
scene phase; existing item sync names them `cs_gorge_1`/`cs_gorge_2`.

These controllers cannot be advanced by copying their temporary bank or
replaying another player's callback. Their local cutscene lifecycles and the
question of shared presentation outside an active local scene remain separate
from ordinary room-NPC synchronization. No new adapter or universal-coverage
claim is made for them in this pass.

Raw evidence: `/tmp/mnsg-world-sync/file74-{main,children,rest}-decompiled.txt`
and `/tmp/mnsg-world-sync/file75-{main,children}-decompiled.txt`.
