# Remote Player Movement Reconstruction

## What the captures show

The original 12.72-second recording averages 58.07 display frames per second,
with no captured gap over 25 ms. The remote skeleton keeps animating while its
root and facing advance in packet-sized bursts. The stutter is therefore in the
remote world transform, not the capture or animation loader.

The first smoothing pass removed those steps with a cubic curve, but the
18.96-second result recording exposes a different failure during the cleanest
jump comparison:

- source takeoff is about 16.85 s and remote takeoff about 17.10 s;
- source apex is about 17.15-17.20 s and remote apex about 17.50-17.65 s;
- source landing is about 17.45 s and remote landing about 17.90 s;
- source airtime is about 0.60 s while remote airtime is about 0.80 s.

The delay grows through the jump and the arc lasts roughly one-third longer,
so this is not a fixed network offset. Two implementation details caused it:

1. The cubic receiver deliberately rendered a complete six-game-tick packet
   behind the newest state while action and animation changes remained
   immediate.
2. `velX/Y/Z` described the average displacement since the previous packet,
   not the velocity of the authoritative endpoint. That averaged away native
   acceleration, the vertical sign change near an apex, collision landings,
   and abrupt stops before the curve used it as an endpoint tangent.

The 21.08-second tuning recording then isolates the remaining cadence errors
because the right window follows the authoritative player while the left
window holds a fixed camera on the remote copy. At 8.47-8.70 seconds, the
source makes one monotonic turn while the remote alternates front/back on at
least six consecutive 30 Hz ticks. During the cleaner 8.90-9.30-second run,
remote screen-space displacement varies by 3.1x between ticks and peaks about
0.23 seconds apart, matching packet/correction cadence. The run cycle keeps
advancing uniformly through both the root troughs and catch-up surges.

## Native movement evidence

The supplied function list is a first-hit coverage trace: all 843 `func_*`
entries are unique and it contains no timestamps, arguments, or repeated frame
markers. It identifies exercised code, while the static recompilation and
Ghidra output establish the per-frame order.

The task scheduler `func_80034734_35334` calls a player's callback slots in
`+0x08`, `+0x0C`, `+0x10` order:

1. `func_801CB824_587734` advances the animation and performs high-level
   checks.
2. The action callback runs. During running, the trace reaches
   `func_801E1928_59D838`, `func_801E1C90_59DBA0`, and
   `func_801E3DE0_59FCF0`. The current input/action computes commanded
   locomotion in task fields `+0xE4/+0xE8`; there is no long absolute-position
   easing stage.
3. `func_801CBAF8_587A08` performs post-action movement. Its
   `func_801CC4C0_5883D0` path advances persistent vertical velocity at `+0xA4`
   and consumes one-shot auxiliary offsets at `+0xC0/+0xC4/+0xC8`; ordinary
   horizontal locomotion flows from `+0xE4/+0xE8` into
   `func_801CD310_589220` for collision/contact resolution.
   `func_801CD084_588F94` records actual displacement magnitude and finalizes
   transform/orientation from the resolved result.

The visible model stores float XYZ at `+0x08/+0x0C/+0x10` and signed 16-bit
rotations at `+0x14/+0x16/+0x18`. Anchor samples these fields after
`func_80002040_2C40` returns, so every published position is already the
sender's post-collision authoritative result.

`func_801CBF0C_587E1C` is an initialization/reset path, and object `+0x28` is
the animation frame. Neither is the steady movement integrator.

`func_801CB824_587734` advances object `+0x28` each native tick by the action
record rate at task `+0xD0` plus the live adjustment at player work `+0x1C`.
`func_801DAD68_596C78` seeds `+0xD0` from the selected action record and clears
that adjustment on an action bind. Therefore the correct network animation
rate is the actual loop-aware `+0x28` delta from the latest sender tick, not a
packet-frame delta divided by an assumed six ticks.

The clock boundary is important. `main_thread_entrypoint` counts VI messages
and wakes `game_thread_entrypoint` when the counter reaches
`g_system.field15_0x3adca`; the normal step/reset path sets that divisor to 2.
Consequently `func_80002040_2C40` runs at 30 game ticks per second even when
RT64 presents near 60 display frames per second. Six publisher calls are about
200 ms, not 100 ms.

The normal jump path seeds task `+0xA4` to 6. The player-work initializer stores
`0xBF2AAAAA` (-2/3) in work `+0x48`; `func_801CC4C0_5883D0` adds that value to
persistent vertical velocity `+0xA4` after movement and clamps falling velocity
at -8. That produces the capture's roughly 9-tick ascent and 18-tick airtime.
The client uses only the -2/3 value to seed a coherent airborne entry (including
a zero-velocity ledge fall) or an unambiguous first midair sample, then replaces
it with acceleration measured from transmitted endpoints.

## Sender sampling

Normal movement remains coalesced to about 5 Hz, so its steady-state packet
count is unchanged. The game-side publisher now samples the final float
transform every valid game tick and computes `velX/Y/Z` from the last
tick-to-tick displacement at 30 Hz.
It also computes shortest-arc `rotVelX/Y/Z` from consecutive signed 16-bit
rotations. These endpoint velocities are attached only when the existing packet
policy sends a state.
The publisher similarly computes loop-aware `animStep100` from consecutive
native animation frames. `hasAnimStep` distinguishes a valid zero-rate paused
clip from an older peer that did not send an endpoint animation rate.

Starts, stops, and strong sign reversals in either linear or angular endpoint
velocity bypass the six-tick timer. These are sparse state edges, not a higher
steady-state cadence; they prevent a wall clamp, landing, or released turn from
remaining unknowable to the receiver for up to another 200 ms.
The start/stop classifier uses a 25-percent hysteresis band, so signed-angle
quantization hovering around the threshold cannot bypass the rate limit every
game tick.

This matters most on Y: when those frames are sampled, packets retain changing
upward/downward speed and a collision-clamped zero at landing instead of one
six-tick average. The Python bridge accepts this endpoint velocity directly.
Its old packet-interval estimate remains only as compatibility behavior for
callers that omit the new arguments.

Hot movement sequences are compared modulo the positive 31-bit counter before
they enter the shared player state. When both packets have sender timestamps,
time must also move forward; this prevents a delayed packet from before a
reconnect from winning merely because the reset sequence makes it look
forward. Full lobby membership snapshots preserve the latest same-client,
same-room movement, appearance, and character state while still removing
departed players and invalidating transforms across room changes.

Room changes, invalid player pointers, disconnects, and reconnects clear the
frame baseline. A first sample in a new authority therefore reports zero
instead of deriving a false high velocity across unrelated coordinates.

## Receiver reconstruction

`src/anchor_remote_motion.c` follows the native command-before-integration
order without running a second gameplay simulation:

- A new coherent packet installs its newest endpoint velocity for the current
  game tick and integrates it once. There is no six-tick presentation
  buffer and no cubic position curve.
- Between packets, X/Z carry that post-collision per-frame displacement at
  constant speed. Prediction is limited to one expected packet interval, then
  holds if the stream is late.
- For consecutive samples in the same action, Y also derives a bounded
  acceleration from the change in endpoint velocity and sender elapsed frames.
  It integrates `vy += ay` before Y each missing frame, matching the shape of
  native gravity through a jump apex without running floor or wall collision.
  Both seeded and endpoint-derived downward acceleration clamp at the native
  -8 units-per-tick terminal velocity unless the authoritative endpoint itself
  proves that a special action is moving faster.
- Action edges that cross the airborne boundary pin Y to the packet immediately
  and reset the acceleration estimate. Transitions among airborne actions
  `0x17-0x1a` keep the derived gravity instead of hovering near an apex. A
  negative-to-zero vertical endpoint also clears all Y correction debt as a
  collision-clamped landing. Non-airborne vertical stops receive the same
  treatment, so moving-platform motion cannot turn into a false fall.
- The difference between the predicted transform and the authoritative packet
  is treated only as correction debt. It is paid in constant four-tick batches,
  rather than with an ease-in/ease-out curve that changes the apparent airtime
  or concentrates a visible speed pulse into two ticks. If a direction-safe
  cap prevents one batch from repaying a large residual, the remainder is
  retained for the next batch instead of being discarded into a repeating
  half-speed/full-speed sawtooth. Residual repayment pauses with the prediction
  horizon, so correction alone cannot drag a stale model backward.
  Each moving axis is capped around half its carried endpoint speed (with a
  small near-stationary precision floor), and opposing correction is further
  direction-limited. A large residual can slow continuing motion but cannot
  reverse it or create a catch-up surge.
- A zero endpoint on either horizontal axis after motion pins that axis to the
  authoritative packet and clears its old debt, so a diagonal wall contact can
  stop wall-normal X or Z without freezing the tangent axis.
- On a packet frame, at most one step of the previous correction is applied.
  That step is first rebound against the newly installed endpoint velocity, so
  old debt cannot overpower a sharp slowdown or nonzero direction reversal.
  The newly measured correction begins on the next frame, preventing two
  correction impulses on one game tick.
- Rotation prediction uses a conservative rate bounded by the one-tick
  endpoint derivative and the packet-average shortest arc. A 32-angle-unit
  per-tick deadzone treats a finished native turn as stopped. Every new packet
  discards stale angular correction, pays the new shortest-arc residual in
  three equal steps beginning on the packet tick, and bounds reversals by the
  previous and new rates. Opposing correction is capped by the continuing turn
  rate, while coherent slow turns below the deadzone remain carried. This
  prevents the correction from crossing its moving target, reversing a live
  turn, or carrying a one-frame angular spike through six ticks.
- Sender timestamp and sequence deltas determine acceleration timing and reset
  guards through render hitches and normal arrival jitter. A packet observed a
  receiver tick late is projected by that measured phase difference instead of
  correcting backward toward its past endpoint, including carrying the
  projected vertical-velocity phase. That phase lead persists across later
  normally timed intervals until an early interval pays it back, preventing a
  one-packet slowdown pulse. Early packets accumulate signed phase down to one
  full packet interval and back-project the endpoint by that many native steps,
  rather than turning repeated early arrivals into catch-up bursts. On an early
  endpoint-rate change, the current receiver tick finishes at the previous
  rate and adopts the future rate on the following tick. If multiple states
  were coalesced, the receiver rebases once to the newest authority and keeps
  its endpoint velocity instead of repaying several missed intervals as a
  concentrated burst.
- The remote animation clock consumes `animStep100` directly, applies the same
  signed root phase, and retains the action record's native rate only as a
  compatibility fallback. Same-action phase error is loop-safe and repaid at
  no more than half a clip frame per tick; only a true discontinuity over three
  frames snaps. Ordinary forward loop wraps preserve the measured endpoint
  rate; negative or implausibly large same-action resets fall back to the native
  record. Paused clips, short loops, early edge packets, and source/target
  clip-count mapping therefore stay synchronized with the reconstructed root.
- The Python lobby view is sampled on every 30 Hz game tick, avoiding the extra
  phase-dependent delay and edge coalescing caused by polling it at 15 Hz.
- The prediction horizon remains the sender's fixed six-tick policy. Immediate
  action/appearance packets do not train it downward and create a later hold.
- First samples, room changes, sequence resets, stale gaps, and teleport-sized
  residuals restart the state. A discontinuity holds at the new authority until
  a coherent follow-up packet instead of trusting a cross-boundary velocity.

X, Y, and Z remain floating point through the remote cutscene-model object and
nameplate projection. Action, animation frame/count, appearance, and character
remain discrete authoritative state. Root motion is no longer intentionally
held one packet behind, and action edges crossing the airborne boundary align
the vertical root; XZ residual and deliberate rotation correction can still
settle briefly.

## Why native collision is not replayed

The native order is the guide: choose current motion, integrate one frame,
resolve collision, then render the final float transform. The transmitted
vector is a finite difference of two final display transforms, not the native
`+0xE4/+0xE8` command or `+0xA4` velocity. Replaying floor tests or wall
collision on a visual-only remote would create a second simulation with
different timing and geometry state. The client instead carries the latest
resolved displacement for a bounded interval, derives only a short-lived Y
acceleration from authoritative endpoints, and absorbs the next residual.

Host-side trajectory tests cover in-phase constant motion, alternating 5/7-tick
arrivals, native takeoff gravity and terminal fall speed, sparse
start/stop/reversal detection, per-axis wall stops, unfinished-correction
retargeting, retained large-deceleration debt, nonzero reversal rebounding,
angular deceleration and stale-debt reversal, quantized slow turns,
shortest-arc wrap, bounded prediction, sender-time cadence, room changes,
teleports, stale gaps, sequence reordering, and membership-state preservation.
Animation-clock tests cover native and endpoint rates, early edges, signed root
phase, pause, short loops, natural-wrap classification, bounded correction,
discontinuity snaps, and clip-count mapping.
Build/package success is still not runtime proof; a two-client run is required
to judge the final feel under real packet timing.
