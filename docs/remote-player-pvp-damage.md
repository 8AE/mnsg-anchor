# Remote player damage intake

The receiving client applies a PvP hit only to its current real local player.
Remote render tasks remain visual actors with the separate collision wrapper;
they are never passed to the native playable-player damage routines.

`anchor_player_damage_apply(x, y, z, damage)` runs at the real local player's
`func_801CB824_587734` pre-update entry, after transport validation. The helper
temporarily supplies a type-1 generic attacker descriptor to the local task's
incoming-hit slot, calls native intake, and restores that slot before returning.
The descriptor is not inserted into any task, actor, or renderer list.

The sender reads the outgoing attack's native kind byte at task `+0x4c` when
the native sphere scanner visits it. The ordinary actor intake in
`func_80218350_5D3820` subtracts 2 units for kinds `0x16`/`0x23` (silver),
3 for `0x1b` (Fire Ryo), 4 for `0x17`/`0x24` (gold), 8 for `0x22`, and 1 for
other kinds such as base `0x15`/`0x19` and ordinary Ryo `0x1f`. Goemon and
Ebisumaru use `0x15`/`0x16`/`0x17` for weapon tiers zero through two;
Sasuke and Yae use `0x19`/`0x23`/`0x24`. These are native half-heart units.
The sender includes that bounded amount in the same
transient `MNSG_PLAYER_HIT` packet as the hit kind and position. The receiver
accepts only the mapped values 1, 2, 3, 4 and 8 and rejects noninteger values;
an older
packet with no amount retains the former one-unit baseline. Both clients need
this update for source-specific damage.

## Ebisumaru meat-hammer life steal

A meat-hammer strike on a remote player heals the attacking Ebisumaru by
the HP that the victim's native hit intake actually removed. Armour and
invulnerability grant no healing; lethal hits grant only the remaining HP,
and Sudden Impact's native vulnerability multiplier is included. No Hit
race mode's additional forced death is excluded from the weapon's damage.
Native healing clamps at Ebisumaru's maximum HP and plays its normal sound.

The sender identifies the real playable task, character byte `+0x60 == 1`,
and native attack-kind byte `+0x4c == 0x1d`. Decompilation and disassembly of
`func_801E3834_59F744` show selected-weapon-2 table `0x802046c8` assigning
`[0x15, 0x1d, 0x1c, 0x15]` by character. Ordinary Ebisumaru mallets use
`0x15/0x16/0x17`, so owning the hammer alone does not enable life steal.

Only these strikes add `lifeSteal: 1` to `MNSG_PLAYER_HIT`. After applying
the validated hit, the victim consumes a local receipt and sends one direct,
transient `MNSG_PLAYER_HIT_RESULT` with the original room, sessions, epochs,
hit sequence and measured `healthLoss`. The attacker accepts that result
once against a pending outgoing hit, then applies healing only to the same
living local Ebisumaru lifetime. A confirmed lethal result remains valid
after the victim's death pose. Receipt and pending storage are bounded to
64 entries, native drains to 16 per frame, and results expire after 1000 ms.
If pending confirmation storage fills, additional hits retain ordinary PvP
damage while omitting life-steal confirmation tracking for those hits.
Both packet types have a 512-byte serialized budget; there are no retries,
periodic sends, team broadcasts or durable queued events for this effect.

The actual HP gained is excluded from team HP synchronization, preserving
unrelated damage/healing in the same frame without healing the victim back.
Both clients need this update for life steal; older clients continue their
ordinary PvP behavior and do not produce healing confirmations.

`bash tests/run_player_lifesteal.sh` exercises native loss/healing wrappers
with mocked game functions and the production transport's two-peer
hit/receipt/result flow. `bash tests/run_player_chain.sh` covers attack
classification and per-target deduplication. These checks and MIPS builds
do not establish fresh in-game two-client behavior.

## Chain-pipe player grapples

Goemon's chain-pipe head and links treat a multiplayer player overlap as a
grapple opportunity. When A chains B, the native block-grapple latch pulls A
toward B using the ground pull/release actions `0x73`/`0x74` or the airborne
actions `0x7A`/`0x7B`. Chain contacts never send `MNSG_PLAYER_HIT`, including
when a grapple is declined. Ordinary native block and enemy contacts retain
precedence across every victim group in the collision pass; the copied remote
contact commits only at the following native chain-head update.

The latch receives temporary target records containing copied coordinates.
They are never registered as native actors or victims, and the native latch
retains no target pointer. Its native vertical alignment places A's feet at
B's feet. During the pull, the native chain endpoint and horizontal steering
follow the current B position. Contact between the two player bodies starts
native release, since the native block arrival radius is smaller than solid
multiplayer bodies. A changed target session/epoch, vanished or ineligible
target, scripted control, vertical movement beyond the native arrival
tolerance, or a 180-tick blocked pull also releases the chain. Local hurt and
character-change states retain control of their own native actions.

The existing player-pose stream carries the chain phase and endpoint for
remote presentation. The chain head and links are excluded from thrown-shot
capture, consumption and stop messages; other projectile families retain
their existing handling.

`bash tests/run_player_chain.sh` checks attack damage suppression, real native
wrapper calls with mocked native behavior, collision-group precedence,
session/lifetime changes, moving targets, native release, and projectile
exclusion. These host checks and MIPS compilation do not verify fresh
two-client grapple motion or animation playback in the game.

### Comparing native pull speed

The native latch writes acceleration `1.5606060028` to player work `+0x04`
and speed caps `18.7272720337` to task `+0xE4/+0xE8`. Work mode `3` supplies
the forced pull heading. The multiplayer wrapper uses this native latch;
it does not multiply those values. The host fixture checks these exact native
values across ground, alternate and airborne action entry and target updates.

A firing-client diagnostic captured a floor-level pull whose speed stayed at
one acceleration step despite retaining mode, acceleration and caps. Native
`func_801CD310_589220` explains that result: on floor types `0x95` and `0x9A`,
a derived input magnitude below `0.1` discards accumulated velocity even when
forced movement mode is active. It then adds the native acceleration again,
producing the observed `1.560606` speed plateau with neutral stick input.

For an eligible active player grapple on those two floor types, the wrapper
temporarily supplies the existing native forced acceleration as that physics
call's non-dead input magnitude. Native code still calculates velocity using
its own heading, acceleration, collision and caps. The original input float
is restored bit-for-bit on return, including when the player changes during
the call. Other movement, native block pulls, raw controller records and
cached inputs retain their existing values. No speed multiplier is applied.
The host physics fixture reproduces the old plateau and checks accumulation
to the native cap with neutral input across ground, alternate and air pulls;
a fresh in-game run remains necessary to confirm the corrected travel rate.

The debug package records `[chain_pull]` diagnostics for player and ordinary
block grapples. To compare them, manually launch the game with its output
redirected, perform one player grapple and one ordinary chain-block grapple
in the same session, then inspect `/private/tmp/mnsg-chain-pull.log`:

```sh
"/Applications/Goemon64Recompiled.app/Contents/MacOS/Goemon64Recompiled" > /private/tmp/mnsg-chain-pull.log 2>&1
```

Records identify `peer` or `block` and pair `before`/`after` movement samples.
They include action, chain phase, movement mode, the player `+0x61` movement
bypass state, work `+0x69` action lock, gravity, derived input magnitude,
acceleration, caps, velocity,
previous velocity magnitude (`b0_1000`), collision flags, ground contact and
surface normal. `*1000` fields use native units multiplied by 1000;
`xyz100` and `delta100` use native world units multiplied by 100. A nonfinite
or out-of-range diagnostic value is printed as `2147483647`.

Each pull records the first 12 movement ticks, then ticks 30, 60, 90, 120, 150
and 180, with a player-latch commit/release record when available. Release
packages omit the diagnostic hooks and logging import. These records expose
the firing client's actual movement; a post-fix recording can confirm native
velocity accumulation and compare it with the chain-block baseline.

## Frozen cube impacts

The thrown frozen cube arms a fixed native impact sphere with kind `0x17`,
the gold tier's four-unit native damage in the common actor intake. Its PvP
splash sends four units per intersecting remote player other than its frozen
occupant, once per target per impact, independent of the thrower's equipment.
The carrier's target client ID and player epoch identify that occupant;
another life of the same client remains eligible. The existing
cube throw, impact control and thaw messages retain their timing. Individual
actors with custom hit handlers may react differently to the native attack
kind; those handlers need an in-game check. The four-unit value is established
for the common actor intake.

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
| attacker `+0x6d` | byte | Validated source damage, 1/2/3/4/8 half-heart units |
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
