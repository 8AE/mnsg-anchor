# Dharumanyo native evidence

This note records the native boundaries used for the shared Dharumanyo fight
and reward. It is source-level evidence, not rendered runtime certification.
The review used the US recompiled instruction output and complete ROM-suffixed
symbols; no game was launched.

## Actor topology and combat

| Native boundary | Evidence and consequence |
| --- | --- |
| `func_08000104_6C8314` | Creates the visible actor and establishes the bidirectional link at task `+0xDC` to the hidden carrier. The visible actor owns AI, movement, animation and projectile spawning. |
| `func_08003E64_6CC074` | Initializes the entity-`0xCC` carrier with 12 real lives at `+0xD1`. Actor identity is `+0x5C`, entity identity is `+0x5E`, generation is `+0x74`, display object is `+0x18`, and the task-list backlink is `+0x04`. These fields fence retained native handles. |
| `func_08003F84_6CC194` | Runs the carrier's ordinary collision/damage pass once per scheduler frame, resets generic HP `+0x8D` to 10, advances orbit yaw `+0xD8` and writes the carrier position. A synchronized follower keeps this pass; only shared life loss is intercepted. |
| `func_80218350_5D3820` | The carrier calls the common inner damage routine directly. Accepted local contact is consumed before it can alter the replica independently. The owner replays one verified native unit-hit input during a carrier pass. |
| `func_0800432C_6CC53C` | Consumes one real life. When the final life enters this callback, the stock destruction chain begins. Shared terminal adoption queues this boundary rather than setting a completion flag or jumping to a later callback. |
| `func_08000970_6C8B80` | First stable neutral combat callback after the native intro. It is the safe ready boundary for capture, election and late checkpoint adoption. |
| `func_08003810_6CBA20` | Visible root private post callback. Followers preserve it so local collision, rendering and native post work continue while autonomous AI is held. |
| `func_80034734_35334` | Loads each task's resource context, then invokes pre (`+0x08`), AI (`+0x0C`) and post (`+0x10`). Pending checkpoint holds are still installed when pre runs; root, carrier and projectile identity must consult saved native post callbacks while held. |
| `08003844-0800388C` in `func_08003810_6CBA20` | Mirrors script flag `0x16C` into global `D_8015CC30 + 0xD4` bit 0. Controller `func_08006100_6CE310` clears `0x16C` and input ownership, leaving the next root post to clear this mirror. A hold based on the mirror itself prevents that cleanup. Track only mirrors observed from this root; other native systems also write the global bit. |

The verified visible-root callback graph begins at `08000970` and contains 31
combat callbacks through `080025C8`. The wire sends their bounded phase index,
never a function pointer. Root state also includes timer `+0x8A`, animation
clip/frame, selected private attack fields, position, rotation, scale, velocity
and collider extents. Contact pointers and transient local collision bits stay
local.

The root's target object pointer is at task `+0x84`. Verified target reads occur
in `08000BD8`, `08001260` and `08001CC8`, including the projectile attack's aim
setup. The authority temporarily substitutes a stable position proxy only
around those native callbacks, then restores the original local target.

## Travelling projectiles

| Native boundary | Evidence and consequence |
| --- | --- |
| `func_08002410_6CA620` | Allocates a file-31 projectile task, links it back to the visible root through `+0xDC`, and optionally selects a variant at `+0xD3`. |
| `func_08002620_6CA830` | Initializes entity `0xCC`, attack type 6, destination/motion fields and one of the two travelling callbacks. Only actors that retain the verified root link and native post callback enter the shared set. |
| `func_0800284C_6CAA5C` / `func_08002A40_6CAC50` | The two accepted travelling callbacks. Their later path continues through `08002B30`, `08002C00` or `08002C74` and `08002E08`. Trail and impact children are derived local effects and are excluded from the wire. |

The native reconstruction uses the room's already loaded file-31 resource and
the stock constructor, then installs the checkpoint's current position,
destination, velocity and timer without replaying historical collision.
Ordinary local post/collision processing continues afterward. Complete
checkpoints remove authority-expired projectiles and deduplicate IDs across
retries and handoffs.

## Death and Miracle Flower

| Native boundary | Evidence and consequence |
| --- | --- |
| `080030A8 -> 0800314C -> 080036FC -> 080037C0 -> 08003804` | Native final-life destruction sequence reached from `0800432C`. Followers run the sequence locally instead of seeking into it. |
| `func_0800636C_6CE57C` | Owns the post-fight gauge/fade controller and waits for destruction state `0x16B`. |
| `func_080066E0_6CE8F0` | Combat teardown callback reached after `080065E8` raises transient state `0x16D`, which wakes the already placed reward controller. Held remote progression remains deferred at this boundary. |
| `func_08000090_72F920`, entity `0x34F` | The 29-state placed reward controller. Its identity-checked return observer releases held remote progression only after native scenario `0xD0` has set flag `0x18`; the original controller continues through states 25-28. |
| Scenario `0x72`, file 93, segmented `0x08002CDC`, ROM `0x74220C` | Started by Dharumanyo's reward controller. Its command at ROM `0x742284` writes `1` to `0x8015C860`, save offset `+0x258`, the existing `mi_flower` field. |
| Scenario `0xD0` | Runs later in the reward controller. It sets the remaining story state and durable packed flag `0x18` (`fl_dharmanyo`) before the final control-release scenario. |
| Scenario `0x73`, file 93, segmented `0x08002D88`, ROM `0x7422B8` | Belongs to Tsurami's reward controller and writes `mi_star` at save offset `+0x250`. It is not Dharumanyo's reward. |

The exact reward order is Miracle Flower, story flags `0x73/0x70/0x71`, story
flag `0x72`, `fl_dharmanyo`, then the final control release. The multiplayer
bridge releases its held durable completion after observing the native
`fl_dharmanyo` write, without taking control of the remaining reward states.
Room exit actors
read `fl_dharmanyo`; they do not read the inventory field. The generic durable
field sync already carries `mi_flower`, so a player elsewhere or a later save
reconciliation receives the reward without a separate transient packet.
