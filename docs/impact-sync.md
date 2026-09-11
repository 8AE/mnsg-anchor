# Shared Impact boss fights

The four giant-robot **Impact** battles share one dedicated battle-state block
in the `file_13` overlay:

| Arena | Boss | Native stage | Encounter selector | Root task ID |
| --- | --- | --- | --- | --- |
| 5 | Kashiwagi (Impact 1) | `0x0220` | `1` | `0x50` |
| 6 | Thaisamba 2 (Impact 2) | `0x0221` | `2` | `0x5A` |
| 7 | Balberra (Impact 3) | `0x0222` | `3` | `0x78` |
| 8 | D'Etoile (Impact 4) | `0x0223` | `4` | `0x64` |

Unlike the ordinary room-actor bosses, an Impact fight is owned by the
dedicated overlay entry and the shared state pointer
`D_8020EED0_63A2B0`. This mod elects one client per live Impact stage and
mirrors that encounter so every participant fights the same robot.

## What is synchronised

The elected authority publishes one checkpoint containing the five battle-state
words:

* the **boss health** at battle-state `+0x60`;
* the **player/mech Ryo ammunition** at battle-state `+0x64`;
* the **player/mech health** at battle-state `+0x68`;
* the **combat-pause** byte at `+0x2C0`;
* the per-encounter **battle clock** at `+0x2C8`.

The checkpoint also carries the encounter selector and the live Impact stage
(`0x021C`–`0x023C` in story mode, `0x0260` in the title-menu boss rush), so
checkpoints from a different giant-robot encounter are rejected on both sides
of the bridge.

Follow-up hits are reported as HP deltas and applied only by the authority,
keeping one canonical health pool. Followers keep running their own boss AI
(spawn, introduction and attacks) while adopting the authority's health and
ammunition; the boss's own transform/phase mirroring is intentionally deferred
so the multi-frame introduction is never frozen.

## Native evidence

Extracted from the USA `file_13` overlay (ROM `0x5F6840`, VRAM `0x801CB460`,
size `0x43A70`; decompressed-ROM SHA1
`6ea0ed71032ce08fc2745f412d84936382197494`):

| Symbol | Role |
| --- | --- |
| `D_8020EED0_63A2B0` | shared battle-state pointer |
| `D_8015C5C8_15D1C8 + 0x3ADF4` | encounter selector (`1..4`) |
| `D_800C7AB2` | live Impact stage |
| battle state `+0x60` | signed boss HP (`FUN_801D3954_5FED34` subtracts) |
| battle state `+0x64` | signed Ryo ammunition (`FUN_801D3A68_5FEE48` adjusts) |
| battle state `+0x68` | signed mech HP (`FUN_801D38A4_5FEC84` subtracts) |
| battle state `+0x1D8` | boss root task |
| battle state `+0x1E0` | boss model object |
| battle state `+0x2C0` | combat-pause byte |
| battle state `+0x2C8` | per-encounter battle clock |
| `func_801E4800_60FBE0` | Kashiwagi root initializer |
| `func_801EF2E0_61A6C0` | Thaisamba root initializer |
| `func_80200200_62B5E0` | Balberra root initializer |
| `func_801FAEB0_626290` | D'Etoile root initializer |

The root's model object uses the standard display layout: position at
`+0x08/+0x0C/+0x10`, rotation halfwords at `+0x14/+0x16/+0x18`, scale at
`+0x1C/+0x20/+0x24` and animation frame at `+0x28`. The task's AI callback is
at `+0x0C`, timer `+0x8A`, colliders `+0x3C/+0x3E/+0x40` and remove-pending
bit `0x2` at `+0x68`.

## Network contract

`py/anchor_impact.py` uses the transient, team-scoped `MNSG_IMPACT` protocol
with protocol version `1`. The transport room tracks the live Impact stage, so
peers on a different giant-robot encounter never exchange checkpoints. The
checkpoint payload is `{"k":<encounter>,"s":<stage>,"r":[5 words]}`; the five
words are plain integers, so no pointer or float ever crosses the bridge.

Election, checkpoint coalescing, keepalive, lease expiry, hit retry and
acknowledgement reuse the shared `anchor_boss_transport.py` core used by
Congo, Dharumanyo and Tsurami. Impact opts out of the save and ordinary-room
requirements (`require_save=False`, `require_room=False`) because the boss rush
runs from the title menu without either, and instead scopes checkpoints with
`state_scope` on the checkpoint's own stage. The Impact advertisement is
included in `UPDATE_CLIENT_STATE` so peers discover each other's readiness.
Hot packets are never placed in Anchor's durable queue.

## Verification

* `tests/test_impact_transport.py` covers the strict checkpoint schema,
  metadata versioning, stage scoping, two-client election and follower
  adoption, cross-stage packet rejection and packet budget.
* `tests/test_impact_codec.c` covers the native checkpoint encode/decode and
  malformed-input rejection.
* The release and debug MIPS packages build and contain the six native hooks
  and the embedded `anchor_impact.py`.

## Known limitations (runtime validation still required)

This change mirrors the boss HP, mech HP and Ryo ammunition for all four Impact
bosses, in both story stages and the title-menu boss rush. It does **not** yet:

* mirror the boss's own transform/phase (followers run their own boss AI and
  share only the health and ammunition pools), so the boss may move slightly
  differently on each screen;
* reconstruct the boss's attack projectiles, so followers see the boss phase
  and pose but not its travelling shots;
* replicate the authority's mech transform, so each client still sees and
  pilots its own local mech while sharing the health and ammunition pools;
* hide the remote on-foot player models that the ordinary room pipeline would
  render inside an Impact stage.

With two updated clients on the same Room ID, verify each boss: both clients
enter the same Impact stage, one becomes the authority, the boss HP / mech HP /
ammo match on both HUDs, follower hits reduce the shared boss HP, and damage to
the mech is shared. Then check a client that joins mid-fight and an authority
that leaves mid-fight.
