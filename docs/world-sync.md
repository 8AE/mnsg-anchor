# Shared room NPCs and world objects

The world-sync layer combines the guarded normal-plus-partition actor roster with
a separate native-child lifecycle. It runs in rooms with **zero enemies**,
including towns, and script-only rooms without a placed actor wave.
Only peers with the same room, team, save-loaded state and actor-layout signature
exchange state. Both players need this version of the mod.

## Implemented coverage

| Family | Shared state and behavior |
| --- | --- |
| Placed NPCs using native NPC setup | Position, all three angles, scale/visibility, animation clip/frame/rate, velocity and path checkpoint. This includes ordinary town NPCs, named NPCs using that setup, and signposts. |
| Placed birds/dogs `0x2C2–0x2C4` | Native path-based movement and animation, without trying to start dialogue on animals. |
| Moving platform `0x3E0`, variants `0–13` | Native phase, timers, motion, angles, saved origin and fall state. Rectangular, orbiting, bobbing and rotating variants retain local motion between checkpoints. The player riding a falling platform can take ownership of that actor. |
| Placed and dropped ryo/health/food | Shared identity, current motion and lifetime; a pickup claim goes to one arbiter before a single winner executes the native award. Only the collector receives health or ryo, including when race delta sharing is enabled. Placed pickups keep their roster identity across proximity culling. Food retains its native shine child and safe cleanup. |
| Breakable item containers `0x192` | Actual break marks the placed container removed for other clients and later entrants. Distance culling does not count as breaking. |
| Timed emitters `0x19A`, subtypes `0–2` | Shared countdown and cycle. Their existing rock/flame children carry independent current-state snapshots; a late entrant reconstructs those children without replaying the emitter. |
| Platform variant 9 child hazards | Current live child set, position, phase, timer, bounce/orbit state and lifetime, including late entry and ownership handoff. |
| Dynamic NPCs using common native NPC/path setup | Stable birth identity, matching to an existing local task, or reconstruction using resident native model, dialogue and path state. Independently allocated copies of the same NPC are coalesced. |
| Other mechanisms | Falling traps `0x197`, elevators `0x1FC`, tilting platforms `0x1FD`, path crates `0x1F7`, pushable blocks `0x245`, and rising platforms `0x34A` have distinct phase adapters. Riders/pushers and opposite-floor elevator callers can take ownership. |
| Doors and breakable walls | Door animation/mesh/pose checkpoints preserve local input and travel callbacks. Actual breaks of wall `0x23D` subtypes 1/3/4 propagate removal; subtype 3/4 progression flags retain the existing durable path. |
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

`MNSG_WORLD` and `MNSG_WORLD_ACTORS` are quiet, transient `targetTeamId` packets
with **no offline queue**. The client supplies its assigned root `clientId`;
Anchor relays it without authenticating or injecting it. Receivers check roster,
team, session and room eligibility. Metadata is
`worldSync = [3, interactionSession, visit, rawRoom, rosterSignature]`.
Older world-sync versions are incompatible: version 3 adds door playback direction. Both players must update.
Every packet also carries this metadata, a sequence, part index/count, presence,
interaction and removal bitmaps, and compact scalar actor rows.

- Capacity: 256 roster entries, 39 integers per actor, 24 rows per packet,
  at most 11 parts; every part is limited to 8,192 bytes including its delimiter.
- Changed owner checkpoints: at most 5 Hz. Presence-only/unchanged refresh: 1 Hz.
  Membership, interaction, removal and door playback-direction/stop edges can send early, with a 50 ms floor.
- Complete batches replace state atomically. Duplicate/older packets, malformed
  rows and mismatched sessions, visits, teams, rooms or layouts are rejected.
- Presence is independent of ownership. Only the elected simulator publishes
  each actor's full row; other peers advertise its presence.
- Cached peers expire after 1.5 seconds. Same-room removals live while clients
  retain that occupied-room state; an empty room has no durable world snapshot.
- Addresses never cross the wire. Platform continuations are locally validated
  indices into a native callback allowlist. Native source descriptors identify
  placed actors, not allocation order or pool pointers.

In a placed row, fields `0–3` identify the actor and interaction; `4–16` encode transform,
animation and velocity; `17–25` hold verified platform phase/timer scalars;
`26–29` encode scale/visibility/gravity/mesh/terrain/door animation state; `30–37` hold NPC path scalars (platform
variant 9 reuses field 31 for its emission counter); field 38 is world-pause state.
For doors, field 29 bit 1 carries native reverse playback (task flag 0x01000000); bit 4 carries playback enable. Other actor kinds retain their existing bit meanings.
Emitter actors reuse field 18 for their emission counter. Position uses hundredths,
velocity/scale thousandths, animation frame hundredths, and angles 1/1024 turns.

Animated travel-door observers retain the local input callback. When the remote
interaction ends or its player leaves, they reverse the existing clip from its
current frame and clear playback at frame zero. The native common post updates
the animated mesh. Actual local travel can reopen the door during closing.
Progression barriers (0x23A) and static scripted door pieces retain native
behavior. Door closing does not call another player's travel, camera, fade or
control-reset callbacks.

## Dynamic lifetime and claims

`MNSG_WORLD_ACTORS` carries at most 128 records, 74 integers each, in batches of
12 rows (at most 11 parts). Parts are capped at 8,192 bytes including NUL and
applied atomically. Peer/pending caches are bounded to 32 and expire after 1.5 s.
Owner motion sends at most 5 Hz, replica presence at 1 Hz. Lifecycle, ownership,
interaction and claim edges can bypass that cadence with a 50 ms floor.

Dynamic hazard identity is `(birth client, session, visit, serial)`, independent
of the current simulator. Initial speculative emitter births wait for placed
presence and retire locally when another peer owns the parent. Native NPC birth
descriptors identify duplicate local NPC allocations; placed pickups use the
room roster slot, and parented loot uses the parent slot, kind and birth ordinal.
Simultaneous copies of a container's contents therefore share their identities. Native pointers,
model-resource bindings and callbacks are never supplied by network data.
Reconstruction checks local asset residency, clip and collision-list bounds,
route instruction boundaries and a family-specific continuation allowlist.
A failed resource check defers reconstruction. Shadows and food's shine child
are created through their own native setup.

A contacted pickup is held while its claim is resolved. The arbiter commits one
winner and broadcasts a removal containing both winner and committer identity.
Native award/removal waits for a successful local TCP send of that commit; a
throttle or failed write retains the retry. A silent but still eligible arbiter
does not automatically lose its award lease merely because its hot snapshot
expired. Actual roster departure permits handoff. This is a client protocol,
not a server transaction or a guarantee under arbitrary malicious peers.

Only the collector receives the pickup's health or ryo. The actual capped gain
is excluded from race delta sharing, preserving unrelated changes in the same
frame. Primary model tracking ignores linked shadow bindings: native shadow
creation also calls the static binder on the parent task with slot 0, which
previously replaced coin slot 4 or health slot 3 and prevented registration.
Loot replicas retain the constructors' exact `0x8000` orientation on each axis. Coin texture playback loops locally through the native common post. Snapshots preserve the loop flag and do not reset texture cursors; the shared timer, blinking and removal still govern despawn.

Removal history is bounded to 4,096 identities per occupied-room scope and fails
closed if exhausted. Nothing persists an empty room's dynamic actors offline.
The dynamic maximum is independent of the 256-slot placed roster. At maximum
capacity a sender can burst 11 packets; with `T` active team members and a batch
size of `B` bytes, owner-motion server egress is `5 * B * (T - 1)` bytes/s per
owner sender, plus replica refreshes at `B * (T - 1)` each second. A full
multi-owner worst case multiplies the first term by `T`. This is a bounded
protocol budget, not evidence that a 32-client maximum-load game is playable.

## Evidence and coverage boundary

The recording/log identified towns `0x161`/`0x15F` and Fuji `0x12D`/`0x12E`.
The wider ROM inventory inspected 800 metadata slots, finding 333 rooms with
placed actor data and 255 distinct placed entity types. The adapters above were
traced against their real native initializers, continuations and cleanup paths.
Static Fuji props `0x33B/0x33C` have no moving state to stream.

The original gaps for common dynamic NPCs, container/enemy loot and late-entry
emitter/platform hazards now have implementations. That does **not** establish
universal support for every native actor:

- Minigame and cutscene controls stay local. A reconstructed NPC uses the common
  dialogue/path continuation; custom scene-controller pointers and private quest
  continuations are not reconstructed.
- Room-specific puzzle controllers and other unaudited actor families still
  need explicit adapters. In particular, shared progression flags alone do not
  synchronize every temporary room flag or already-loaded puzzle state.
- The tracked hazard recipes are the File_34 emitter children and File_44
  platform children listed above, not arbitrary projectiles or visual effects.
- The live-set and removal-history limits are explicit. Repeated scripted NPC
  births at an identical descriptor need game testing beyond the current
  simultaneous-copy/ordinal tests.
- Native callback/path validation uses the US actor data. A modified path
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
   Break a pot and defeat an enemy from each client, then let the other player
   take the drops. Only the collector should gain health/ryo, including with
   race sharing enabled and when near the health/ryo caps.
4. Enter after several emitter/platform-hazard cycles and after a container
   breaks; compare existing children, then disconnect the original simulator.
   Simultaneously touch the same pickup and confirm exactly one native reward.
5. Have one player go through each travel-door family while the other stays.
   Confirm reverse closing and collision, then pass through from the remaining
   client. Reopen during closing, and enter/disconnect while it closes. Verify
   coins spin through several loops on both clients until shared despawn.
6. Call elevators from both floors, push a block from either client, and check
   falling/tilting platforms. Open slow doors while the other player watches.
7. Exercise dynamic NPC talk/path handoff and room-specific scripted progression;
   record any unsupported controller instead of treating generic pose sync as
   proof that its complete native state is shared.

Current validation results are recorded in `world-sync-validation-2026-09-16.json`.
The later loot playtest fix and its checks are recorded in
`loot-sync-validation-2026-09-16.json`.
Coin loop playback and door reverse-closing checks are in
`world-animation-validation-2026-09-16.json`.
A fresh two-client in-game run has **not** been performed; host native mocks,
loopback TCP checks and successful packages do not establish visual, collision,
or universal room coverage.

`tools/test_world_dynamic_anchor_local.py --port PORT` adds a three-client
128-child/11-part probe: atomic current-state late entry, claims/commit,
sender/team isolation, duplicate rejection and origin disconnect/handoff.
