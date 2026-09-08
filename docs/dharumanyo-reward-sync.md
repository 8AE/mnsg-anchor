# Dharumanyo reward synchronization

Dharumanyo awards the **Miracle Flower**, despite some references calling the
reward the Miracle Star. The native reward script is scenario `0x72`; it writes
`1` to `0x8015C860`, the signed 32-bit save word at `+0x258`. The multiplayer
key for this word is `mi_flower`. Scenario `0x73` instead writes the Miracle
Star word at `+0x250` and belongs to Tsurami's separate reward controller.

This note records the native boundaries used to keep Dharumanyo's reward and
post-fight progression synchronized. It is based on static source, symbol,
scenario-table, and US ROM disassembly review completed on 2026-09-07. No game
was launched.

## Native reward ownership

The arena's placed reward controller is entity `0x34F`, resource file `71`,
with constructor `func_08000000_72F890`. The constructor allocates its private
state and tests packed save flag `0x018`. An already completed encounter removes
the controller. Otherwise it installs the 29-state callback
`func_08000090_72F920`.

Dharumanyo's native death path `func_080065E8_6CE7F8` sets transient event
`0x16D`; it does not set durable flag `0x018`. Controller state 0 consumes that
event and starts the native reward, camera, scenario, and player-control chain.
The visible reward is a child created by `func_08000F68_7307F8` with resource
file `0x47`; the child uses live entity identity `0x350` and visual callback
`func_08000E90_730720`.

This is one scripted post-boss controller, not a proximity pickup with separate
selection, pickup-start, and completion callbacks. Its later states release
native player control through `func_80221FB0_5DD480`, clear the cutscene state,
and finish through the common actor removal path. A multiplayer hook that
removes the visible child early could bypass that ownership and cleanup.

## Script and flag ordering

The verified native sequence is:

| Controller boundary | Native result |
| --- | --- |
| State 4, call at ROM `0x72FC2C` | Starts scenario `0x72`. Its command at ROM `0x742284` targets `0x8015C860`, and the assignment at `0x74228C` writes `1` to `mi_flower` (`+0x258`). State 5 waits for the scenario PC to clear. |
| State 9, call at ROM `0x72FDF8` | Starts scenario `0xCE`; state 10 waits for it. |
| State 13, call at ROM `0x72FFE8` | Starts scenario `0xCF`. That script sets story flags `0x073`, `0x070`, and `0x071`; controller states 16 through 18 observe them before continuing. |
| State 20 | Starts scenario `0xD0`. The script sets story flag `0x072` at ROM `0x749434`, then durable Dharumanyo completion flag `0x018` at ROM `0x74956C`. It subsequently increments `0x8015C89C`, writes `1` to `0x8015C820`, and ends at ROM `0x7495B4`. |
| States 25 through 28 | Run the final `0xD7` scenario, release scripted control and camera state, retire the visual child, and remove the controller through native cleanup. |

The durable order is therefore:

`mi_flower` -> `cs_dhrm_1` -> `cs_dhrm_2` -> `cs_dhrm_3` ->
`cs_dhrm_4` -> `fl_dharmanyo`.

The packed IDs and multiplayer keys are:

| Save state | ID or offset | Multiplayer key |
| --- | --- | --- |
| Miracle Flower ownership | signed word `+0x258` | `mi_flower` |
| Dharumanyo completion | packed flag `0x018` (save byte `+0x03`, mask `0x01`) | `fl_dharmanyo` |
| Reward story steps | packed flags `0x073`, `0x070`, `0x071`, `0x072` | `cs_dhrm_1` through `cs_dhrm_4` |

## Deferred-completion boundary

`func_080066E0_6CE8F0` is only the end of combat teardown. It runs after
`func_080065E8_6CE7F8` sets transient event `0x16D`, which is the event that
wakes the placed reward controller. Releasing an incoming `fl_dharmanyo` at
`066E0` would therefore expose completion before the Flower and native reward
scenarios have run.

The multiplayer bridge marks combat complete at `066E0` but keeps an accepted
remote completion deferred. It observes the return of
`func_08000090_72F920` only for room `0x49`, placed entity `0x34F`. The bridge
commits and acknowledges `fl_dharmanyo` only after the original callback has
set native flag `0x018`. It does not change the controller actor or its state,
so states 25 through 28 retain ownership of the final scenario, child cleanup,
camera restoration, and player-control release.

The boss bridge retains its validated terminal checkpoint after combat removes
the root and carrier. That checkpoint remains advertised through the reward
sequence and is released at the same verified D0 boundary. A player arriving
during the ceremony therefore follows the terminal state instead of creating a
new encounter in the temporarily empty arena.

## Network consequence

All six durable values already use item sync's existing queued `SET_FLAG` path
and compact team snapshot. Snapshot serialization and application walk 32-bit
fields before packed flags, so `mi_flower` is reconciled before
`fl_dharmanyo`. Incremental monitoring also walks fields first and retains the
existing global limit of one queued delta every four item-sync updates. The
reward adds no continuous or reward-specific packet stream.

For a remote defeat, the same deferred `fl_dharmanyo` value is acknowledged at
the verified native D0 boundary; no second completion packet is created. The
cache is aligned with the native flag before ordinary monitoring resumes, so
the follower does not echo the owner's completion back to the team.

The Flower and four `cs_dhrm_*` values also rise inside every follower's native
reward scripts. While that remote defeat is in progress, their local monitor
accepts those five writes into its cache without sending queued `SET_FLAG`
duplicates. The elected simulator remains the durable publisher. The D0
completion hook aligns all five current values before it releases the remote
window, including the final story flag written by D0 itself.

The native world actors identified after this fight test only packed flag
`0x018`. The file-59 constructors `func_080004B8_71BF38`,
`func_08000524_71BFA4`, and `func_08001454_71CED4` select their completed-world
variants from that flag and do not read `mi_flower`. A late client therefore
cannot leave an exit or world actor waiting if it receives completion before
the inventory delta: the world uses `0x018`, and the existing durable field
reconciliation grants `mi_flower` when its delta or snapshot arrives.

A client present for the synchronized defeat already has the placed controller;
the transient native defeat path starts its complete post-fight sequence. A
client entering after completion has flag `0x018`, so the constructor skips the
finished cinematic while the ordinary item snapshot supplies the Flower. No
extra Flower hot packet, reward-child removal hook, pickup-state protocol, door
packet, or camera packet is required.
