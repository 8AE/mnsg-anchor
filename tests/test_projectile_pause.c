/* Exercise the real frame coordinator and source queue without a game. The
 * fixture stores host pointers at N64 record offsets, so compile this harness
 * with alignment sanitization disabled (the target uses 32-bit pointers). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define recomp_free free
#include "../src/anchor_projectiles.c"

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
static unsigned char owner_storage[128], model_storage[128];
static void *owner_link;
static int paused, loaded, session, epoch;
static int resets, ticks, spawn_calls, reads, acks, sends, pending_remote;
static int source_task;

int anchor_dialog_world_paused(void) { return paused; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_get_projectile_session(void) { return session; }
int anchor_player_models_get_epoch(void) { return epoch; }
int anchor_remote_model_pool_contains(const void *p) { return p != 0; }
int anchor_projectile_capture_first_update(const void *task)
{ (void)task; return 0; }
int anchor_projectile_capture_fields(const void *task, const void *object,
                                     AnchorProjectileSpawn *out)
{ (void)task; (void)object; (void)out; return 0; }
void anchor_projectile_models_reset(void) { ++resets; }
void anchor_projectile_models_tick(void *owner)
{ assert(owner == D_801FC604_5B8514); ++ticks; }
int anchor_projectile_models_spawn(const AnchorProjectileRemote *remote,
                                   void *owner)
{
    assert(owner == D_801FC604_5B8514);
    assert(remote->cid == 9 && remote->spawn.id == 7);
    ++spawn_calls;
    return 1;
}
char *anchor_get_projectile_spawns_json(void)
{
    const char *row = pending_remote ?
        "[{\"cid\":9,\"session\":3,\"epoch\":4,\"age\":0,"
        "\"id\":7,\"kind\":14,\"x100\":0,\"y100\":0,\"z100\":0,"
        "\"vx100\":100,\"vy100\":0,\"vz100\":0,\"rx\":0,\"ry\":0,\"rz\":0,"
        "\"scale100000\":100000}]" : "[]";
    char *copy = malloc(strlen(row) + 1);
    assert(copy);
    strcpy(copy, row);
    ++reads;
    return copy;
}
int anchor_ack_projectile_spawn(int cid, int remote_session,
                                 int remote_epoch, int id)
{
    assert(cid == 9 && remote_session == 3 && remote_epoch == 4 && id == 7);
    ++acks;
    pending_remote = 0;
    return 1;
}
int anchor_send_projectile_spawn_json(int sent_session, int sent_epoch,
                                      const char *json)
{
    assert(sent_session == session && sent_epoch == epoch && json && json[0]);
    ++sends;
    return 1;
}

static void reset_fixture(void)
{
    void *backlink = &owner_link;
    memset(owner_storage, 0, sizeof(owner_storage));
    memset(model_storage, 0, sizeof(model_storage));
    memset(&s_source, 0, sizeof(s_source));
    D_801FC604_5B8514 = owner_storage;
    D_801FC60C_5B851C = model_storage;
    owner_link = owner_storage;
    memcpy(owner_storage + 4, &backlink, sizeof(backlink));
    memcpy(owner_storage + 0x18, &D_801FC60C_5B851C, sizeof(void *));
    s_owner = 0;
    s_session = s_epoch = s_world_was_paused = 0;
    s_tick = 0;
    paused = pending_remote = 0;
    loaded = session = epoch = 1;
    D_800C7AB2 = 22;
    resets = ticks = spawn_calls = reads = acks = sends = 0;
    anchor_projectiles_frame();
    assert(resets == 1 && ticks == 1 && reads == 1);
    resets = ticks = reads = 0;
}

static void frozen_projectiles_resume_without_recreation(void)
{
    unsigned int tick;
    int i;
    reset_fixture();
    tick = s_tick;
    pending_remote = 1;
    paused = 1;
    ++epoch; /* The owned modal invalidates local hit authority. */
    for (i = 0; i < 90; ++i)
        anchor_projectiles_frame();
    assert(s_tick == tick);
    assert(!resets && !ticks && !reads && !spawn_calls && !acks);
    assert(pending_remote);
    paused = 0;
    ++epoch;
    anchor_projectiles_frame();
    assert(s_tick == tick + 1 && !resets && ticks == 1);
    assert(reads == 1 && spawn_calls == 1 && acks == 1 && !pending_remote);
    anchor_projectiles_frame();
    assert(spawn_calls == 1 && acks == 1); /* No replay after resuming. */
}

static void local_pending_retries_do_not_expire_during_pause(void)
{
    AnchorProjectileSpawn local = {.kind = 14, .scale100000 = 100000};
    int i;
    reset_fixture();
    /* The final native frame can capture a throw just before the prompt. */
    assert(anchor_projectile_source_capture(&s_source, &source_task,
                                            &local, s_tick, 1));
    paused = 1;
    ++epoch;
    anchor_projectiles_frame();
    for (i = 0; i < 90; ++i)
        anchor_projectiles_frame();
    assert(s_source.count == 1 && !sends);
    paused = 0;
    ++epoch;
    anchor_projectiles_frame();
    assert(sends == 1 && !s_source.count);
}

static void lifetime_cleanup_still_runs_while_paused(void)
{
    reset_fixture();
    paused = 1;
    ++epoch;
    anchor_projectiles_frame();
    assert(!resets);
    ++session;
    anchor_projectiles_frame();
    assert(resets == 1 && !ticks && !reads);
    ++D_800C7AB2;
    anchor_projectiles_frame();
    assert(resets == 2 && !ticks && !reads);
    loaded = 0;
    anchor_projectiles_frame();
    assert(resets > 2 && !s_owner && !ticks && !reads);

    reset_fixture();
    ++epoch; /* A normal life/script boundary retains the previous reset. */
    anchor_projectiles_frame();
    assert(resets == 1 && ticks == 1);
}

int main(void)
{
    frozen_projectiles_resume_without_recreation();
    local_pending_retries_do_not_expire_during_pause();
    lifetime_cleanup_still_runs_while_paused();
    puts("Projectile modal freeze, resume, queue and lifecycle tests passed");
    return 0;
}
