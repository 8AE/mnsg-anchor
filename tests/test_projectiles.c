#include "anchor_projectiles.h"
#include "anchor_projectile_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AnchorProjectileSpawn sample(void)
{
    AnchorProjectileSpawn spawn = {
        .id = 1, .kind = 0xe, .x100 = -12345, .y100 = 1234, .z100 = 9876,
        .vx100 = -300, .vy100 = 500, .vz100 = 400,
        .rx = -32768, .ry = 512, .rz = 32767, .scale100000 = 10000,
    };
    return spawn;
}

static void test_codec(void)
{
    AnchorProjectileSpawn spawn = sample();
    AnchorProjectileRemote received[2];
    char json[ANCHOR_PROJECTILE_JSON_SIZE];
    char rows[1200];
    assert(anchor_projectile_spawn_encode(&spawn, json, sizeof(json)));
    assert(strlen(json) < 240);
    assert(!strstr(json, "recipe") && !strstr(json, "material") && !strstr(json, "frame"));
    snprintf(rows, sizeof(rows),
        "[{\"cid\":2,\"session\":3,\"epoch\":4,\"age\":7,%s]", json + 1);
    assert(anchor_projectile_spawns_decode(rows, received, 2) == 1);
    assert(received[0].cid == 2 && received[0].session == 3 && received[0].epoch == 4);
    assert(memcmp(&received[0].spawn, &spawn, sizeof(spawn)) == 0);
    snprintf(rows, sizeof(rows),
        "[{\"id\":9},{\"cid\":2,\"session\":3,\"epoch\":4,\"age\":7,%s]", json + 1);
    assert(anchor_projectile_spawns_decode(rows, received, 2) == 1);
    snprintf(rows, sizeof(rows),
        "[{\"cid\":2,\"session\":3,\"epoch\":4,\"age\":751,%s]", json + 1);
    assert(anchor_projectile_spawns_decode(rows, received, 2) == 0);
    assert(anchor_projectile_spawns_decode("[{", received, 2) == 0);
    assert(anchor_projectile_spawns_decode("[]", received, 2) == 0);
    assert(!anchor_projectile_spawn_encode(&spawn, json, 10));
    assert(!json[0]);
    spawn.vy100 = 1000001;
    assert(!anchor_projectile_spawn_encode(&spawn, json, sizeof(json)));
    spawn = sample();
    spawn.scale100000 = 0;
    assert(!anchor_projectile_spawn_valid(&spawn));
    spawn = sample();
    spawn.rx = 32768;
    assert(!anchor_projectile_spawn_valid(&spawn));
    spawn = sample();
    spawn.x100 = -1000000000;
    spawn.vy100 = -1000000;
    spawn.scale100000 = 1000000;
    assert(anchor_projectile_spawn_encode(&spawn, json, sizeof(json)));
}

static void test_throw_queue(void)
{
    AnchorProjectileSourceState state = {0};
    AnchorProjectileSpawn spawn = sample();
    const AnchorProjectileSpawn *pending;
    int tasks[65];
    int id;
    unsigned int i;
    assert(anchor_projectile_source_capture(&state, &tasks[0], &spawn, 10, 1));
    pending = anchor_projectile_source_peek(&state, 11);
    assert(pending && pending->kind == spawn.kind);
    id = pending->id;
    /* Many native updates and a failed send do not create additional events. */
    assert(!anchor_projectile_source_capture(&state, &tasks[0], &spawn, 12, 1));
    assert(anchor_projectile_source_peek(&state, 12)->id == id);
    anchor_projectile_source_forget_task(&state, &tasks[0]);
    assert(anchor_projectile_source_peek(&state, 13)->id == id);
    assert(anchor_projectile_source_capture(&state, &tasks[0], &spawn, 13, 1));
    anchor_projectile_source_ack(&state);
    assert(anchor_projectile_source_peek(&state, 13)->id != id);
    anchor_projectile_source_ack(&state);
    assert(!anchor_projectile_source_peek(&state, 13));
    /* A shot observed offline cannot be emitted halfway through its flight. */
    assert(!anchor_projectile_source_capture(&state, &tasks[1], &spawn, 14, 0));
    assert(!anchor_projectile_source_capture(&state, &tasks[1], &spawn, 15, 1));
    /* Connection/life gates can clear pending events while preserving which
     * native lifetimes have already been observed. */
    assert(anchor_projectile_source_capture(&state, &tasks[2], &spawn, 16, 1));
    anchor_projectile_source_clear_pending(&state);
    assert(!anchor_projectile_source_peek(&state, 17));
    assert(!anchor_projectile_source_capture(&state, &tasks[2], &spawn, 17, 1));
    anchor_projectile_source_reset(&state);
    assert(anchor_projectile_source_capture(&state, &tasks[2], &spawn, 20, 1));
    assert(anchor_projectile_source_peek(&state, 43));
    assert(!anchor_projectile_source_peek(&state, 44));
    anchor_projectile_source_reset(&state);
    for (i = 0; i < ANCHOR_PROJECTILE_SOURCE_MAX; ++i)
        assert(anchor_projectile_source_capture(&state, &tasks[i], &spawn, 100, 1));
    assert(!anchor_projectile_source_capture(&state, &tasks[64], &spawn, 100, 1));
    for (i = 0; i < ANCHOR_PROJECTILE_SOURCE_MAX; ++i)
    {
        assert(anchor_projectile_source_peek(&state, 101));
        anchor_projectile_source_ack(&state);
        anchor_projectile_source_forget_task(&state, &tasks[i]);
    }
    assert(!anchor_projectile_source_peek(&state, 101));
    /* Unsigned tick wrap still expires bounded retries. */
    assert(anchor_projectile_source_capture(&state, &tasks[0], &spawn, 0xfffffffau, 1));
    assert(anchor_projectile_source_peek(&state, 2));
    assert(!anchor_projectile_source_peek(&state, 18));
}

int main(void)
{
    test_codec();
    test_throw_queue();
    puts("Projectile spawn codec and queue tests passed");
    return 0;
}
