# File30 remaining lifecycle inventory

This records inspected native behavior and implementation gaps. It does not
classify every nonmoving model as covered. The complete current production
placement counts are in `world-placement-coverage-2026-09-19.json`.

Native evidence comes from `/codex-temp/world_file_30.elf` and
`/mnsg.us.0.z64`, with the same US-ROM hash used in the File40 audit.

| Family | Inspected behavior | Coverage implication |
| --- | --- | --- |
| `0x196` |44 placements; constructor080001F0 installs flags802006E1 and an empty0800028C continuation | Common hit/death and contact admission still require classification. An empty AI does not establish static-only lifecycle. |
| `0x322`/`0x33B`/`0x33C` | Constructor-selectable visibility;080003B0 changes upper visibility state from native status20000 | No autonomous travel, but native admission/view behavior needs a complete classification. |
| `0xCA` | Constructor tests save12E; continuation08000970 is empty | Save-derived appearance; already-loaded state still needs review. |
| `0x19D` | Six slicer emitters inAB/AC; immutable initial delay byteD2, mutable timerH8A, native blade birth | Full root, blade, common hit/death and loot lifecycle remains unimplemented. |
| `0x331` | Static root080049A4/08004AA0 and a child08004AB4 rotating yaw32 | Linked child presentation and lifetime need explicit classification. |
| `0x339` | Save32 selects static root appearance; empty08004AE8 continuation | Save-derived loaded-state reconciliation needs review. |
| `0x330` |58 placements; local tint cycle changes byteDC by8 | Common damage admission remains unverified; do not infer harmless scenery from the tint-only callback. |
| `0x332` | Five room85 props; fixed meshes, allocated private geometryE4, animated child, scroll helper and local support impulses | Shared geometry/animation versus local support behavior remains to be separated. |
| `0x35F` | Miracle Moon acquisition | Existing dedicated `anchor_miracle_moon.c` adapter; preserve its local scenario cleanup. |
| `0x3D2`/`0x3D4`/`0x3D5` | Equipment idle rotation and scenario acquisition | Current pass implements loaded idle pickup/shine reconciliation through collection flags; see the equipment audit. |
| `0x3EC` | Local tint update | Common damage/lifetime still needs verification. |
| `0x3EF` | Room91 producer uses random20+30 with global tick modulo; child07B28 binds model12F and07BDC turns toward its local target while counting down90 | Random birth authority, target binding, current-state reconstruction and explicit removal remain unimplemented. |

## Slicer evidence and required boundary

Root constructor `0800447C` installs the initial-delay continuation08004550.
The definition byte tuple D0/D1/D2/D3 means subtype/repeat interval/initial
delay/speed. The constructor copies D2 to signed timerH8A. Initial wait
decreases H8A by2 and schedules birth when its new value is0;08004594
creates a category1 child at zero owner-relative offset, stores the local
parent pointer at childDC, and emits sound271. Continuation08004654 uses
a signed halfword repeat timer and schedules birth when the old value is0.
Each attempt consumes D1+2 callbacks, including allocation failure. There is
no native emission ordinal; the inherited byte74 is not a unique blade ID.

Placed tuples are AB indices5/6 `(0,120,120,0)`/`(1,120,180,1)` and AC
indices5–8 `(2,60,30,2)`, `(2,50,30,1)`, `(2,40,40,0)`, `(2,50,20,1)`.

Child initializer `08004694` binds model19D, animation0, sphere1, scale and
speed from its local parent. Its resources are File30 code, primary1D6,
secondary180 and common152; clip0 is0800001C and sphere1 is080001D8.
Continuation0800488C steers subtype0/1 through the pure velocity helper
8021B988 or decrements the subtype2 lifetime (121 calls from timer120).
Animation0 has255 frames, rate256 and no loop; native common post advances
frame and position at twice the stored rate/velocity. Reaching the endpoint or timer
expiry deletes the current task directly, without native kill loot.

Both root and child install flags002006E1, including incoming-hit200000 and
outgoing-scan40; common initialization gives them HP1. Neither slicer
initializer sets the no-loot auxiliary flag8000. Accepted lethal common damage
therefore takes the native death/loot path. Any implementation must distinguish
that kill from endpoint expiry and proximity unloading, coalesce the same
blade across clients, and permit only the committed kill to produce loot.
The normal local contact pass must run once; neither attack nor damage can be
replayed to catch up a newly reconstructed blade.

Root tasks use category3 and a placed source pointer; blades use category1
with source70=0. Native allocation places them in a global category list,
with only childDC linking directly to the emitter. No root-removal cascade
was established. A restored child therefore needs a safe local source for
its immutable subtype/speed even after the emitter is culled or killed;
copying the raw parent pointer is invalid.

This audit is evidence for the next adapter, not an implementation or a fresh
in-game test. The bounded native report is
`/tmp/mnsg-world-sync/file30-slicer-native-audit-2026-09-19.txt`.
Root model slot, outer source-pointer assignment, exact inherited target
context and external room teardown remain explicit verification gaps.
