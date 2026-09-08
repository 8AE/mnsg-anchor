# Shared Congo fight

Congo's arena (`0x0016`, decimal 22) uses one elected client to run the fight.
The other clients adopt that client's boss state, including health, attack
phase, timers, orientation, body-part animation and active attack hazards.
Joining an existing fight requests its current checkpoint. It does not start
another independent boss simulation. All participating clients need this
version of the mod; older terminal-only boss synchronization cannot reproduce
the shared fight.

Each client can hit its local native Congo actor with its own player or a
locally thrown projectile. That contact becomes a small hit intent. The elected
client applies accepted intents through the game's damage routine, including
the ordinary attack strength and recovery window. A remote projectile's visual
copy cannot submit the same hit again. Boss attacks retain their native local
collision and player-damage paths on each recipient.

The boss can aim at the local player or an eligible visible teammate in the
room. Target selection reads copied player positions and refreshes them every
owner frame. It keeps the current participant through an attack and can rotate
to another participant between attacks. Dead, scripted, stale, out-of-room and
disconnected remote players are excluded. It does not pass a remote rendering
task to functions that require a native playable character.

## Miracle Moon and the exit

Miracle Moon ownership (`mi_moon`, save word `+0x254`) and completion of its
pickup scene (`fl_mi_moon`, packed save flag `0xA4`) are shared separately.
The native script grants the item before the reward actor finishes the scene
and sets its completion flag. Sharing only the inventory word leaves the
other client's reward and exit waiting for that final native state.

Receiving completion removes the bound Moon reward while it is still
uncollected. It does not interrupt a local pickup whose scenario has already
started; that scene keeps its native control-release and cleanup sequence,
then reaches the same completion flag. Congo's existing exit door observes
the flag and runs its normal opening animation. The native camera can then
show the exit through its own reaction. This does not move another player's
character through the doorway or replay the pickup scene for them.

Both values use the existing durable `SET_FLAG` queue and compact team
snapshots, including late joins and reconnects. The shared send budget remains
one `SET_FLAG` every four item-sync updates; there is no new Moon, door or
camera packet stream. All clients should use the updated mod so they recognize
the completion flag and its live reward behavior.

## Network contract

`py/anchor_congo.py` owns the transient `MNSG_CONGO` protocol. Encounter identity
contains the creator's client ID, connection session and native room visit.
Authority terms distinguish a successor from an expired owner. Packets must
match the current room, team, session and encounter; no packet supplies native
task pointers or arbitrary callback addresses. The C adapter maps a bounded
phase number to verified callbacks from the loaded Congo overlay.

An initial discovery period is 350 ms. A current owner remains in charge while
its membership and three-second lease are valid. If it leaves or stalls, a
qualified replica can continue from the cached checkpoint under a new term.
An intro actor can receive an existing checkpoint before it is ready to become
the simulator. A connected client awaiting its first checkpoint holds the
shared boss rather than running an independent combat timeline.

The owner coalesces changed checkpoints to at most 10 per second. An unchanged
state uses a one-second keepalive, and a solo owner maintains its cache without
sending a continuous state stream. Late-join requests are limited to one per
second. A checkpoint contains the root state, six part states, a spin generation,
the encounter tick and the active flame birth records. Native code validates
the decoded values, including float bit patterns, before touching an actor.

An offline relay test with six animated parts and eight active flames emitted
20 boss-state packets and 14,869 upstream bytes over two seconds. The result
was identical with 7 and 35 participants: approximately 7.4 KB/s from the one
simulator. Anchor still relays each team packet to the other team members, so
server outbound bandwidth grows with the number of recipients.

Hit intents contain a sequence and one of the native damage amounts
`1`, `2`, `3`, `4` or `8`. The bridge attaches the attacker's client ID, session
and player epoch. Retries occur at 250 ms intervals; acknowledgments travel
with a subsequent checkpoint after the owner has processed the intents.
Native vulnerability and recovery can reject a delivered hit. Acknowledgment
means the intent was processed, not that health necessarily decreased.

The owner retains one highest sequence per current client identity and sends at
most 64 acknowledgment rows in one checkpoint. Pending rows rotate through
later 10 Hz checkpoints, receivers merge the slices for authority handoff, and
a late-join request replays the history in the same bounded form. Slice
selection advances from the last successful row, so repeated requests cannot
starve higher client IDs. A failed socket write consumes no rows and advances
no cadence timer. The client rejects outbound and inbound Congo frames above
the 8 KiB hot-packet budget.

The local native layer and Python each bound pending hit queues. Authority
delivery processes at most 32 different senders per frame, rotating among
senders with pending work. These work budgets do not limit the number of
players in the room. Hit deduplication survives authority handoff. A new
encounter, room visit, connection session or player life invalidates stale
work instead of relabeling an old attack with the new identity.

Start and the invitation dialog pause local application and hit delivery.
If the elected simulator pauses, its checkpoint pauses the shared boss
combat timeline. Networking remains active. A paused recipient retains the latest
checkpoint and applies it after the local world resumes.

## Native ownership and damage

`anchor_congo_native.c` binds the room's existing boss, six visible parts and
native hazards. It does not create a second boss root. Retained handles are
checked against room visit, task linkage, actor identity, generation and display
object. Followers hold the root's autonomous combat callback while keeping the
native collision, rendering and contact-processing lifecycle. Global part
animation pulses are represented by settled per-part state for late joining.
Flame birth records allow native effects to be reconstructed and advanced to
their current age without replaying historical collision scans. Checkpoint
application is queued until the root's native pre-update, where the scheduler
has already activated its overlay resources. Capture remains unavailable while
adoption is pending, preventing hit acknowledgments or publication of an old
local state during a handoff. Overlay callback tables are populated at runtime
on each root initialization so room reloads use the current overlay addresses.

Early invitation joins preserve the intro's one-time boss-music cue before
skipping to combat. Later checkpoints do not restart it. The root status wire
word uses bit `1` for native recovery and bit `2` for the authority's camera
quake event. Only bit `1` is copied into native actor status; bit `2` controls
transient camera event `0xB`. Sending the actual event also respects Hyper
Congo's suppression of a previously consumed threshold. The native camera
uses the encounter tick for its shake cadence during its own callback.

All twelve spin rays reconcile against the shared phase and timer. The native
stop flag is consumed by ray callbacks and is insufficient as a checkpoint.
Winddown's last 30 ticks put every owned ray into native fade; a later attack
phase removes any remaining rays. Late constructors adopt the same fade age,
including during an authority handoff. Repeated checkpoints cannot brighten
fading rays, and queued duplicate direction/material pairs are retired.
These effects use the existing checkpoint fields and 10 Hz publisher, with
no new event packets. All clients should update together for the camera bit.

`anchor_congo_damage.c` observes local native attack episodes and intercepts
only the exact bound Congo root at the common damage intake. It consumes the
contact before that routine can change health or invoke a hit/death callback.
It also clears the root's consumed attacker reference because Congo's usual
post-update cleanup is conditional on native recovery. The source attacker's
collision result remains available to its normal projectile/impact callback.

The owner uses a scoped attacker descriptor with the native attack type and
runs the real damage intake in Congo's native current-task context. The call
preserves the real root's vulnerability, deflection, recovery and health
checks. It restores the previous current task and attacker pointer after the
call, keeping the resulting recovery or death state. It does not simulate a
normal attack through the native one-point synthetic-damage bypass.

`boss_sync.c` still handles terminal synchronization for the other bosses.
For a shared Congo encounter, checkpoint state triggers victory; a legacy
room-only defeat packet cannot independently kill an owner or follower.
Congo's durable kill/reward flags stay deferred until the native victory
callback has begun, including the frame between health reaching zero and that
callback running. Outside shared mode, the prior native defeat path remains
available.

The first received victory checkpoint starts the complete native death
sequence once, even if that checkpoint is already near its end. Subsequent
checkpoints cannot replace its root/part callbacks, timers or camera events.
This lets each client's white fade run to zero and release its native render
object before the camera returns control. A network wait, an owner leaving,
or a missing final checkpoint cannot suspend this cleanup. Native local
Start/flute/dialog pauses still apply. The inherited combat shake flag is
released at victory entry so it cannot trap the camera in its spin callback.

## Verification

Focused host C tests cover native damage strength and recovery, duplicate
contacts, owner/follower guards, pending checkpoints, pause, actor reuse,
queue backpressure, allocation failure and native context restoration. The
coordinator and Python transport tests cover checkpoint adoption, pause,
authority changes, hit retry/deduplication and stale session/visit/epoch work.
UndefinedBehaviorSanitizer and MIPS compilation are used alongside source
review of the native instruction paths. The final run passed 119 Python tests
and 12 C/UndefinedBehaviorSanitizer programs. Both release and debug archives
passed integrity checks and contain both Python modules exactly as built from
source; their compiled mod binaries differ as required.

Effect regressions exercise the intro music boundary, actual quake-bit
round trips and scoped clock restoration, twelve scheduled ray constructors,
missed stop/ending updates, fade-age catch-up, pause, duplicate checkpoints,
allocation retry and owner handoff. The transport regression retains exactly
20 snapshots over two seconds while toggling the quake flag.

The victory regression models the native terminal callbacks and owned white
overlay. It reproduces the old failure by delivering a completed checkpoint
at full white, then verifies one-time overlay release for first checkpoints
at every terminal phase, stale combat checkpoints, missing packets/parts,
network waiting and a local pause/resume. This is an offline lifecycle test,
not a rendered two-client result.

Moon regressions cover local pickup ownership, scheduler-selected pickup
callbacks, room and actor identity, valid task-list relinking, remote reward
removal, deferred live versus durable completion, and stale/partial snapshot
merges that must preserve an unsent local gain.

No in-game or fresh two-client certification was performed for this change;
the user requested to do that verification. The native simulation and effect
reconstruction still need that runtime check, especially a late join during
fire, spin, damage recovery and the victory sequence, plus an owner leaving
during combat.
