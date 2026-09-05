# Boss arena invitations

This first draft supports Congo only. Entering Congo's arena (room `0x0016`,
distinct from its `0x001A` approach) sends other online teammates an invitation:

> [Player Name] has entered Congo's Arena. Would you like to join them?

The prompt uses the game's native patterned text window and Yes/No cursor.
Yes closes the dialog and asks the native room loader to move the recipient
to Congo's normal entrance. No closes the dialog. The saved respawn position
is preserved. An existing conversation, cutscene, pause, death or room load
defers a pending invitation. A sender leaving the arena, disconnecting,
unloading their save or changing teams invalidates their invitation, including
one already on screen. Players already in Congo's arena do not get invited.

All participating clients need this build. Older clients neither publish nor
consume arena events. Invitations are live events, so they are not replayed to
players who were offline when someone entered.

## Event and native lifecycle

`anchor_boss_invite_world.c` observes completed native stage loads and assigns a
visit counter. `anchor_boss_invites.c` coordinates presentation and acceptance.
The Python bridge sends `MNSG_BOSS_ARENA` only for entry/exit edges or a new
native visit, independently of movement and `ALL_CLIENT_STATE` refreshes:

```json
{"type":"MNSG_BOSS_ARENA","clientId":2,"targetTeamId":"blue","arena":1,"entered":true,"session":123,"seq":7}
```

Arena `1` identifies Congo. `session` identifies the sender's live connection;
`seq` identifies an ordered event within that connection. No coordinates, save
flags or text scripts are accepted from a network packet. The recipient uses
roster metadata for the display name, confirms current team/room/session, and
rechecks those conditions before accepting a join. A short metadata grace
period handles entry packets racing a roster update; a confirmed invitation
can wait through a long conversation. Duplicate events cannot reopen a
dismissed invitation. Queues grow with the roster, with one event per sender.

The save-sign recording and function trace identify a scenario script feeding
the native text window and choice handler. The custom dialog supplies its own
bounded text/choice script and completion callback without executing the
sign's save operation. Native font limitations require a bounded display name
and replacement of unsupported characters. The question and both choices
remain visible.

After Yes, the world adapter calls `func_8000607C_6C7C` with the native default
entrance table entry, then `func_80003728_4328(12)`. That step's native loader
consumes the prepared destination. It owns the current-room change and player
initialization; the mod does not overwrite the current room or save spawn.

## Validation

The automated tests cover serialized sender packets through the receiver,
team and session identity, delayed metadata, reconnects, deduplication,
same-room reloads, sender departures, queued dialogs, Yes/No/cancel behavior,
and native destination preparation before changing engine steps. Native
symbol signatures, dialog script controls and entrance values are checked
against the USA game code/data. Release and debug packages use the same flow.

In-game validation is left to the user. With two updated clients on the same
team in different rooms, enter Congo on one client and test No, then leave
and re-enter to test Yes. Also enter while the recipient is reading a sign:
the invitation should wait until that dialog closes. Check that an invitation
closes if its sender leaves, and that a client on another team receives none.
