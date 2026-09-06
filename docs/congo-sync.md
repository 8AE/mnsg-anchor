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

The local native layer and Python each bound pending hit queues. Authority
delivery processes at most 32 different senders per frame, rotating among
senders with pending work. These work budgets do not limit the number of
players in the room. Hit deduplication survives authority handoff. A new
encounter, room visit, connection session or player life invalidates stale
work instead of relabeling an old attack with the new identity.

Start and the invitation dialog pause local application and hit delivery.
If the elected simulator pauses, its checkpoint pauses the shared boss
timeline. Networking remains active. A paused recipient retains the latest
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
For a shared Congo encounter, checkpoint state controls victory; a legacy
room-only defeat packet cannot independently kill an owner or follower.
Congo's durable kill/reward flags stay deferred until the native victory
callback has begun, including the frame between health reaching zero and that
callback running. Outside shared mode, the prior native defeat path remains
available.

## Verification

Focused host C tests cover native damage strength and recovery, duplicate
contacts, owner/follower guards, pending checkpoints, pause, actor reuse,
queue backpressure, allocation failure and native context restoration. The
coordinator and Python transport tests cover checkpoint adoption, pause,
authority changes, hit retry/deduplication and stale session/visit/epoch work.
UndefinedBehaviorSanitizer and MIPS compilation are used alongside source
review of the native instruction paths. The final run passed 118 Python tests
and 10 C/UndefinedBehaviorSanitizer suites. Both release and debug archives
passed integrity checks and contain both Python modules exactly as built from
source; their compiled mod binaries differ as required.

No in-game or fresh two-client certification was performed for this change;
the user requested to do that verification. The native simulation and effect
reconstruction still need that runtime check, especially a late join during
fire, spin, damage recovery and the victory sequence, plus an owner leaving
during combat.
