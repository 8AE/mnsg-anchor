# Shared room NPCs and world objects

The world-sync layer uses the same guarded normal-plus-partition actor roster as
regular enemies. It also runs in rooms with **zero enemies**, including towns.
Only peers with the same room, team, save-loaded state and actor-layout signature
exchange state. Both players need this version of the mod.

## Implemented coverage

| Family | Shared state and behavior |
| --- | --- |
| Placed NPCs using native NPC setup | Position, all three angles, scale/visibility, animation clip/frame/rate, velocity and path checkpoint. This includes ordinary town NPCs, named NPCs using that setup, and signposts. |
| Placed birds/dogs `0x2C2–0x2C4` | Native path-based movement and animation, without trying to start dialogue on animals. |
| Moving platform `0x3E0`, variants `0–13` | Native phase, timers, motion, angles, saved origin and fall state. Rectangular, orbiting, bobbing and rotating variants retain local motion between checkpoints. The player riding a falling platform can take ownership of that actor. |
| Ryo `0x82/0x83`, health `0x84/0x85` | Actual collection removes the placed object on other clients and later same-room entrants. Rewards remain governed by native item/race rules. Known remote collection suppresses the local award callback. |
| Breakable item containers `0x192` | Actual break marks the placed container removed for other clients and later entrants. Distance culling does not count as breaking. |
| Timed emitters `0x19A`, subtypes `0–2` | Shared countdown and emission counter. A newly observed cycle runs the local native emission once; duplicate checkpoints do not respawn it. |
| Platform variant 9 child hazards | Shared cycle counter reproduces a newly observed native emission once, independently of smooth bobbing motion. |
| Existing progression objects | Save flags, dolls, keys, upgrades, relevant doors and boss rewards continue through their existing synchronization modules. |

A player talking to an NPC temporarily owns that NPC. Dialogue, camera/control
state, task pointers and native contact dispatch stay local. A replica does not
execute another player's path script. The path checkpoint is retained for a
subsequent ownership handoff, with route identity and instruction boundaries
validated against all 163 native routes.

Each actor chooses an active loaded peer, preferring its current interaction and
then the lowest client ID. This allows one player to keep an actor running when
another has distance-culled it. Missing/stale snapshots release native control;
a missing actor is never interpreted as a collection or destruction event.

## Transport

`MNSG_WORLD` is a quiet, transient `targetTeamId` packet with **no offline queue**.
The server supplies the authoritative root `clientId`. Metadata is
`worldSync = [version, interactionSession, visit, rawRoom, rosterSignature]`.
Every packet also carries this metadata, a sequence, part index/count, presence,
interaction and removal bitmaps, and compact scalar actor rows.

- Capacity: 256 roster entries, 39 integers per actor, 24 rows per packet,
  at most 11 parts; every part is limited to 8,192 bytes including its delimiter.
- Changed owner checkpoints: at most 5 Hz. Presence-only/unchanged refresh: 1 Hz.
  Membership, interaction and removal edges can send early, with a 50 ms floor.
- Complete batches replace state atomically. Duplicate/older packets, malformed
  rows and mismatched sessions, visits, teams, rooms or layouts are rejected.
- Presence is independent of ownership. Only the elected simulator publishes
  each actor's full row; other peers advertise its presence.
- Cached peers expire after 1.5 seconds. Same-room removals live while clients
  retain that occupied-room state; an empty room has no durable world snapshot.
- Addresses never cross the wire. Platform continuations are locally validated
  indices into a native callback allowlist. Native source descriptors identify
  placed actors, not allocation order or pool pointers.

Fields `0–3` identify the actor and interaction; `4–16` encode transform,
animation and velocity; `17–25` hold verified platform phase/timer scalars;
`26–29` encode scale/visibility/gravity; `30–37` hold NPC path scalars (platform
variant 9 reuses field 31 for its emission counter); field 38 is world-pause state.
Emitter actors reuse field 18 for their emission counter. Position uses hundredths,
velocity/scale thousandths, animation frame hundredths, and angles 1/1024 turns.

## Evidence and remaining scope

The supplied recording/log led to town rooms `0x161`/`0x15F` and Fuji rooms
`0x12D`/`0x12E`. Native tracing additionally covered all platform-family variants,
the large-food shine-child cleanup, breakable containers and periodic emitters.
Static Fuji props `0x33B/0x33C` are collision geometry whose initial visibility is
set locally; they do not have a moving state to stream.

This is **not yet universal synchronization of every native object**:

- Dynamic NPCs created outside the placed room roster are not identified here.
- Existing emitted hazards are not reconstructed for a late entrant. New
  emission cycles are shared; child trajectories/contact still run locally.
- Container contents are spawned by the breaking client; their dynamically
  allocated child actors are not assigned shared identities by this layer.
- Other object families, scripted controllers, minigames, pushable objects and
  room-specific switches need their own verified adapters. Durable progression
  flags alone do not prove that every already-loaded object refreshes correctly.
- Two near-simultaneous native collections can still precede the removal packet;
  this protocol does not provide transactional single-award arbitration.
- Native callback/path validation uses the US actor data. A modified NPC path
  program requires regenerating/reviewing its instruction-boundary masks.

## Validation and playtest

Run `bash tests/run_world_sync.sh` for the real C implementation under UBSan and
release-style floating-point flags, the C/Python codecs, metadata merging, framed
receive-loop routing and transport lifecycle tests. The native test uses simulated
native tasks; it does not run the game's renderer, scheduler or collision engine.

`python3 tools/inspect_world_actors.py /path/to/mnsg.us.decompressed.z64` verifies
the 163 derived path masks without copying game assets into the repository.
`tools/test_world_anchor_local.py --port PORT` requires a disposable loopback
Anchor and checks 256 actors/11 parts with three real TCP clients, team isolation,
sender exclusion, removal state, interaction ownership and expiry. Never point
synthetic tests at the shared public service.

Before calling this game-verified, use two fresh clients to check:

1. Enter the recorded towns at different times, walk opposite directions and
   compare NPC paths/animals; talk to an NPC from each client and release it.
2. Enter Fuji at different times, compare the rotors and ride them; leave the
   other player's culling range, pause, return, disconnect and reconnect.
3. Collect food/ryo and break a container; confirm removal, late-entry behavior
   and normal room respawn after both leave. Check large-food shine cleanup.
4. Observe several emitter/platform-hazard cycles; check duplicate emissions,
   ownership handoff and the documented late-entry child limitation.
5. Exercise falling/orbiting/rectangular platforms and scripted NPC progression.

Current evidence: the full Python suite passes (293 tests, one existing skip),
the world native harness passes under UBSan, the three-client loopback probe
passes, and release/debug packaging passes. A fresh two-client in-game run has
**not** been performed; these checks do not establish visual or collision parity.
