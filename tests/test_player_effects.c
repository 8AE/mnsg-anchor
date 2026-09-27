#include <assert.h>
#include <stddef.h>
#include <string.h>

#define ANCHOR_PLAYER_EFFECTS_HOST_TEST
#include "../src/player/anchor_player_effects.c"

static unsigned char manager_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][0xf0];
static int managers_created;
static int managers_destroyed;
static int purple_bursts;
static int effect_children_live;
static int aura_children_live;
static int aura_bursts;
static int resources_ready = 1;
static int install_native_post;
static int native_post_calls;
short D_800C7A72_C8672;
static unsigned char charge_object_storage[8][0x98];
static int charge_objects_created;
static unsigned char aura_task_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][6][0xf0];
static unsigned char aura_object_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][6][0x98];
static unsigned char purple_task_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][4][0xf0];
static unsigned char purple_object_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][4][0x98];
static unsigned char successor_task_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][0xf0];
static unsigned char successor_object_storage[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2][0x98];
static void put_pointer(void *task, unsigned int offset, void *pointer);

static void mock_native_post(void *task, void *object)
{
    (void)object;
    ++native_post_calls;
    ((unsigned char *)task)[0xa4] = 0x7au;
}

int anchor_remote_model_pool_contains(const void *pointer)
{
    unsigned long p = (unsigned long)pointer;
#define INSIDE(storage) (p >= (unsigned long)(storage) && \
                         p < (unsigned long)(storage) + sizeof(storage))
    return INSIDE(aura_task_storage) || INSIDE(aura_object_storage) ||
           INSIDE(purple_task_storage) || INSIDE(purple_object_storage);
#undef INSIDE
}

static int manager_index(void *manager)
{
    int i;
    for (i = 0; i < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2; ++i)
        if (manager == manager_storage[i])
            return i;
    assert(0);
    return -1;
}

static void init_effect_child(void *task, void *object, void *manager,
                              void *owner, unsigned short depth, int kind)
{
    memset(task, 0, 0xf0);
    memset(object, 0, 0x98);
    put_pointer(task, 0x18, object);
    put_pointer(task, 0x5c, owner);
    put_pointer(task, 0x84, manager);
    *(unsigned short *)((unsigned char *)task + 0x20) = depth;
    ((unsigned char *)task)[0x64] = kind;
    memset((unsigned char *)task + 0xa4, kind, 32);
}

static void append_unrelated_successor(void *manager, void *last, int index)
{
    unsigned char *task = successor_task_storage[index];
    unsigned char *object = successor_object_storage[index];
    memset(task, 0, 0xf0);
    memset(object, 0, 0x98);
    *(unsigned int *)(object + 0x2c) = 0xdeadbeefu;
    put_pointer(task, 0x18, object);
    put_pointer(last, 0, task);
    (void)manager;
}

void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                          unsigned short flags)
{
    (void)parent;
    (void)update;
    (void)flags;
    assert(managers_created < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 2);
    memset(manager_storage[managers_created], 0,
           sizeof(manager_storage[managers_created]));
    return manager_storage[managers_created++];
}

void func_80034EF8_35AF8(void *task)
{
    assert(task != NULL);
    ++managers_destroyed;
}

void *func_800141C4_14DC4(unsigned int file_id)
{
    assert(file_id == 0x80u || file_id == 0x152u);
    return resources_ready ? (void *)manager_storage : (void *)-1;
}

void *func_8000DBF0_E7F0(void *task, unsigned int model,
    unsigned int material, float x, float y, float z,
    short rx, short ry, short rz, float sx, float sy, float sz,
    short file8, short file9)
{
    (void)task; (void)model; (void)material;
    (void)x; (void)y; (void)z;
    (void)rx; (void)ry; (void)rz;
    (void)sx; (void)sy; (void)sz;
    (void)file8; (void)file9;
    assert(charge_objects_created < 8);
    return charge_object_storage[charge_objects_created++];
}

int func_801E8C44_5A4B54(void *manager, unsigned char kind)
{
    assert(manager != NULL);
    assert(kind == NATIVE_PURPLE_EFFECT_KIND || kind == NATIVE_SUDDEN_AURA_KIND);
    return kind == NATIVE_PURPLE_EFFECT_KIND ?
        effect_children_live && ((unsigned char *)manager)[0x80] :
        aura_children_live && ((unsigned char *)manager)[0x81];
}

void func_801F11F0_5AD100(void *manager, unsigned int kind)
{
    int index = manager_index(manager), i;
    void *owner = read_task_pointer(manager, 0x5c);
    assert(manager != NULL);
    assert(kind == NATIVE_PURPLE_EFFECT_KIND);
    ++purple_bursts;
    effect_children_live = 1;
    ((unsigned char *)manager)[0x80] = 1;
    put_pointer(manager, 0, purple_task_storage[index][0]);
    for (i = 0; i < 4; ++i)
    {
        init_effect_child(purple_task_storage[index][i],
                          purple_object_storage[index][i], manager, owner,
                          1, NATIVE_PURPLE_EFFECT_KIND);
        if (i < 3)
            put_pointer(purple_task_storage[index][i], 0,
                        purple_task_storage[index][i + 1]);
    }
    append_unrelated_successor(manager, purple_task_storage[index][3], index);
}

void func_801F15E0_5AD4F0(void *manager, unsigned int kind)
{
    void *owner = read_task_pointer(manager, 0x5c);
    int i, index = manager_index(manager);
    assert(kind == NATIVE_SUDDEN_AURA_KIND);
    ++aura_bursts;
    ((unsigned char *)manager)[0x81] = 1;
    put_pointer(manager, 0, aura_task_storage[index][0]);
    for (i = 0; i < 6; ++i)
    {
        init_effect_child(aura_task_storage[index][i],
                          aura_object_storage[index][i], manager, owner,
                          i ? 2 : 1, i ? 0 : NATIVE_SUDDEN_AURA_KIND);
        if (!i && install_native_post)
            put_pointer(aura_task_storage[index][i], 0x10,
                        (void *)mock_native_post);
        if (i < 5)
            put_pointer(aura_task_storage[index][i], 0,
                        aura_task_storage[index][i + 1]);
    }
    append_unrelated_successor(manager, aura_task_storage[index][5], index);
    aura_children_live = 1;
}

static void put_pointer(void *task, unsigned int offset, void *pointer)
{
    memcpy((unsigned char *)task + offset, &pointer, sizeof(pointer));
}

static AnchorPlayerModelRemote remote_state(int ch, int action)
{
    AnchorPlayerModelRemote remote;
    memset(&remote, 0, sizeof(remote));
    remote.cid = 2;
    remote.interaction_session = 5;
    remote.player_epoch = 7;
    remote.ch = ch;
    remote.action = action;
    remote.anim_frame_100 = 100;
    return remote;
}

static void apply(AnchorPlayerEffectState *state, void *task, void *object,
                  const AnchorPlayerModelRemote *remote, unsigned short room)
{
    anchor_player_effects_set_context(state, task, remote, room);
    anchor_player_effects_apply(state, task, object, remote, room);
}

static void test_purple_action_edges_and_lifecycle(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_EBISUMARU,
                                                  ACTION_MINI_SHRINK);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    int before;
    put_pointer(task, 0x18, object);
    effect_children_live = 0;
    managers_created = 0;
    before = purple_bursts;

    remote.anim_frame_100 = 1200; /* The first packet missed action entry. */
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 1);
    assert(managers_created == 1);
    assert(read_task_pointer(state.manager, 0x5c) == task);
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 1);

    remote.action = ACTION_MINI_GROW;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 1); /* First burst still occupies manager. */
    effect_children_live = 0;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 2);
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    effect_children_live = 0;
    apply(&state, task, object, &remote, 10);
    assert(managers_destroyed == 2);
    assert(state.manager == NULL);

    remote.action = ACTION_MINI_SHRINK;
    remote.anim_frame_100 = 500;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 3);
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    effect_children_live = 0;
    apply(&state, task, object, &remote, 10);
    remote.action = ACTION_MINI_SHRINK;
    remote.anim_frame_100 = 100;
    resources_ready = 0;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 3);
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 4);
    anchor_player_effects_reset(&state, 1);
}

static void test_mermaid_both_transitions_and_reconnect(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_YAE,
                                                  ACTION_MERMAID_ENTER);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    int previous_destroys = managers_destroyed;
    int previous_bursts = purple_bursts;
    put_pointer(task, 0x18, object);
    managers_created = 0;
    effect_children_live = 0;

    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == previous_bursts + 1);
    remote.action = ACTION_MERMAID_EXIT;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == previous_bursts + 1);
    effect_children_live = 0;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == previous_bursts + 2);
    assert(managers_destroyed == previous_destroys + 1);
    remote.player_epoch++;
    apply(&state, task, object, &remote, 10);
    assert(managers_destroyed == previous_destroys + 2);
    assert(purple_bursts == previous_bursts + 3);
    remote.interaction_session++;
    apply(&state, task, object, &remote, 10);
    assert(managers_destroyed == previous_destroys + 3);
    assert(purple_bursts == previous_bursts + 4);
    apply(&state, task, object, &remote, 11);
    assert(managers_destroyed == previous_destroys + 4);
    assert(purple_bursts == previous_bursts + 5);
    anchor_player_effects_reset(&state, 1);
}

static void test_purple_retry_expires_and_rebind_drops_pending(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_YAE,
                                                  ACTION_MERMAID_ENTER);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    int before = purple_bursts;
    int i;
    put_pointer(task, 0x18, object);
    effect_children_live = 0;
    resources_ready = 0;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == ACTION_MERMAID_ENTER);
    assert(purple_bursts == before);
    remote.action = 0;
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == -1 && purple_bursts == before);

    resources_ready = 0;
    remote.action = ACTION_MERMAID_ENTER;
    apply(&state, task, object, &remote, 10);
    anchor_player_effects_suspend(&state);
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == -1 && purple_bursts == before);
    remote.player_epoch++;
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == -1 && purple_bursts == before);

    resources_ready = 0;
    remote.action = ACTION_MERMAID_ENTER;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == ACTION_MERMAID_ENTER);
    remote.player_epoch++;
    remote.action = 0;
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(state.purple_pending_action == -1 && purple_bursts == before);

    resources_ready = 0;
    remote.action = ACTION_MERMAID_EXIT;
    for (i = 0; i < TRANSFORM_RETRY_FRAMES; ++i)
        apply(&state, task, object, &remote, 10);
    assert(state.purple_retry_frames == 0 && purple_bursts == before);
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before); /* Held stale action cannot replay. */
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    remote.action = ACTION_MERMAID_ENTER;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == before + 1);
    anchor_player_effects_reset(&state, 1);
}

static void test_bounded_manager_pool(void)
{
    AnchorPlayerEffectState states[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 1] = {0};
    unsigned char tasks[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 1][0xf0] = {{0}};
    unsigned char objects[ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 1][0x98] = {{0}};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_YAE,
                                                  ACTION_MERMAID_ENTER);
    int i;
    int before = purple_bursts;

    effect_children_live = 1;
    managers_created = 0;
    for (i = 0; i < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 1; ++i)
    {
        remote.cid = i + 2;
        put_pointer(tasks[i], 0x18, objects[i]);
        apply(&states[i], tasks[i], objects[i], &remote, 10);
    }
    assert(purple_bursts - before == ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT);
    assert(managers_created == ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT);
    for (i = 0; i < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT + 1; ++i)
        anchor_player_effects_reset(&states[i], 1);
    assert(s_manager_count == 0);
}

static void test_slot_reuse_drops_old_visual_owner(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_YAE,
                                                  ACTION_MERMAID_ENTER);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    int previous_bursts = purple_bursts;
    int previous_destroys = managers_destroyed;

    put_pointer(task, 0x18, object);
    effect_children_live = 1;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == previous_bursts + 1);
    anchor_player_effects_reset(&state, 1);
    assert(managers_destroyed == previous_destroys + 1);
    remote.cid = 99;
    apply(&state, task, object, &remote, 10);
    assert(purple_bursts == previous_bursts + 2);
    assert(state.cid == 99);
    anchor_player_effects_reset(&state, 0);
    assert(managers_destroyed == previous_destroys + 1);
    assert(s_manager_count == 0);
}

static void test_charge_models_and_release(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_EBISUMARU, 0);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    unsigned int *commands;
    int i;
    managers_created = 0;
    charge_objects_created = 0;
    effect_children_live = 0;
    *(float *)(object + 0x1c) = 0.1f;
    *(float *)(object + 8) = 10.0f;
    *(float *)(object + 0xc) = 20.0f;
    put_pointer(task, 0x18, object);
    anchor_player_effects_load_resources();
    D_800C7A72_C8672 = 0;
    memset(s_charge_materials + CHARGE_MATERIAL_WORDS, 0xa5, 32);
    remote.appearance_flags = ANCHOR_APPEARANCE_WEAPON_CHARGE;
    for (i = 0; i < 10; ++i)
        apply(&state, task, object, &remote, 10);
    assert(charge_objects_created == 3);
    assert(state.charge_bank == 0 && s_charge_banks == 1);
    assert(*(unsigned int *)(state.charge_object[0] + 0x2c) == 0x1900012cu);
    assert(*(unsigned int *)((unsigned char *)state.charge_object[1] + 0x2c) == 0x1900025cu);
    assert(*(unsigned int *)((unsigned char *)state.charge_object[2] + 0x2c) == 0x1900037cu);
    assert(*(float *)((unsigned char *)state.charge_object[0] + 0xc) == 25.0f);
    assert(state.charge_scale > 0.099f && state.charge_scale <= 0.101f);
    commands = s_charge_materials;
    assert(commands[1] == 0xe0204c28u && commands[3] == 0xfff5b800u &&
           commands[5] == 0xfffa24ffu);
    assert(((unsigned char *)(s_charge_materials + CHARGE_MATERIAL_WORDS))[0]
           == 0xa5);
    remote.appearance_flags |= ANCHOR_APPEARANCE_WEAPON_CHARGE_FULL;
    D_800C7A72_C8672 = 1;
    apply(&state, task, object, &remote, 10);
    assert((s_charge_materials + CHARGE_MATERIAL_WORDS)[5] == 0xfffa24ffu);
    assert(commands[5] == 0xfffa24ffu);
    assert(*(unsigned short *)((unsigned char *)state.charge_object[0] + 0x16) == 0x1c);
    remote.appearance_flags = 0;
    for (i = 0; i < 8; ++i)
        apply(&state, task, object, &remote, 10);
    assert(state.charge_alpha == 0);
    apply(&state, task, object, &remote, 10);
    assert(state.manager == 0 && s_charge_banks == 0);
    D_800C7A72_C8672 = 0;
    anchor_player_effects_reset(&state, 1);
}

static void test_goemon_charge_active_full_and_release(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, 0);
    unsigned char task[0xf0] = {0}, object[0x98] = {0};
    int i;
    managers_created = 0;
    charge_objects_created = 0;
    effect_children_live = aura_children_live = 0;
    *(float *)(object + 0x1c) = 0.2f;
    *(float *)(object + 0xc) = 20.0f;
    put_pointer(task, 0x18, object);
    remote.appearance_flags = ANCHOR_APPEARANCE_WEAPON_CHARGE;
    apply(&state, task, object, &remote, 10);
    assert(charge_objects_created == 3 && state.charge_alpha == 0xff);
    assert(*(float *)((unsigned char *)state.charge_object[0] + 0xc) ==
           30.0f); /* Goemon native Y offset = 50 * owner scale. */
    remote.appearance_flags |= ANCHOR_APPEARANCE_WEAPON_CHARGE_FULL;
    apply(&state, task, object, &remote, 10);
    assert(*(unsigned short *)((unsigned char *)state.charge_object[0] +
                               0x16) == 0x1c);
    remote.appearance_flags = 0;
    for (i = 0; i < 8; ++i)
        apply(&state, task, object, &remote, 10);
    assert(state.charge_alpha == 0);
    apply(&state, task, object, &remote, 10);
    assert(state.manager == NULL && s_charge_banks == 0);
    anchor_player_effects_reset(&state, 1);

    /* Yae's separate native mode does not request selector 0x11. */
    remote.ch = CHARACTER_YAE;
    remote.appearance_flags = ANCHOR_APPEARANCE_WEAPON_CHARGE;
    charge_objects_created = 0;
    apply(&state, task, object, &remote, 10);
    assert(charge_objects_created == 0 && state.manager == NULL);
    anchor_player_effects_reset(&state, 1);
}

static void test_sudden_aura_rebinds_remote_broad(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, ACTION_SUDDEN_IMPACT_START);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    unsigned char broad[4] = {0};
    int i;
    managers_created = 0;
    effect_children_live = 0;
    aura_children_live = 0;
    put_pointer(task, 0x18, object);
    put_pointer(object, 0x40, broad);
    *(unsigned short *)(object + 0x3c) = 0x120;
    *(float *)(object + 0x28) = 4.0f;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == 1 && state.aura_spawned);
    assert(s_aura_count == 1);
    assert(task[0x90] == 0 && task[0xcc] == ACTION_SUDDEN_IMPACT_START);
    for (i = 0; i < 6; ++i)
        assert(read_task_pointer(aura_object_storage[0][i], 0x40) == broad);
    assert(*(unsigned int *)(successor_object_storage[0] + 0x2c) ==
           0xdeadbeefu);
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == 1);
    remote.action = ACTION_SUDDEN_IMPACT_ACTIVE;
    apply(&state, task, object, &remote, 10);
    assert(task[0xcc] == ACTION_SUDDEN_IMPACT_ACTIVE);
    aura_children_live = 0;
    remote.action = 0;
    apply(&state, task, object, &remote, 10);
    assert(state.manager == 0);
    assert(s_aura_count == 0);
    anchor_player_effects_reset(&state, 1);
}

static void test_aura_material_post_and_flat_boundary(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0, ACTION_SUDDEN_IMPACT_START);
    unsigned char task[0xf0] = {0}, object[0x98] = {0}, broad[4] = {0};
    unsigned char *child, *child_object, *low;
    EffectPostCallback post;
    int index;
    managers_created = 0;
    effect_children_live = aura_children_live = 0;
    D_800C7A72_C8672 = 0;
    memset(s_effect_materials, 0xa5, 64);
    install_native_post = 1;
    native_post_calls = 0;
    put_pointer(task, 0x18, object);
    put_pointer(object, 0x40, broad);
    *(unsigned short *)(object + 0x3c) = 0x120;
    *(float *)(object + 0x28) = 4.0f;
    apply(&state, task, object, &remote, 10);
    assert(state.aura_spawned && state.material_bank == 0);
    index = manager_index(state.manager);
    child = aura_task_storage[index][0];
    child_object = aura_object_storage[index][0];
    low = s_effect_materials;
    assert(anchor_remote_model_pool_contains(child));
    assert(!resident_pointer(child));
    assert(low[0] == NATIVE_SUDDEN_AURA_KIND && low[32] == 0xa5);
    assert(*(unsigned int *)(child_object + 0x30) ==
           ((unsigned int)(unsigned long)low | 0x60000000u));
    post = read_task_pointer(child, 0x10);
    assert(post == material_post_update);
    child[0xa4] = 0x33;
    D_800C7A72_C8672 = 1;
    post(child, child_object);
    assert(native_post_calls == 1 && low[32] == 0x7a && low[0] ==
           NATIVE_SUDDEN_AURA_KIND);
    assert(*(unsigned int *)(child_object + 0x30) ==
           ((unsigned int)(unsigned long)(low + 32) | 0x60000000u));
    anchor_player_effects_suspend(&state);
    assert(*(unsigned int *)(successor_object_storage[index] + 0x2c) ==
           0xdeadbeefu);
    assert(!(successor_object_storage[index][0x64] & 1u));
    anchor_player_effects_set_context(&state, task, &remote, 10);
    assert(state.manager == NULL && s_material_banks == 0);
    install_native_post = 0;
    D_800C7A72_C8672 = 0;
    anchor_player_effects_reset(&state, 1);
}

static void test_material_scene_generation_and_manager_reuse(void)
{
    AnchorPlayerEffectState old_state = {0}, new_state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_YAE,
                                                  ACTION_MERMAID_ENTER);
    unsigned char old_task[0xf0] = {0}, new_task[0xf0] = {0};
    unsigned char old_object[0x98] = {0}, new_object[0x98] = {0};
    void *old_manager;
    int destroys;
    managers_created = 0;
    effect_children_live = 0;
    put_pointer(old_task, 0x18, old_object);
    put_pointer(new_task, 0x18, new_object);
    apply(&old_state, old_task, old_object, &remote, 10);
    assert(old_state.manager && s_material_banks == 1);
    old_manager = old_state.manager;
    destroys = managers_destroyed;
    anchor_player_effects_load_resources(); /* Old scene's native tasks reset. */
    assert(s_material_banks == 0 && s_manager_count == 0);
    anchor_player_effects_set_context(&old_state, old_task, &remote, 10);
    assert(old_state.manager == NULL && managers_destroyed == destroys);
    remote.cid = 3;
    apply(&new_state, new_task, new_object, &remote, 10);
    assert(new_state.manager && new_state.material_bank == 0);
    assert(s_material_banks == 1 && s_manager_count == 1);
    assert(new_state.manager != old_manager);
    anchor_player_effects_reset(&old_state, 0);
    assert(s_material_banks == 1 && s_manager_count == 1);
    anchor_player_effects_reset(&new_state, 1);
    assert(s_material_banks == 0 && s_manager_count == 0);
}

static void test_sudden_aura_first_active_and_retry(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0,
                                                  ACTION_SUDDEN_IMPACT_ACTIVE);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    unsigned char broad[4] = {0};
    unsigned char *saved_charge_textures = s_charge_textures;
    int before = aura_bursts;
    put_pointer(task, 0x18, object);
    put_pointer(object, 0x40, broad);
    *(unsigned short *)(object + 0x3c) = 0x120;
    *(float *)(object + 0x28) = 1.0f;
    aura_children_live = 0;
    s_charge_textures = 0; /* Aura only needs file 0x152, not charge arena. */
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before); /* 0x83 alone is not the gold phase. */

    remote.appearance_flags = ANCHOR_APPEARANCE_SUDDEN_IMPACT;
    resources_ready = 0;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before && state.aura_retry_frames > 0);
    resources_ready = 1;
    s_manager_count = ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before); /* Native manager temporarily unavailable. */
    s_manager_count = 0;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before + 1 && state.aura_spawned);
    assert(task[0xcc] == ACTION_SUDDEN_IMPACT_ACTIVE);
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before + 1);
    aura_children_live = 0;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before + 1); /* Retired children do not retrigger. */
    remote.action = 0;
    remote.appearance_flags = 0;
    apply(&state, task, object, &remote, 10);
    remote.action = ACTION_SUDDEN_IMPACT_ACTIVE;
    remote.appearance_flags = ANCHOR_APPEARANCE_SUDDEN_IMPACT;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before + 2);
    anchor_player_effects_reset(&state, 1);
    s_charge_textures = saved_charge_textures;
}

static void test_sudden_aura_retry_expires(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(0,
                                                  ACTION_SUDDEN_IMPACT_ACTIVE);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    unsigned char broad[4] = {0};
    int before = aura_bursts;
    int i;
    put_pointer(task, 0x18, object);
    put_pointer(object, 0x40, broad);
    *(unsigned short *)(object + 0x3c) = 0x120;
    remote.appearance_flags = ANCHOR_APPEARANCE_SUDDEN_IMPACT;
    resources_ready = 0;
    for (i = 0; i < TRANSFORM_RETRY_FRAMES; ++i)
        apply(&state, task, object, &remote, 10);
    assert(state.aura_retry_frames == 0 && aura_bursts == before);
    resources_ready = 1;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before);
    remote.action = 0;
    remote.appearance_flags = 0;
    apply(&state, task, object, &remote, 10);
    remote.action = ACTION_SUDDEN_IMPACT_ACTIVE;
    remote.appearance_flags = ANCHOR_APPEARANCE_SUDDEN_IMPACT;
    apply(&state, task, object, &remote, 10);
    assert(aura_bursts == before + 1);
    anchor_player_effects_reset(&state, 1);
}

static void test_sudden_aura_pool_is_bounded(void)
{
    AnchorPlayerEffectState states[ANCHOR_REMOTE_AURA_LIMIT + 1] = {0};
    unsigned char tasks[ANCHOR_REMOTE_AURA_LIMIT + 1][0xf0] = {{0}};
    unsigned char objects[ANCHOR_REMOTE_AURA_LIMIT + 1][0x98] = {{0}};
    unsigned char broad[4] = {0};
    AnchorPlayerModelRemote remote = remote_state(0, ACTION_SUDDEN_IMPACT_START);
    int i, before = aura_bursts;
    managers_created = 0;
    effect_children_live = 0;
    aura_children_live = 1;
    for (i = 0; i < ANCHOR_REMOTE_AURA_LIMIT + 1; ++i)
    {
        remote.cid = i + 2;
        put_pointer(tasks[i], 0x18, objects[i]);
        put_pointer(objects[i], 0x40, broad);
        *(unsigned short *)(objects[i] + 0x3c) = 0x120;
        *(float *)(objects[i] + 0x28) = 4.0f;
        apply(&states[i], tasks[i], objects[i], &remote, 10);
    }
    assert(aura_bursts - before == ANCHOR_REMOTE_AURA_LIMIT);
    assert(s_aura_count == ANCHOR_REMOTE_AURA_LIMIT);
    anchor_player_effects_reset(&states[0], 1);
    remote.cid = ANCHOR_REMOTE_AURA_LIMIT + 2;
    apply(&states[ANCHOR_REMOTE_AURA_LIMIT],
          tasks[ANCHOR_REMOTE_AURA_LIMIT],
          objects[ANCHOR_REMOTE_AURA_LIMIT], &remote, 10);
    assert(aura_bursts - before == ANCHOR_REMOTE_AURA_LIMIT + 1);
    for (i = 1; i < ANCHOR_REMOTE_AURA_LIMIT + 1; ++i)
        anchor_player_effects_reset(&states[i], 1);
    assert(s_aura_count == 0);
}

static void test_hidden_owner_defers_manager_cleanup(void)
{
    AnchorPlayerEffectState state = {0};
    AnchorPlayerModelRemote remote = remote_state(CHARACTER_EBISUMARU, 0);
    unsigned char task[0xf0] = {0};
    unsigned char object[0x98] = {0};
    int before = managers_destroyed;
    managers_created = 0;
    charge_objects_created = 0;
    memset(manager_storage, 0, sizeof(manager_storage));
    *(float *)(object + 0x1c) = 0.1f;
    put_pointer(task, 0x18, object);
    remote.appearance_flags = ANCHOR_APPEARANCE_WEAPON_CHARGE;
    apply(&state, task, object, &remote, 10);
    assert(state.manager && state.charge_object[0]);
    anchor_player_effects_suspend(&state);
    assert(state.aura_drop_pending);
    assert(*(unsigned int *)((unsigned char *)state.charge_object[0] + 0x2c) == 0);
    assert(managers_destroyed == before); /* No scheduler deletion in callback. */
    anchor_player_effects_set_context(&state, task, &remote, 10);
    assert(managers_destroyed == before + 1);
    assert(state.manager == NULL && s_charge_banks == 0);
    anchor_player_effects_reset(&state, 1);
}

int main(void)
{
    anchor_player_effects_load_resources();
    test_purple_action_edges_and_lifecycle();
    test_mermaid_both_transitions_and_reconnect();
    test_purple_retry_expires_and_rebind_drops_pending();
    test_bounded_manager_pool();
    test_slot_reuse_drops_old_visual_owner();
    test_charge_models_and_release();
    test_goemon_charge_active_full_and_release();
    test_sudden_aura_rebinds_remote_broad();
    test_aura_material_post_and_flat_boundary();
    test_material_scene_generation_and_manager_reuse();
    test_sudden_aura_first_active_and_retry();
    test_sudden_aura_retry_expires();
    test_sudden_aura_pool_is_bounded();
    test_hidden_owner_defers_manager_cleanup();
    return 0;
}
