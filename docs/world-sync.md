# Shared room NPCs and world objects

The world-sync layer combines the guarded normal, partition and resident actor roster with
a separate native-child lifecycle. It runs in rooms with **zero enemies**,
including towns, and script-only rooms without a placed actor wave.
Only peers with the same room, team, save-loaded state and actor-layout signature
exchange state. Both players need this version of the mod.

## Implemented coverage

| Family | Shared state and behavior |
| --- | --- |
| Placed NPCs using native NPC setup | Position, all three angles, scale/visibility, animation clip/frame/rate, velocity and path checkpoint. This includes ordinary town NPCs, named NPCs using that setup, and signposts. |
| Path actors `0x2C2–0x2C4` | Native path-based movement and animation. Native NPC setup determines talkability; `0x2C4` is the town pickpocket. |
| Moving platform `0x3E0`, variants `0–13` | Native phase, timers, motion, angles, saved origin and fall state. Rectangular, orbiting, bobbing and rotating variants retain local motion between checkpoints. The player riding a falling platform can take ownership of that actor. |
| Placed and dropped ryo/health/food | Shared identity, current motion and lifetime; a pickup claim goes to one arbiter before a single winner executes the native award. Only the collector receives health or ryo, including when race delta sharing is enabled. Placed pickups keep their roster identity across proximity culling. Food retains its native shine child and safe cleanup. |
| Breakable item containers `0x192` | Actual break marks the placed container removed for other clients and later entrants. Distance culling does not count as breaking. |
| Timed emitters `0x19A`, subtypes `0–2` | Shared countdown and cycle. Their existing rock/flame children carry independent current-state snapshots; a late entrant reconstructs those children without replaying the emitter. |
| Platform variant 9 child hazards | Current live child set, position, phase, timer, bounce/orbit state and lifetime, including late entry and ownership handoff. |
| Dynamic NPCs using common native NPC/path setup | Stable birth identity, matching to an existing local task, or reconstruction using resident native model, dialogue and path state. Independently allocated copies of the same NPC are coalesced. |
| File30 slicer emitters `0x19D`, rooms `0xAB`/`0xAC` | Six placed invisible emitters share their countdown, repeat period and speed parameters. Each independently scheduled flying blade gets a typed identity, a motion checkpoint and an owner-only expiry; a committed kill publishes its drops through the existing per-parent loot identity, and a retired emitter cannot resurrect a blade. |
| File30 RNG spawner `0x3EF`, room `0x91` | The one placed invisible root rolls its native birth period and spawn position on the owner alone. Each `0x12F` child carries a typed identity, a checkpointed target X/Z and shared lifetime; a replica turns from that target without dereferencing the local player pointer, and a silent expiry commits no loot. |
| File_40 bomb blocks `0x1A9`, rooms `0x65`/`0x66` | Seven placed roots share their arming cause, fall, fuse tint, the single committed explosion and the twelve transient children: four spinning rings, two of them registered attackers, and eight particles. The native proximity AI is not replayed, and only the named committer runs the explosion callback. Nothing here awards loot. |
| File_40 counterweights `0x3CB`, room `0x6B` | Three placed six-piece graphs share parent motion, child placement and native continuation. |
| File_46 Koryuta wave graph, room `0x155` | The placed body's invisible producer and its `0x12D`/`0xFA` children share the encounter stop latch, child identity, checkpointed pursuit destination and owner-only retirement. The local live-wave census and spawn positions stay local, and no wave drops loot. |
| Custom quest presentations, File_53/46/62/74/75 | Gateway Viewpoint (`0x316`, room `0x153`), Koryuta's body, Kihachi's scene (`0x315`, rooms `0x16A`/`0x182`) and Gorgeous Music Castle (`0x35C`/`0x35D`, room `0xC1`) publish only the visible scalar state of their scripted actor graphs, on a separate `MNSG_WORLD_QUEST` packet. Dialogue, camera, fade, player control and the temporary-bit banks stay local; a render-only proxy binds native models and is hidden while no valid reconstruction is active. |
| File30 save-controlled appearances `0xCA`/`0x339` | Room `0x14` placement 8 and rooms `0x14E`/`0x15C` placements 20/15 adopt their native visible model, flags, scale and clip when the shared save flag (`0x12E` / `0x32`) arrives after the actor was already constructed, and hide that presentation again while the flag is set. Their idle continuations and local controls are untouched. |
| File30 placed one-HP objects `0x196`/`0x330`/`0x331`/`0x332`/`0x339`/`0x3EC` | Six placed root families share their hit/claim, one-HP kill, committed tombstone and native loot identity. `0x331` and `0x332` also track the linked child their own native birth allocates. Proximity culling and the save-completed disappearance of `0x339` are not treated as kills. |
| File34 falling boulders `0x3DA`, rooms `0x13D`/`0x13F` | All 20 native-placed boulders share phase, pose, age and expiry. The race multiplier's extra copies key on the placed roster slot plus the race ordinal, so a duplicate is a distinct actor at the original birth coordinate with its own offset pose. Expiry is not a kill and carries no committer, and a missing native copy is never reconstructed: a replica waits for a live capture instead of inventing a task. |
| File68 fish `0x338`, seven rooms | All 33 placed fish share their per-fish native save flag. Collection is a same-room one-winner claim resolved before the native pickup runs, so both peers cannot run that pickup. The native count increments only when the active quest gate, fish variant and colour cap (8/5/3) permit, but a later native continuation sets the per-fish flag regardless of whether the increment happened; the placement flags therefore cannot reconstruct the exact counter, and the two cross-room concurrency cases below are confirmed gaps. The fish's private animation object stays local. |
| Purchases versus progression | Ordinary main-shop armor and consumable purchases stay personal. Cat Eyes rewards are quest progression despite being bought: `fl_ce_dharma`/`fl_ce_notice`/`fl_ce_doll` (save bits `0x1C5`–`0x1C7`) now share through the durable flag table like other quest flags, and no synthetic Doll count increment is published, so existing counter sync carries any native count change. Quest collectibles, keys, world equipment and their counts remain shared. |
| Other mechanisms | Indexed rotors `0x356` and ride-triggered platforms `0x3B4` now include their native motion/wait phases and timers. Falling traps `0x197`, elevators `0x1FC`, tilting platforms `0x1FD`, path crates `0x1F7`, pushable blocks `0x245`, and rising platforms `0x34A` have distinct phase adapters. Riders/pushers and opposite-floor elevator callers can take ownership. |
| Room switches and puzzle mechanisms | Placed `0x226` switches share hit/pressed/settled state and their definition-selected temporary or save flag. `0x324` variants 0–11 and `0x326` variants 0–7 share native activation, motion and endpoint continuations. Progress survives late entry, ownership handoff and proximity culling. |
| File_44 linked platforms | `0x228` shares activation, endpoint motion and native model setup. `0x1FE` also shares fire-triggered shake, melt/wait/reform phases, tint and its parent-owned child collider. Local fire can take ownership; reconstruction validates resources, immutable parameters and child generation. |
| Ghost Toys crane room `0x31` | One atomic checkpoint covers the moving crane, both pressure pads and the Wind-up Camera reward. Pad occupancy combines both players; leaving a pad does not release it while another player remains. Native motion/timers, power, reward removal, late-entry reconstruction and owner handoff share state; local scenes, camera and control remain local. Camera charging capability now accompanies ownership through item sync. |
| Timed shutters `0x354`, room `0xB2` | Phase, countdown, animation and emission ordinal survive culling and handoff. Each emitted `0xFC` robot has a distinct shared identity, current route checkpoint and late-entry reconstruction. Either player can kill it; one arbiter commits native death and generates its shared drop. Route completion removes it without a loot roll. |
| Super Pass Bridge, room `0x15E` | One checkpoint covers both `0x2D0` guards, the `0x240` gate and `0x311` blocker. Local accepted dialogue contributes the shared movement latch. Route progress, opening animation and blocker removal survive late entry and handoff; dialogue, blinking, shadow allocation and local control locks stay local. |
| Mt. Fear weapon obstacle `0x325`, room `0x14B` | One checkpoint covers both pieces, the shake/rise/split sequence, timer, model selection and completion. A local qualifying weapon hit requests ownership before taking camera/control; replicas retain their own cameras. Missing child reconstruction and continuation after the owner leaves use guarded local resources. |
| Silver Doll container `0x3D6`, rooms `0x16A`/`0x182` | Repeatable opening and closing cycles share a placed checkpoint. The nested Doll has one parent-based identity, shared descent from Y125 to Y34, late-entry reconstruction and competitive pickup arbitration. Only the winner runs the native collection dialogue/control sequence; Doll progress continues through the existing durable item synchronization. |
| Spike floors `0x3CA`, rooms `0x32`/`0x34`/`0x3A` | All46 placements share staggered wait, extension/retraction, animation frame and reverse state, including pause, late entry and culling/reload. Subtype2 player-distance admission and the native contact pass remain local. See [the spike audit](world-spike-audit-2026-09-19.md). |
| Jump ropes `0x1AA`, room `0x41` | All six placements share their complete native rotation accumulator, including pause, late entry, handoff and culling/reload. Each client runs the native rotation and contact passes once per update. See [the rope audit](world-rope-audit-2026-09-19.md). |
| Giant tops `0x365` and rotating platforms `0x366`, rooms `0x3E`/`0x3F` | All12 placements share yaw and, for tops, cosine travel phase and origin. Native motion continues locally between checkpoints; pause, late entry and culling/reload use established receipts. Static model bindings and each player's support/contact pass remain local. See [the File40 audit](world-file40-audit-2026-09-19.md). |
| File30 equipment `0x3D2`/`0x3D4`/`0x3D5` | Meat Hammer, Fire Ryo and Bazooka collection flags retire an already-loaded idle pickup and its local shine. A collector that already started keeps its native dialogue/control cleanup. Fire Ryo is equipment progression; ordinary health and ryo rewards remain collector-local. See [the equipment audit](world-equipment-audit-2026-09-19.md). |
| Doors and breakable walls | Door animation/mesh/pose checkpoints preserve local input and travel callbacks. Actual breaks of wall `0x23D` subtypes 1/3/4 propagate removal; subtype 3/4 progression flags retain the existing durable path. |
| Existing progression objects | Save flags, dolls, keys, upgrades, relevant doors and boss rewards continue through their existing synchronization modules. |

File_59 town NPCs carry 22 validated native continuations. These include gated
arrivals and the pickpocket's approach, return, delay and escape states. An
existing local NPC restores its own continuation; a reconstructed NPC also binds
native face selectors and the validated local escape-route table. Blinking stays
local. Native path/animation flags, timers, gravity and collision masks accompany
the checkpoint. Conversation callbacks and return/target pointers stay local.

A player talking to an NPC temporarily owns that NPC. Dialogue, camera/control
state, task pointers and native contact dispatch stay local. A replica does not
execute another player's path script. The path checkpoint is retained for a
subsequent ownership handoff, with route identity and instruction boundaries
validated against all 163 native routes.

Each actor chooses an active loaded peer. Cyclic actors first require a native
checkpoint receipt before a new instance may compete with established copies.
Among established copies, current interaction and then the lowest client ID
choose the simulator. This allows one player to keep an actor running when
another has distance-culled it. A native travel-door interaction can reopen a
closing door immediately. Missing/stale snapshots release native control;
a missing actor is never interpreted as a collection or destruction event.
One-way switches and mechanisms instead prefer the furthest native phase before
the interaction/client-ID tie-break. This prevents an older idle copy from
undoing an activation. Equal phases prefer an unpaused simulator. Their last
checkpoint remains as paused presence while unloaded, until room/reset cleanup;
reloading restores that checkpoint after the native model initializer is ready.

## Transport

`MNSG_WORLD`, `MNSG_WORLD_ACTORS` and `MNSG_WORLD_QUEST` are quiet, transient
`targetTeamId` packets with **no offline queue**. The client supplies its
assigned root `clientId`; Anchor relays it without authenticating or injecting
it. Receivers check roster, team, session and room eligibility. All three packet
types carry the same
`worldSync = [19, interactionSession, visit, rawRoom, rosterSignature]`
metadata. Protocol 19 is the current package. Older world-sync versions are
incompatible: version 19 added the File68 `0x338` fish claim lifecycle, version
18 added the File34 `0x3DA` boulder lifecycle, and version 17 added the six
File30 placed one-HP families
`0x196`/`0x330`/`0x331`/`0x332`/`0x339`/`0x3EC` to the dynamic lifecycle.
Version 16 added the
File30 slicer/RNG-spawner and File_40 bomb/counterweight children to the dynamic
lifecycle, the File_46 Koryuta wave graph, and the custom quest presentation
transport. Version 15 added the File40 top
and rotating-platform checkpoints. Version 14 added the File24 spike
cycle and jump-rope rotation checkpoints. Version 13 added the File_62
container checkpoint and its nested File_26 Doll lifecycle. Version 12 added the File_64
two-piece obstacle checkpoint. Version 11 added the coupled bridge
checkpoint and its accepted-event latch. Version 10 added typed shutters and
their dynamic robot lifecycle. Version 9 added the coupled crane
checkpoint and per-client pad inputs to version 8 physics/reward lifecycle and
version 7 instance/receipt and established-presence arbitration.
Both players must update.
Every packet also carries this metadata, a sequence, part index/count, presence,
interaction and removal bitmaps, and compact scalar actor rows.

The quest transport reuses that metadata and peer eligibility, and carries at
most 32 records of 64 words in batches of 8 rows and at most 4 parts. Its rows
are visible-scalar only: dialogue, camera, fade and temporary-bit state never
leave the client. Dynamic kinds 8–14 are the File30 slicer emitter and its
blades, the File30 RNG spawner, the File_40 bomb block, the File_46 wave graph,
the File30 placed one-HP objects, the File34 falling boulder and the File68
fish; the File_40 counterweight is placed kind 12.

Shutter kind 8 uses the same 50-word budget: timer17, phase18, immutable
emitter19, monotone emission ordinal20 and playback/reverse29. Only its simulator
executes the callback that creates robots. Dynamic kind6 is a bounded 83-word
route-57 robot recipe (entityFC/modelFB), keyed by room/layout/parent/emission.
It restores local resources and pointers before running its native continuation.
Word43 holds immunity,44 distinguishes a kill from route removal, and45 holds
route status400 as a boolean. Its one-HP lethal hits are deferred before native
damage, then committed through the existing claim lease and successful-send
gate. Non-committing peers suppress native loot creation; rewards still belong
only to the eventual collector. Full native evidence is in the
[shutter audit](world-shutter-audit-2026-09-19.md).

Bridge kind 9 also uses 50 words. Fields4–9 hold temp0/1, fresh-constructor
provenance, gate phase/frame/animation and explicit blocker removal. Each guard
has a 14-word block at10 or24: phase, pose, animation and route scalars. Field38
is pause,39–44 are velocities,45 is reserved zero,46 is local accepted input,
and47 is the aggregate delivered to native code. Fields46/47 stay zero on the
wire; the existing bounded `u` field carries at most one two-bit latch. Unlike
crane occupancy, accepted bridge latches remain relevant while a client pauses.
This adds no packet type, offline queue, per-frame publication or fan-out route.

An established native receipt takes priority over bridge phase progress. A fresh
save1 constructor uses different guard poses and must accept the incumbent's
live route checkpoint before competing for authority. Native apply defers during
local conversation, accepted-result cleanup or missing resources. Guards keep
real native dialogue continuations in their local return pointers. The two
verified movement-only routes can predict between snapshots; their completion
uses scalar state without allocating another shadow or releasing an unowned
control lock. See the [bridge audit](world-bridge-audit-2026-09-19.md).

File_64 kind 10 uses 50 words: both pieces' XYZ/angles, root model slot,
signed native timer, an explicit phase, collision/draw bits and completion.
Native application validates phase-specific timers and model slots before
touching either piece. The simulator receives a local ownership confirmation
before starting a camera sequence; this echo adds no network packet. Competing
hits can settle during the native60-update wait. Once a local
camera starts, its owner retains authority while paused so it can release its
own resources on resume. Camera-free owners can hand off during pause. Replicas
predict movement within the current phase and wait at its boundary for the
owner. A surviving owner can finish without constructing a camera sequence.
Save flag `0xA1` yields a canonical completed row even after native deletion.
The existing item-sync path also retains it as `wl_mt_gate` for teammates
outside the room and later team entrants. Receiving completion during a local
camera sequence leaves that sequence running through its own cleanup.
The [File_64 audit](world-gate64-audit-2026-09-19.md) records the field layout,
resource and lifecycle guards, and remaining runtime checks.

File_67's all-four-Miracle travel cinematic and File_70's entrance cinematics
keep native cameras, player positioning, dialogue, warp and finite particles
local. File_67's inventory prerequisites already share. File_70's room0x14D
completion flag0xC4 now shares as `fl_shore_entry`, alongside existing0xC3.
Receiving it suppresses a future native entrance sequence; it does not skip
cleanup of an already-active local sequence.
Placed packets include a `z` pause bitmap even when the sender does not own any
full rows. It prevents stale cached pause state from blocking ownership handoff.
The `h` bitmap distinguishes an established instance from one still waiting for
its initial checkpoint. Both bitmaps must be subsets of presence and identical
across an atomic batch.

Every placed-world part also carries `u`, at most one `[placedIndex,padMask]`
pair in room `0x31`, empty elsewhere. Inputs are transient, identically repeated
across a batch and combined only from present, unpaused, live same-room peers.
Word 46 is the local pad mask and 47 the aggregate delivered to native code;
both are zero on wire. The owner receives its own native input echo without
applying that echo as a pose update. Crane phases, power, completion and pad
input changes use the existing 50 ms edge floor. One row still has 50 words.

- Capacity: 256 roster entries, 50 integers per actor, 24 rows per packet,
  at most 11 parts; every part is limited to 8,192 bytes including its delimiter.
- Changed owner checkpoints: at most 5 Hz. Presence-only/unchanged refresh: 1 Hz.
  Membership, establishment, interaction, pause, removal, controller phase/flag and door playback-direction/stop edges can send early, with a 50 ms floor.
- Complete batches replace state atomically. Duplicate/older packets, malformed
  rows and mismatched sessions, visits, teams, rooms or layouts are rejected.
- Presence is independent of ownership. Only the elected simulator publishes
  each actor's full row; other peers advertise its presence.
- Cached peers expire after 1.5 seconds. Same-room removals live while clients
  retain that occupied-room state; an empty room has no durable world snapshot.
- Addresses never cross the wire. Platform continuations are locally validated
  indices into a native callback allowlist. Native source descriptors identify
  placed actors, not allocation order or pool pointers.

Placed fields `39–47` and dynamic fields `74–82` carry the optional File_59
continuation checkpoint: phase, two halves of D8, DC, E4/E6/E8/EA and local
route-table identity plus the native path status bit. The table index is checked
against its actual waypoint bounds before a continuation may run. Placed NPCs
also use fields 17 and 19–22 for timer, escape flags, collision mask, gravity and
playback enable. Continuation changes can send at the existing 50 ms edge floor.

In a placed row, fields `0–3` identify the actor and interaction; `4–16` encode transform,
animation and velocity; `17–25` hold verified platform phase/timer scalars;
`26–29` encode scale/visibility/gravity/mesh/terrain/door animation state; `30–37` hold NPC path scalars (platform
variant 9 reuses field 31 for its emission counter); field 38 is world-pause state.
For doors, field 29 bit 1 carries native reverse playback (task flag 0x01000000); bit 4 carries playback enable. Other actor kinds retain their existing bit meanings.
Switch kind 6 uses the same reverse/enable bits. Its fields 18–22 contain the
five-state continuation index, definition mode (0 temporary / 1 save), flag ID,
normalized flag value and native pressed mesh state. The mode and ID must match
the local definition before applying a flag. The effect return callback is
reconstructed locally; witnessed presses emit one native spark burst, while a
late or reloaded settled switch does not replay it. The separate timed/rearming
File_50 routines cannot be selected through its native entity initializer:
the initial byte test consumes word values 2/3 before the later word comparisons
can select those branches. This also holds for dynamically supplied definitions.

Mechanism `0x324` uses continuation indices 53–55, and `0x326` uses 56–59.
Fields 19–21 carry the immutable variant, definition-selected flag ID and its
normalized value. Their verified native motion callbacks operate on the shared
XYZ/velocity fields and retain local mesh resources. Only these typed flag
outputs are applied; neither temporary flag bank is copied wholesale.
For `0x326`, field 24 marks a completed pose selected by the save-aware
constructor. That pose outranks an untriggered copy but yields to an observed
move or real completion, so a late entrant cannot finish everyone else's
animation merely by loading an already-set save flag.

`0x228` uses phases 60–63, immutable decoded endpoint/speed in fields 19–22,
initialized state in 23, gate/model selectors in 24–25 and sound selector in 31.
`0x1FE` uses phases 64–68 and the same endpoint/initialized fields. It stores
melt state/shake timer in 24–25, immutable flags in 30, tint angle in field 31's
low ten bits and travel direction in bit 10. Fields 32–33 and 37 hold child
collision enable, presence and native immunity byte. Fields 40–41 hold red/green
work in hundredths, 42 the parent's collision mode, 43–44 signed halves of the
packed visible RGBA, and 45–47 the child's positional offset in hundredths.
Field 39 remains zero. Its display list and child/hitter pointers are rebuilt
or retained locally. A failed child allocation retries without decoding the
native parameters twice. Culling retires the owned child and retains a paused
parent checkpoint; disabling synchronization leaves native children alive.

Activation is ranked separately from these cyclic movement phases.

Fields 48–49 are native/Python bridge bookkeeping, always zero in peer packets.
Field 48 identifies this local task incarnation; field 49 acknowledges an offered
checkpoint. Python addresses each offer to the current incarnation and issues a
receipt token that does not reset with a room/team scope. The token's bit 30 marks
initial bootstrap, which may correct a fresh rider's platform before treating
that rider as an established owner. The native code echoes the token only after
application succeeds. Active local dialogue/travel does not falsely acknowledge
an unapplied pose. A failed application reports `0x3FFFFFFF`, revoking readiness
until recovery. Reused slots and changed scopes cannot reuse old receipts.

A late entrant waits for an established copy, including a paused incumbent,
then becomes eligible only after native application. Only a checkpoint included
in the owner's latest complete publication can bootstrap an instance; a former
owner's presence-only refresh cannot revalidate its old cached pose. The initial
server membership snapshot must arrive before world authority starts. New member
metadata prompts a checkpoint refresh; unanswered/stale members use the existing
1.5-second liveness bound rather than permanently preventing local simulation.
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

`MNSG_WORLD_ACTORS` carries at most 128 records, 83 integers each, in batches of
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
The wider ROM inventory inspected 800 metadata slots, finding 374 metadata
records, 333 actor-data waves, 292 nonempty placed rosters and 255 distinct
placed entity types. All three native source lists total 3,888 placements;
the largest room roster has 57. The adapters above were
traced against their real native initializers, continuations and cleanup paths.
File30 `0x322` (six placements), `0x33B` (41) and `0x33C` (21) need no
shared-state adapter on the inspected placed path: their constructors bind
static models, and the shared callback `080003B0` only toggles a local render
bit (task+0x68 `0x20000`) that main `801E63AC`/`801E674C` selection and helpers
`801E6BC8`/`801E6D30`/`801E6F00` set and clear during local traversal. Sharing
it would alter another player's local view, and no persistent world mutation was
found; unrelated dynamic invocation is not proven absent. All 33 placed
`0x1F4` records carry `D4` gate 0 and are static geometry; an optional nonzero
`D4` one-shot effect has no persistent motion to stream, though a scripted or
dynamic invocation is not exhaustively excluded. File12 `0x064` also needs no
adapter on the inspected placed path: all 34 records' constructor `80213F20`
selects a static model/draw from `D0`/`D4`, its callback `80213F14` is empty, and
no mutable state, hit, loot or child was found. The production classifier
matches 884 placed-world roots, 742 regular enemies and three coupled bridge
members. The remaining 2,259 placements are what the classifier did not match,
not a list of unsupported actors: they include runtime NPC promotion, static or
save-derived actors, bosses and genuine gaps. That remainder is a worklist, not
an unsupported-actor count and not proof of complete lifecycle coverage.

The original gaps for common dynamic NPCs, container/enemy loot and late-entry
emitter/platform hazards now have implementations, as do the File30 slicer and
RNG spawner, the File_40 bomb and counterweight graphs, the File_46 wave graph
and the four quest presentations above. That does **not** establish universal
support for every native actor. The next native controller audit is tracked in
`world-controller-audit-2026-09-16.md`:


- Minigame and cutscene controls stay local. The four quest families above
  publish visible scalars only; their dialogue, camera, fade, player-control and
  temporary-bit scripts are not reconstructed. A reconstructed NPC uses the
  common dialogue/path continuation except for the 22 File_59 continuations
  described above. The native scene graphs are recorded in
  [the quest controller audit](world-quest-native-audit-2026-09-19.md).
- Room-specific puzzle controllers and other unaudited actor families still
  need explicit adapters. In particular, shared progression flags alone do not
  synchronize every temporary room flag or already-loaded puzzle state.
- The tracked hazard recipes are the File_34 emitter children, File_44 platform
  children, File30 slicer/RNG children, File_46 wave children and File30 one-HP
  children listed above, not arbitrary projectiles or visual effects. An empty
  actor-specific callback is not enough to classify a prop as static: common
  hit/death and child lifecycles must also be checked. The remaining work is the
  actor families that still have no adapter.
- The live-set and removal-history limits are explicit. Repeated scripted NPC
  births at an identical descriptor need game testing beyond the current
  simultaneous-copy/ordinal tests.
- File30's placed one-HP objects share kills and drops but are never
  reconstructed from a snapshot: late reconstruction is disabled for that kind,
  so a peer only tracks the copies its own native initializer created, and the
  `0x331`/`0x332` linked children match only to their own native birth.
- The File68 fish claim is same-room only, and its two concurrency cases are
  confirmed gaps, not merely unproven. Flag IDs repeat across the room pairs
  `0x168`/`0x17E` and `0x16B`/`0x180`, so two clients collecting that pair in
  different rooms at the same instant is a team-global lease gap the typed claim
  does not cover; a simultaneous collection of different fish whose shared
  per-colour counters then take a maximum is the second. The native count
  increments only when the active quest gate, variant and colour cap
  (8/5/3) permit, while the later continuation sets the per-fish flag regardless
  of whether the increment happened, so the placement flags cannot reconstruct
  the exact count. The existing Anchor relay
  has no atomic cross-room claim, so an exact fix needs a durable per-flag
  contribution ledger with team-wide arbitration and a saved baseline. Neither
  case has a live two-client run.
- Native callback/path validation uses the US actor data. A modified path
  program requires regenerating/reviewing its instruction-boundary masks.

## Validation and playtest

Run `bash tests/run_world_sync.sh` for the real C implementation under UBSan and
release-style floating-point flags, the C/Python codecs, metadata merging, framed
receive-loop routing and transport lifecycle tests. The native test uses simulated
native tasks; it does not run the game's renderer, scheduler or collision engine.
The runner also builds the quest native harness and the File_40 bomb adapter;
the dynamic children harness carries the slicer, RNG-spawner, wave and File30
one-HP-object lifecycle tests, and the Python discovery run covers the slicer,
RNG-spawner, bomb, counterweight, wave and quest transport suites.

`python3 tools/inspect_world_actors.py /path/to/mnsg.us.decompressed.z64` verifies
the 163 derived path masks without copying game assets into the repository.
`python3 tools/inspect_world_roster.py /path/to/mnsg.us.decompressed.z64` checks
the normal, partition and resident lists. The latter adds room `0x30`'s two
`0x1FC` platforms and room `0x131`'s five `0xFC`/`0xFE` enemies to the existing
native adapters. Resident sources append after the normal and sorted partition
slots; they retain their raw source/definition pointers locally. Changed roster
signatures exclude clients that still omit these sources. Version 16 adds the
quest packet type and its records, version 17 adds the File30 one-HP dynamic
kind, version 18 adds the File34 boulder dynamic kind and version 19 adds the
File68 fish dynamic kind, but none adds a new per-frame publication, offline
queue or fan-out route. The production roster harness
checks capacity, deduplication, reloads, pool reuse and missing-wave retries.
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
8. Activate a gated `0x228` platform and hit `0x1FE` with fire from either client.
   Join during shake, fade, wait and reform; compare motion, tint, child collision
   and remaining timers. Cull/reload the parent and test owner departure. Include
   a lower-ID late entrant, a paused incumbent, resource failures and reused
   native instances when validating bootstrap in game.
9. In crane room `0x31`, power the puzzle from either client. Hold separate pads,
   then the same pad; one player leaving must not release the other player's
   input. Join during each crane/reward phase, pause the simulator, leave and
   return, and replace culled children. Collect the Wind-up Camera and verify
   ownership plus charging on both clients, with no remote scene/control replay.

Current validation results are recorded in `world-sync-validation-2026-09-16.json`.
The later loot playtest fix and its checks are recorded in
`loot-sync-validation-2026-09-16.json`.
Coin loop playback and door reverse-closing checks are in
`world-animation-validation-2026-09-16.json`.
Crane room, pad inputs and native lifecycle checks are recorded in
`world-crane-validation-2026-09-19.json`.
A fresh two-client in-game run has **not** been performed; host native mocks,
loopback TCP checks and successful packages do not establish visual, collision,
or universal room coverage.

`tools/test_world_dynamic_anchor_local.py --port PORT` adds a three-client
128-child/11-part probe: atomic current-state late entry, claims/commit,
sender/team isolation, duplicate rejection and origin disconnect/handoff.

The later File_59 continuation and rotor/platform checks are recorded in
`world-continuation-validation-2026-09-16.json`.
Switch/mechanism checks are in `world-controller-validation-2026-09-16.json`;
File_44 platform checks are in `world-linked-platform-validation-2026-09-16.json`.
Physics puzzle checks, package hashes and local Anchor results are in
`world-physics-validation-2026-09-19.json`.
Native receipt and occupied-room handoff checks are in
`world-bootstrap-validation-2026-09-17.json`.

## Physics puzzle 0x3D0

File_30 room 0x35 now shares its carried/rolling pose, previous X/Z velocity,
throw countdown, geometry-contact bits, native task mode, shrink and two
completion clips, reward timer and terminal state. Native player carry pointers
remain local. A genuine grab takes simulation ownership; conflicting grabs
resolve to one simulator and the losing local carry action is ended through
native helpers. Imported held objects are excluded from the local pickup search.
The local player's health only affects simulator eligibility; health is not sent.

Phases 69–74 identify the six native continuations. Phase 75 records an actual
break or fall removal. Native proximity culling clears source +0x70 first and
does not create that break. A subsequent native source allocation after a known
break advances the attempt counter. First-time entrants restore the current
attempt; ordinary culling retains it. The counter is bounded to 8,388,607 and
never wraps into a previous attempt. Completion is permanent for the room visit and outranks even a later retry.
Decreasing reward timers are ordered so older checkpoints cannot rewind them.

The authoritative reward loop preserves the native 200-through-zero timer,
21 emission opportunities, 80/20 coin/health choice, upward speed 5 and horizontal
speed 0.9. A deterministic hash of roster signature and reward slot
chooses the item and angle independently of the local input RNG. Native child
allocation and item initializers still build each drop. Slots 0xFF00–0xFF14 use
the existing per-parent/per-kind loot identity, so revisiting a boundary during
handoff coalesces with its live or already-collected item. Native duplicate
copies are retired without a new shared tombstone. Healing and ryo remain
exclusive to the winning collector.

Rows keep 50 words: +19/+20 are previous X/Z velocity times 1000, +21 is the
0–5 throw countdown, +22 encodes airborne/contact/held bits, +23 is attempt,
+24/+25 hold native actor health/invincibility bytes, and +30 selects one of seven
verified task-flag values. +17 is used only for the 0–200 reward timer. No native
callback, source, ground, player or resource pointer is transmitted. Motion and
geometry integration pause with the simulator while retaining velocity for a
handoff. Static-to-animated reconstruction guards both model waves and File_30.

This adapter has native-host, codec and transport coverage; native routines in
the host harness are substitutes. Fresh two-client game validation remains open.

## Local travel decisions

File67's departure choice0x6B and consumed dialogue cue0x6C are excluded from
item replication. This also rejects old `fl_outerspace`/`fl_to_space` keys in
queued deltas and live/stored snapshots. The four Miracle items and durable
File70 entrance completions0xC3/0xC4 still share. Native local travel, camera,
dialogue and existing save bits keep their normal lifecycle; old saves are not
rewritten. See [the travel flag audit](world-travel-flags-audit-2026-09-19.md).
