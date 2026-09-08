#define ANCHOR_WORLD_MAP_HOST_TEST

#include "../src/anchor_world_map.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *recomp_alloc(unsigned long size)
{
    return malloc(size);
}

void recomp_free(void *memory)
{
    free(memory);
}

static void parser_filters_and_preserves_order(void)
{
    char json[] =
        "[{\"cid\":9,\"mr\":300,\"mx\":-22550,\"mz\":12,\"mhp\":1,\"ch\":3},"
        "{\"cid\":12,\"mr\":550,\"mx\":0,\"mz\":0,\"mhp\":1,\"ch\":1},"
        "{\"cid\":15,\"mr\":301,\"mx\":2,\"mz\":3,\"mhp\":0,\"ch\":2},"
        "{\"cid\":16,\"mr\":301,\"mz\":3,\"mhp\":1,\"ch\":2},"
        "{\"cid\":17,\"mr\":301,\"mx\":2,\"mhp\":1,\"ch\":2},"
        "{\"cid\":18,\"mr\":302,\"mx\":4,\"mz\":5,\"mhp\":1,\"ch\":99},"
        "{\"cid\":0,\"mr\":303,\"mx\":6,\"mz\":7,\"mhp\":1,\"ch\":0}]";
    char original[sizeof(json)];

    strcpy(original, json);
    assert(parse_world_map_remotes(json) == 2);
    assert(strcmp(json, original) == 0);
    assert(s_map_roster[0].cid == 9);
    assert(s_map_roster[0].room == 300);
    assert(s_map_roster[0].world_x_100 == -22550);
    assert(s_map_roster[0].character == 3);
    assert(s_map_roster[1].cid == 18);
    assert(s_map_roster[1].room == 302);
    assert(s_map_roster[1].character == 0);
}

static void grouping_places_faces_side_by_side(void)
{
    int index;

    assert(mnsg_array_reserve((void **)&s_map_roster,
                              &s_map_roster_capacity, 6,
                              sizeof(*s_map_roster)));
    s_local_map_position.x = 10.0f;
    s_local_map_position.y = 0.3f;
    s_local_map_position.z = 5.0f;
    for (index = 0; index < 6; ++index)
    {
        s_map_roster[index].base.x = index < 3 ? 10.0f : -4.0f;
        s_map_roster[index].base.y = 0.3f;
        s_map_roster[index].base.z = index < 3 ? 5.0f : 8.0f;
    }

    /* The local face owns ordinal zero at (10,.3,5). */
    assert(world_map_group_ordinal(0) == 1);
    assert(world_map_group_offset(world_map_group_ordinal(0)) == 0.9f);
    assert(world_map_group_ordinal(1) == 2);
    assert(world_map_group_offset(world_map_group_ordinal(1)) == -0.9f);
    assert(world_map_group_ordinal(2) == 3);
    assert(world_map_group_offset(world_map_group_ordinal(2)) == 1.8f);

    /* A remote-only location keeps its first face on the canonical point. */
    assert(world_map_group_ordinal(3) == 0);
    assert(world_map_group_offset(world_map_group_ordinal(3)) == 0.0f);
    assert(world_map_group_ordinal(4) == 1);
    assert(world_map_group_offset(world_map_group_ordinal(4)) == 0.9f);
    assert(world_map_group_ordinal(5) == 2);
    assert(world_map_group_offset(world_map_group_ordinal(5)) == -0.9f);
}

static void refresh_slots_reuse_objects_and_retire_missing_clients(void)
{
    AnchorWorldMapRemote *nine;
    AnchorWorldMapRemote *two;
    AnchorWorldMapRemote *five;

    s_map_remote_count = 0;
    assert(mnsg_array_reserve((void **)&s_map_remotes,
                              &s_map_remote_capacity, 3,
                              sizeof(*s_map_remotes)));
    begin_world_map_remote_refresh();
    nine = claim_world_map_remote(9);
    two = claim_world_map_remote(2);
    assert(nine && two);
    nine->object = (void *)(unsigned long)0x9000;
    two->object = (void *)(unsigned long)0x2000;

    begin_world_map_remote_refresh();
    assert(!nine->active);
    assert(!two->active);
    assert(claim_world_map_remote(2) == two);
    five = claim_world_map_remote(5);
    assert(five);
    assert(two->active);
    assert(two->object == (void *)(unsigned long)0x2000);
    assert(five->active);
    assert(five == nine);
    assert(five->cid == 5);
    assert(five->object == (void *)(unsigned long)0x9000);
    assert(s_map_remote_count == 2);
}

static void only_local_face_uses_native_blink_phase(void)
{
    unsigned char local_object[WORLD_MAP_OBJECT_VISIBILITY_OFFSET + 1] = {0};
    unsigned char active_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET + 1] = {0};
    unsigned char inactive_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET + 1] = {0};
    AnchorWorldMapRemote remote = {0};
    int local_hidden;

    for (local_hidden = 0; local_hidden <= 1; ++local_hidden)
    {
        local_object[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] =
            (unsigned char)(0xD4u | (unsigned int)local_hidden);
        active_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] = 0xA5u;
        remote.object = active_remote;
        remote.active = 1;
        update_world_map_remote_visibility(&remote);

        assert(local_object[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] ==
               (unsigned char)(0xD4u | (unsigned int)local_hidden));
        assert(active_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] == 0xA4u);
    }

    inactive_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] = 0xA4u;
    remote.object = inactive_remote;
    remote.active = 0;
    update_world_map_remote_visibility(&remote);
    assert(inactive_remote[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] == 0xA5u);

    remote.object = 0;
    update_world_map_remote_visibility(&remote);
}

int main(void)
{
    parser_filters_and_preserves_order();
    grouping_places_faces_side_by_side();
    refresh_slots_reuse_objects_and_retire_missing_clients();
    only_local_face_uses_native_blink_phase();
    puts("world map roster, grouping, and remote visibility passed");
    return 0;
}
