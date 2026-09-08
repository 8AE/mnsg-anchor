# Shared Tsurami fight

Tsurami's Festival Temple arena (`0x0071`, decimal 113) uses one elected
simulator. Other participating clients adopt its health, attack phase,
timers, movement, animation, visual child, and active attack hazards. The
local introduction completes through its native camera sequence before
combat checkpoint adoption. All participating clients need the updated mod.

Native player contacts submit hit intents to the simulator. A hit addresses
either the boss or a particular reflectable projectile by its encounter ID.
The simulator applies the contact through the native damage routine; a
reflected projectile then follows the original return-shot and explosion
path. Remote player-projectile visuals cannot submit another player's hit.
Attack strength and native vulnerability/recovery checks are preserved.
A final boss hit reaches HP one, which invokes Tsurami's custom destruction
sequence instead of the common actor disposal branch.

The owner chooses among eligible local and remote players in the arena,
keeping the selected player through an attack. Pausing with Start or the
invitation dialog pauses the local application of checkpoints. A paused
owner pauses the shared combat timeline; networking continues. A checkpoint
cannot acknowledge a queued hit until the mapped native callback has had a
chance to process it.

Death ends that player's fight participation. Respawning starts a fresh
transport visit even if the native room resources were retained, so peers
can accept the returning player and supply the ongoing fight. A returning
player discards expired cached checkpoints before requesting current state;
a live participant's current checkpoint can still be adopted immediately.
This prevents stale boss state from taking authority after a respawn.

The blue hit overlay follows its native allocation and release. Freed effect
records retain an old global pointer and reset their opacity to 255, so they
must not be published as active flashes. Only the boss's live, linked effect
is captured or removed, and an unrelated effect cannot block fight updates.
This also keeps stale effect pointers from leaving projectile collision
callbacks paused, which prevented remote reflection hits from being captured.

## Miracle Star and story completion

Tsurami awards **Miracle Star**, the signed save word at `+0x250` (`mi_star`).
His file-73 controller creates and retains the reward actor, removes it
through the native sequence, and starts scenario `0x73`, which grants the
item. The subsequent story completion uses flag `0x74` (`cs_tsurami`).
This is a different lifecycle from Congo's collectible Miracle Moon.

Star ownership and story progress use the existing durable item/flag
transport and team snapshots. Remote story completion implies Star ownership.
While the local Star controller is active, remote story completion waits for
its normal control release and room transition. The controller's reward
child is never deleted by the network adapter. If a stored boss-completion
flag arrives after a local root has already been created, a scoped constructor
guard preserves that living encounter's required reward controller.

Shared defeat checkpoints retain the complete native destruction and reward
sequence. Replica-generated Star/story progress is suppressed from duplicate
publication. Completion is committed after the controller releases player
control and requests its native exit, including when the network connection
was reset during that scene.

## Transport and work limits

`MNSG_TSURAMI` version 1 uses the existing shared boss coordinator. It carries
an encounter identity, owner term, sequence, and connection/room-visit
metadata. Receivers check current team, room, roster, session and encounter.
Hot state uses a latest-value cache and is never queued as durable progress.

Changed state is limited to 10 Hz, unchanged state to a one-second keepalive.
Solo owners maintain their checkpoint without broadcasting a continuous
stream. Hit intents retry every 250 ms and retain their projectile target
through owner changes. Each checkpoint carries at most eight rotating
acknowledgment rows; native delivery processes at most 32 intents per frame.
The native layer bounds live synchronized hazards at 32.

The pointer-free state has 35 root words, four visual words, a tick and
projectile serial, and up to 32 twenty-word hazard records. Floats travel as
IEEE-754 bit patterns and receive native range checks. No packet contains a
native callback or task address. State is capped at 7,680 bytes, complete
NUL-framed packets at 8,192 bytes, and local bridge results at 12,288 bytes.
The larger local result accommodates a checkpoint plus 32 delivered hits;
it is not an additional network stream.

## Verification

Focused C tests cover checkpoint encoding, frame coordination, native actor
adoption, damage and reflected-projectile targets, terminal progression,
and Miracle Star controller ownership. Native projectile fixtures reproduce
the constructor's special `0x8000` orientation and verify owner capture and
follower recreation for all five travelling modes; the marker must survive
validation unchanged or projectiles prevent checkpoint publication.
All four ring variants also retain the native roll inherited from their shot.
The reflection integration test covers stale-flash recovery, local contact
across pending adoption, owner callback execution, and returning-projectile
adoption on a fresh follower. Rejoin tests cover death without a native room
reload and return to an ongoing three-player fight.
Python tests exercise framed transport, projectile appearance/removal,
late joins, authority handoff, duplicate/stale/session rejection, pause,
retry, target retention, rotating acknowledgments, and packet byte limits.
Existing Congo and Dharumanyo tests are retained.

Run `tests/run_tsurami_sync.sh` for the focused C and Python checks, or
`UBSAN=1 tests/run_tsurami_sync.sh` to enable undefined-behavior checks in
the C fixtures. `python3 -m unittest discover -s tests` runs all Python tests.

These are source, host-fixture and build checks. Fresh two-client game
validation is still required for native camera timing, every attack and
reflection, Hyper mod combinations, and the complete Star/story exit scene.
A controlled framed relay test is not a live game or live Anchor-server test.
