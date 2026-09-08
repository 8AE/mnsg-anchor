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
IDs below `0x0100`, high-bit global stop/control commands, and the known
`0x026D` player loop are excluded. The stock mixer keys active sounds by cue,
not by player, so replaying a remote `0x826D` stop could also stop the local
player's or another peer's copy. An owner-aware loop manager is required before
continuous cues can be synchronized safely.

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
native task rejection.

A fresh runtime certification still requires two clients in the same game room
and a third client in another game room within the same Anchor room. Exercise
all four characters' attacks, damage, healing, switching, room transitions,
death/respawn, Ebisumaru's camera shutter, Sasuke's bomb throw and impact, each
switched-to opening vocal, and simultaneous equal cues. Bomb impact audio
currently plays from the remote player's displayed position rather than the
projectile's exact impact position; exact projectile-origin audio would require
transporting a per-cue source position. Continuous `0x026D` behavior should
remain local until an owner-aware loop implementation exists.
