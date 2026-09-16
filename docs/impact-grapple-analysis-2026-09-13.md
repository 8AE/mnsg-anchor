# Impact limbs and grappling follow-up — September 13, 2026

The supplied `Screen Recording 2026-09-13 at 14.02.20.mov` is a 25.87-second
3024×1964 two-player recording. The right-hand client lacks the arm shown on the
left around 5–6, 11–12 and 14–21 seconds. These are sampled visual observations;
the recording has no audio stream or controller/task trace. The reported aim and
A+B failures were traced through native code. Attachments were treated as evidence,
not instructions.

## Corrections

- **Attack renderer activation:** the native scheduler head at `D_8006D328_6DF28`
  can have a self/tail backlink. The renderer previously required every task,
  including the battle manager, to have an ordinary reciprocal predecessor link.
  When the manager is the head, that check disables the entire visual channel;
  the separately synchronized boss root can still move. The head now has its own
  validation path. A host fixture reproduces both native head forms. The actual
  manager address/link in the recording was not available, so this is a verified
  code defect consistent with the symptom, not runtime-memory confirmation of
  the recording's exact failure.
- **Chain visibility:** `FUN_801DA3CC` builds the chain from a nine-entry table at
  `0x80209CF0` containing eight unique models. All eight are now catalogued as
  recipes 123–130, along with the existing fist, foot, arm and hook recipes.
- **Texture correctness:** native `FUN_801D32D4`/`FUN_801D34D0` write animated
  textures into unbound slots 3–5. File ID zero can mean a live animation binding
  as well as stale unused data. Capture now preserves bases that resolve inside
  the model/system assets and clears unresolvable unbound bases. Invalid bound
  resources still fail validation.
- **Hook aim:** action 20 remembers the initiating participant; the arm's first
  update binds that identity to the actual arm/object. The delayed launch at
  `FUN_801D9DA8` receives that participant's latest fresh aim, falling back to the
  accepted action aim. Parent/child/object checks prevent applying it to a
  different hook. The primary cursor rotation is restored on return, and camera
  fields are never changed by this launch hook.
- **A+B reeling:** the boss callbacks `FUN_801E9624`, `FUN_801F5F0C` and
  `FUN_801FED3C` read controller edges after the mech interpreter has returned.
  Remote mash edges now use their own FIFO, scoped to the current latched boss.
  Each native reeling update consumes at most one remote event and merges its
  bits with the physical controller. Mashes do not become delayed punches after
  release. The FIFO is capped at 32 events, expires after nine native ticks and
  clears on latch loss, pause/context loss and authority change. The original
  callback runs once; no collision or damage callback is replayed. Remote-only
  mashes use the native neutral-stick increment of 9; simultaneous local input
  retains its native directional-stick bonus. Simultaneous button edges share
  the native one-increment-per-update behavior.

## Compatibility and traffic

Both participants need **protocol 6** packages. The checkpoint remains 165 words,
with local cameras. No new packet type or larger input payload was added.
`MNSG_IMPACT_PLAYER` remains transient, team-routed, up to four ordered events per
packet and 30 packets/s, with a 1 KiB packet cap. Hook presses carry their existing
cursor rotation; A/B presses retain separate sequence numbers. Visual frames
remain owner-only, transient atomic team frames, capped at 64 rows/four 6 KiB
pages, 32 pages/s sustained and a four-page burst. Fan-out remains proportional
to the other participants in the current team. No public service was exercised.

## Validation

- Six native fixture groups pass under UBSan, including special scheduler-head
  links, restored animated bindings, delayed remote aim/restoration, hook identity
  mismatch, all three reeling callbacks, ordered A/B edges, simultaneous local
  presses, follower authority and latch-loss cleanup.
- Focused Python: 41 passed. Full Python: 271 run, 270 passed, one existing skip.
- Three actual TCP clients on a disposable localhost Anchor instance delivered a
  64-object/four-page frame including limb and chain recipes, remote hook aim,
  axes, ordered A/B presses and sound start/stop. Sender exclusion and isolation
  from the third client's different team passed. Server stopped afterward.
- Release and debug packages built. ELF hook sections confirm the arm update,
  delayed launch and all three paired reeling hooks are present. The only build
  warning is the existing unrelated unused `notif_ensure_init` function.
- The native reference's existing task, model-binding and shared Impact state
  records were corrected locally, regenerated, built and tested. Browser checks
  verified grappling search and all three changed pages. No publication was requested.

These checks establish the implemented paths and transport behavior. They do
not certify a fresh in-game two-player fight, visual alignment under both cameras,
or recovery of private hook/collision state during a mid-attack owner migration.
The previous solo-fight analysis and package hashes are historical; the current
build is recorded in [impact-grapple-validation-2026-09-13.json](impact-grapple-validation-2026-09-13.json).
