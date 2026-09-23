# C source layout

The mod's C sources in `src/` and public headers in `include/` use matching
feature directories. Put a new file beside the code whose behavior it owns;
do not add new C files or public headers at either root.

| Directory | Ownership |
| --- | --- |
| `core/` | Anchor transport, runtime state, and dialog integration |
| `player/` | Remote player actors, models, animation, motion, appearance, sounds, and model pool |
| `combat/` | Collision, player attacks and damage, projectiles, render scratch, and render budget |
| `bosses/` | Shared boss state, invitations, and arenas; boss-specific code and codecs live in `bosses/congo/`, `bosses/dharumanyo/`, `bosses/impact/`, or `bosses/tsurami/` |
| `world/` | NPCs, enemies, room actors, mechanisms, dungeon maps, and world codecs |
| `progression/` | Item and flag synchronization, rewards, and reconciliation |
| `race/` | Race-specific gameplay patches |
| `ui/` | Connection/startup UI, nameplates, and game-sourced icons |
| `utils/` | Only reusable array, JSON, room, string, and texture utilities |
| `platform/` | Headers for the recomp/modding and graphics APIs (`include/` only) |

Use paths relative to the `include/` search root for public headers, such as
`#include "world/anchor_world.h"` or `#include "bosses/impact/anchor_impact_native.h"`.
Keep a private header beside its `.c` file and include it by basename; for
example, `src/progression/anchor_item_reconcile.h` is private to progression.
Keep `.inc` implementation fragments beside their including `.c` file:
`src/world/anchor_world_dynamic.c` includes its world fragments locally.
Data `.inc` files used by multiple source files belong under the owning
public header directory and are included by the path from `include/`.

`Makefile` recursively discovers `src/**/*.c`; check path-specific object
flags when moving a source. After a move, update direct source includes in C
tests and source/header path literals in tests, tools, and active Markdown docs.
Leave historical `docs/*validation*.json` paths and hashes unchanged unless
deliberately regenerating and revalidating those artifacts. Then run the focused
checks and `bash build_mod.sh -j4`. The release and debug packages must both
build. Path changes should not change network protocol or gameplay behavior.
