# Congo native evidence

This maintenance note records the native evidence used for the shared Congo
damage and target boundaries. It is not runtime certification. No raw ROM
data is included.

- ROM: US decompressed image, SHA-256
  `e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
- Evidence source: native MIPS instruction comments emitted in the local
  `Goemon64Recomp/RecompiledFuncs` checkout, cross-checked against the complete
  ROM-suffixed symbols in `Goemon64RecompSyms`. Runtime addresses in Congo's
  actor overlay must retain their ROM suffix to disambiguate overlays.
- Verification date: 2026-09-06. No game launch.

| Native boundary | Evidence and consequence |
| --- | --- |
| `func_08005EDC_6B917C` | Root initializer sets `+8D=30`, `+60=0x002006E1`, `+64 |= 0x8000`, `+E8 |= 0x04000000` and installs private post callback `0800000C`. The six visible parts start with capability `+60=0x20`; the separate `666C` helper child uses `0x80000000`. They do not acquire the root's `0x00200000` damage-receive capability. |
| `func_80218A54_5D3F24` | Placed-actor setup stores actor identity at `+5C/+5E`, generation at `+74` (`80218B78`) and installs pre callback `8021925C` via `80035214` at `80218BFC`. `8021925C` calls `80218FE8`. |
| `func_80218FE8_5D44B8` | `8021912C` reads victim `+38`; `80219150` sets status `+68` bit `0x80`. Earlier instructions mirror receive/attack capabilities into collision byte `+30`. This pre callback remains necessary when autonomous AI is held. The same routine clears or sets callback-disable bit `0x00800000` in `+C/+10`; temporary callback ownership must account for that bit. |
| `func_80033688_34288` | After the native sphere/body test succeeds, `8003386C` stores the victim in attacker `+34`, and `80033870` stores the attacker in victim `+38`. A shared hit adapter must preserve the source attacker's native impact result. |
| `func_80218350_5D3820` | Normal intake requires capability `+60 & 0x00200000`, contact `+68 & 0x80` and nonnull attacker `+38`. Capability `0x00400000` deflects without reducing HP; status bit `1` rejects a hit during recovery. Attacker type byte `+4C` selects the damage table below. |
| `802184B4..80218528` | Nonzero HP sets status bit `1`; `+90` either supplies an immediately installed/invoked hit callback or the routine writes recovery byte `+8C=60`. Zero HP enters `80218548` immediately. Restoring HP after this routine would not undo those side effects. |
| `80218364..80218398` | Status bit `0x00040000` takes a one-point fast path that bypasses ordinary vulnerability, attacker and recovery checks. It is unsuitable for general shared player hits. |
| `func_0800000C_6B32AC` | ABI consumes both task `a0` and object `a1`; it calls `80218E7C` then `0800A228`. At `08000170`, a clear recovery bit exits before attacker cleanup. The ordinary cleanup clears root `+38` at `08000358` and root `+34` at `0800035C`. Suppressed contacts therefore need explicit victim-reference consumption. |
| `func_0800A228_6BD4C8` | `+E8` bit `0x04000000` gates root HP thresholds and death. Zero HP clears remove bit `2`, clears damage capability `0x00200000`, restores HP to `1`, sets save flag `0x1A1` and schedules `08007D24`. Other effect bits in `+E8` still run if only the health-controller bit is masked. |
| `func_802197D8_5D4CA8` | Verified signature: `int func_802197D8_5D4CA8(void *task, int turn_step)`. It reads target display-object pointer `task+84` and its XYZ at `+8/+C/+10`, forwards to `802196FC`, and returns its integer result without modification. |
| `func_802196FC_5D4BCC` | Computes target yaw and turns the actor's display-object yaw by twice `turn_step`, modulo `1024`. It returns `0` when aligned (`80219790`), `1` for one turn direction (`802197B4`) or `2` for the other (`802197C4`). Congo `080073FC` calls it through `197D8(root,1)` at `08007458` and branches on `v0` at `08007460`. |
| Congo camera `func_080083BC_6BB65C` | Entry-table location ROM `0x5E3FC4` within `D_802287BC_5E3C8C` identifies placed entity `0xCE`. Packed model metadata at ROM `0x5E4CA6` gives model/file `29` and owner category `6`. The ordinary placed-actor constructor creates its `+18` display object; `83BC` additionally stores camera/light handles at `+D0/+D4` and does not clear `+18`. Root entity `0x323` also uses model `29`, category `11`. |

| Attacker `+4C` | HP subtracted by ordinary `80218350` |
| --- | --- |
| `0x1B` | 3 |
| `0x16`, `0x23` | 2 |
| `0x17`, `0x24` | 4 |
| `0x22` | 8 |
| Other native types, including `0x15` and `0x1F` | 1 |

Congo's root sets `+64 & 0x8000`, excluding the generic drop branch in
`80218548`. Task initialization `80034A94..80034AAC` clears `+60..+EC`,
including `+90`; Congo does not install a reaction callback there. Its normal
capabilities make `80217F1C` return `0xFFFF` at `80218018` without creating a
generic death effect. The adapter verifies these prerequisites before every
delivery. This path inspects only the synthetic source's `+4C` byte, retains
no attacker pointer, and needs no actor resource-file or TLB activation.

The mod's remote projectile renderer explicitly writes task `+5C=0` when
creating a visual copy. Native locally owned projectile constructors store
the real playable task there. The damage observer accepts only the real
player or a linked attacker with that real local owner, preventing duplicate
damage submissions from remote visuals.
