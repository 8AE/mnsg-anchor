# File62 container and nested Silver Doll

This pass adds shared state for entity `0x3D6` in rooms `0x16A` and `0x182`.
It does not complete the separate Kihachi `0x315` scene graph or universal
world coverage. A fresh two-client in-game run remains required.

## Native evidence

The US ROM (`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`)
has identical zero-parameter container definitions at room-roster indices 7
and 2 respectively. File62 callbacks `08002514_723B34`, `080025A8_723BC8`,
`08002618_723C38`, `0800266C_723C8C` and `08002740_723D60` initialize, accept a
weapon contact, open, attempt a reward birth, and close. Pitch increases by 8
to 70, then decreases by 16 to zero. The hit resets the native health byte;
the container is repeatable.

Native birth uses helper `080027AC_723DCC`. That helper creates a category9
File26 task through `802171A8_5D2678`, writes its file/resource pointer, byte
generation, pre/post callbacks, fixed XYZ `(-6,125,-102)`, and save flag `0xEE`
before the child's scheduled initializer executes. The child inherits entity
and model selectors zero. Its visible resource is model1/static slot2.
Native helper `080028D8_723EF8` subtracts one from its Y each update and removes
only itself through implicit-current `80035020_35C20` after Y falls below35:
91 updates, ending at34. The category9 Doll survives that removal.

File26 `08000514_6AECF4` initializes the Doll, `080005F8_6AEDD8` handles the
local pickup/flag/dialogue/control lock, and `080006E0_6AEEC0` waits for the
dialogue before updating the Doll counter, releasing controls and deleting
the current task. These callbacks must execute in that task's scheduler slot.
Replaying them to remove a remote replica would apply local reward/scene work.

The detailed native evidence is in
`/tmp/mnsg-world-sync/file62-doll-native-audit-2026-09-19.txt`. The shared local
API reference's `nested-doll-container-lifetime` table was contributed and
validated in the preceding native-audit pass; no publication is implied.

## Implementation

World protocol13 adds a 50-word kind11 container row with its pitch, phase,
cycle counter and spawned bit. A completed idle cycle keeps its counter;
the next contact advances it, so an older closing row cannot rewind a reopen.
A remote player may propose an idle hit, but birth waits for confirmed
authority. Retained state restores a culled/re-created root after native setup.

Dynamic kind7 adds the nested Doll to the existing 83-word child transport.
Its canonical key uses the room signature, room and verified one-based
container index (8 or3), independent of allocation order or client id.
Validators pin its zero selectors, static slot2, parent/ordinal and trajectory.
The lowest observed Y wins checkpoint reconciliation, including the settled
Y34 phase. Pickup uses the existing sticky claim arbiter and successful-send
barrier before native awards can run.

Reconstruction validates File26, File62, common resources and model1 slots.
It installs the native pre-initializer fields and runs initialization only in
the new child's scheduler slot. It also clears dimensions, body collision and
mask fields inherited from the player list parent: the original native helper
supplies zero in those fields, and File26 initialization does not overwrite
all of them. Native helper births, including offline
births, are explicitly attached to the same parent identity. Their old helper
retires in its own scheduler slot; the tracked Doll owns descent, eliminating
the helper's dangling-child risk after early collection. Lost task slots retain
a reconstructible checkpoint and do not manufacture a collection.

Only the winning collector executes the native pickup scene. Its cleanup
continues after disconnect until controls are released. Remote removal only
marks the local visual for deletion. Save0xEE also removes an obsolete visual
if durable progress arrives before its room tombstone. Existing Doll progress
sharing is preserved; ordinary health and ryo drops remain collector-local.

## Network cost and verification

Both rows use the existing quiet, transient `targetTeamId` routes; neither is
queued offline. Root client id, room, team, signature, session and sequence
checks remain required. Ordinary changed state has the existing 5Hz ceiling,
replica presence refreshes at1Hz, and semantic edges retain the50ms safety
floor. For a packet of B bytes and T teammates, each send costs at most
`B*(T-1)` relay bytes. Durable save progress retains the separate existing
item flag/snapshot path.

The native tests exercise production controller/dynamic code with verified
native substitutes, including initialization ordering, full descent, local
scene cleanup, pause, resource failure, culling/reuse and duplicate births.
Transport tests cover codec parity, bootstrap/reopening, coalescing, claims,
failed sends and isolation. The loopback probe uses two teammates and one
different-team peer through real NUL-framed Anchor TCP. See the accompanying
validation JSON for current commands, results and package hashes.

These checks establish source, native-harness and local transport behavior.
They do not establish in-game rendering/collision fidelity. Playtest both rooms,
hit the container from each client, enter during descent, collect while falling
and at rest, compete for pickup, pause/disconnect the owner, and verify the
winner's dialogue releases controls while the other player remains independent.
