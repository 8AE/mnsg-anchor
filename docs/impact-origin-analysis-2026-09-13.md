# Impact attack origin and draw timing — September 13, 2026

This follows the report that remote attacks still use the primary player's
cursor and remote limbs remain missing. The earlier grappling follow-up is a
historical checkpoint; its hook-specific aim workaround has been replaced.

## Confirmed native paths

`game_thread_entrypoint` calls `80002040`, which dispatches the Impact handler
`801CB518`. That handler calls scheduler `80034734`. After the dispatcher and
its return hooks finish, the game clears draw buckets (`8000AAE8`), collects
objects by mode (`8000AA00`), initializes rendering (`80016950`), then draws the
buckets through `800087C4` and model renderer `80016C44`.

The previous mod rendered replicas at scheduler return, then hid every replica
inside `visuals_tick` at dispatcher return. Thus the real draw saw hidden models.
Replica preparation now hooks entry to `func_8000AA00_B600`, after networking
and before object collection. New objects and changed render modes participate
in that frame's collection. Native matching models remain hidden until the next
simulation/capture boundary; replicas retain no AI or collision callbacks.

The input interpreter is also too short a scope for attack aim. Native punch/arm
updater `801D9030` rereads state yaw/pitch at `+4/+8` on every animation update.
The MIPS instructions at `801D93D4..801D9448` establish these reads; cached
pseudocode omitted the relevant arguments. Guard placement `801DD3DC`, delayed
laser initialization `801DC194`, hook launch `801D9DA8` and Ryo constructor
`801DAFCC` also consume aim outside, or through different cursor pointers from,
the initial input reader.

## Implementation

Both action dispatchers bind the initiating participant to new native tasks.
`80034B58` inserts its result immediately at `parent.next`; its return hook can
bind task-only special attacks before their first update. Descendants inherit
the source from their parent. `80034A10` clears metadata on task reset, preventing
address reuse from inheriting another attack's source.

The scheduler calls `8001481C(task)` before each runnable task's pre/update/post
callbacks. That boundary scopes the attack initiator's yaw, pitch and both native
cursor rotations for the task. The next task and scheduler return restore local
aim. Camera and boss tasks therefore read their own unmodified local state.
Fresh control samples update an ongoing remote attack; stale samples fall back
to its accepted aim. Guided-fist axes use the task's source too, replacing the
single global last-fist identity. The earlier arm-first-update/hook-launch
workaround is removed in favor of this common task lifecycle.

Pending native actions retain their source until dispatch. Competing remote
presses remain queued, and physical presses during a waiting remote action are
queued with local identity. Held-only remote guards select the holder's aim.
Pauses clear input queues while preserving surviving attack metadata; authority,
manager, stage, encounter and visit changes fence the metadata. Storage is bounded
at 128 attack tasks. No packet schema, routing, cadence or protocol change was
needed; packages remain protocol 6.

## Validation and limits

- Six native fixture groups pass with UBSan. The action-source fixtures cover all
  17 dispatcher IDs, inherited delayed children, independent local aim, physical
  and remote pending-input competition, held guards, pause/resume, address reuse
  and follower authority. These exercise the mod hooks against native layouts;
  they do not execute all 17 original game actions.
- The visual fixture uses the production hook registration and native frame
  ordering to check first allocation, visibility through draw and render-mode
  changes before collection. Deliberately retiming the hook to simulation fails
  the visibility assertion; retiming it after collection fails the bucket check.
- Focused Python: 41 passed. Full Python: 271 run, 270 passed, one existing skip.
- Release/debug builds and archive integrity/source checks passed. Compiled ELF
  sections contain all 11 required dispatcher, allocation, task and draw hooks.
  The sole compiler warning remains the unrelated unused `notif_ensure_init`.
- Three existing native-reference records were updated locally, regenerated,
  built, tested (34 tests) and checked in the browser. Nothing was published.

A fresh in-game two-client fight was not run. The fixtures do not execute the
complete game or GPU renderer. Prior localhost transport checks remain historical
and were not rerun for these native-only changes. Package hashes and exact checks
are in [impact-origin-validation-2026-09-13.json](impact-origin-validation-2026-09-13.json).
