# Congo native evidence

This maintenance note records the native evidence used for the shared Congo
fight, reward and exit boundaries. It is not runtime certification. No raw ROM
data is included.

- ROM: US decompressed image, SHA-256
  `e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
- Evidence source: native MIPS instruction comments emitted in the local
  `Goemon64Recomp/RecompiledFuncs` checkout, cross-checked against the complete
  ROM-suffixed symbols in `Goemon64RecompSyms`. Runtime addresses in Congo's
  actor overlay must retain their ROM suffix to disambiguate overlays.
- Verification date: 2026-09-06. No game launch.

## Miracle Moon and the Congo exit

The room `0x0016` placement records and native callbacks identify the reward
and exit independently of the boss root. The item-grant script and reward
completion are different stages; an inventory write alone cannot stand in
for the completion flag. These paths were reviewed on 2026-09-07 without a
game launch.

| Native boundary | Evidence and consequence |
| --- | --- |
| Moon reward `func_08005018_6C4768` | Placed entity `0x35F`, file `30`, rewrites its native identity to `0x350`. The initializer tests saved flag `0xA4` and skips reward setup when already complete. `08005090_6C47E0` waits for Congo victory flag `0x12B`; `08005154_6C48A4` brings the reward in, followed by idle callbacks `080051F4_6C4944` and `080052A0_6C49F0`. |
| Pickup `func_0800532C_6C4A7C` | Starts scenario `0x71` and acquires native pickup control through `80221F70`. Once this starts, preserve the local scenario and its completion path rather than treating the actor as an idle collectible. |
| Scenario `0x71` | File `93`, segmented address `0x08002C48`, ROM `0x742178`. Commands at ROM `0x7421F0/0x7421F8` write `1` to `0x8015C85C`, the signed 32-bit Miracle Moon field at save base `+0x254`. This write precedes the actor's final completion flag. |
| Completion `func_080053A4_6C4AF4` | Waits for scenario PC zero, calls `80221FB0` to release native pickup control, sets saved flag `0xA4` through `80024038`, then marks task status `+0x68` with remove bit `0x2`. Packed flag `0xA4` is save byte `+0x14`, mask `0x10`; it is unrelated to the Goemon weapon-tier word at numeric byte offset `+0xA4`. |
| Exit `func_080060A0_6F9580` through `func_08006228_6F9708` | The actual room exit is placed entity `0x34B`, file `43`, initialized as `0x23C`. `08006174_6F9654` waits for saved flag `0xA4`; `080061C8_6F96A8` starts its 120-update timer, opening sound `0x224` and capability `+0x60` bit `1` before `08006228_6F9708` continues the native opening. This is not the generic key-padlock actor. The placement at `(90,-70,-124)` pairs with a type-`0x8C` portal to room `0x28`; the return portal to room `0x1A` is separate. |
| Congo root and camera | Root initializer `08005EDC_6B917C` removes the root when saved flag `0xA4` is already set; camera initializer `080083BC_6BB65C` skips the intro. Live camera `08008A38_6BBCD8` tests `0xA4` with `0x132` and selects `080094D4_6BC774` with timer `150`. That callback clears `0x132` and views the exit; `080095B0_6BC850` adds a 20-update wait before returning to the ordinary camera. Preserve this native reaction instead of forcing camera or player movement. |

The file-29 rotating-object callbacks beginning at `08005D98_6B9038` are not
the exit identified by this room's placements. They must not be used as a
substitute for the verified file-43 door.

## Fight and victory

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
| `func_08008560_6BB800` | At camera countdown `+8A == 25`, queues Congo music using `void func_80038B98_39798(unsigned short)` with cue `0x003D`. The constructor starts at 30. Skipping this early intro must preserve that cue; later intro states have already consumed it. |
| `func_80023E94_24A94` | Signature `int func_80023E94_24A94(int event_id)`. Reads the transient event bit bank; event `0xB` returns 0 or 8, requiring boolean normalization. Congo `A228` sets it at HP 25/20/15/10/5 and clears it at other positive values. Hyper Congo's return hook can clear it while HP remains on a consumed threshold, so HP alone does not describe the settled camera event. |
| `func_08008E54_6BC0F4` | Ordinary camera `08008A38` selects this callback when event B is set. Reads u16 system `+0x3ADCE` modulo 4 and sets camera target Y offsets `[30,33,30,27]` when squared player distance is above 10000, otherwise `[10,13,10,7]`. Returns to `8A38` after B clears. A scoped clock hook preserves the local view and native callback lifecycle. |
| `func_08007C18_6BAEB8` | Spin winddown starts at 60 ticks and reasserts root `+E8 & 0x80` every tick at remaining timer <=30. This repeated pulse lets all twelve rays transition even though each fading child can consume the same bit. |
| `func_080009C4_6B3C64` | `077C` selects this ray fade callback upon observing parent bit `0x80`; there is no extra initializer. It clears that parent bit at `AA8..AB4`, follows parent yaw, reduces ray byte `+EC` from 64 toward 4 by 2, updates native material colors, and removes the native current task at 4. Only use initialized `05F4` rays with live parent, current-task ownership and file29 context. |
| `func_080005F4_6B3894` | Ray material `+E8 & 0x4000` initializes XYZ scales to 1.1; the others start at 1.0. `077C` grows Z by 1 per update up to 30 for all variants. The twelve direction/material identities use masks `1/2/4/8` combined with `0`, `0x100`, or `0x4000`. Local contact bits `0x400/0x800` are not checkpoint state. |
| Victory `08007D24` through `080083A0` | `7D24` sets events 1..5 and installs `7DEC` with timer120. `7DEC` runs the explosion interval, then `8194` waits30 ticks and sets event0. `820C` initializes a white overlay; `8280` raises its opacity by4 to255; `82F8` lowers it by5 to0 and releases it. `833C` subsequently sets flags `0x12B/0x12E`, disables root drawing and clears `D_8015C562`. Installing a later callback from a checkpoint can skip the sole release path. |
| `func_8022026C_5DB73C`, `func_80220210_5DB6E0`, `func_80220168_5DB638` | White fade state is in the controller at full system `+0x3B018`, not Congo's root `+DC`. Controller bytes `+D/+E/+F` are RGB and `+10` is opacity. Initialization calls `80036B7C` to attach a render object to the root. Fade-out reaches zero before calling `80036158(root,controller,1)` to unlink/free it. The step helpers' return values are unspecified if the global controller pointer is null, so an uninitialized fade cannot be advanced safely. |
| Camera `08009184` through `080093FC` | The native death camera has its own letterbox/control chain. `9298` waits for completion flag `0x12E`, then `930C` waits90 ticks; `934C` restores the viewport and `93FC` restores player control. None frees the white overlay. Early synthetic completion flags can therefore unlock a fully white view. |

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
