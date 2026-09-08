# Boss arena invitations

Entering a supported boss arena sends other online teammates an invitation:

> [Player Name] entered Congo's Arena. Would you like to join them?

The name and destination change for each boss. Control Machine is in Koryuta's
dragon-flight room. Congo's approach (`0x001A`) is separate from its arena.

| Arena ID | Boss | Native room | Default entrance (X, Y, Z) |
| --- | --- | --- | --- |
| 1 | Congo | `0x0016` (22) | `(60, -70, 171)` |
| 2 | Dharumanyo | `0x0049` (73) | `(145, -70, -99)` |
| 3 | Tsurami | `0x0071` (113) | `(0, -71, 318)` |
| 4 | Control Machine | `0x0155` (341) | `(27, 239, 131)` |

The prompt uses the game's native patterned text window and Yes/No cursor.
While it is open, the recipient's local world is paused: the player, enemies,
remote players and projectiles stop moving. Networking and the dialog keep
running. Selecting Yes or No resumes the world after the window closes; B
also declines. Cancellation, including a sender leaving, releases the pause.
Yes closes the dialog and asks the native room loader to move the recipient
to the selected arena's normal entrance. No closes the dialog. The saved respawn position
is preserved. An existing conversation, cutscene, pause, death or room load
defers a pending invitation. A sender leaving the arena, disconnecting, or
unloading their save invalidates their invitation, including one already on
screen. Players already in the destination arena do not get
invited to it; players in a different boss arena can still receive invitations.

Update all participating clients to receive the three added bosses. Congo's
wire ID remains `1`; Congo-only builds ignore the other arena IDs. Invitations
are live events, so they are not replayed to
players who were offline when someone entered.

## Event and native lifecycle

`anchor_boss_invite_world.c` observes completed native stage loads and assigns a
visit counter. `anchor_boss_invites.c` coordinates presentation and acceptance.
The Python bridge sends `MNSG_BOSS_ARENA` only for entry/exit edges or a new
native visit, independently of movement and `ALL_CLIENT_STATE` refreshes:

```json
{"type":"MNSG_BOSS_ARENA","clientId":2,"targetTeamId":"default","arena":1,"entered":true,"session":123,"seq":7}
```

Arena IDs use the fixed mapping above. `session` identifies the sender's live connection;
`seq` identifies an ordered event within that connection. No coordinates, save
flags or text scripts are accepted from a network packet. The recipient uses
roster metadata for the display name, confirms current team/room/session, and
rechecks those conditions against the selected arena before accepting a join.
Entering another arena replaces the sender's old invitation, and an exit
names the arena that sender last successfully announced. A short metadata grace
period handles entry packets racing a roster update; a confirmed invitation
can wait through a long conversation. Duplicate events cannot reopen a
dismissed invitation. Queues grow with the roster, with one event per sender.

The save-sign recording and function trace identify a scenario script feeding
the native text window and choice handler. The custom dialog supplies its own
bounded text/choice script and completion callback without executing the
sign's save operation. Native font limitations require a bounded display name
and replacement of unsupported characters. The question and both choices
remain visible. All four full arena names fit the native window: the longest
arena line is 220 pixels wide, and the largest supported prompt uses 99 of
the native window's 120 glyph records.

The pause uses the Start menu's native task-scheduler mask (`system+0x3AE24`,
bit `1`) only around `func_80034734_35334`. It does not activate the Start menu
state at `system+0x3AE26`. The mask skips flagged world tasks and their children,
including the scenario manager. The adapter advances only its owned private
scenario with `func_8003CFD0_3DBD0`, in the manager's task context, once per frame.
A hook records any normal scenario update so it cannot be advanced twice.
Native window/cursor rendering remains outside the paused scheduler. Other
native mask bits and pre-existing pauses are preserved.

Mod callbacks outside the scheduler defer remote poses, projectile simulation
and incoming gameplay mutations until the dialog releases the world. They
still handle disconnects, save unloads and stale room/owner cleanup. Pending
throws survive the modal's hit-authority epoch changes. Network expiration
and native wall clocks retain their normal behavior; other clients continue
playing independently.

After Yes, the world adapter calls `func_8000607C_6C7C` with the native default
entrance table entry, then `func_80003728_4328(12)`. That step's native loader
consumes the prepared destination. It owns the current-room change and player
initialization; the mod does not overwrite the current room or save spawn.

## Validation

The automated tests cover serialized sender packets through the receiver,
fixed routing and session identity, delayed metadata, reconnects, deduplication,
same-room reloads, sender departures, queued dialogs, Yes/No/cancel behavior,
and native destination preparation before changing engine steps. The pause
checks cover frozen task subtrees, responsive choices, exactly one scenario
update per frame, pause-mask ownership, cancellation, and projectile queue
and lifecycle behavior across pause/resume. Native
symbol signatures, dialog script controls and entrance values are checked
against the USA game code/data. Release and debug packages use the same flow.

In-game validation is left to the user. With two updated clients using the same
Room ID but in different game rooms, enter each supported arena on one client and test No,
then leave and re-enter to test Yes. For Control Machine, use the dragon-flight
encounter. Also enter while the recipient is reading a sign:
the invitation should wait until that dialog closes. Check that an invitation
closes if its sender leaves, and that a client using another Room ID receives none.
While the invitation is open, check that nearby enemies and existing local
and remote projectiles stop, the choice cursor still works, and No/B resumes
the same scene. Yes should release the pause and load the named arena normally.
