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

The elected authority publishes one checkpoint containing:

* the **boss** — the live `file_13` AI callback (so the current attack/neutral
  phase matches), the model task-object position, rotation and scale, the
  animation frame, the native timer and the collider extents;
* the **boss health** at battle-state `+0x60`;
* the **player/mech health** at battle-state `+0x68`;
* the **player/mech Ryo ammunition** at battle-state `+0x64`.

Followers freeze the boss AI and adopt each accepted checkpoint, so the boss
moves, animates and phases identically on every screen while the local
collision and rendering passes still run. Follower attacks on the boss are
reported as HP deltas and applied only by the authority, keeping one canonical
health pool.

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
checkpoint payload is `{"k":<encounter>,"s":<stage>,"r":[21 words]}`, where
float fields cross the bridge as IEEE-754 bit patterns and the root callback is
validated against `file_13`'s own executable range before it is installed.

Election, checkpoint coalescing, keepalive, lease expiry, hit retry and
acknowledgement reuse the shared `anchor_boss_transport.py` core used by
Congo, Dharumanyo and Tsurami. Hot packets are never placed in Anchor's durable
queue.

## Verification

* `tests/test_impact_transport.py` covers the strict checkpoint schema,
  metadata versioning, stage scoping, two-client election and follower
  adoption, cross-stage packet rejection and packet budget.
* `tests/test_impact_codec.c` covers the native checkpoint encode/decode and
  malformed-input rejection.
* The release and debug MIPS packages build and contain the six native hooks
  and the embedded `anchor_impact.py`.

## Known limitations (runtime validation still required)

This change mirrors the boss, boss HP, mech HP and Ryo ammunition. It does
**not** yet:

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
