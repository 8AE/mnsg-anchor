#include <assert.h>
#include <stddef.h>
#include <string.h>

#define ANCHOR_REMOTE_SMOKE_HOST_TEST
#include "../src/player/anchor_remote_smoke.c"

static unsigned char task_storage[300][0xf0];
static unsigned char display_storage[300][0xb0];
static unsigned char common_storage[16];
static unsigned char texture_storage[16];
static int task_count, display_count, deleted_count, texture_steps;
static int reject_display_count;
const void *D_80204A10_5C0920[6] =
    {(void *)1, (void *)2, (void *)2, (void *)1, (void *)2, (void *)1};
const unsigned char D_80204AD0_5C09E0[8] = {0x0a, 0, 0x69, 0x80, 2};
SmokeSceneResource D_80167FC0_168BC0[48];
short D_800C7A72_C8672;

void *func_80013B14_14714(unsigned int file_id)
{
    assert(file_id == SMOKE_FILE || file_id == SMOKE_TEXTURE_FILE);
    return common_storage;
}

void *func_800141C4_14DC4(unsigned int file_id)
{
    return file_id == SMOKE_FILE ? common_storage :
           file_id == SMOKE_TEXTURE_FILE ? texture_storage : (void *)-1;
}

void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                           unsigned short flags)
{
    (void)parent;
    assert(update == smoke_task_update && flags == 0);
    assert(task_count < 300);
    return task_storage[task_count++];
}

void func_80034EF8_35AF8(void *task)
{
    assert(task != NULL);
    ++deleted_count;
}

void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                           float x, float y, float z, short rx, short ry,
                           short rz, float sx, float sy, float sz,
                           short file8, short file9)
{
    (void)task;
    assert(!model && !material && !x && !y && !z);
    assert((unsigned short)rx == 0x8000u &&
           (unsigned short)ry == 0x8000u &&
           (unsigned short)rz == 0x8000u);
    assert(!sx && !sy && !sz && !file8 && !file9);
    if (reject_display_count)
    {
        --reject_display_count;
        return NULL;
    }
    assert(display_count < 300);
    return display_storage[display_count++];
}

void func_80033898_34498(unsigned short rx, unsigned short ry,
                           unsigned short rz, float *x, float *y, float *z)
{
    (void)rx;
    (void)y;
    (void)rz;
    if (ry == 1)
    {
        float old_x = *x;
        *x = -*z;
        *z = old_x;
    }
}

void func_801E8820_5A4730(void *object, unsigned short segment,
                            const void *descriptor)
{
    assert(object && segment == 10 && descriptor);
}

void func_801E8858_5A4768(void *object, unsigned short segment)
{
    assert(object && segment == 10);
    ++texture_steps;
}

static AnchorPlayerModelRemote remote_state(int ch, int action)
{
    AnchorPlayerModelRemote remote;
    memset(&remote, 0, sizeof(remote));
    remote.cid = 3;
    remote.ch = ch;
    remote.action = action;
    remote.jet_velocity_100 = ANCHOR_REMOTE_JET_SPEED_UNAVAILABLE;
    remote.anim_frame_100 = 100;
    remote.interaction_session = 5;
    remote.player_epoch = 8;
    return remote;
}

static void init_owner(unsigned char *task, unsigned char *object)
{
    void *pointer = object;
    memset(task, 0, 0xf0);
    memset(object, 0, 0xb0);
    memcpy(task + 0x18, &pointer, sizeof(pointer));
    *(float *)(object + 8) = 10.0f;
    *(float *)(object + 0xc) = 20.0f;
    *(float *)(object + 0x10) = 30.0f;
    *(float *)(object + 0x1c) = 0.1f;
    *(float *)(object + 0x20) = 0.1f;
    *(float *)(object + 0x24) = 0.1f;
}

static void apply(AnchorRemoteSmokeState *state, unsigned char *task,
                  unsigned char *object, AnchorPlayerModelRemote *remote,
                  unsigned short room)
{
    anchor_remote_smoke_set_context(state, task, object, remote, room);
    anchor_remote_smoke_apply(state, task, object, remote, room);
}

static void assert_camera_facing(const void *object)
{
    const unsigned char *record = object;
    assert(*(const unsigned short *)(record + 0x14) == 0x8000u);
    assert(*(const unsigned short *)(record + 0x16) == 0x8000u);
    assert(*(const unsigned short *)(record + 0x18) == 0x8000u);
}

static void test_switch_visual_and_context(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    int displays_before = display_count;
    int delete_before = deleted_count;
    int i;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    assert(display_count == displays_before);
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(display_count == displays_before + 6);
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_SWITCH_PARTS; ++i)
        assert_camera_facing(state.object[i]);
    assert(state.particle[0].active);
    assert(state.particle[0].scale > 0.03f);
    assert(*(unsigned int *)((unsigned char *)state.object[0] + 0x2c) == SWITCH_MODEL);
    assert(material_for(&state, 1)[3] == 0x808080ffu);
    assert(state.particle[0].y == 4.9f);
    assert(*(float *)((unsigned char *)state.object[0] + 0xc) == 24.9f);
    *(unsigned short *)((unsigned char *)state.object[0] + 0x14) = 0;
    *(unsigned short *)((unsigned char *)state.object[0] + 0x16) = 0;
    *(unsigned short *)((unsigned char *)state.object[0] + 0x18) = 0;
    remote.ch = 1;
    remote.action = 0;
    apply(&state, task, object, &remote, 9);
    assert_camera_facing(state.object[0]);
    /* The character-ID packet can follow the switch action one frame later.
     * Keep the in-flight burst instead of restarting its six records. */
    assert(state.particle[0].scale > 0.04f);
    for (i = 0; i < 40; ++i)
        apply(&state, task, object, &remote, 9);
    assert(!state.particle[0].active);
    assert(state.task == NULL && state.bank == -1);
    assert(deleted_count == delete_before + 1);
    remote.ch = 0;
    remote.action = 0;
    remote.anim_frame_100 = 0;
    apply(&state, task, object, &remote, 9);
    assert(state.particle[0].active);
    remote.player_epoch++;
    apply(&state, task, object, &remote, 9);
    assert(deleted_count == delete_before + 2);
    assert(!state.particle[0].active);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_jet_emission_and_stop(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(2, 0);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    int i;
    init_owner(task, object);
    *(unsigned short *)(object + 0x16) = 1;
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_JETPACK;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before);
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + 2);
    assert_camera_facing(state.object[6]);
    assert_camera_facing(state.object[13]);
    assert(state.particle[6].active && state.particle[13].active);
    assert(state.particle[6].scale > 0.02f);
    assert(state.particle[6].x == 11.7f);
    assert(state.particle[6].z == 31.5f);
    assert(state.particle[6].y == 23.8f); /* Legacy speed fallback. */
    assert(material_for(&state, 6)[3] == 0xffffffd8u);
    *(unsigned short *)((unsigned char *)state.object[6] + 0x14) = 0;
    *(unsigned short *)((unsigned char *)state.object[6] + 0x16) = 0;
    *(unsigned short *)((unsigned char *)state.object[6] + 0x18) = 0;
    apply(&state, task, object, &remote, 9);
    assert_camera_facing(state.object[6]);
    remote.action = 0;
    for (i = 0; i < 20; ++i)
        apply(&state, task, object, &remote, 9);
    assert(!state.particle[6].active && !state.particle[13].active);
    assert(state.task == NULL && state.bank == -1);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_native_jet_drift_and_fade(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_SASUKE, 0);
    unsigned char task[0xf0], object[0xb0];
    float left_y, right_y;
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_JETPACK;
    remote.jet_velocity_100 = 250;
    apply(&state, task, object, &remote, 9);
    apply(&state, task, object, &remote, 9);
    assert(state.particle[6].active && state.particle[13].active);
    left_y = state.particle[6].y;
    right_y = state.particle[13].y;
    apply(&state, task, object, &remote, 9);
    assert(state.particle[6].y > left_y - 0.033f &&
           state.particle[6].y < left_y - 0.031f);
    assert(state.particle[13].y > right_y - 0.033f &&
           state.particle[13].y < right_y - 0.031f);

    remote.jet_velocity_100 = 500;
    left_y = state.particle[6].y;
    right_y = state.particle[13].y;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + 4);
    assert(state.particle[7].active && state.particle[14].active);
    assert_camera_facing(state.object[7]);
    assert_camera_facing(state.object[14]);
    assert(state.particle[6].y > left_y + 1.217f &&
           state.particle[6].y < left_y + 1.219f);
    assert(state.particle[13].y > right_y + 1.217f &&
           state.particle[13].y < right_y + 1.219f);
    assert(state.particle[7].vy > 1.217f && state.particle[7].vy < 1.219f);

    remote.jet_velocity_100 = -100;
    left_y = state.particle[6].y;
    apply(&state, task, object, &remote, 9);
    assert(state.particle[6].y == left_y - 2.0f);
    assert(state.jet_tick == 0);
    remote.action = 0;
    left_y = state.particle[6].y;
    apply(&state, task, object, &remote, 9);
    assert(state.particle[6].y == left_y - 2.0f);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_bounded_banks(void)
{
    AnchorRemoteSmokeState states[ANCHOR_REMOTE_SMOKE_BANK_LIMIT + 1] = {{0}};
    unsigned char tasks[ANCHOR_REMOTE_SMOKE_BANK_LIMIT + 1][0xf0] = {{0}};
    unsigned char objects[ANCHOR_REMOTE_SMOKE_BANK_LIMIT + 1][0xb0] = {{0}};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    int i;
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT + 1; ++i)
    {
        init_owner(tasks[i], objects[i]);
        remote.cid = i + 2;
        apply(&states[i], tasks[i], objects[i], &remote, 9);
        remote.action = ACTION_SWITCH;
        apply(&states[i], tasks[i], objects[i], &remote, 9);
        assert((states[i].bank >= 0) == (i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT));
        remote.action = 0;
    }
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT + 1; ++i)
        anchor_remote_smoke_reset(&states[i], 1);
    for (i = 0; i < ANCHOR_REMOTE_SMOKE_BANK_LIMIT; ++i)
        assert(!s_bank_owner[i]);
}

static void test_hidden_owner_hides_then_releases_at_frame_end(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    int before = deleted_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(state.task && state.bank >= 0);
    anchor_remote_smoke_suspend(&state);
    assert(state.suspended && !state.particle[0].active);
    assert(*(unsigned int *)((unsigned char *)state.object[0] + 0x2c) == 0);
    assert(deleted_count == before);
    anchor_remote_smoke_set_context(&state, task, object, &remote, 9);
    assert(state.task == NULL && state.bank == -1);
    assert(deleted_count == before + 1);
    {
        int displays_after_release = display_count;
        remote.action = 0;
        apply(&state, task, object, &remote, 9);
        assert(display_count == displays_after_release);
        assert(!state.switch_pending && !state.particle[0].active);
    }
    anchor_remote_smoke_reset(&state, 1);
}

static void test_late_switch_and_rebind_do_not_duplicate(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0], rebuilt_task[0xf0], rebuilt_object[0xb0];
    int before = display_count;
    init_owner(task, object);
    init_owner(rebuilt_task, rebuilt_object);
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_SWITCH;
    remote.anim_frame_100 = 1700;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active);
    /* A valid same-peer render-object rebuild retains the consumed edge. */
    apply(&state, rebuilt_task, rebuilt_object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(!state.particle[0].active);
    remote.ch = 1;
    remote.action = 0;
    apply(&state, rebuilt_task, rebuilt_object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(!state.switch_pending);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_first_seen_switch_action(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(1, ACTION_SWITCH);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    init_owner(task, object);
    remote.anim_frame_100 = 1100;
    apply(&state, task, object, &remote, 9);
    /* A join snapshot cannot establish a newly observed switch edge. */
    assert(display_count == before);
    remote.action = 0;
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_switch_bank_wait_and_expiry(void)
{
    AnchorRemoteSmokeState states[3] = {{0}};
    AnchorPlayerModelRemote remotes[3];
    unsigned char tasks[3][0xf0], objects[3][0xb0];
    int i;
    int before = display_count;
    for (i = 0; i < 3; ++i)
    {
        init_owner(tasks[i], objects[i]);
        remotes[i] = remote_state(0, 0);
        remotes[i].cid = i + 10;
        apply(&states[i], tasks[i], objects[i], &remotes[i], 9);
        remotes[i].action = ACTION_SWITCH;
        remotes[i].anim_frame_100 = 1200;
        apply(&states[i], tasks[i], objects[i], &remotes[i], 9);
    }
    assert(states[2].switch_pending && states[2].bank == -1);
    assert(states[2].switch_retry == SWITCH_RETRY_FRAMES - 1);
    anchor_remote_smoke_reset(&states[0], 1);
    remotes[2].action = 0;
    apply(&states[2], tasks[2], objects[2], &remotes[2], 9);
    assert(display_count == before + 3 * ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(states[2].particle[0].active);
    anchor_remote_smoke_reset(&states[1], 1);
    anchor_remote_smoke_reset(&states[2], 1);

    /* No material/bank availability is bounded; a stale burst cannot appear
     * long after its switch, even if capacity returns later. */
    for (i = 0; i < 3; ++i)
    {
        remotes[i].action = 0;
        apply(&states[i], tasks[i], objects[i], &remotes[i], 9);
        remotes[i].action = ACTION_SWITCH;
        apply(&states[i], tasks[i], objects[i], &remotes[i], 9);
    }
    assert(states[2].switch_pending);
    remotes[2].action = 0;
    for (i = 1; i < SWITCH_RETRY_FRAMES; ++i)
        apply(&states[2], tasks[2], objects[2], &remotes[2], 9);
    assert(!states[2].switch_pending && states[2].bank == -1);
    anchor_remote_smoke_reset(&states[0], 1);
    before = display_count;
    apply(&states[2], tasks[2], objects[2], &remotes[2], 9);
    assert(display_count == before);
    anchor_remote_smoke_reset(&states[1], 1);
    anchor_remote_smoke_reset(&states[2], 1);
}

static void test_hidden_new_switch_and_started_smoke(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    anchor_remote_smoke_suspend(&state);
    remote.ch = 1;
    remote.action = ACTION_SWITCH;
    remote.anim_frame_100 = 1700;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active);
    anchor_remote_smoke_suspend(&state);
    assert(!state.particle[0].active);
    remote.action = 0;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(!state.particle[0].active);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_switch_epoch_edges_preserve_one_burst(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    void *smoke_task;
    int before = display_count;
    int deleted_before = deleted_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);

    /* Native work+0x69 changing to one advances the epoch with action BA. */
    remote.player_epoch++;
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(state.particle[0].active);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    smoke_task = state.task;

    /* Character rebind and the second alive-state epoch edge keep that burst. */
    remote.player_epoch++;
    remote.ch = 1;
    apply(&state, task, object, &remote, 9);
    assert(state.task == smoke_task);
    assert(state.particle[0].active);
    assert(state.particle[0].scale > 0.04f);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(deleted_count == deleted_before);
    remote.action = 0;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_character_edge_and_plain_epoch(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    remote.player_epoch += 2; /* Both epoch snapshots may be coalesced. */
    remote.ch = 1;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active);
    remote.player_epoch++;
    apply(&state, task, object, &remote, 9);
    assert(!state.particle[0].active);
    assert(state.task == NULL);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_pending_switch_survives_epoch_and_suspend(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    unsigned int *arena = s_material_arena;
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    s_material_arena = NULL;
    remote.player_epoch++;
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(state.switch_pending && state.switch_retry == SWITCH_RETRY_FRAMES - 1);
    anchor_remote_smoke_suspend(&state);
    assert(state.switch_pending);
    remote.player_epoch++;
    remote.ch = 1;
    apply(&state, task, object, &remote, 9);
    assert(state.switch_pending && !state.particle[0].active);
    s_material_arena = arena;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active && !state.switch_pending);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_join_reconnect_and_room_baselines(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, ACTION_SWITCH);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    assert(display_count == before);
    remote.interaction_session++;
    remote.player_epoch++;
    remote.ch = 1;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before);
    remote.player_epoch++;
    remote.ch = 2;
    apply(&state, task, object, &remote, 10);
    assert(display_count == before);
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 10);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    anchor_remote_smoke_reset(&state, 1);
}

static void test_failed_jet_allocation_releases_bank(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_SASUKE, ACTION_JETPACK);
    unsigned char task[0xf0], object[0xb0];
    int before = deleted_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    reject_display_count = ANCHOR_REMOTE_SMOKE_JET_PARTS;
    apply(&state, task, object, &remote, 9);
    assert(!state.particle[6].active && !state.particle[13].active);
    assert(state.release_idle && state.bank >= 0 && state.task);
    assert(deleted_count == before);
    apply(&state, task, object, &remote, 9);
    assert(state.bank == -1 && state.task == NULL);
    assert(deleted_count == before + 1);
    apply(&state, task, object, &remote, 9);
    assert(state.particle[6].active && state.particle[13].active);
    anchor_remote_smoke_reset(&state, 1);
}

static unsigned int *graphics_material(const AnchorRemoteSmokeState *state,
                                       int index, int graphics_bank)
{
    return s_material_arena +
        ((state->bank * ANCHOR_REMOTE_SMOKE_PARTS + index) * 2 +
         graphics_bank) * MATERIAL_WORDS;
}

static void test_native_graphics_bank_ownership(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    unsigned int first_bank_color;
    unsigned int second_bank_color;
    int i;
    init_owner(task, object);
    memset(s_host_material_arena, 0x5a, sizeof(s_host_material_arena));
    D_800C7A72_C8672 = 0;
    apply(&state, task, object, &remote, 9);
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(state.material_bank == 0);
    assert(graphics_material(&state, 0, 0)[0] == 0x06000000u);
    assert(graphics_material(&state, 0, 1)[0] == 0x5a5a5a5au);
    for (i = 0; i < 3; ++i)
        apply(&state, task, object, &remote, 9);
    assert(graphics_material(&state, 0, 1)[0] == 0x5a5a5a5au);
    first_bank_color = graphics_material(&state, 0, 0)[3];

    D_800C7A72_C8672 = 1;
    apply(&state, task, object, &remote, 9);
    assert(state.material_bank == 1);
    assert(graphics_material(&state, 0, 1)[0] == 0x06000000u);
    assert(graphics_material(&state, 0, 0)[3] == first_bank_color);
    second_bank_color = graphics_material(&state, 0, 1)[3];

    D_800C7A72_C8672 = 2;
    apply(&state, task, object, &remote, 9);
    assert(*(unsigned int *)((unsigned char *)state.object[0] + 0x2c) == 0);
    assert(graphics_material(&state, 0, 0)[3] == first_bank_color);
    assert(graphics_material(&state, 0, 1)[3] == second_bank_color);
    D_800C7A72_C8672 = 1;
    apply(&state, task, object, &remote, 9);
    assert(*(unsigned int *)((unsigned char *)state.object[0] + 0x2c) == SWITCH_MODEL);
    anchor_remote_smoke_reset(&state, 1);
    D_800C7A72_C8672 = 0;
}

static void test_invalid_graphics_bank_defers_new_switch(void)
{
    AnchorRemoteSmokeState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0], object[0xb0];
    int before = display_count;
    init_owner(task, object);
    apply(&state, task, object, &remote, 9);
    D_800C7A72_C8672 = -1;
    remote.action = ACTION_SWITCH;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before);
    assert(!state.switch_pending && state.last_action == 0);
    D_800C7A72_C8672 = 0;
    apply(&state, task, object, &remote, 9);
    assert(display_count == before + ANCHOR_REMOTE_SMOKE_SWITCH_PARTS);
    assert(state.particle[0].active);
    anchor_remote_smoke_reset(&state, 1);
}

int main(void)
{
    anchor_remote_smoke_load_resources();
    test_switch_visual_and_context();
    test_jet_emission_and_stop();
    test_native_jet_drift_and_fade();
    test_bounded_banks();
    test_hidden_owner_hides_then_releases_at_frame_end();
    test_late_switch_and_rebind_do_not_duplicate();
    test_first_seen_switch_action();
    test_switch_bank_wait_and_expiry();
    test_hidden_new_switch_and_started_smoke();
    test_switch_epoch_edges_preserve_one_burst();
    test_character_edge_and_plain_epoch();
    test_pending_switch_survives_epoch_and_suspend();
    test_join_reconnect_and_room_baselines();
    test_failed_jet_allocation_releases_bank();
    test_native_graphics_bank_ownership();
    test_invalid_graphics_bank_defers_new_switch();
    assert(texture_steps > 0);
    return 0;
}
