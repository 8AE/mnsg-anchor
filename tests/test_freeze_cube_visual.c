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
static unsigned int material[96];
static unsigned char resource[3];
static int peer_current = 1;
static int peer_frozen = 1;
static int local_frozen;
static AnchorFreezeVisualTarget mock_targets[ANCHOR_FREEZE_VISUAL_MAX];
static int mock_target_count;
void anchor_player_cube_reset(void) {}
int anchor_player_cube_visual_owned(int cid, int session, int epoch)
{
    (void)cid; (void)session; (void)epoch;
    return 0;
}
int anchor_player_cube_visual_native_pose(int cid, int session, int epoch)
{
    (void)cid; (void)session; (void)epoch;
    return 0;
}

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
int anchor_player_models_peer_frozen(int cid, int session, int epoch)
{
    return peer_frozen && anchor_player_models_peer_is_current(cid, session,
                                                               epoch);
}
int anchor_player_freeze_active(void) { return local_frozen; }
int anchor_dialog_busy(void) { return 0; }
int anchor_freeze_prompt_visible(void) { return 0; }
int anchor_player_models_get_freeze_visual_targets(AnchorFreezeVisualTarget *out,
                                                    int capacity)
{
    int i, count = mock_target_count < capacity ? mock_target_count : capacity;
    for (i = 0; i < count; ++i)
        out[i] = mock_targets[i];
    return count;
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
    memset(mock_targets, 0, sizeof(mock_targets));
    mock_target_count = 0;
    D_800C7AB2 = 10;
    peer_current = 1;
    peer_frozen = 1;
    local_frozen = 0;
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
        s_slots[i].target.scale = 1.0f;
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

static void target_one(void)
{
    mock_target_count = 1;
    mock_targets[0] = s_slots[0].target;
}

static void test_shatter_and_tombstone(void)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    void *task = 0, *object = 0;
    float scale = 0;
    int i;
    setup();
    target_one();
    *(unsigned int *)(tasks[0].bytes + 0x30) = 4u;
    *(unsigned int *)(tasks[0].bytes + 0x48) = 0x12345678u;
    assert(anchor_player_freeze_visual_shatter(1, 4, 5));
    assert(!anchor_player_freeze_visual_shatter(1, 4, 5));
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                ANCHOR_FREEZE_VISUAL_MAX) == 7);
    assert(anchor_player_freeze_visual_get_native(1, 4, 5,
                                                  &task, &object, &scale));
    assert(task == tasks[0].bytes && object == objects[0].bytes);
    assert(scale == 1.0f);
    assert(objects[0].bytes[4] == 2);
    assert(objects[0].bytes[0x64] & 1u);
    assert(*(unsigned int *)(objects[0].bytes + 0x2c) == 0);
    assert(*(unsigned int *)(tasks[0].bytes + 0x30) == 4u);
    assert(*(unsigned int *)(tasks[0].bytes + 0x48) == 0x12345678u);
    for (i = 0; i < (int)IMPACT_FRAMES; ++i)
    {
        anchor_player_freeze_visual_tick(s_owner);
        freeze_visual_task_update(task, object);
        assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    }
    assert(!anchor_player_freeze_visual_get_native(1, 4, 5,
                                                   &task, &object, &scale));
    assert(s_slots[0].shattered);
    mock_target_count = 0;
    for (i = 0; i < 40; ++i)
        anchor_player_freeze_visual_tick(s_owner);
    target_one();
    anchor_player_freeze_visual_tick(s_owner);
    assert(s_slots[0].shattered);
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    peer_frozen = 0;
    mock_target_count = 0;
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].shattered);
    peer_frozen = 1;
    target_one();
    anchor_player_freeze_visual_tick(s_owner);
    assert(anchor_player_freeze_visual_has_cube(1, 4, 5));
}

static void test_shatter_reset(void)
{
    setup();
    target_one();
    assert(anchor_player_freeze_visual_shatter(1, 4, 5));
    D_800C7AB2 = 11;
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].tombstoned);
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    anchor_player_freeze_visual_reset();
    assert(!s_slots[0].shattered);
}

static void test_early_thaw_keeps_impact_object(void)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    void *task;
    void *object;
    int i;
    setup();
    task = s_slots[0].task;
    object = s_slots[0].object;
    assert(anchor_player_freeze_visual_shatter(1, 4, 5));
    peer_frozen = 0;
    for (i = 0; i < (int)IMPACT_FRAMES - 1; ++i)
    {
        anchor_player_freeze_visual_tick(s_owner);
        freeze_visual_task_update(task, object);
        assert(s_slots[0].shattered && !s_slots[0].tombstoned);
        assert(s_slots[0].task == task && s_slots[0].object == object);
        assert(anchor_player_freeze_visual_owns_task(task));
        assert(((unsigned char *)object)[4] == 2);
        assert(((unsigned char *)object)[0x64] & 1u);
        assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                    ANCHOR_FREEZE_VISUAL_MAX) <= 7);
    }
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].shattered);
}

static void test_remote_thaw_hides_cube_immediately(void)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    void *task = 0, *object = 0;
    float scale = 0;
    setup();
    target_one();
    peer_frozen = 0;
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                ANCHOR_FREEZE_VISUAL_MAX) == 0);
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].active && !s_slots[0].shard_frames);
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    assert(!anchor_player_freeze_visual_get_native(1, 4, 5,
                                                   &task, &object, &scale));
    assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                ANCHOR_FREEZE_VISUAL_MAX) == 0);
    assert(objects[0].bytes[0x64] & 1u);
    assert(*(unsigned int *)(objects[0].bytes + 0x2c) == 0);
    freeze_visual_task_update(tasks[0].bytes, objects[0].bytes);
    assert(!anchor_player_freeze_visual_has_cube(1, 4, 5));
    /* A stale frozen target in the same frame cannot redraw the cube. */
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].active);
    peer_frozen = 1;
    anchor_player_freeze_visual_tick(s_owner);
    assert(anchor_player_freeze_visual_has_cube(1, 4, 5));
}

static void test_missing_remote_target_keeps_frozen_cube(void)
{
    AnchorFreezeCubeCollision cube;
    setup();
    target_one();
    anchor_player_freeze_visual_tick(s_owner);
    mock_target_count = 0;
    anchor_player_freeze_visual_tick(s_owner);
    assert(s_slots[0].active && !s_slots[0].shard_frames);
    assert(anchor_player_freeze_visual_has_cube(1, 4, 5));
    assert(anchor_player_freeze_visual_get_cubes(&cube, 1) == 1);
    target_one();
    anchor_player_freeze_visual_tick(s_owner);
    assert(anchor_player_freeze_visual_has_cube(1, 4, 5));
}

static void test_local_thaw_hides_cube_immediately(void)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    setup();
    s_slots[0].target.cid = 0;
    s_slots[0].target.session = 0;
    local_frozen = 1;
    target_one();
    anchor_player_freeze_visual_tick(s_owner);
    assert(anchor_player_freeze_visual_has_cube(0, 0, 5));
    local_frozen = 0;
    assert(!anchor_player_freeze_visual_has_cube(0, 0, 5));
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].active && !s_slots[0].shard_frames);
    assert(!anchor_player_freeze_visual_has_cube(0, 0, 5));
    assert(anchor_player_freeze_visual_get_cubes(cubes,
                                                ANCHOR_FREEZE_VISUAL_MAX) == 7);
    anchor_player_freeze_visual_tick(s_owner);
    assert(!s_slots[0].active);
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
    test_shatter_and_tombstone();
    test_shatter_reset();
    test_early_thaw_keeps_impact_object();
    test_remote_thaw_hides_cube_immediately();
    test_missing_remote_target_keeps_frozen_cube();
    test_local_thaw_hides_cube_immediately();
    puts("freeze cube visual lifecycle tests passed");
    return 0;
}
