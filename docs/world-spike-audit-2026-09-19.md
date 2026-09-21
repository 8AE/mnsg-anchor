# File24 spike-floor synchronization

Protocol14 adds all46 placed entity `0x3CA` spike floors:21 in room `0x32`,
17 in `0x34`, and8 in `0x3A`. These three native subtypes share their current
waiting, extension and retraction state, including the initial stagger,
countdown, animation frame and reverse bit. Local contact stays in the native
common post callback; subtype2 recomputes its player-distance admission locally.

## Native evidence

The US decompressed ROM SHA-256 is
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
`func_08000980_6ACED0(void *task, void *object)` is File24 VRAM `0x08000980`,
ROM `0x006ACED0`, size `0x384`. Register/call disassembly and every placement
definition were checked. The callback keeps its identity and selects its body
with task byte `+D4`:0 initializes,1 waits,3 extends,4 retracts. Phase2 is unused.

Definition halfwords `+4/+6` become immutable task subtype/seed `+D0/+D2`.
Subtype0 waits seed*6 and resets to155; subtype1 waits seed*4 and resets to90;
subtype2 waits `((20-seed)*20)%130` and resets to90. Waiting decrements the
unsigned `+8A` halfword and tests its old value, so the transition stores
`FFFF`. Extension emits local positional sound `0x271`. On completion,
retraction starts at frame2.0; its completion clears play/reverse and resets
the waiting countdown from signed halfword `+D6`.

Model `+5E` stays `0x3CA`; actor code selector `0x18` means File24. Native model
resources are filepair `0x24F/0` with common file `0x152`. Subtypes0/2 bind
animation/command slots0/1; subtype1 binds2/3. Both clips have three frames.
The animation step is12, the fixed-point truncation of0.05*256. Subtype0
Z scale is1.2; other scales are1.0. Subtype2 starts with yaw `0x100`.

The callback changes its own timing/animation and local sound. It does not
allocate children, award items, write save/temp flags, warp, or dispatch contact.
Subtype2 also sets/clears task mask `0xC0` using absolute local-player Z
distance less than30. These bits are deliberately absent from the packet.
The common `func_80218F30_5D4400` contact pipeline remains a single native pass.

## Portable checkpoint and lifecycle

The existing50-word platform row uses marker76 and variant27. Marker75 remains
the virtual broken-physics state. Fields19..23 carry subtype, native phase,
seed, reset delay and reverse. Field17 is the countdown;10..13 are clip,
frame, rate and status;29 carries visible/play bits. Remaining private fields
are zero. Matching C/Python validation rejects constructor phase0, unused
phase2, resource-pointer values, invalid stagger parameters and inconsistent
animation/reverse states.

Apply waits for this local task's scheduled constructor, resources, model and
immutable subtype/seed. It checks these conditions even for unchanged receipt
tokens. It neither invokes the constructor nor copies native pointers. Each
replica runs the verified callback once per normal local update after applying
a changed checkpoint. Repeated unchanged input cannot reset its predicted timer.
A paused owner holds timer and animation; subtype2 admission remains local.
Culling retains the checkpoint and restores it after the new task initializes.
An entering instance cannot compete with established copies until native apply
acknowledges the offered state.

`MNSG_WORLD` remains quiet, transient, team-routed latest state with no offline
queue. Native phases1/3/4 trigger the existing50ms edge floor; ordinary timer
and frame changes use200ms checkpoints. No packet type, row width, chunk bound
or server fan-out route was added. The current validation report records exact
package/source hashes and the measured local packet size.

## Verification limits

The production native harness checks initialized restore, unchanged receipts,
resource/model/parameter rejection, paused reverse state, local admission and
culling/reload. Its small callback stub verifies scheduling and field transfer;
it does not execute the real renderer/contact engine. Python tests compile the
production C codec and compare its results, exercise transport bootstrap,
handoff, stagger identities, malformed input, cadence and packet bounds.

The private three-client Anchor check exercises real TCP framing, relay,
sender exclusion, team isolation, replay rejection and handoff. Native receipts
are simulated in that test. A fresh two-client game run remains required for
visual timing, native resource lifetimes and contact behavior. Universal world
coverage remains unfinished; the complete worklist is in the controller audit.

Scratch evidence: `/tmp/mnsg-world-sync/file24-spike-native-audit-2026-09-19.txt`
and `file24-spike-placements.tsv`. The ROM and extracted resource bytes are not
repository inputs. Verified native API findings were added to the local sibling
reference and checked through generation, build, tests, browser and MIPS example
syntax; they have not been published.
