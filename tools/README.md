# Tools

## Anchor Stress Client

`anchor_stress.py` opens many real Anchor TCP client sessions so the multiplayer
mod can be stress tested without launching many game instances.

Run it with UV:

```sh
uv run tools/anchor_stress.py --clients 25 --host localhost --port 43383 --room stress-test
```

Useful startup options:

- `--clients N`: number of synthetic clients to spawn.
- `--host HOST` and `--port PORT`: Anchor server target.
- `--room ROOM`: required private Anchor room id. The hidden `mnsg-` prefix is added automatically.
- `--team TEAM`: team id for all synthetic clients.
- `--x`, `--y`, `--z`: initial world coordinates.
- `--room-id 0x1d1`: initial game room id. Decimal and hex both work.
- `--character Goemon`: initial selected character.
- `--rate 5`: position updates per second per client.
- `--follow TARGET`: immediately follow a server client by id or name substring.

Interactive commands:

```text
status
list
set all 100 0 200 0x1d1 Goemon
move 1-10 250 0 300
room all 0x15f
char all Yae
follow 123
stop-follow
spread 120
rate 10
quit
```

`follow <clientId|name>` makes the synthetic clients form a ring around the
selected server client. While following, they copy the target client's room id
and selected character, then continuously send `MNSG_PLAYER_POS` packets to
exercise the mod's remote marker and nameplate paths.
