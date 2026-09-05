#define ANCHOR_ACTORS_HOST_TEST
#include "../src/anchor_actors.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_alloc;

void *recomp_alloc(unsigned long size)
{
    return fail_alloc ? 0 : malloc(size);
}

void recomp_free(void *memory)
{
    free(memory);
}

static char *roster(int players)
{
    char *json = malloc((unsigned long)players * 512 + 4);
    char *p = json;
    int i;
    *p++ = '[';
    for (i = 0; i < players; ++i)
        p += sprintf(p,
            "%s{\"cid\":%d,\"n\":\"Player%d\",\"room\":%d,\"hp\":1,"
            "\"ch\":%d,\"x\":%d,\"y\":12,\"z\":-17,\"vx\":30,\"s\":5,"
            "\"t\":2147483647,\"a\":11,\"af\":320,\"al\":1200,\"as\":100,"
            "\"ah\":1,\"rx\":-32768,\"ry\":32767,\"rz\":0,\"rvx\":16,"
            "\"ap\":7,\"cd\":1,\"dx\":-12000,\"dz\":2500,\"pe\":9,\"ps\":17}",
            i ? "," : "", i + 1, i + 1, i < 40 ? 99 : 1, i % 4, i * 10);
    *p++ = ']';
    *p = 0;
    return json;
}

static void large_roster_and_reuse(void)
{
    char *json = roster(96);
    RemoteSmoothing *smooth;
    RemotePlayer result;
    int i;
    int capacity;
    RemotePlayer *previous;
    assert(parse_lobby_positions(json) == 96);
    assert(s_remote_capacity >= 96 && s_model_capacity >= 96);
    /* Forty off-room members must not consume an admission limit. */
    assert(s_remote_players[95].cid == 96 && s_remote_players[95].room == 1);
    assert(s_remote_players[95].x == 950 && s_remote_players[95].vx == 30);
    assert(s_remote_players[95].timestamp_ms == 2147483647);
    assert(s_remote_players[95].appearance_flags == 7);
    assert(s_remote_players[95].collision_disabled == 1);
    assert(s_remote_players[95].drive_x == -12000);
    assert(s_remote_players[95].player_epoch == 9);
    assert(s_remote_players[95].interaction_session == 17);
    assert(s_remote_players[95].anim_frame_100 == 320);
    assert(s_remote_players[95].rot_x == -32768);
    begin_remote_smoothing_frame();
    for (i = 0; i < 96; ++i)
        smooth_remote_player(&s_remote_players[i], &result);
    end_remote_smoothing_frame();
    assert(s_smoothing_capacity >= 96);
    smooth = find_remote_smoothing(96, 0);
    assert(smooth && smooth->player_epoch == 9 && smooth->interaction_session == 17);
    s_remote_players[95].player_epoch = 10;
    s_remote_players[95].x = 9000;
    smooth_remote_player(&s_remote_players[95], &result);
    assert(result.x == 9000); /* New life cannot smooth from the old body. */
    capacity = s_remote_capacity;
    previous = s_remote_players;
    free(json);
    json = roster(3);
    assert(parse_lobby_positions(json) == 3);
    assert(s_remote_capacity == capacity && s_remote_players == previous);
    begin_remote_smoothing_frame();
    for (i = 0; i < 3; ++i)
        smooth_remote_player(&s_remote_players[i], &result);
    end_remote_smoothing_frame();
    assert(!find_remote_smoothing(96, 0));
    free(json);
    json = roster(200);
    fail_alloc = 1;
    assert(parse_lobby_positions(json) == -1);
    assert(s_remote_players == previous && s_remote_players[2].cid == 3);
    fail_alloc = 0;
    assert(parse_lobby_positions(json) == 200);
    assert(s_remote_players[199].cid == 200);
    free(json);
}

static void escaped_names_and_legacy_defaults(void)
{
    char json[] = "[{\"cid\":1,\"n\":\"A } \\\"cid\\\":999 {\\\\ B\"},"
                  "{\"cid\":2,\"n\":\"Q\\\"R\",\"x\":2147483648,\"room\":9,"
                  "\"ap\":7,\"cd\":1,\"pe\":20,\"ps\":21}]";
    char original[sizeof(json)];
    char missing[] = "[{\"cid\":3}]";
    char malformed[] = "[{\"cid\":4,\"n\":\"unterminated";
    strcpy(original, json);
    assert(parse_lobby_positions(json) == 2);
    assert(strcmp(json, original) == 0); /* Temporary terminators restored. */
    assert(s_remote_players[0].cid == 1);
    assert(strcmp(s_remote_players[0].name, "A } \"cid\":999 {\\ B") == 0);
    assert(s_remote_players[0].room == -1);
    assert(s_remote_players[0].appearance_flags == 0);
    assert(s_remote_players[0].player_epoch == 0);
    assert(s_remote_players[1].cid == 2 && s_remote_players[1].room == 9);
    assert(s_remote_players[1].x == 0); /* Decimal overflow cannot wrap. */
    assert(strcmp(s_remote_players[1].name, "Q\"R") == 0);
    assert(parse_lobby_positions(missing) == 1);
    assert(s_remote_players[0].collision_disabled == 0);
    assert(s_remote_players[0].appearance_flags == 0);
    assert(s_remote_players[0].drive_x == 0);
    assert(s_remote_players[0].player_epoch == 0);
    assert(parse_lobby_positions(malformed) == 0);
}

int main(void)
{
    large_roster_and_reuse();
    escaped_names_and_legacy_defaults();
    puts("dynamic remote roster, smoothing, bounded object and name tests passed");
    return 0;
}
