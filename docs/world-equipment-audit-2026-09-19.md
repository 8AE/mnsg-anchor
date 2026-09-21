# File30 equipment pickup reconciliation

| Equipment | Entity / room | Collection flag | Idle hook |
| --- | --- | --- | --- |
| Meat Hammer | `0x3D2` / `0x62` | `0x1A6` | `func_080073BC_6C6B0C` |
| Fire Ryo | `0x3D4` / `0x6E` | `0x1A4` | `func_080075DC_6C6D2C` |
| Bazooka | `0x3D5` / `0x81` | `0x1A5` | `func_080077C8_6C6F18` |

The constructors read their collection flags. Already-loaded idle callbacks
rotate yaw by4 and start local acquisition on status68 contact bit200, without
rereading the flag. Consequently shared progression could leave a second idle
copy present. Fire Ryo's1A4 collection flag was also missing from the durable
flag catalog; it is now included as `pk_fire_ryo`. This equipment flag is
separate from a player's current ryo balance.

`src/anchor_world_equipment.c` hooks only the three idle entries. Exact
room/entity/model/object identity and a set collection flag are required. The
hook clears contact/received-hit/fast-damage status, marks native removal,
hides the model and disables the attack cylinder. The original idle callback
then cannot start a second acquisition scenario. Native common finalization
performs removal in that task's ordinary scheduler slot.

The shine linked at rootE4 is signaled only after checking native RAM alignment,
entity identity, generation, object and one of its two exact callbacks.
`func_802139E0_5CEEB0` initializes a category13 shine, sets model1 and binds its
local billboard/texture state; it does not clear termination byteD0.
`func_80213A9C_5CEF6C` animates local tintD4/D5 and deletes the current task
when D0 is nonzero. Neither function is replayed by the synchronization hook.

The native cylinder setter `func_80218DA8_5D4278` takes
`(void *task, int radius_times_ten, unsigned short height, short offset)`.
It sets W48=FFFFFFFF and stores signed radius/10 at H4E, height at H50,
offset at H52. Constructor arguments500/25/0 become radius50/height25;
zero arguments disable the shape. All four arguments use registers.

Local acquisition continuations `08007494`, `080076B4` and `080078A0` wait
for the local scenario before releasing player control. They remain untouched,
including if another client collects concurrently after the local sequence
has already started. No remote health/ryo award, scenario, camera lock,
constructor, shine allocation or poof is executed by reconciliation.

The existing durable item flag path carries this state; no packet layout,
cadence or route was added. Native tests cover all three pickups, idle behavior
without a flag, invalid identity, repeated reconciliation, absent/pre-init
shine and recycled child slots. Production item tests cover apply, queued
publish, snapshot and stale-zero behavior for all three flags. They do not run
the real scheduler, collision or dialogue engine. Fresh two-client collection
and control-release checks remain unverified.
