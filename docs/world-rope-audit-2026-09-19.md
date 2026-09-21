# File24 jump-rope synchronization

Protocol 14 also adds entity `0x1AA`: all six placed ropes in room `0x41`,
indices 7 through 12. Their immutable definition word `+4` is 10, copied by
the native placement initializer to task word `+D0`. Every other private
definition word is zero.

`func_08000760_6ACCB0` initializes the native object and binds animation slot 0
at rate 2.0 with looping enabled, command slot 1, flags `0xE0` and unit scale.
It installs `func_0800074C_6ACC9C`, whose complete 20-byte body adds task word
`+D0` to object halfword `+14` and stores the low 16 bits. It has no calls,
private phases, timers, child births, rewards, flag writes or audio. The common
post callback owns the separate contact/damage pass.

The continuation is File24 VRAM `0x0800074C`, ROM `0x006ACC9C`, size `0x14`.
Initialization is VRAM `0x08000760`, ROM `0x006ACCB0`, size `0x9C`. Native model
resources are primary file `0x1DF`, common file `0x152`, animation slot 0 at
segmented `0x08000064` and command slot 1 at `0x080000F0`. The animation has
10 frames, but the native play flag stays clear: the common animation helper
returns immediately without it. Consequently the rope keeps frame 0, rate 512
and animation status low bits 1 while rotating the entire object. The codec
requires that exact state. Render trig and attachment rotation consume the
angle modulo 1024; the native accumulator itself still wraps at 65536.
Neighboring routines `08000808` and `080008BC` belong to the
separate spear family and are not included in this adapter.

The existing platform row uses variant 28 and marker 77. Word 19 preserves the
full native rotation accumulator as a signed 16-bit scalar; generic pose word 7
contains its matching low 10 bits. Word 20 must equal the immutable speed 10.
This retains native wrap at 65536 without treating rendering-angle normalization
as the native actor state. Other unused private fields remain zero.

Apply requires the local constructor to have completed, the exact continuation,
model/resources, and native immutable speed. It checks these before acknowledging
an unchanged offer. Replicas run the verified rotation callback once per local
update, and the native common contact pipeline remains separate. Paused state,
late entry and culling/reload use the same retained-checkpoint and applied-receipt
barriers as the spike floors. Rotation changes use the existing 200 ms checkpoint
cadence; presence and pause transitions use the existing bounded edge cadence.

The production host harness checks full-angle restoration and wrap, unchanged
receipt prediction, pause, resource/model/speed rejection and local reload.
Python tests compile the production C codec for parity and check lower-ID late
entry, ownership handoff and rotation cadence. The shared loopback harness runs
this family with `--family rope`; its native receipts are simulated. These
checks do not replace a fresh two-client game run for visual/collision fidelity.

Native source and resource evidence use US ROM SHA-256
`e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c`.
The bounded scratch audit is
`/tmp/mnsg-world-sync/file24-rope-native-audit-2026-09-19.txt`.
No ROM or extracted resources are repository inputs.
