# Remote player collision

Remote models retain their standalone task and animation architecture. A local
contact body now constrains their displayed position against the room, registered
moving collision meshes, the local player, and other visible remote players.
The packet target and animation timeline remain the sender's state. Contact
changes the receiving client's resolved position; it does not run a second
playable character's input, combat, camera, or gravity callbacks.

`src/anchor_collision_world.c` uses the native static and dynamic wall correction
queries (`func_8002EB10_2F710`, `func_80030730_31330`) and nearest geometry ray
query (`func_8002C9D4_2D5D4`). Sweeps cover the body's leading edge, shoulders,
feet, and head; the wall probes use the native normal-player envelope, scaled
with the model, including Mini Ebisumaru. Floor and ceiling corrections preserve
received airborne height rather than applying local gravity. Rays are divided
into 32-unit segments because the dynamic query internally initializes its
nearest squared distance to 2000. Extreme position corrections advance at most
512 world units per update and still check intervening geometry.

`src/utils/anchor_collision_math.c` sweeps cylinders against peer bodies and
preserves tangential movement. The per-character radii/heights come from the
native unsigned tables at `D_801FC660_5B8570` / `D_801FC668_5B8578`, multiplied by
the model scale. Native code uses those tables for hit detection; the mod uses
their dimensions for solid contact without registering remote combat actors.
The shared contact solver checks the result against both peers and the world.
Overlapping spawns try bounded alternative separation directions; a model with
no free placement stays hidden and retries instead of becoming an overlapping
or invisible obstacle.

The local player's `func_801CBAF8_587A08` entry/return hooks capture native
movement and constrain it against visible remote bodies. A contact correction
is checked against world geometry and applied to the native player's three
display records before the frame-end position publisher runs. Scene-owner
changes, hidden models, disconnected peers, and retired slots clear contact
state. Nameplates follow the resolved displayed position.

## Pushing and combat

Moving into another player pushes that player's actual local body. The sender
publishes native attempted horizontal travel before peer contact as `driveX`
and `driveZ` (hundredths of a world unit per second; compact `dx`/`dz`). This
retains pressure when contact stops the sender's published position. The
recipient uses only approaching, vertically overlapping contacts, applies a
bounded displacement, then checks its own walls and other player bodies. It
publishes the corrected position normally. Injected pressure is excluded from
outgoing drive so it cannot feed back between clients. Drive stops immediately
on release or scripted control; a remote sample older than 12 game ticks stops
applying pressure. Coincident-spawn separation remains the collision solver's
responsibility.

Combat observes the game's active attack spheres at `func_80033404_34004`
inside the player-manager scan. The hook accepts only the actual local player
or projectile tasks owned by that player. Sphere offsets use native scale and
rotation before intersection with visible remote bodies. Each attack activation
can hit each remote life once; multiple spheres and native victim-group scans
share the same hit history. New melee swings rearm on descriptor or animation
restart, while looping projectile animation does not rearm a projectile.

`MNSG_PLAYER_HIT` is a targeted, nonpersistent event. The victim applies it at
the real player's `func_801CB824_587734` pre-update boundary through native
damage intake. Ordinary hits start at one HP unit (half a heart); native armour
and Sudden Impact vulnerability still apply. Native hurt actions, knockback,
invulnerability, context-specific reactions and death handling remain in use.
Existing native enemy hits and environmental hazards take priority. Collision
pressure by itself does not cause damage. Players can attack peers on either
team in the same area.

Movement carries a per-connection `interactionSession` and a `playerEpoch`
that changes with the local owner, room, death state or scripted-control gate
(compact `ps`/`pe`). Hit events bind both participants' sessions and epochs,
and use per-sender ordering. The receiver rejects self hits, wrong targets,
other areas, departed or mismatched sessions/lives, repeated events and events
older than 500 ms in its local queue. Disconnect clears queued hits. Old clients
without this context can still render and collide but cannot exchange combat
or pushing. Update both clients.

PvP HP loss is removed from the race Team Damage Sync comparison baseline.
Other damage or healing in the same frame still syncs, and the No Hit race
challenge still treats a damaging player attack as a hit.

## Scripted movement

Collision bypasses immediately when either native condition is active:

- `D_800C7AE0 & 3`: the native player collision-suspension mask.
- `D_800C7AE3 != 0`: scripted player input, including move-toward and Eocs input
  playback.

The sender includes `collisionDisabled` in `MNSG_PLAYER_POS`; the compact lobby
snapshot exposes `cd`. Entry and exit bypass both send throttles. The receiver
disables contact when either its local gate or that remote's flag is active.
When control resumes, collision starts at the current scripted destination,
without sweeping back through the previous path. Legacy packets omit the flag
and default to collision enabled; use the updated mod on both clients for the
remote cutscene exemption.

## Validation

Host tests cover swept peer crossings, vertical separation/contact, sliding,
coincident spawns, Mini dimensions, native script gates, static/dynamic walls,
fast movement, floor/ceiling clearance, step/headroom conflicts, and moving
geometry pushing a stationary model. Native geometry calls in those tests are
analytic mocks. Python tests cover the actual packet send/merge/snapshot path,
ordering, room invalidation, legacy defaults, and both cutscene transitions.
Additional tests cover attack geometry and activation deduplication, native
damage intake with mocked armour/invulnerability/hazard/cleanup paths, targeted
event session/life/expiry checks, bidirectional push pressure and its rejection
at walls or a third player. These are host tests, not native gameplay captures.

These checks and MIPS package builds do not certify native gameplay. A fresh
two-client run still needs to cover all four characters, walking/jumping into
each other, walls and movable objects, Mini tunnels, scripted destinations,
room changes, reconnects, and busy rooms with many remote models. Native scene
geometry can differ from the analytic test fixtures; special swimming/crouching
poses and runtime performance require that gameplay check.
Also check each character's melee and projectile attacks in both directions,
armour, repeated attacks during recovery, lethal hits, race damage sync and
No Hit mode, pushing a stationary peer into walls, and entering/exiting a
cutscene with a hit in flight.
