# Shared Dharumanyo fight

Dharumanyo's arena (`0x0049`, decimal 73) uses one elected client to run the
boss. Other clients adopt that simulator's native fight state, including the
visible boss, linked twelve-life carrier, animation, attack phase, timers,
movement and active travelling projectiles. A client entering after combat has
started joins the current encounter instead of beginning an independent copy.
All participants need this version of the mod for live fight synchronization.

The visible actor and damage carrier are separate native tasks. The elected
client advances the visible AI. Followers hold that autonomous AI while
retaining native rendering, collision and contact processing. The carrier
continues its local collision/orbit pass, then checkpoints correct its shared
life count and position. This keeps Dharumanyo's attacks capable of damaging
each local player without sending per-frame collision data.

Checkpoint adoption follows the native scheduler order: pre, AI, then post.
While a checkpoint is pending, identity checks use the saved native post
callback for temporarily held actors. The pre callback can therefore adopt
the checkpoint and release the owner for the same scheduler pass; followers
resume local post work while continuing to hold autonomous AI.

Scripted pause flag `0x16C` is mirrored into a global pause bit by the boss's
native post. When that script flag clears, the post is allowed to clear its
own mirror. Independent native pauses, Start, flute and invite pauses remain
gated. The sync code does not write the global pause bit itself.

Each player can strike the local carrier with native melee or a locally owned
projectile. The contact becomes a one-life hit intent. Only the elected client
applies accepted hits, through the carrier's normal damage and recovery path,
then shares the resulting life count. Render-only remote projectiles cannot
submit a duplicate hit. Pauses and native invulnerability still reject or
delay damage in the same places as the original fight.

The simulator can aim at its local player or a current teammate in the arena.
It keeps a selected target through an attack and rotates candidates from a
neutral phase. Remote targets use a position proxy only around the verified AI
callbacks that read the native target pointer; no remote rendering task is
passed to code that requires a playable-character task.

## Projectiles

Dharumanyo's travelling projectiles are represented by a complete bounded set
in each checkpoint. Every record contains a generated ID, birth tick, native
variant, position, yaw, destination, vertical velocity and timer. A follower
creates the ordinary native travelling actor, installs the authority's current
motion state, corrects it at later checkpoints and removes it when its ID leaves
the authority's active set. Trail and impact children remain local native
effects.

The native projectile constructor sets the special orientation value `0x8000`.
Checkpoints preserve that value as well as ordinary yaw angles `0..1023`.
Rejecting the special value previously prevented the authority from publishing
any checkpoint while a travelling projectile was active, leaving that
projectile absent from followers' screens.

The set holds at most 16 travelling projectiles, matching the verified native
and Hyper Dharumanyo working bound. A malformed count, duplicate ID, impossible
age, invalid callback phase or non-finite coordinate is rejected before game
memory changes.

## Reward progression

The item following this fight is **Miracle Flower**, despite sometimes being
called Miracle Star. Dharumanyo's native reward controller starts scenario
`0x72`, which sets `mi_flower` at save offset `+0x258`. Miracle Star
(`mi_star`, `+0x250`) belongs to Tsurami's scenario `0x73`.

Every client that follows the shared terminal checkpoint enters Dharumanyo's
real last-life, destruction and reward-controller sequence. The existing item
sync then shares `mi_flower` through its durable field update and compact team
snapshot. No continuous reward stream or synthetic pickup actor is needed.
Clients elsewhere or joining later receive the inventory value through that
same durable path.

The elected simulator publishes the Flower and four reward-owned story deltas.
A follower still lets the native scripts update its own save, but aligns those
five monitor baselines without echoing duplicate queued `SET_FLAG` packets.
The verified D0 completion boundary aligns the final values before releasing
that follower-only suppression window.

The native reward script grants Miracle Flower before its later story flags and
`fl_dharmanyo`. The completion flag remains deferred through combat teardown;
an entity-`0x34F` return observer releases it only after scenario `0xD0` has
set native flag `0x18`. The original reward callback then continues through
states 25-28 and releases its camera and player-control state normally.

The terminal checkpoint remains available after the visible root and carrier
have been removed and until that verified `0xD0` boundary completes. A client
entering during the reward ceremony therefore adopts the terminal encounter
instead of electing a new authority and starting a fresh boss.

## Network contract

`py/anchor_dharumanyo.py` uses the transient, team-scoped
`MNSG_DHARUMANYO` protocol. Encounter identity contains the creator client ID,
connection session and native room visit. Every packet also carries protocol
version `1`; recipients validate the sender, team, current room, connection
session, encounter, authority term and sequence. Hot packets are never placed
in Anchor's durable queue.

The owner coalesces changed checkpoints to at most 10 per second. Unchanged
state uses a one-second keepalive, late joins request a checkpoint at most once
per second, and the authority lease expires after three seconds. One cached
checkpoint lets another eligible client take over if the owner leaves or
stalls. Hits retry every 250 ms until an acknowledgment is included in a later
checkpoint.

The authority keeps one highest acknowledged hit sequence for each current
client session and player epoch. A checkpoint carries at most 64 acknowledgment
rows, and pending rows rotate through later 10 Hz checkpoints until all have
been published. Receivers merge those slices into their cached checkpoint so a
new authority retains the complete deduplication history. A late-join request
replays the current history in the same bounded slices. The successful-send
cursor keeps advancing even if requests repeat during that replay, preventing
lower client IDs from starving later rows. Failed socket writes do not consume
a slice or advance the publish and retry timers.

The native state is encoded as unsigned integer words. Float fields cross the
bridge only as IEEE-754 bit patterns and are range-checked in native code. The
checkpoint has 29 root words, nine carrier words, two scalar counters and up to
16 eleven-word projectile records. Both the checkpoint JSON and full framed
hot packet have explicit 4 KiB and 8 KiB limits, respectively. The client
enforces the full-frame limit in both send and receive paths before a boss
packet reaches its transport.

An offline relay run with all 16 projectile slots active emitted 20 state
packets and 28,318 upstream bytes over two seconds. The result was identical
with 7 and 35 participants: about 14.2 KB/s from the one simulator. Anchor's
outbound relay traffic still grows with the number of recipients.

The boss transport owns bounded per-sender hit queues and processes at most 32
senders in one frame. Those work limits do not cap the number of players in the
arena. The same reusable election core serves Congo and Dharumanyo, while each
fight keeps its own packet type, metadata, room, state validator and authority.
An additional 512-member transport test publishes a maximal Dharumanyo state
and all 512 acknowledgment identities in eight bounded frames without exceeding
the 8 KiB limit.

Start, Yae's flute, a boss invitation dialog and the native scripted-pause flag
pause local checkpoint application and damage delivery. An owner still renews
its network lease while paused. A follower keeps only the newest checkpoint
and applies it after local world control resumes.

## Verification

Offline Python relay tests cover election, a late join, checkpoint validation,
pause/resume, hit retry and deduplication, stale identity rejection, room/team
changes, authority handoff and packet budgets. Focused host C tests cover the
bridge codec, coordinator, native identity, follower holds, target proxy,
projectile reconstruction, local hit interception and terminal handoff. The
transport suite also covers 512-player acknowledgment rotation, cross-slice
handoff deduplication, failed-send retry state and inbound hot-frame rejection.
The release and debug MIPS packages are also built and checked as archives.

No in-game or fresh two-client certification was performed for this change;
the user requested to do that verification. Runtime testing should cover a
late join during each attack, hits from both players, target changes, active
projectiles, pausing, owner departure and the complete Miracle Flower scene.
