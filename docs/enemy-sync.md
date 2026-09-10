# Regular enemy synchronization

Regular enemies are distinct from the shared boss fights: they are numerous,
transient, and respawn whenever their room is loaded. This feature synchronizes
them in two layers:

1. **Live authority simulation** — one elected occupant of a room runs the
   native enemy AI and streams each live enemy's transform, animation and
   health. Every other occupant holds that enemy's AI and mirrors the stream,
   so all players see and fight the same enemy while the room stays occupied.
2. **Defeat persistence** — an enemy killed by any player is removed from every
   teammate's room, and the dead set is remembered for the room's occupancy
   epoch so a late entrant does not re-fight it. Once everyone leaves, the next
   visit respawns the room's enemies normally.

Enemy-owned attacks and hazards are **not** streamed yet; see
[Limitations](#limitations).

## Live authority simulation

`get_enemy_authority` elects the lowest client id among same-team, save-loaded
peers currently in the local raw room (the local client counts, so a lone
player is their own authority). The authority's role is published to native code
through `anchor_get_enemy_authority`, which runs once per frame from
`item_sync_update`.

- **Authority** (`ENEMY_ROLE_OWNER`): runs the native AI untouched and captures,
  for every live roster enemy, `index, actor id, health, x, y, z, yaw, animation
  clip and frame`. It broadcasts `MNSG_ENEMY_LIVE` to the team on a fixed
  low cadence. It also gathers every live player — local and visible
  teammates — and makes each enemy pursue whichever one is **nearest to it**
  (see [Player targeting](#player-targeting)).
- **Replica** (`ENEMY_ROLE_REPLICA`): during the native task scheduler it
  replaces each streamed enemy's AI callback (`task+0x0C`) with a no-op and
  zeroes its velocity, then after the scheduler applies the authority transform
  with bounded convergence. Animation clip changes are requested through the
  native `func_8021664C_5D1B1C` setter and the frame is written directly.
  Health is pinned to the authority value each frame.

A role swap (the elected client changes), a room change, or a disconnect
releases every held AI callback and invalidates the stream.

### Shared health

The authority owns a single health pool per enemy. A replica's local player
attacks still run through the native damage pipeline on the mirror; the exact
health delta removed by `func_80218350_5D3820` is captured and forwarded to the
authority as a targeted `MNSG_ENEMY_HIT`. The authority applies non-lethal
damage directly to the shared health and, for a lethal amount, arms the native
zero-health path so the real reaction, counters, effects and cleanup run and the
death broadcasts through the existing defeat path. A `func_80218E7C` entry hook
skips forwarding the synthetic remote kill so it is never echoed back.

### Player targeting

Goemon is a single-player engine: enemies pursue "the player" through the
current player display object `D_801FC60C_5B851C` (read directly, or via the
player task `D_801FC604_5B8514` at `+0x18`, or through the target-object field
`task+0x84`). To make room enemies pursue a teammate, the authority first
collects every eligible player's collision-body position
(`anchor_player_models_get_boss_targets`). Inside each enemy's own AI callback
the authority then selects the candidate **nearest that enemy**, and — when
that candidate is a remote peer — temporarily moves the player object onto the
candidate's position, calls the enemy's real AI, and restores it. The local
player's own update, rendering and camera never observe the temporary position
because the swap is scoped to the enemy callback. A remote teammate is only
chosen when it is actually the closest player, so enemies still chase the host
when the host is nearest.

The swap is delivered through a per-enemy **AI trampoline**. On the authority
the scheduler begin installs the trampoline over each tracked enemy's `+0x0C`
callback, and the scheduler end restores it. Because the native AI advances its
state by rewriting `+0x0C`, the trampoline saves whatever callback it replaced,
calls it, and only restores when the AI did not move to a new state; a state
change is preserved and re-wrapped on the next frame. The trampoline also
substitutes the generic `+0x84` target object during the native turn helper
`func_802197D8_5D4CA8`, covering actors that turn through that field.

With no remote teammate present the candidate list is local-only, the
trampoline is not installed, and single-player behavior is unchanged.

### Wire format

`MNSG_ENEMY_LIVE` carries `{"r":room,"s":roster-signature,"q":sequence,"d":[…]}`
where `d` is a flat integer array with a nine-integer stride per enemy. A client
accepts a stream only from its elected authority, only for a matching roster,
and only with a strictly newer sequence; an authority change resets the
sequence. A malformed snapshot is fully validated before any enemy state is
mutated, so it can never leave a partially-applied set. Positions are whole
world units and the animation frame is fixed hundredths; live packets are capped
at 3072 bytes by the Python receive budget.

## Enemy identity

Static room actors are described by a native `0x14`-byte instance record
(`position`, `rotation`, `definition`, `is_spawned`). The mod enumerates the
room's actor-data wave and builds a deterministic **roster** that indexes those
records:

- the normal instance array, then the partitioned/proximity lists, deduplicated
  and ordered by source address so every client derives the same index;
- each index records the resolved actor id and mixes it, the index, and the
  instance position into a 16-bit **signature**.

A death is therefore identified by `(room, signature, index, actor_id)`. The
signature rejects clients whose room layout differs (randomizers, version
mismatch), and the actor id prevents a recycled address from matching the wrong
enemy. Bosses, reward controllers, spawners, projectiles, hazards and
destructibles are excluded by an explicit sorted actor-id table in
`src/enemy_sync.c`.

## Native integration

The roster is built from the native actor-data/spawn stage
(`func_8020D848_5C8D18`) and each static enemy is bound when its initializer
runs (`func_80218A54_5D3F24`). If a death predates the actor's spawn, the
initializer marks it `remove-pending` so the game's own cleanup consumes it
without replaying drops.

A remote death is applied through the native damage pipeline: at the last
point before the common actor update (`func_80218E7C_5D434C`) the actor's
health is clamped to `1` and the synthetic fast-hit bit `0x00040000` is set, so
`func_80218350_5D3820` runs the real zero-health reaction, animation, counters,
effects and cleanup. The authoritative local death edge is the native
health-zero path (`func_80218548_5D3A18`); unloads and distance culls do not
call it. `func_80218F30_5D4400` clears stale actor ownership on removal.

The roster check is stored as a sorted table rather than a `switch`: LLVM lowers
a dense switch to a MIPS jump table whose computed branch the live recompiler
can mistake for an indirect function dispatch and crash. This matches the fix
already used by the collision enemy collector.

## Packets

| Packet          | Audience            | Purpose                                        |
| --------------- | ------------------- | ---------------------------------------------- |
| `MNSG_ED`       | team room broadcast | One observed local enemy death.                |
| `MNSG_ER`       | team room broadcast | Request the room's current bitmap.             |
| `MNSG_ES`       | team broadcast / direct reply | A client's compact dead-enemy bitmap. |
| `MNSG_ENEMY_LIVE` | team broadcast    | Authority's live transform/animation/health set. |
| `MNSG_ENEMY_HIT`  | direct to authority | A replica's forwarded local damage delta.     |

Each occupant retains a trimmed 256-bit bitmap (one bit per roster index) in
its replace-style Anchor client state as `er`/`es`/`eb`. Because Anchor replaces
the stored client-state object, `update_client_state` re-sends the complete
enemy record on every metadata update, so room/character/race edges cannot drop
it. Python validates and normalizes the hex (lowercase, high zero bytes
trimmed, leading low-index zero bytes preserved) and ORs matching teammates'
bitmaps for `get_enemy_room_state`.

A client entering an occupied room imports the merged bitmap before spawning
actors, then publishes its own state and sends a bounded request as a fallback.
Python elects a single matching room occupant to answer (`MNSG_ER`), so a busy
room does not answer once per player.

## Limitations

- Enemy-spawned attacks, projectiles and hazards are not streamed. A replica
  freezes the enemy body but does not reconstruct the shots it fires, so those
  attacks land only on the authority's player. Replicating them is the next
  step and would reuse the Congo child-seed pattern.
- Only the enemies present in the authority's live roster are mirrored. A
  proximity-spawned enemy that appears later joins the stream on the next
  snapshot; one that the authority has already removed is covered by the
  defeat bitmap.
- Targeting relocates the shared player display object, so it covers enemies
  that read the player position through the player object, the player task's
  `+0x18`, or `+0x84`. Enemies that cache the player task pointer and read
  task-only fields, or that use a separate hard-coded player structure, may
  still pursue the authority's player.
- Each enemy picks its own nearest player, but the choice is expected to be
  stable frame to frame; there is no hysteresis, so an enemy equidistant from
  two players could switch targets on consecutive frames.
- Health applied to a replica is pinned from the authority each frame, so a
  replica sees the shared HP rather than its own local damage total.

## Validation

`tests/test_enemy_room_state.py` covers hexadecimal normalization and
endianness, publish replacement semantics, invalid-value rejection, peer
filtering and OR-combination, snapshot/incremental merge carrying `er`/`es`/`eb`
without erasing them, answer election, authority election (lowest matching room
occupant, self when alone, room/team/save/online filtering), and
disconnect/connect cleanup. The MIPS package builds and links with the new
native hooks, and the mod tool accepts the enemy hooks and produces the `.nrm`.

Host tests and a successful package build are not native gameplay proof. A
two-client run still needs to verify that teammates in one room cannot fight
duplicated enemies, that both players' attacks drain one health pool, that the
enemy visibly turns toward a remote player, that a killed enemy disappears for
everyone, that leaving and re-entering once everyone has left restores vanilla
respawns, and that a late entrant to an occupied room receives the current live
set and dead set without duplicate drops.
