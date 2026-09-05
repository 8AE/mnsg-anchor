# Remote player damage intake

The receiving client applies a PvP hit only to its current real local player.
Remote render tasks remain visual actors with the separate collision wrapper;
they are never passed to the native playable-player damage routines.

`anchor_player_damage_apply(x, y, z)` runs at the real local player's
`func_801CB824_587734` pre-update entry, after transport validation. The helper
temporarily supplies a type-1 generic attacker descriptor to the local task's
incoming-hit slot, calls native intake, and restores that slot before returning.
The descriptor is not inserted into any task, actor, or renderer list.

## Verified native interfaces

Verified using live Ghidra decompilation/disassembly and the current
`Goemon64RecompSyms/mnsg.syms.toml` / `mnsg.datasyms.toml` on 2026-09-05.

```c
int func_801D9E9C_595DAC(void *player);
void func_801E8E24_5A4D34(void *player, unsigned char reason);
int func_801DCD48_598C58(signed char delta);
```

`func_801D9E9C_595DAC` first services zero health, floor hazards, and crushing,
then reads the incoming attacker from player task `+0x38`. For attacker type 1,
the descriptor reads are:

| Field | Native type | Meaning |
| --- | --- | --- |
| attacker `+0x18` | object pointer | Attacker object used for hit direction |
| attacker `+0x4c` | byte | Hit type; 1 selects ordinary damage |
| attacker `+0x6d` | byte | Damage amount; 1 is the ordinary half-heart baseline |
| object `+0x08/+0x0c/+0x10` | three floats | Hit-origin world coordinates |

The type-1 branches do not retain this descriptor. The native function's
descriptor-retaining interaction at player `+0xe0` belongs to type 2, which the
helper never supplies. The ordinary branch calls `func_801DA67C_59658C` for
armour, `func_801DA708_596618` / `func_801DA758_596668` for health and attack-state
cleanup, and `func_801CF51C_58B42C` plus `func_801DA84C_59675C` for normal hurt
direction and reaction. Other current-player action contexts follow the native
water, rope, and transformed-character branches.

`func_801DA758_596668` reads the descriptor's damage byte, uses a one-point
fallback for zero/null damage, and doubles the loss for Goemon's active Sudden
Impact vulnerability. It immediately clears ordinary incoming collision bit
`0x01` at player `+0x30`. That prevents a second queued PvP hit during the initial
hurt animation. Recovery through `func_801E3EBC_59FDCC` calls
`func_801E3134_59F044` / `func_801E3158_59F068`, setting the player's `+0xd4`
invulnerability countdown to 60. Armour-blocked type-1 hits also start this
countdown even though intake returns zero.

The ordinary `func_801CB824_587734` caller invokes
`func_801E8E24_5A4D34(player, 1)` when intake returns nonzero. The helper mirrors
that call. Its verified assembly at `0x801e8e24` reads the owned-projectile list
through task `+0xdc`, filters projectile types using `D_80204914`, then marks
eligible projectile task bytes `+0x65 = 1` and `+0x66 = reason`.

`func_801DCD48_598C58` accepts a sign-extended byte in `a0` and returns an integer
clamp indicator. Assembly at `0x801dcd48` shows signed-byte extension; subsequent
loads/stores update the complete 32-bit current-health word at `0x8015c5e4`
(whose low byte is `0x8015c5e7`). Negative deltas clamp below zero; positive
deltas clamp to the maximum at `0x8015c5e0` and play the native healing sound.
It adjusts health alone; death/action handling belongs to native player intake
and the normal hurt update.

## Intake guards and accounting

The helper requires a linked current player, its current object and work area,
a valid owned-projectile-list pointer, loaded save, positive health, normal
incoming-hit enablement, no invulnerability, no current native attacker, and no
character-resource swap in progress. It rejects reentrant calls and discards
further accounting if the current player changes during the call.

The native pre-update permits intake only with `D_800C7AE2 == 0`,
`D_800C7AE3 == 0`, and `D_800C7AE0` equal to 0 or 4; these exact byte exports
have no ROM suffix. The helper matches that availability and the shared
scripted-state collision gate.

To preserve environmental-hit priority, it refuses PvP when floor-contact IDs
at player `+0x96` or `+0x98` equal `0x92` or `0x93`, or crushing byte `+0x63` is
set. These are the exact early checks in `func_801D9D74_595C84` and
`func_801D9E28_595D38`. The ordinary native pre-update remains responsible for
those events, so their HP loss cannot be classified as PvP and hidden from team
damage synchronization.

After a PvP health loss, `item_sync_exclude_pvp_damage` subtracts only that loss
from the team's comparison baseline. This retains an unrelated enemy hit or
heal earlier in the same frame. The baseline is signed to retain a full heal
even if its adjusted comparison value becomes negative. No Hit races use the
native health-adjustment function to reduce the remaining health to zero and
exclude that PvP-induced loss from team echo as well. Native death handling
then executes through the normal player update.

## Verification and limits

`tests/test_player_damage.c` mocks native intake and checks descriptor scope,
slot restoration, reentry/replay protection, native armour/Sudden Impact
delegation, projectile cleanup, availability and hazard guards, owner changes,
lethal and No Hit behavior, and same-frame team-accounting deltas. Its private
test arena models the low address bits required by the pointer guards; it does
not inspect or modify a running game.

Host tests and MIPS compilation pass. These verify the wrapper and its native
interfaces, not fresh two-client combat behavior. Real hurt animations,
projectile cancellation, armour depletion, transformed-character reactions,
and death/respawn still require live two-client certification.
