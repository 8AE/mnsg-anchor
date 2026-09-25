#include <assert.h>
#include <stdio.h>
#include <string.h>

#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define DEBUG_BUTTON_ENABLED 0
#include "../src/combat/anchor_player_freeze_visual.c"

typedef union MockRecord
{
    void *alignment;
    unsigned char bytes[0x100];
} MockRecord;

void *D_801FC604_5B8514;
unsigned short D_800C7AB2;
unsigned char *D_8015C5C8_15D1C8;
unsigned int *D_8015C5CC_15D1CC;
unsigned char *D_80168504_169104;
short D_800C7A72_C8672;
FreezeSceneResource D_80167FC0_168BC0[48];
static MockRecord tasks[ANCHOR_FREEZE_VISUAL_MAX + 1];
static MockRecord backlinks[ANCHOR_FREEZE_VISUAL_MAX + 1];
static MockRecord objects[ANCHOR_FREEZE_VISUAL_MAX];
static unsigned int material[64];
static unsigned char resource[3];
static int peer_current = 1;

int anchor_remote_model_pool_contains(const void *pointer)
{
    int i;
    for (i = 0; i <= ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (pointer == tasks[i].bytes || pointer == backlinks[i].bytes)
            return 1;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (pointer == objects[i].bytes)
            return 1;
    return 0;
}
int anchor_player_models_get_epoch(void) { return 5; }
int anchor_player_models_peer_is_current(int cid, int session, int epoch)
{
    return peer_current && cid > 0 && session == 4 && epoch == 5;
}
int anchor_dialog_busy(void) { return 0; }
int anchor_player_models_get_freeze_visual_targets(AnchorFreezeVisualTarget *out,
                                                    int capacity)
{
    (void)out;
    (void)capacity;
    return 0;
}
void *func_800141C4_14DC4(unsigned int file_id)
{
    (void)file_id;
    return 0;
}
void *func_80013B14_14714(unsigned int file_id)
{
    (void)file_id;
    return 0;
}
void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                          unsigned short flags)
{
    (void)parent;
    (void)update;
    (void)flags;
    return 0;
}
void *func_8000DBF0_E7F0(void *task, unsigned int model,
                          unsigned int material_value, float x, float y,
                          float z, short rx, short ry, short rz,
                          float sx, float sy, float sz,
                          short file8, short file9)
{
    (void)task; (void)model; (void)material_value;
    (void)x; (void)y; (void)z; (void)rx; (void)ry; (void)rz;
    (void)sx; (void)sy; (void)sz; (void)file8; (void)file9;
    return 0;
}

static void link(MockRecord *task, MockRecord *backlink)
{
    *(void **)(task->bytes + 4) = backlink->bytes;
    *(void **)backlink->bytes = task->bytes;
}

static void setup(void)
{
    int i;
    memset(tasks, 0, sizeof(tasks));
    memset(backlinks, 0, sizeof(backlinks));
    memset(objects, 0, sizeof(objects));
    memset(s_slots, 0, sizeof(s_slots));
    D_800C7AB2 = 10;
    peer_current = 1;
    link(&tasks[ANCHOR_FREEZE_VISUAL_MAX],
         &backlinks[ANCHOR_FREEZE_VISUAL_MAX]);
    s_owner = D_801FC604_5B8514 = tasks[ANCHOR_FREEZE_VISUAL_MAX].bytes;
    s_material_arena = material;
    for (i = 0; i < 3; ++i)
        s_resources[i] = resource + i;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        unsigned char *object = objects[i].bytes;
        link(&tasks[i], &backlinks[i]);
        *(void **)(tasks[i].bytes + 0xc) = (void *)freeze_visual_task_update;
        *(void **)(tasks[i].bytes + 0x18) = object;
        s_slots[i].task = tasks[i].bytes;
        s_slots[i].object = object;
        s_slots[i].target.cid = i + 1;
        s_slots[i].target.session = 4;
        s_slots[i].target.epoch = 5;
        s_slots[i].room = 10;
        s_slots[i].active = 1;
        object[4] = 2;
        *(unsigned int *)(object + 0x2c) = 0x48000500u;
        *(float *)(object + 8) = 10.0f;
        *(float *)(object + 0xc) = 100.0f;
        *(float *)(object + 0x10) = 20.0f;
        *(float *)(object + 0x1c) = 2.0f;
        *(float *)(object + 0x20) = 2.0f;
        *(float *)(object + 0x24) = 2.0f;
    }
}

int main(void)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    setup();
    assert(anchor_player_freeze_visual_get_cubes(cubes, 4) == 4);
    assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                ANCHOR_FREEZE_VISUAL_MAX) == 8);
    assert(cubes[0].cid == 1 && cubes[0].session == 4 && cubes[0].epoch == 5);
    assert(cubes[0].cube.min.x == -90.0f && cubes[0].cube.max.x == 110.0f);
    assert(cubes[0].cube.min.y == 0.0f && cubes[0].cube.max.y == 200.0f);
    assert(cubes[0].cube.min.z == -80.0f && cubes[0].cube.max.z == 120.0f);
    assert(anchor_player_freeze_visual_has_cube(1, 4, 5));
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 6));
    s_slots[0].active = 0;
    s_slots[0].shard_frames = 40;
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    s_slots[0].active = 1;
    objects[0].bytes[0x64] |= 1u;
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    objects[0].bytes[0x64] = 0;
    s_slots[0].room = 11;
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    s_slots[0].room = 10;
    peer_current = 0;
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    puts("freeze cube visual lifecycle tests passed");
    return 0;
}
