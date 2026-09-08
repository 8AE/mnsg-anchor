# Tsurami native synchronization evidence

The implementation is in `src/anchor_tsurami_native.c`, with the public integer
checkpoint layout in `include/anchor_tsurami_native.h`. Native behavior below
was checked against the documentation checkout, the Hyper Tsurami reference,
recovered file 29 disassembly/decompilation, and the local upstream recompilation.
Compilation and host fixtures do not certify two running game clients.

## Sources and overlay identity

- Documentation: `https://8ae.github.io/mnsg-documentation/`, especially the
  ROM-qualified root `func_080017F4_6B4A94`, private post
  `func_08000388_6B3628`, persistent model `func_080045F8_6B7898`, and
  travelling-attack constructor `func_080049A4_6B7C44`.
- Reference implementation: sibling `mnsg-extra-options/src/hyper_tsurami.c`.
  Hyper registers the root, model, and exact travelling attack constructors;
  extra root AI/movement/animation ticks exclude the private collision/damage
  post and all intro, hit-reaction, and victory callbacks.
- Recovered native file 29: temporary research artifacts
  `/private/tmp/tsurami-log.tHvJM7/file_29.s` and `decomp_0800*.c`.
  These callback addresses share runtime base `0x08000000` and ROM base
  `0x006B32A0`; full ROM-qualified names match the checked-in symbol tables.
  The temporary directory is evidence input, not a build dependency.
- Upstream native MIPS comments in sibling
  `Goemon64Recomp/RecompiledFuncs/funcs_52.c` (`func_80218350_5D3820`),
  `funcs_41.c` (`func_8021664C_5D1B1C`), `funcs_47.c`
  (`func_8022026C_5DB73C`), `funcs_46.c` (`func_80220210_5DB6E0`),
  `funcs_50.c` (`func_8021DD4C_5D921C`), and `funcs_40.c`
  (`func_8021A26C_5D573C`).
- The currently open Ghidra base-ROM program did not contain these imported
  file 29 overlay functions. The existing recovered overlay artifacts and
  native recompilation were used instead; no live Ghidra decompilation of
  those overlay functions is claimed.

## Root and model identity

Room is `0x71`; actor/resource family at task `+0x5C` is `0xCB`. Root identity
requires the exact private post, linked task/backlink, object, generation, and
local room-visit token. Entity `0xCB` alone also admits model and projectile
children and therefore is insufficient.

`017F4` initializes HP `+0x8D=12`, recovery byte `+0x8C=180`, flags
`+0x60=0x002006C1`, no-drop bit `+0x64|=0x8000`, special controller bit
`+0xE8|=0x08000000`, and custom remaining-HP callback `+0x90=02E00`.
It enqueues `045F8` with owner `+0xD0=root`, and separate intro/destruction
child `03DB0`. Only `045F8` is the persistent model; its `046B8`/`0476C` AI
copies root XYZ with Y minus 16. Flag `0x178` selects the second model state.
The root reaches combat at `01D54`, after the local intro/camera completes.

The 35 root words contain explicit phase ID, timer, HP/recovery, clip/frame,
packed animation state, masked flags, recovery status, transform/scale,
velocity, collider extents, spin shot index `+0xD3`, palette bytes `+0xEC`,
shared scratch spin speed `D_8015CC30+0x184`, burst `+0x186`, cached flight
velocity `+0x188`, target XYZ `+0x18C/+0x190/+0x194`, scene flags
`0x178/0x179/0x1BC`, and owned hit-flash presence/opacity. No pointer is sent.
The four model words contain clip 4, frame, masked flags, and recurring phase.

Phase IDs 1–27 cover Hyper's verified combat callback allowlist; 28–32 cover
`02E34`, `02EC8`, `02F0C`, `02F50`, `02FA4`. ID 33 is a terminal marker,
not an arbitrary native death callback to seek. The clip rate and animation
flags live at object `+0x7E` and `+0x7C`: native `1664C` stores rate times 256
and the fourth argument there. Frame application also clamps to the loaded
animation's native length.

## Damage, reflection, and victory

Private root post `00388` skips its body when `D_8015C5E4==0`, rejects contact
attack types `0x1A` and `0x1C`, runs common `80218E7C`, calls `A228`, and clears
root contact links. The damage bridge consumes local player contact intents
before common `18350` can schedule a local divergent reaction. Native
`18350` maps attack types to 1/2/3/4/8 points at `0x80218410..0x80218498`;
custom `+0x90` does not itself force damage to one point.

Root callback `02E00` schedules `02E34`; `02E34` enters `02FFC` only when HP
is exactly one. Zero HP uses a different generic death path. The authoritative
damage adapter leaves one HP for a lethal shared hit, so the native custom
victory path remains responsible for destruction. Terminal checkpoint
adoption queues the existing `boss_sync` native last-hit path and never seeks
past death-owned resources. The terminal checkpoint remains available after
root removal until the native Miracle Star reward controller completes.

Mode-1 travelling attacks have `+0x90=05B1C`. That callback schedules `05B50`,
which spawns a mode-16 returning attack and removes the old projectile.
`+0xD4` on a returning attack points to this encounter's local root.
Remote reflection intents use stable projectile IDs; addresses never cross
the transport. Only still-reflectable mode-1 identities resolve as targets.
The elected owner retains native returning-attack and ring collisions.

## Travelling attacks and expanding rings

`049A4` installs travelling AI `04ED0`, common post `80218F30`, entity `0xCB`,
and modes `1/2/4/8/16` in low `+0xE8`. Bit `0x40` changes shot orientation;
bit `0x200` records the returning shot's chosen target. Mode 8 can emit floor
rings, while returning mode 16 creates reflected-impact rings on arrival.
Exact ring constructor `056FC` installs entity `0x7E`, AI `0587C`, initial
opacity `+0xD1`, and a native outgoing-damage collider. These are gameplay
hazards, not merely untracked particles.

Travelling constructor `049A4` calls `func_8021A310_5D57E0` at native
`0x08004E50..0x08004E5C` (upstream `funcs_105.c`). The helper writes the
special orientation marker `0x8000` to object rotation `+0x14/+0x16/+0x18`
(`funcs_41.c`, native `0x8021A318..0x8021A324`). The checkpoint must retain
this exact marker for projectile pitch/yaw in addition to ordinary
`0..1023` angles. Rejecting it makes owner capture fail for the entire
checkpoint whenever a native travelling attack is present, preventing the
follower from receiving and creating the attacks. Root rotations still use
ordinary angles. The helper alone does not prove a universal camera-facing
interpretation. The native fixture now reproduces these constructor writes;
the regression failed at owner capture before the validator fix and passes
capture/recreation checks for modes `1/2/4/8/16` afterward.

All four `056FC` allocation sites in travelling AI `04ED0` use the shot as
their owner (`funcs_103.c`: `0x080053EC/0x0800544C/0x08005608/0x08005668`).
`802171A8` passes the owner's rotation halfwords through the task allocator;
`80035964` stores roll unchanged at `0x800359BC..0x800359C0`. Ring constructor
`056FC` overrides pitch to 512 for flag `0x100`, while ring AI `0587C` changes
yaw. Neither changes the inherited roll `0x8000`. Because replica rings are
allocated from the root, correction restores this fixed native roll locally;
pitch and yaw remain checkpoint values. No extra packet word is needed.
The ring fixture reproduces owner rotation inheritance for all four kinds
and failed on replica roll before this correction. This proves native state
matching, not an observed in-game rendering outcome.

Each complete checkpoint carries at most 32 active travelling attacks/rings,
20 words each: stable ID, birth tick, kind, flags, timer, packed HP/recovery,
XYZ/yaw, velocity, XYZ scale, opacity, AI variant, pitch, and gravity byte.
A missing child is reconstructed using its exact native constructor while
file 29 is mapped, then corrected to current state without historical AI or
collision replay. Absent IDs retire expired children. Ordinary native collision
pre/post remains active on followers; their autonomous targeting, reflection,
and ring spawning AI is held.

The native `8021DDE8` actor list/group argument is preserved:

| Native child | Group |
| --- | ---: |
| Ordinary travelling modes 1/2/4/8 | 6 |
| Returning mode 16, created by `05B50` | 5 |
| Floor rings, flags 0 or `0x20` | 10 |
| Reflected-impact rings, flags `0x80` or `0x100` | 8 |

`0587C` decreases ring `+0xD1` opacity, expands its scale, rotates yaw, and
updates the local material. Correcting the byte alone cannot update a held
follower's display list. `8021DD4C` and `8021A26C` both consume
`(task, alpha, red, green, blue)` and return the generated display list;
verified native stores write packed RGBA at object `+0x8C`. Rings with
`0x20` use half the opacity; `0x80/0x100` use full opacity through `DD4C`;
the default ring uses `A26C`.

The 32-active-hazard cap is an explicit shared-mode packet budget. Excess
shared constructors retire before their first collision pass, rather than
publishing partial active sets or permanently stopping synchronization.
Offline native/Hyper allocations are retained; tracked excess offline actors
retire when shared mode begins. Hyper's allowed owner AI cadence remains
unchanged below this shared cap. The cap and checkpoint correction cadence
need confirmation under a real full Hyper fight.

## Local resources, timing, and verification

Hit-reaction `02E34` allocates a blue screen overlay through `8022026C` and
`80036B7C`. Its pointer is stored in system state `+0x3B018`, aliasing
`D_800C7CD8` with native system base `0x8008CCC0`; alpha is render-object
`+0x10`. `02EC8` calls `80220210` to add 80 per step, clamping at 255;
`02F0C` calls `80220168` to subtract 80 until `80036158` releases the record.

The global alias is **not cleared by release**. The native release chain
`80036158 -> 80035FDC / 80036308 -> 80007C90 / 8000A228` unlinks the record
and puts it back in the pool. `8000A228` writes free-kind `+4=0x81` and
resets RGBA, including `+0x10=255` (`funcs_10.c`, `0x8000A234` and
`0x8000A2AC..0x8000A2B8`). Pointer equality alone therefore published a
permanent fully opaque flash after the real owner's fade had finished.
The native fixture reproduced this failure before the lifetime fix.

Flash capture now requires an active kind-1/group-11 record still linked in
the tracked root's native object list. A `80036308` entry hook drops ownership
before the pool record can be reused. Follower correction and reset unlink
only the owned live record with `80036158(root, record, 1)`; they preserve the
global alias and unrelated overlays. A stale freed alias permits the next
hit's allocation, while an unrelated live overlay does not stall adoption
of the boss gameplay checkpoint. Tests cover fade-out, repeated hits, address
reuse, alias replacement, foreign effects, and reset.

The same stale alias also blocked reflection: the old `correct_flash`
rejected a non-null freed cursor when this follower had no tracked flash.
Checkpoint adoption then stayed pending, which held the projectile's common
post callback and prevented local contact from reaching `80218350`.
`test_tsurami_reflection_integration.c` combines the real native adapter and
damage adapter with the native contact/callback sequence. Restoring only the
old flash correction in a temporary build reproduced the stuck adoption;
the fix passes local contact capture, owner `05B1C -> 05B50`, creation of a
new mode-16 returning projectile, and its adoption on the follower. The test
models native contact records; it does not certify live collision geometry.

On authority handoff, a root in fade phase 29/30 with no owned effect (or a
foreign global alias) skips those visual steps and continues at native
`02F50`. That callback waits for animation completion, and `02FA4` clears
recovery and returns to neutral; neither depends on a flash allocation.
This avoids running the native global-alias fade against another effect.

The supplied September 8 single-player trace records `02E34 -> 02EC8 ->
02F0C -> 02F50 -> 02FA4` at lines 1019–1032, consistent with normal recovery.
Function names appear once, so this trace establishes visited paths rather
than frame counts, repeated calls, or network ordering.

Checkpoints and remote damage are applied in the exact root `8021925C` pre,
after the scheduler has mapped file 29. The frame-end bridge only queues
values. Holds preserve native callback-disable tags, and native private-post
observations drive the encounter tick. Followers scope off only the extra
Tsurami branch of `A228`; common collision/damage/animation remains active.
Remote target XYZ is substituted only inside verified Tsurami homing routines
and the root-facing helper; unrelated tasks retain the real player pointer.

`tests/test_anchor_tsurami_native.c` covers root/model binding, checkpoint
adoption, follower AI/post gates, preserved native pause tags, invalid floats
and phases, missing resource retry, child reconstruction/deduplication/removal,
reflection target IDs, exact constructor groups, ring material alpha, the
shared hazard cap, hit-flash allocation/release, and terminal retention after
root removal. Native imports were checked against the local symbol tables.
Fresh two-client gameplay, Hyper choreography, audio/particle appearance,
contact fairness under latency, and the complete boss-to-Star scenario still
require live runtime verification.
