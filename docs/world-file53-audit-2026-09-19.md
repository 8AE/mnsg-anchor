# File53 coupled scene: native audit

This is a native lifecycle audit for the remaining coverage worklist, not an
implemented adapter or a live multiplayer result. The complete raw audit is
`/tmp/mnsg-world-sync/file53-coupled-native-audit-2026-09-19.txt`.

US ROM SHA-256:
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
Ghidra programs: `/codex-temp/world_file_53.elf` and `/mnsg.us.0.z64`.
File53 spans ROM `0x70E0B0..0x710660`. All 16 overlay functions and the
direct allocation, route, dialogue and resource helpers were inspected.

## Placed root and progression

Room `0x153`, placed index4, entity `0x316`, source ROM `0x12D8E80`,
definition ROM `0x12D8DF0`. Signed source XYZ is `(-126,33,-10)` and all
three definition parameters are zero. Other placement words remain undecoded.

`func_08000000_70E0B0` creates two visible children. With save `0x40` clear,
it also creates decoration and stays inert. With `0x40` set and `0x17` clear,
it arms native proximity. With `0x17` set it deletes before creating children.

The durable commit is now verified: player-task phase14 opens dialogue
`0x2F4`. Its File116 script has opcode `0x8020`, argument `0x17`, at ROM
`0x7A7BDC`. `func_8003D68C_3E28C` handles that opcode by calling
`func_80024038_24C38(0x17)`. Existing item sync carries this as `fl_kyushu`.
A visual checkpoint must not manufacture this dialogue-owned completion.

## Visible children and local tasks

| Role | Native callbacks | Portable state and reconstruction boundary |
| --- | --- | --- |
| Child A | `08001680_70F730`, shake helper `08001B90_70FC40` | Model `0x24E`, static slot1, then looping clip0 at rate0.05. Phase `+D0`, shake counter `+D8`, pose, flags and animation are scalars. It shakes by two X units and eventually deletes. |
| Child B | `08001868_70F918` | Model `0x24E`, static slot2, starts at `(-3,153,-3)`. Phase `+D0`, timer `+D4`, shake counter `+D8`, pose and flags. Sixty updates of shake, 270 rising by0.74, then900 rising by2.5. It births the mesh without retaining a child pointer. |
| NPC | `080007F4_70E8A4`, `080009DC_70EA8C` | Model `0x31B`, routes `0x41/0x42`, then direct movement. The pre-callback also opens dialogues, sets temporary flags and installs a dialogue continuation; it is not a safe catch-up initializer. Reproduce only verified model, shadow, route and pose setup. |
| Mesh | `08001BD0_70FC80`, `08001CE4_70FD94`, `08001E60_70FF10` | Allocate once in its own scheduler slot. Task `+D0` is a private pointer, not phase. Portable phase/timer are `+E0/+E4`; private alpha is byte `+27`. Array pointers `+40..+60` stay local. |
| Decoration | `0800220C_7102BC`, `08002200_7102B0` | Model `0x33F`, static slot0, scale0.7, yaw `0x200`, XYZ `(480,110,91)`. Derived from the save40-clear layout; inert until room unload. |
| Player/dialogue | `0800037C_70E42C` | Local scripts68/69, player pose, dialogue, control locks and final cleanup. Must not be replayed for remote visual restoration. |
| Camera / screen fade | `08000BC8_70EC78`, `08002018_7100C8` | Local camera handles, root/NPC pointers, HUD and global fade manager. No portable visual actor checkpoint can substitute for their cleanup. |

The NPC's portable route fields are `+C4` unsigned-halfword route ID,
`+C6` signed-halfword timer, `+C8/+CA/+CC` signed-halfword origins,
`+CE/+CF` state/program-counter bytes and `+AA` facing selector. Its `+9C`
self pointer, `+D8` root-phase pointer and `+B4` dialogue continuation stay
local. Route41 moves from `(40,64,66)` toward `(338,82,48)` and returns1;
route42 has five native waypoints and returns1. Later movement uses speed0.8
through `(478,105,300)`, `(478,105,658)` and `(571,105,706)`.

Mesh initialization allocates a private0x80-byte record and the common mesh
path allocates its arrays. Geometry is a15-by15 grid; four central entries
are set to5000. Its sequencer waits210 updates, raises alpha by2 to255,
waits1050, then lowers alpha by2 before requesting common teardown. The
common teardown is now verified: `func_80024888_25488` clears object `+2C`,
sets task `+DC=5` and installs `func_80024848_25448`. Its fifth callback
frees the arrays through `func_80024734_25334` and deletes the task. Ownership
of the separate private0x80-byte allocation remains unresolved. Never run
this initializer twice or while a different task is current.

All visible births use `func_802171A8_5D2678(parent, callback, 0)`.
`func_80218C28_5D40F8` copies entity `0x316` into every child, including
the mesh and decoration. Model assignment does not establish distinct
entity IDs. Identity must therefore include the root placement and a fixed
role ordinal; an entity-only key aliases the entire graph.

Resources must be resident before local construction: actor file `0x35`,
common file `0x152`, model `0x24E` primary file `0x200`, model `0x31B`
primary file `0x2BA`, and model `0x33F` primary file `0x2B4`. Retain local
shadow-allocation success in actor flags instead of copying another peer's
shadow-present bit or pointer.

## Remaining design boundary

Temporary bits0–17 are a handshake between visible actors and local player,
dialogue, camera and fade tasks. Copying that bank would advance local
controls. Conversely, freezing every visible callback while leaving its
local scene waiting on those callbacks can strand the scene. The adapter
must explicitly resolve scene ownership and safe cleanup during authority
departure before introducing checkpoint application.

Use separate role checkpoints and explicit removal for A, B, NPC and mesh.
Retain their state across task recycling and reconstruct missing resources
locally. Keep dialogue-owned progression on the existing durable channel;
receiving completion during an active local scene must allow its native
cleanup. A late entry must distinguish retained active visuals from a fresh
completed-room layout. No File53 callback was found to apply damage, loot
or warp; the NPC floor probe can modify its local Y.

Unresolved: semantic model names, non-XYZ placement words, private mesh-state
allocation cleanup, scene ownership/recovery implementation and fresh two-client
gameplay. These findings do not close File53's shared-visual coverage gap.

## Scene gate follow-up

The bounded follow-up and raw disassembly are in
`/tmp/mnsg-world-sync/file53-scene-gates-followup-2026-09-19.txt` and
`file53-followup-disassembly-2026-09-19.txt`. Proximity acceptance runs through
`func_802221D4_5DD6A4` when root `+34` is nonzero and immediately invokes
`func_080001BC_70E26C` phase0. Root `+C0` is integer10000, yielding signed
halfword1000 at `+4E`; it is not a floating-point field.

The root does not retain the player task pointer. That task must be observed
at `func_0800037C_70E42C`; it waits for visible milestones3 and16. After
dialogue2F4 commits save17, phase15 must still wait for dialogue inactivity,
restore player input and emit temporary17. Camera release, HUD restoration
and root retirement depend on this later cleanup. Camera `+E4` points into
the root until phase2 caches its NPC object; `+E0` remains dereferenced
through phase6. Receiving save17 cannot safely retire an active root early.

Mesh scheduling is `func_08001BD0_70FC80` to `func_80024670_25270` /
`func_800246BC_252BC`, then array initialization at `func_80024160_24D60`,
the one-shot `func_08001CE4_70FD94` and steady `func_80025B38_26738`.
