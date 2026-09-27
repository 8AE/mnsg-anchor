# Remote player sound effects

Remote players now emit the same one-shot sound cues as the real player task,
including attacks, damage reactions, healing, and character/action changes.
Playback uses the visible remote model's current position, so the game's native
camera-relative pan and distance attenuation remain in control.

## Native capture and playback

`func_80038C30_39830` is the final eight-entry native sound-command queue. A
hook observes that function but records a cue only while
`D_8016DAB4_16E6B4`, the scheduler-current task, is the real playable task
`D_801FC604_5B8514` or a live native task directly owned by that player through
task offset `+0x5C`. Both the player and child linkage are checked, and
Anchor's render-only remote-model task is explicitly excluded. This keeps UI,
enemies, ambient actors, and unrelated scene sound calls out of the player
channel while covering player-owned weapon effects. Before attribution, the
hook mirrors the native queue's zero/full/duplicate checks, so a cue rejected
locally is not broadcast as though it played. The hook performs no Python or
socket work; it retains at most eight unique cues until the frame-end publisher
runs. The batch retains only the authoritative player task, model
object, room, interaction session, lifecycle epoch, and cached scripted-control
state; it never retains the child task that produced a cue.

The additional trace identifies two child-task cues: Ebisumaru's camera update
`func_801EBAA8_5A79B8` queues shutter cue `0x0207`, and Sasuke's bomb update
`func_801EBF48_5A7E58` spatializes explosion cue `0x0104` at impact. The switch
path has a separate lifecycle edge: `func_801DCE10_598D20` queues transition
cue `0x021A` after marking the old character inactive, while
`func_801E0944_59C854` clears that marker and queues the selected character's
opening vocal through `func_801DD830_599740` group 3. Those alive-state changes
advance the movement epoch before frame-end. Entry/return hooks around the
exact switch dispatcher and group-3 vocal selector mark and isolate only their
synchronous cue. Those batches may cross exactly one alive-only epoch edge;
ordinary cues remain strict across death/respawn, and task, model, room,
session, multi-epoch, or scripted-control changes discard the batch. Inbound
deferred sounds remain strict on every epoch or connection-session change.

The two possible opening vocal cues per character are Goemon `0x0378`/`0x0382`,
Ebisumaru `0x036A`/`0x0364`, Sasuke `0x0392`/`0x038A`, and Yae
`0x03A5`/`0x039E`. The actual native RNG-selected cue is transported; receivers
do not choose or reconstruct a voice locally.

Only one-shot SFX IDs from `0x0100` through `0x7FFF` are synchronized. Music
IDs below `0x0100`, high-bit global stop/control commands, Sasuke's `0x014F`
jetpack loop, Yae's `0x0170` flute loop, and the known `0x026D` player loop
are excluded. The stock mixer keys active sounds by cue, not by player, so
replaying a remote `0x814F`, `0x8170`, or `0x826D` stop could also stop the
local player's or another peer's copy. An owner-aware loop manager is required
before these continuous cues can be
synchronized safely. The flute's native action also starts sequence `0x000D`
and later queues sequence `0x000E`; raw sequence replay is excluded too.
Sasuke action `0x9B` queues the jetpack's `0x014F` start from the real player
task in file_11 `func_801E75D4_5A34E4`. Its player-owned emitter callback
`func_801EDCE0_5A9BF0` queues global stop `0x814F` on exit. Capturing the
start as a one-shot while filtering the stop could leave a remote loop playing.
Native `func_80038F68_39B68` sends high-bit SFX commands to
`func_8003A24C_3AE4C`, which strips the high bit and records only the cue ID
for stopping. `func_800390D8_39CD8` also reuses an active sound slot when its
cue ID matches. Neither path carries a player or peer identity. Tracking
remote flute actions would therefore still share one native `0x0170` voice:
the local player's `0x8170` stop, or one remote's stop, could silence another
active flute. The available native API has no verified independent voice
handle for spatially playing and stopping each flute owner.

The native file_11 player actions queue Fire Ryo `0x015A` at selector `0x12`,
Sasuke kunai (including ice) `0x020B` at selectors `0x0E`-`0x10`, and Sasuke
bazooka `0x0103` at selectors `0x1A`/`0x1B`. These enqueue during the real
player task and use the existing one-shot capture path. Fire Ryo impact
queues spatial `0x029A` from its player-owned projectile task; the receiver
currently places it at the remote avatar rather than the impact position.
The Ryo trail itself
is a visual child emitted by `func_801ED26C_5A917C`, without a separate sound
call in that updater. A fresh two-client run is needed to establish whether
the weapon cues are actually heard at the receiver during each action.

Native coin `func_802145F0_5CFAC0` queues `0x026B`; small-health
`func_80214AEC_5CFFBC` and large-food `func_08000434_6AEC14` queue `0x026C`
only on their contact bit. These callbacks run as pickup actors, so their
existing world hooks explicitly observe the exact cue before native queueing.
Dynamic health and food reward calls are also observed at their local award
site; both queue `0x026C`. Explicit capture still requires the scheduler-current task to be
the same pickup actor, a live player/model and room/session/epoch, and native
queue capacity without a duplicate ID. The transient sound packet replays
these pickup sounds at the collecting remote player's position.
Dungeon keys and equipment rewards use scenario/message paths; no direct
one-shot cue has been verified for them, so this pickup path does not claim
to reproduce their acquisition audio.

The placed world protocol already carries pickup tombstones and travel-door
state. A fresh accepted remote tombstone for coin actor `0x082`/`0x083` arms
the native visual-only coin pickup child on that still-live actor's next
scheduler callback. A fresh pot `0x192` tombstone plays its verified break cue
`0x026E` at the pot object with the native 800-unit range. Initial snapshots,
repeat tombstones, stale/replaced actors, and room transitions do not replay
these effects. If the actor disappears before its next callback, the pending
effect is discarded.

For placed travel-door actors, the remote door's first accepted state
establishes an audio baseline. Later opening transitions play the native
opener cue selected by the door's unsigned subtype byte at task `+0xD0`,
spatialized at its object position. The verified mappings are:

| Actor | Subtype to cue mapping |
| --- | --- |
| `0x23C` | `0` to `0x0224`; others to `0x015E` |
| `0x23E` | `0/1/2/6` to `0x0224`; `3` to `0x0227`; `4/8` to `0x015F`; `7` to `0x015D`; others to `0x015E` |
| `0x23F` | `0/1/4` to `0x0224`; `2/3` to `0x015D` |
| `0x241` | `0x015E` |
| `0x242` | `0/9/11` to `0x0224`; others to `0x015E` |
| `0x24D` | `0/1/2/4/7` to `0x0224`; others to `0x015E` |
| `0x31F` | `0` to `0x0227`; others to `0x0224` |
| `0x321` | `0/1` to `0x0224`; other ordinary subtypes to `0x015E` |
| `0x32F` | `0x015F` |

`0x23F` subtype `5` uses a separate transition callback with cue `0x0108`;
its accepted ordinary door state does not establish that transition, so it is
omitted. `0x321`
subtype `2` creates a child with a separate sound condition and is omitted
from ordinary door audio. The `0x23A` progression barrier has a different
lifecycle and is excluded. On an accepted remote reversal the same opener
cue is reused once: native animation-complete and idle callbacks have no
distinct close cue. Native opener and travel callbacks are never called to
reproduce audio.

Fresh remote cues are resolved against the sender's active remote-model slot.
`func_8000F420_10020` then reads that displayed xyz position and spatializes
the sound through `D_8020CBF0_5C8B00` with the stock documented 400-unit range.
At most four remote cues are submitted per frame, reserving part of the native
eight-command queue for the local game. Replay reads the native queue's actual
count and packed IDs at frame-end, so player, UI, enemy, ambient, music, and
control commands all reduce capacity and reserve duplicate IDs. Remote cues
with an ID already queued are held in a bounded native queue and serialized
across later frames, but never beyond the packet's original 500 ms receive-time
deadline. The C bridge carries the remaining budget and converts it to an
absolute `osGetTime` deadline, so slow rendering does not extend freshness.
This avoids the stock queue's global duplicate-cue suppression silently
discarding simultaneous sounds. Deferred cues are revalidated and cannot
cross a local task/model/room/session/epoch transition. The replay guard
prevents the native spatial helper from being captured and sent back across
the network. If Python sees a just-arrived movement generation one frame before
the native remote-model snapshot does, the safe cue waits for that next
snapshot without extending its original deadline.

## Wire contract

The sender emits at most one compact room broadcast per game frame:

```json
{"type":"MNSG_PLAYER_SOUND","clientId":1,"currentRoomId":10,"interactionSession":101,"playerEpoch":3,"soundSeq":42,"sourcePosSeq":900,"soundT":123456789,"soundIds":[530,603],"quiet":true}
```

- `soundIds` contains one to eight unique, validated one-shot cues in native
  enqueue order.
- The packet is capped at 320 bytes and has no `targetClientId`,
  `targetTeamId`, or `addToQueue`; Anchor broadcasts it only to the private
  server room and excludes the sender.
- `interactionSession`, `playerEpoch`, and `sourcePosSeq` bind the event to the
  same live player generation and movement stream as the remote model. Session
  and epoch must already match the receiver's current sender state; the ordered
  TCP stream publishes any generation-changing movement sample first.
- `soundSeq` uses a 31-bit sequence with a 64-event replay window, allowing
  limited packet reordering without replaying a duplicate batch.
- `soundT` is compared only with the same sender's movement timestamp. Local
  receive time controls the 500 ms expiry window.

The Python receiver owns a dedicated 64-cue queue. It rejects self, unknown,
offline, cross-room, retired-session, stale-lifecycle, malformed, oversized,
duplicate, unsafe-cue, and unconfirmed-generation packets. Once session and
epoch are confirmed, a sound whose movement sequence is just ahead of the local
cache is held briefly and becomes playable only after that sample arrives.
Sound packets never enter durable team state or the general C packet queue.

## Validation

Automated tests cover envelope bounds, strict types, unsafe cue rejection,
failed sends, batching, replay-window deduplication and wrap, reordering,
future-movement deferral, rejection of unconfirmed generations without replay
map growth, worst-case packet size, queue capacity, expiry, lifecycle
invalidation, receive-loop routing, native capture filtering, direct
player-owned child attribution, the camera and bomb cues, safe character-switch
epoch rebasing, ordinary/death/multi-epoch/scripted transition rejection, all
eight opening voice possibilities, reconnect invalidation, full global native
queue occupancy and duplicate reservation, spatial replay, feedback
suppression, the four-cue frame drain, scripted-position fallback, and stale
native task rejection. Focused world tests additionally cover pickup actor
capture, first-snapshot suppression, one-shot remote coin/pot transitions,
door subtype cue selection, opening/reversal playback, and no sound
publication from remote pickup snapshot processing.

A fresh runtime certification still requires two clients in the same game room
and a third client in another game room within the same Anchor room. Exercise
all four characters' attacks, damage, healing, switching, room transitions,
death/respawn, Ebisumaru's camera shutter, Sasuke's bomb throw and impact, each
switched-to opening vocal, and simultaneous equal cues. Bomb impact audio
currently plays from the remote player's displayed position rather than the
projectile's exact impact position; exact projectile-origin audio would require
transporting a per-cue source position. Continuous `0x026D` behavior should
remain local until an owner-aware loop implementation exists. Sasuke's jetpack
and Yae's flute loops likewise remain local. Fresh in-game checks should include coin and
health/food pickup, pot break, the nine mapped travel-door families, and
their animation reversals; host tests do not confirm audible mixing or visual
particle placement in a running game.
