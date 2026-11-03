/* Real projectile classification/capture/retirement, with its unrelated frame
 * coordinator discarded by the host linker's unused-section elimination. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define recomp_free free
#include "../src/combat/anchor_projectiles.c"

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
static int s_is_chain, s_fields, s_events;

int anchor_remote_model_pool_contains(const void *p) { return p != 0; }
int anchor_player_chain_is_native(const void *p)
{ (void)p; return s_is_chain; }
int anchor_projectile_capture_kind(int kind) { return kind == 1 || kind == 2; }
int anchor_projectile_capture_fields(const void *task, const void *object,
                                     AnchorProjectileSpawn *spawn)
{
    (void)task; (void)object;
    memset(spawn, 0, sizeof(*spawn));
    spawn->kind = 1;
    ++s_fields;
    return 1;
}
int anchor_projectile_source_capture(AnchorProjectileSourceState *state,
                                     const void *task,
                                     const AnchorProjectileSpawn *spawn,
                                     unsigned int tick, int publish)
{
    (void)state; (void)task; (void)spawn; (void)tick; (void)publish;
    ++s_events;
    return 1;
}
int anchor_projectile_source_task_id(const AnchorProjectileSourceState *state,
                                     const void *task)
{ (void)state; (void)task; return 0; }

int main(void)
{
    unsigned char task[0x100] = {0}, object[0x80] = {0};
    int owner;
    void *link = task;
    void *backlink = &link;
    void *object_pointer = object;
    void *owner_pointer = &owner;
    memcpy(task + 4, &backlink, sizeof(backlink));
    memcpy(task + 0x18, &object_pointer, sizeof(object_pointer));
    memcpy(task + 0x5c, &owner_pointer, sizeof(owner_pointer));
    D_801FC604_5B8514 = owner_pointer;
    task[0x64] = 2;
    s_is_chain = 1;
    assert(!anchor_projectiles_is_native_throw(task));
    capture_throw(task);
    anchor_projectiles_on_player_hit(task);
    assert(!s_fields && !s_events && task[0x65] == 0 && !s_stop_count);

    s_is_chain = 0;
    task[0x64] = 1;
    assert(anchor_projectiles_is_native_throw(task));
    capture_throw(task);
    assert(s_fields == 1 && s_events == 1);
    anchor_projectiles_on_player_hit(task);
    assert(task[0x65] == 1);
    puts("native chains bypass projectile capture, consumption and stop events");
    return 0;
}
