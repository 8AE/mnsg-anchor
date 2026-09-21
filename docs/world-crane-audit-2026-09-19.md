# Ghost Toys crane controller audit

Room `0x31`, File_30, US decompressed ROM SHA-256
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
The typed crane adapter is implemented in `src/anchor_world_crane.c` and world
protocol 9. Host and loopback validation is recorded separately from gameplay.
Ghidra used explicit programs `world_file_30.elf` and `mnsg.us.0.z64`.

## Native graph

| Placed entity | Entry | Children / role |
| --- | --- | --- |
| `0x1B9` | `func_0800097C_6C00CC` | Four children: `func_08000ACC_6C021C` (kind 3), `func_08001D0C_6C145C` (kind 0), `func_08001EE4_6C1634` (kind 3), and `func_080024A0_6C1BF0` (kind 3). Root becomes inert at `func_08000AC0_6C0210`. |
| `0x1BA` | `func_0800211C_6C186C` | Power switch with scenario/fade/control work; no explicit actor child. Already-powered entry reconstructs model state 1 and ends at `func_08002488_6C1BD8`. |
| `0x1BB` | `func_080024F0_6C1C40` | Two pad children, `func_080027CC_6C1F1C` and `func_08002ACC_6C221C`, kind 3. Parent `func_08002624_6C1D74` retains local occupancy/scene work. |
| `0x1BF` | `func_08002D98_6C24E8` | Local camera/controller, not shared object motion. Camera task pointer at +D0. |
| `0x3D3` | `func_080074D4_6C6C24` | Coupled placed interactable; starts `func_0800178C_6C0EDC`, consumes crane milestones, ends in local contact/scenario/completion handling. |

Allocated puzzle children receive resource ID `0x1E` at task +28, its local
pointer at +2C, and their parent pointer at +DC. Roots do not store child
pointers. The moving crane child also creates `func_080016D8_6C0E28`; its
`func_08001778_6C0EC8` continuation copies the moving child's object X.
The `08001EE4` visual child creates `0800209C_6C17EC`, has a local render
handle at +E4 and render-animation bytes at +E0/+E1.

The moving crane's normal graph is:

```
08000ACC_6C021C -> 08000BE8_6C0338 -> 08000DB4_6C0504
-> 08000E2C_6C057C -> 08000F34_6C0684 -> 08001018_6C0768
-> 080010B8_6C0808 -> 08001174_6C08C4 -> 08001228_6C0978
-> 08001268_6C09B8 -> 080012B8_6C0A08 -> 08001324_6C0A74
-> 08001388_6C0AD8 -> 080013E4_6C0B34 -> 08000BE8_6C0338
```

Its alternate low-X branch passes through `08001488_6C0BD8`,
`080014C8_6C0C18`, `0800152C_6C0C7C`, `08001594_6C0CE4`,
`08001600_6C0D50`, `0800166C_6C0DBC`, then rejoins at `08001174_6C08C4`.
These are full `func_` symbols in File_30; no callback address belongs on wire.

Pad 0: `080027CC_6C1F1C -> 08002848_6C1F98 -> 08002930_6C2080
-> 080029A0_6C20F0 -> 08002A20_6C2170 -> 08002A98_6C21E8 -> 08002848`.

Pad 1: `08002ACC_6C221C -> 08002B4C_6C229C -> 08002BF8_6C2348
-> 08002C68_6C23B8 -> 08002CEC_6C243C -> 08002D64_6C24B4 -> 08002B4C`.

Each pad lowers Y by 0.3 for eleven updates, waits for departure, then raises Y
by 0.3 for eleven updates. The timer starts at ten and tests its previous value
before decrementing, so zero is also a movement update. Pad entry requires power and a comparison between its
object and the local player task's +A0 pointer. That pointer's exact semantic
name remains unverified. It cannot represent both players' occupancy.

## Shared and local state

| Flag | Observed use |
| --- | --- |
| Temporary 0 / 1 | Pad-held states; crane X += 1 / Z -= 1 per update. |
| Temporary 2 / 3 | Set when the corresponding pad returns; both start a cycle. |
| Temporary 6 / 7 | Target rectangle after lower wait / following animation-complete boundary. |
| Temporary 8 / 9 | Upper Y endpoint / later X/Z endpoint. No consumer of 8 found in this bounded graph. |
| Temporary B | Crane below Y -20; cleared on return to Y -10. Local camera reads it. |
| Temporary E | Cycle latch. Set when both pads complete and cleared at cleanup. |
| Temporary C / F | Local occupancy in `-170 < X < 0, 34 < Z < 123` / one-time scenario D4 trigger. Keep local. |
| Save 15A | Crane power on. |
| Save 15B / 15C | Reversible scene/camera handshake, including cycle cleanup. Not monotonic puzzle progress. |
| Save 15E / 15F | Historical animation-complete / upper-clamp milestones; no native readers or clearers found. |
| Save 1C4 | One-time local scenario D6 gate after power-on. |
| Save 1A3 | Wind-up Camera actor consumed. Scenario 83 separately grants ownership at save +C8 and charging at +D8. |

The shared checkpoint needs crane motion/phase/timer/animation, two pad
phases/timers/Y positions, semantic milestone flags and the coupled 3D3
interactable. Pad occupancy must aggregate each client's current contribution;
a last-writer copy can release a pad occupied by the other player. Room/session
changes and stale/disconnected peers must remove their contributions.

Keep task/object/resource pointers, render handles, `D_8015C562_15D162`,
local camera/forced player position, letterbox, fade and scene state local.
In particular, reconstructing these actors must not replay the power switch's
`0800221C` through `08002428`, root `08002624`, camera `08002D98`/`08002E5C`/
`08003044`/`08003110`/`08003164`, or reward-contact `08001C10_6C1360` merely
to catch up a replica. There is no dynamic loot allocator in this graph.

Scalar fields found in the motion graph are task flags +60, status +68, variant
byte +6C, XYZ velocity +78/+7C/+80 and signed timer +8A, plus object transform,
scale and animation completion at +7C. The original callback must be represented
by a validated local phase enum. Task +DC is a parent pointer on children but a
local byte on 1BF; generic private-memory copying would corrupt it.

Disassembly of `08001D20` through `08001D38` verifies that native helper
`func_8021A26C_5D573C` receives its fifth argument, 50, at stack +10.
Constructor/pad/camera disassembly and raw File_30 constants at `080089B0`
through `08008A1F` were checked by the native research specialist.

The whole-ROM helper-call and scenario-flag scan found no additional readers or
clearers of saves 15E/15F. Temporary bit 8 is write-only in File_30; the common
actor manager clears the full temporary bank on room initialization. These are
observational milestones, not reconstruction selectors.

Scenario 83 resolves through `func_8003D310_3DF10` to file 005D, pointer 08003834.
Direct VM assignments at 0800394C through 08003968 write one to 8015C6D0 and
8015C6E0, save-base offsets C8 and D8. Its dialogue awards Ebisumaru the Wind-up
Camera. The adapter retires a completed 3D3 without replaying this contact scene;
the existing item-sync path now includes the charging scalar as `cam_charge`.
Power flag 15A is durable `cr_power`. The former `cr_off_txt`/`cr_entered`
entries (15B/15C) are removed from the shared flag table: they are local camera
handshakes. Legacy incoming keys are ignored rather than restoring those bits.

Constructors explicitly bind models 1B9, 1BB and 3D3; 1BA retains its placed
model ID. The native category table at ROM 5E54B4 gives 3D3 category 9.
Both pad births and the moving crane use allocator kind 3 in the native roots.
`802171A8` installs the initializer before `80218C28`, which installs common
pre/post and copies the parent generation. Pending births can therefore be
filtered by their exact initializer without waiting for decorative children.

Protocol 9 uses one atomic kind-7 row for main motion, both pads and the reward,
plus a bounded per-client pad mask. Only the elected simulator starts press,
release and cycle transitions. Replicas retain native motion; local contact,
camera, scenes and cleanup remain local. Child replacement retires the previous
owned task, preserves disabled callback bits, and waits for resident model clips
before reconstructing. Failed/deleted births cannot pin the pending list.

Fresh two-client in-game validation of the full room remains outstanding.
