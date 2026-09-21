# File40 top and platform audit

US ROM SHA256: `e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
Native program: `/codex-temp/world_file_40.elf`, ROM `0x6E9150..0x6EE960`.

## Implemented roots

| Entity | Placement coverage | Native continuation | Checkpoint |
| --- | --- | --- | --- |
| `0x365` giant top | Seven: room3E indices5–8, room3F indices10–12 | `func_080018E4_6EAA34` | Static pose, yaw, cosine phase, immutable subtype/radius/step and source X/Z |
| `0x366` rotating platform | Five: room3F indices13–17 | `func_08001CC4_6EAE14` | Static pose, yaw and immutable direction |

The top initializer `0800181C` binds static model0, sets scale0.1, flags60
`0x80000220` and support flag64 `0x4000`. TaskD0/D4/D8 are subtype/radius/step;
DC/E0 are float source X/Z; E4 is a 10-bit phase. Subtypes0/1 move X and2/3
move Z by native cosine times radius, then advance phase by signed low16 step.
Yaw advances4. Verified tuples are `(2,160,1)`/`(3,160,1)` in3E and
`(0,130,2)`/`(0,80,4)` in3F. The callback only calls cosine.

The platform initializer `08001C48` binds static model0, scale0.1, flags60
`0x80000020` and support flag64 `0x4000`. D0=0 subtracts2 from yaw; D0=1
adds2. Yaw wraps to10 bits. The callback makes no calls.

Both use native code file40, common resource152 and secondary168. Top primary
1E9 binds command `0x48000348`; platform primary1EA binds `0x48000258`.
Apply checks all four resident resources, exact model/static slot, command and
file handles, typed callback and immutable parameters before acknowledging a
receipt. Top source centers must match the local placement. No resource,
callback or task pointer travels over the network.

Protocol15 uses the existing50-word placed row. Top variant29/phase78 has
private fields19=phase,20=subtype,21=radius,22=step,23/24=source X/Z.
Platform variant30/phase79 has field19=direction. Static animation fields,
velocity and unused private fields are canonical zero. Generic restoration
leaves local object animation-status/rate storage intact for these models.
The same pause, retained-checkpoint and applied-receipt logic used for other
autonomous platforms covers late entry, culling/reload and authority handoff.

Each client runs only the verified motion callback between checkpoints. The
normal common post remains scheduled once. Its support handler moves the local
player only when that player's support pointer identifies this object; no
remote player/support pointer is copied and no contact pass is replayed.

## Remaining File40 work

Entity1A9 has seven bomb-block placements in rooms65/66. Its HP2 root arms by
local proximity, falls and enters a fuse cycle. A surviving hit invokes its
custom callback and allocates four mixed-kind fragments plus eight particles.
An immediately lethal hit instead follows common death without that fan-out.
This needs explicit cause, child identities/reconstruction, lifetime and
single-shot sound handling. It is not covered by the top/platform adapter.
The adjacent six-child graph belongs to entity3CB and is also separate.

## Verification limits

The production classifier matches every one of the12 placements. Native host
tests cover all six parameter families, phase/yaw restoration, unchanged-row
prediction, pause, missing resources, wrong local bindings, immutable mismatch
and retained restore. C/Python codec parity and three-client private Anchor
tests cover handoff, duplicates, sender exclusion and team isolation.
Native callbacks and receipts are simulated in those tests. Fresh two-client
riding, collision, pause, room reload and renderer checks remain necessary.
