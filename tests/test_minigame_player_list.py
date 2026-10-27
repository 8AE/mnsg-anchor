"""Compile the production player-list parser against its real JSON readers."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def production_parser():
    source = (ROOT / "src/ui/anchor_ui.c").read_text()
    start = source.index("static int plist_parse_transfer_target(")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class MinigamePlayerListParserTests(unittest.TestCase):
    def test_active_yae_marker_and_ordinary_waterfall_coordinates(self):
        harness = r'''
#include <assert.h>
#include "utils/json_utils.h"
#include "world/anchor_minigame_invites.h"
#define TRANSFER_MAX_ROOM_ID 0x225u
int anchor_minigame_invites_is_room(unsigned short room)
{
    return room == 0x1e0 || room == 0x1e1 || room == 0x1e2;
}
PARSER
static int parse(const char *json, unsigned short *room,
                 short *x, short *y, short *z, int *game)
{
    return plist_parse_transfer_target(json, 2, room, x, y, z, game);
}
int main(void)
{
    unsigned short room = 0;
    short x, y, z;
    int game;
    assert(parse("{\"cid\":2,\"room\":340,\"game\":4}", &room, &x, &y, &z, &game));
    assert(room == 0x154 && game == 4 && x == 0 && y == 0 && z == 0);
    assert(parse("{\"cid\":2,\"room\":340,\"x\":-32768,\"y\":32767,\"z\":-19}",
                 &room, &x, &y, &z, &game));
    assert(room == 0x154 && game == 0 && x == -32768 && y == 32767 && z == -19);
    assert(parse("{\"cid\":2,\"room\":482}", &room, &x, &y, &z, &game));
    assert(room == 0x1e2 && game == 0 && x == 0 && y == 0 && z == 0);
    const char *invalid[] = {
        "{\"cid\":2,\"room\":340}",
        "{\"cid\":3,\"room\":340,\"game\":4}",
        "{\"cid\":2,\"room\":339,\"game\":4}",
        "{\"cid\":2,\"room\":482,\"game\":4}",
        "{\"cid\":2,\"room\":340,\"game\":true,\"x\":1,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"game\":3,\"x\":1,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"game\":0,\"x\":1,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"game\":4.5,\"x\":1,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"game\":\"4\",\"x\":1,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"x\":32768,\"y\":2,\"z\":3}",
        "{\"cid\":2,\"room\":340,\"x\":1,\"y\":false,\"z\":3}",
    };
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        assert(!parse(invalid[i], &room, &x, &y, &z, &game));
    return 0;
}
'''.replace("PARSER", production_parser())
        with tempfile.TemporaryDirectory(prefix="mnsg-player-list-") as temporary:
            path = Path(temporary)
            (path / "parser.c").write_text(harness)
            subprocess.run([
                os.environ.get("HOST_CC", "cc"), "-std=c99", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=undefined", "-I", str(ROOT / "include"), str(path / "parser.c"),
                str(ROOT / "src/utils/json_utils.c"), str(ROOT / "src/utils/string_utils.c"),
                "-o", str(path / "parser"),
            ], check=True)
            subprocess.run([str(path / "parser")], check=True)


if __name__ == "__main__":
    unittest.main()
