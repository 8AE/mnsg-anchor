#define ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/world/anchor_castle_return_sign.c"

unsigned short D_800C7AB2;
void *D_802287BC_5E3C8C[0x8c];
CastleSignRoomMetadata *D_80231300_5EC7D0[800];
float D_80239AFC_5F4FCC = 0.1f;
void *D_8016DAB4_16E6B4;
void *D_801FC604_5B8514;

static CastleSignRoomMetadata room_metadata;
static int owner_storage;
static unsigned char task_storage[0x100];
static unsigned char object_storage[0x90];
static unsigned char other_task[0x100];
static unsigned char player_storage[0xe0];
static unsigned char other_player[0xe0];
static int wave_ready, owner_ready, allocation_ready, reenter;
static int wave_checks, owner_calls, allocation_calls, initialize_calls;
static int prompt_ready, prompt_active, prompt_begins, prompt_cancels;
static int warp_ready, warp_calls, boss_invite_active;
static int scripted_input;
static void (*scheduled_callback)(void *, void *);
static int callback_sets;
static int native_scenario_starts;
static int native_scenario_player;
static AnchorDialogResult prompt_result;

unsigned char *anchor_castle_return_sign_host_task_bytes(int actor)
{
    assert(actor == 0x123456);
    return task_storage;
}
int anchor_boss_invite_world_can_prompt(void)
{
    return prompt_ready && !scripted_input;
}
int anchor_boss_invites_active(void) { return boss_invite_active; }
int anchor_dialog_begin_castle_return(void)
{
    if (!prompt_ready || prompt_active)
        return 0;
    prompt_active = 1;
    prompt_result = ANCHOR_DIALOG_PENDING;
    ++prompt_begins;
    return 1;
}
AnchorDialogResult anchor_dialog_poll_for(AnchorDialogOwner owner)
{
    AnchorDialogResult result;
    assert(owner == ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
    result = prompt_result;
    if (result != ANCHOR_DIALOG_PENDING) {
        prompt_active = 0;
        prompt_result = ANCHOR_DIALOG_IDLE;
    }
    return result;
}
void anchor_dialog_cancel_for(AnchorDialogOwner owner)
{
    assert(owner == ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
    ++prompt_cancels;
    prompt_active = 0;
    prompt_result = ANCHOR_DIALOG_CANCELLED;
}
int anchor_boss_invite_world_transfer_to(unsigned short room,
                                         short x, short y, short z)
{
    assert(room == 0x14c && x == 0 && y == -46 && z == -576);
    if (scripted_input)
        return 0; /* Native can_prompt rejects its accepted-talk lock. */
    ++warp_calls;
    return warp_ready;
}
int func_800141C4_14DC4(unsigned int file_id)
{
    assert(file_id == 0x3bc);
    ++wave_checks;
    return wave_ready ? 1 : -1;
}
int *func_80219CA0_5D5170(int *task, unsigned char actor_type)
{
    assert(!task && actor_type == 7);
    ++owner_calls;
    return owner_ready ? &owner_storage : 0;
}
int func_8003555C_3615C(int *owner, void *entry, int arg2, int flags,
                         int x, int y, int z, short pitch, short yaw,
                         short roll, float sx, float sy, float sz,
                         short arg13, short arg14, short model_id)
{
    assert(owner == &owner_storage && entry == (void *)0x1234);
    assert(arg2 == 0 && flags == (int)0xC006D920u);
    assert(x == 0 && y == 0 && z == 0);
    assert(pitch == 0 && yaw == 0 && roll == 0);
    assert(sx == D_80239AFC_5F4FCC && sy == sx && sz == sx);
    assert(arg13 == 0 && arg14 == 0 && model_id == 0);
    ++allocation_calls;
    return allocation_ready ? 0x123456 : 0;
}
void func_80218A54_5D3F24(int actor, CastleSignInstance *instance)
{
    CastleSignDefinition *definition;
    assert(actor == 0x123456 && instance == &s_instance);
    assert(instance->x == 38 && instance->y == -35 && instance->z == -136);
    assert(instance->pitch == 0 && instance->yaw == -32768 &&
           instance->roll == 0);
    assert(instance->spawned == 0);
    definition = instance->definition;
    assert(definition == &s_definition && definition->actor_id == 0x8b);
    assert(definition->params == 0);
    assert(definition->data[0] == 0 && definition->data[1] == 0 &&
           definition->data[2] == 0);
    memset(task_storage, 0, sizeof(task_storage));
    memset(object_storage, 0, sizeof(object_storage));
    *(void **)(task_storage + 0x18) = object_storage;
    *(unsigned short *)(task_storage + 0x5e) = 0x8b;
    task_storage[0x74] = (unsigned char)(initialize_calls + 1);
    *(unsigned int *)(task_storage + 0x68) = 0x400u;
    *(float *)(object_storage + 0x0c) = -50.0f; /* Native ground adjustment. */
    /* Native 80218A54 copies instance yaw +0x08 to object yaw +0x16. */
    *(unsigned short *)(object_storage + 0x16) = (unsigned short)instance->yaw;
    *(float *)(task_storage + 0x78) = 3.0f;
    ++initialize_calls;
    instance->spawned = 1;
    if (reenter)
        anchor_castle_return_sign_finish_actor_stage();
}
void func_8003521C_35E1C(void (*callback)(void *, void *))
{
    assert(D_8016DAB4_16E6B4 == task_storage ||
           D_8016DAB4_16E6B4 == other_task);
    scheduled_callback = callback;
    ++callback_sets;
}
void func_8003D388_3DF88(unsigned short scenario, int player)
{
    assert(scenario == 0x13f && (player == 1 || player == 2));
    ++native_scenario_starts;
    native_scenario_player = player;
}
static void native_idle(void *task, void *object)
{
    (void)task;
    (void)object;
}
void func_802213A4_5DC874(void *task, void *object)
{
    unsigned char *bytes = task;
    (void)object;
    if (prompt_active)
        return; /* Native 8003F1D8 still sees a running scenario. */
    *(unsigned int *)(bytes + 0x68) =
        (*(unsigned int *)(bytes + 0x68) & ~0x100u) | 0x4400u;
    func_8003521C_35E1C(*(void (**)(void *, void *))(bytes + 0xb4));
}

static void reset_test(void)
{
    memset(D_802287BC_5E3C8C, 0, sizeof(D_802287BC_5E3C8C));
    memset(D_80231300_5EC7D0, 0, sizeof(D_80231300_5EC7D0));
    memset(&room_metadata, 0, sizeof(room_metadata));
    room_metadata.actor_data_file_id = 0x3bc;
    D_80231300_5EC7D0[0xa8] = &room_metadata;
    D_802287BC_5E3C8C[0x8b] = (void *)0x1234;
    D_800C7AB2 = 0xa8;
    D_8016DAB4_16E6B4 = 0;
    wave_ready = owner_ready = allocation_ready = 1;
    reenter = 0;
    wave_checks = owner_calls = allocation_calls = initialize_calls = 0;
    prompt_ready = warp_ready = 1;
    prompt_active = prompt_begins = prompt_cancels = warp_calls = 0;
    boss_invite_active = 0;
    scripted_input = 0;
    scheduled_callback = 0;
    callback_sets = 0;
    native_scenario_starts = 0;
    native_scenario_player = 0;
    memset(player_storage, 0, sizeof(player_storage));
    memset(other_player, 0, sizeof(other_player));
    D_801FC604_5B8514 = player_storage;
    prompt_result = ANCHOR_DIALOG_IDLE;
    anchor_castle_return_sign_begin_load();
}
static void accepted_talk(unsigned char *player)
{
    /* Live sign contact arrives with bit 0x100 already set and the object
     * talk bits absent. Native 212F0 has saved its idle callback at +B4. */
    *(unsigned int *)(task_storage + 0x68) = 0x1700u;
    object_storage[0x7c] = 0x02;
    *(unsigned char **)(task_storage + 0xec) = player;
    *(void (**)(void *, void *))(task_storage + 0xb4) = native_idle;
    player[0xcd] = 3;
    scripted_input = 1;
    D_8016DAB4_16E6B4 = task_storage;
    func_80221338_5DC808(task_storage, object_storage);
    assert(player[0xcd] == 4);
    assert(object_storage[0x7c] == 0x02);
    assert(scheduled_callback == func_802213A4_5DC874);
}
static void release_native_player(unsigned char *player)
{
    /* Model 801E1B60's externally scheduled release prerequisites. */
    assert(player[0xcd] == 4 && !prompt_active);
    assert((*(unsigned int *)(task_storage + 0x68) & 0x100u) == 0);
    player[0xcd] = 0;
    scripted_input = 0;
    assert(anchor_boss_invite_world_can_prompt() == prompt_ready);
}

int main(void)
{
    reset_test();
    D_800C7AB2 = 0xa9;
    anchor_castle_return_sign_finish_actor_stage();
    assert(!wave_checks && !initialize_calls);

    reset_test();
    D_80231300_5EC7D0[0xa8] = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(!wave_checks && !initialize_calls);

    reset_test();
    wave_ready = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(wave_checks == 1 && !owner_calls);
    wave_ready = 1;
    D_802287BC_5E3C8C[0x8b] = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(!owner_calls);

    reset_test();
    owner_ready = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(owner_calls == 1 && !allocation_calls);
    owner_ready = 1;
    allocation_ready = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(allocation_calls == 1 && !initialize_calls);
    allocation_ready = 1;
    reenter = 1;
    anchor_castle_return_sign_finish_actor_stage();
    assert(allocation_calls == 2 && initialize_calls == 1);
    anchor_castle_return_sign_finish_actor_stage();
    assert(allocation_calls == 2 && initialize_calls == 1);

    anchor_castle_return_sign_begin_load(); /* Same room, new visit. */
    assert(!s_instance.spawned);
    reenter = 0;
    anchor_castle_return_sign_finish_actor_stage();
    assert(allocation_calls == 3 && initialize_calls == 2);
    assert(anchor_castle_return_sign_owns_task(task_storage));
    assert(!anchor_castle_return_sign_owns_task(other_task));

    /* The constructor return adjusts placement after native ground probing,
     * while retaining the native idle callback and collision handoff. */
    D_8016DAB4_16E6B4 = other_task;
    anchor_castle_return_sign_after_constructor();
    assert(!scheduled_callback && !callback_sets);
    D_8016DAB4_16E6B4 = task_storage;
    anchor_castle_return_sign_after_constructor();
    assert(!scheduled_callback && !callback_sets);
    assert(*(float *)(object_storage + 0x08) == 38.0f);
    assert(*(float *)(object_storage + 0x0c) == -35.0f);
    assert(*(float *)(object_storage + 0x10) == -136.0f);
    assert(*(unsigned short *)(object_storage + 0x16) == 0x8000u);
    assert(*(float *)(task_storage + 0x78) == 0.0f);
    assert(*(float *)(task_storage + 0x7c) == 0.0f);
    assert(*(float *)(task_storage + 0x80) == 0.0f);

    /* The native first-player contact sets +0x100 directly; the object never
     * receives the old 0x08 talk bit. No prompt starts until player CD=3. */
    *(unsigned int *)(task_storage + 0x68) = 0x1700u;
    *(unsigned char **)(task_storage + 0xec) = player_storage;
    player_storage[0xcd] = 2;
    func_80221338_5DC808(task_storage, object_storage);
    assert(!prompt_begins && !callback_sets && player_storage[0xcd] == 2);

    /* A failed private begin must still advance to CD=4 and schedule native
     * completion so the player can leave the talk pose. */
    prompt_ready = 0;
    accepted_talk(player_storage);
    assert(!prompt_begins && !native_scenario_starts);
    scheduled_callback(task_storage, object_storage);
    assert((*(unsigned int *)(task_storage + 0x68) & 0x100u) == 0);
    assert(scheduled_callback == native_idle);
    assert(callback_sets == 2);
    release_native_player(player_storage);

    /* A second-controller contact releases normally without opening the P1
     * travel prompt or the native Kai Highway scenario. */
    prompt_ready = 1;
    accepted_talk(other_player);
    assert(!prompt_begins && !native_scenario_starts);
    scheduled_callback(task_storage, object_storage);
    assert(scheduled_callback == native_idle);
    release_native_player(other_player);

    boss_invite_active = 1;
    accepted_talk(player_storage);
    assert(!prompt_begins);
    scheduled_callback(task_storage, object_storage);
    release_native_player(player_storage);
    boss_invite_active = 0;
    accepted_talk(player_storage);
    assert(prompt_begins == 1 && prompt_active);
    scheduled_callback(task_storage, object_storage); /* Wait for VM close. */
    assert((*(unsigned int *)(task_storage + 0x68) & 0x100u) != 0);
    anchor_castle_return_sign_frame();
    assert(!warp_calls);
    prompt_result = ANCHOR_DIALOG_NO;
    anchor_castle_return_sign_frame();
    assert(!warp_calls);
    scheduled_callback(task_storage, object_storage);
    assert(scheduled_callback == native_idle);
    release_native_player(player_storage);
    accepted_talk(player_storage);
    assert(prompt_begins == 2);
    prompt_result = ANCHOR_DIALOG_YES;
    warp_ready = 0;
    anchor_castle_return_sign_frame();
    assert(warp_calls == 0 && s_warp_pending);
    scheduled_callback(task_storage, object_storage);
    release_native_player(player_storage);
    anchor_castle_return_sign_frame();
    assert(warp_calls == 1 && s_warp_pending);
    warp_ready = 1;
    anchor_castle_return_sign_frame();
    assert(warp_calls == 2 && !s_warp_pending);
    assert(scheduled_callback == native_idle && !native_scenario_starts);

    /* Pool reuse and room departure invalidate the tracked native task. */
    task_storage[0x74]++;
    assert(!anchor_castle_return_sign_owns_task(task_storage));
    *(unsigned char **)(task_storage + 0xec) = player_storage;
    *(unsigned short *)(task_storage + 0xa4) = 0x13f;
    player_storage[0xcd] = 3;
    func_80221338_5DC808(task_storage, object_storage);
    assert(native_scenario_starts == 1 && native_scenario_player == 1 &&
           player_storage[0xcd] == 4);
    *(unsigned char **)(task_storage + 0xec) = other_player;
    other_player[0xcd] = 3;
    func_80221338_5DC808(task_storage, object_storage);
    assert(native_scenario_starts == 2 && native_scenario_player == 2 &&
           other_player[0xcd] == 4);
    assert(!anchor_castle_return_sign_owns_task(task_storage));

    reset_test();
    anchor_castle_return_sign_finish_actor_stage();
    D_8016DAB4_16E6B4 = task_storage;
    anchor_castle_return_sign_after_constructor();
    accepted_talk(player_storage);
    D_800C7AB2 = 0xa9;
    anchor_castle_return_sign_frame();
    assert(prompt_cancels == 1 && !s_prompt_active);
    D_8016DAB4_16E6B4 = task_storage;
    scheduled_callback(task_storage, object_storage);
    assert((*(unsigned int *)(task_storage + 0x68) & 0x100u) == 0);
    release_native_player(player_storage);

    puts("castle sign native contact, private dialog, cleanup, and warp guards passed");
    return 0;
}
