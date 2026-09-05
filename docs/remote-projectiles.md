# Remote projectiles and hurt recovery

Each successful local weapon activation sends one compact spawn event. Other
clients create the projectile at that starting pose, then advance its motion
and impact effects locally. Install the updated package on every client.

## Spawn events

The source hooks the entry and return of the six native projectile update
bodies covering Goemon coins, charged coins, Ebisumaru camera effects, Sasuke
bombs and kunai, and Yae shots. It reads the first initialized native pose and
velocity, including the delayed initialization of thrown bombs. Each native
task lifetime produces one event; ordinary subsequent updates send nothing.
The native task initializer forgets a retired task before its address is reused.

The previous capture filter required display-object byte `+0x65 == 0`.
Native projectile records normally initialize it to `1`, and the game renders
nonnegative values. The corrected decoder accepts that normal native record.
A host fixture specifically covers this failure.

`MNSG_PROJECTILE_SPAWN` carries owner, connection session, room and player epoch,
plus a compact `spawn` array with these twelve integers in order:

`id, kind, x100, y100, z100, vx100, vy100, vz100, rx, ry, rz, scale100000`

Positions and initial velocities use hundredths of a world unit; velocity is
per native 30 Hz tick. Rotations preserve signed 16-bit values, including the
billboard sentinel. Uniform scale uses hundred-thousandths. Kind includes the
native weapon variant. Model pointers, material commands, colors, animation
frames and repeated position snapshots are not transmitted.

Distinct throws use a bounded FIFO. The sender retains failed sends with the
same event ID; the receiver acknowledges only after a local spawn succeeds.
Repeated packets cannot duplicate a projectile. Pending throws expire after
750 ms, and connection, room and player-lifetime changes prevent stale replay.
This transient queue is separate from durable item synchronization.

## Local simulation

The receiver uses native kind-2 display primitives with locally staged weapon
resources. Native motion constants drive linear shots, bomb gravity, rocket
acceleration, charged-coin return and impact growth/fade. Each projectile has
its own lifetime after spawning; it does not need continuing owner snapshots
to remain visible. Native geometry queries determine local world impacts.

The compact event does not transmit a homing target: upgraded Yae shots follow
their initial heading locally. Charged-coin return visuals use the fixed
starting trajectory, and Yae trails use one shared layer to stay within the
display-object budget. These visual approximations do not change the source
player's native projectile or the existing hit synchronization.

The original gameplay constructors also charge local ammo, bind the local
player's private resources and enable outgoing attack descriptors. The remote
visual uses its own update instead, preserving local inventory and the existing
PvP hit-event path. Drawing a projectile cannot apply a second hit.

There are at most 32 simultaneous simulated projectiles, each using up to two
native display objects for trails or layered effects. Objects are reused and
released through native owner teardown. A missing resource or temporary
allocation failure defers the affected spawn; readiness is checked per weapon
family. A departed or replaced owner invalidates its active shots.

Debug builds log capture, successful send, resource readiness and local spawn.
The stress tool can inject a throw and report transport counters, making the
capture-to-packet-to-spawn path observable.

## Hurt-recovery blink

Appearance bit `ANCHOR_APPEARANCE_HURT_RECOVERY` follows the native local
player's recovery timer at task `+0xd4`. Its start and end use the existing
immediate appearance updates. Receivers use their native frame-counter parity
to alternate display-object `+0x64` bit 0 while recovery is active. The exact
blink phase can differ between clients; no packet is needed per blink.

The blink changes render visibility while collision, animation, nameplates,
Mini Ebisumaru scale and Sudden Impact appearance continue normally. Ending
recovery or reusing a remote slot clears the blink bit.

## Validation

Host tests cover the actual native capture record, bounded event encoding,
stable native lifetimes, send retry and queue expiry, packet deduplication,
room/session/life changes, simulated trajectories and impact lifetimes, and
recovery visibility without collision changes. Transport tests include the
actual loopback TCP dispatch path.

Native runtime checks must separately verify both clients' outgoing and
incoming throws, impact effects, character changes, area changes and reconnects.
A successful host test or package build alone is not native gameplay proof.
