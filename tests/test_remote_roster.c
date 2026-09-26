#define ANCHOR_ACTORS_HOST_TEST
#include "../src/player/anchor_actors.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_alloc;
static int cube_owned;
static int cube_native_available;
static int cube_cid;
static int cube_session;
static int cube_epoch;
static float cube_scale;
static unsigned char cube_task[16];
static unsigned char cube_object[0x30];

int anchor_player_cube_visual_owned(int cid, int session, int epoch)
{
    return cube_owned && cid == cube_cid && session == cube_session &&
           epoch == cube_epoch;
}

int anchor_player_freeze_visual_get_native(int cid, int session, int epoch,
                                          void **task, void **object,
                                          float *scale)
{
    if (!cube_native_available || cid != cube_cid ||
        session != cube_session || epoch != cube_epoch)
        return 0;
    *task = cube_task;
    *object = cube_object;
    *scale = cube_scale;
    return 1;
}

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

static RemotePlayer frozen_remote(int cid)
{
    RemotePlayer remote = {0};
    remote.cid = cid;
    remote.room = 1;
    remote.has_pos = 1;
    remote.player_epoch = 9;
    remote.interaction_session = 17;
    remote.appearance_flags = ANCHOR_APPEARANCE_FROZEN;
    remote.seq = 1;
    remote.action = 11;
    return remote;
}

static void frozen_endpoint_follows_between_packets(void)
{
    RemotePlayer remote = frozen_remote(501);
    RemotePlayer displayed;
    int i;

    clear_remote_smoothing();
    cube_owned = 0;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 0.0f);
    remote.x = 60.0f;
    remote.seq = 2;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x > 20.0f && displayed.x < 22.0f);
    for (i = 0; i < 5; ++i)
        smooth_remote_player(&remote, &displayed);
    assert(displayed.x > 54.0f && displayed.x < 60.0f);
    /* A large sequence gap alone cannot snap a frozen display. */
    remote.seq = 1000;
    remote.x = 90.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x > 60.0f && displayed.x < 90.0f);
    remote.x = 700.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 700.0f); /* Implausible relocation snaps. */
}

static void locally_owned_cube_tracks_every_frame(void)
{
    RemotePlayer remote = frozen_remote(502);
    RemotePlayer displayed;
    float *center = (float *)(cube_object + 8);

    clear_remote_smoothing();
    cube_cid = remote.cid;
    cube_session = remote.interaction_session;
    cube_epoch = remote.player_epoch;
    cube_owned = cube_native_available = 1;
    cube_scale = 0.5f;
    center[0] = 100.0f;
    center[1] = 200.0f;
    center[2] = 300.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 100.0f && displayed.y == 150.0f &&
           displayed.z == 300.0f);
    center[0] = 115.0f;
    center[1] = 215.0f;
    center[2] = 285.0f;
    /* No packet changed, yet the displayed victim moves with the cube. */
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 115.0f && displayed.y == 165.0f &&
           displayed.z == 285.0f);
    cube_scale = 0.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == remote.x && displayed.y == remote.y);
    cube_scale = 0.5f;
    remote.player_epoch++;
    remote.x = 42.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 42.0f); /* Old-life cube cannot drive a new body. */
    cube_owned = cube_native_available = 0;
}

static void frozen_lifecycle_resets_pose(void)
{
    RemotePlayer remote = frozen_remote(503);
    RemotePlayer displayed;

    clear_remote_smoothing();
    cube_owned = 0;
    smooth_remote_player(&remote, &displayed);
    remote.x = 60.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x < 60.0f);
    remote.interaction_session++;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 60.0f);
    remote.x = 100.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x < 100.0f);
    remote.player_epoch++;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 100.0f);
    remote.x = 130.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x < 130.0f);
    remote.room++;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 130.0f);
    remote.appearance_flags = 0;
    remote.x = 180.0f;
    remote.seq++;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 180.0f); /* Thaw starts at the sender endpoint. */
    remote.appearance_flags = ANCHOR_APPEARANCE_FROZEN;
    remote.x = 220.0f;
    smooth_remote_player(&remote, &displayed);
    assert(displayed.x == 220.0f);
    drop_remote_smoothing(remote.cid);
    assert(!find_remote_smoothing(remote.cid, 0));
}

int main(void)
{
    large_roster_and_reuse();
    escaped_names_and_legacy_defaults();
    frozen_endpoint_follows_between_packets();
    locally_owned_cube_tracks_every_frame();
    frozen_lifecycle_resets_pose();
    puts("dynamic remote roster, smoothing, bounded object and name tests passed");
    return 0;
}
