#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Host tests include the production implementation directly so they can set
 * up its private renderer slots. Strip recomp-specific section attributes,
 * which are only valid for the MIPS mod build. */
#define __MODDING_H__
#define RECOMP_IMPORT(mod, declaration) declaration
#define RECOMP_EXPORT
#define RECOMP_PATCH
#define RECOMP_FORCE_PATCH
#define RECOMP_DECLARE_EVENT(declaration) declaration
#define RECOMP_CALLBACK(mod, event)
#define RECOMP_HOOK(function_name)
#define RECOMP_HOOK_RETURN(function_name)

#define __RECOMPUTILS_H__
void *recomp_alloc(unsigned long size);
void recomp_free(void *memory);
int recomp_printf(const char *format, ...);
unsigned char *recomp_get_mod_file_path(void);

#include "../src/anchor_player_models.c"

unsigned short D_800C7AB2;
unsigned short D_800C7A78;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_801FC660_5B8570[4];
unsigned short D_801FC668_5B8578[4];
unsigned char *D_80203F34_5BFE44[4];
unsigned char D_80203FF0_5BFF00[4];
unsigned short D_80203FF8_5BFF08[4];
unsigned short D_80204020_5BFF30[4];
void *D_80204048_5BFF58[4];

typedef union TestTaskStorage
{
    void *alignment;
    unsigned char bytes[128];
} TestTaskStorage;

typedef struct TestLinkedTask
{
    TestTaskStorage task_storage;
    TestTaskStorage backlink_storage;
    void *task;
    void *backlink;
} TestLinkedTask;

static TestLinkedTask owner_task;
static TestLinkedTask remote_task;
static TestLinkedTask owned_child_task;
static TestTaskStorage local_object_storage;
static RemoteModelSlot sound_slot;

/* remote_model_task_update is retained because the production linkage guard
 * compares its address. These inert definitions satisfy the callback's dead
 * host-only dependency graph; the regression never executes that callback. */
void *recomp_alloc(unsigned long size)
{
    (void)size;
    return 0;
}

int anchor_is_connected(void)
{
    return 0;
}

int anchor_dialog_world_paused(void)
{
    return 0;
}

int anchor_remote_collision_is_scripted(void)
{
    return 0;
}

int anchor_collision_append_enemies(AnchorCollisionBody **bodies, int *capacity,
                                    int count)
{
    (void)bodies;
    (void)capacity;
    return count;
}

int anchor_collision_move_actors(const AnchorCollisionBody *moving,
                                 const AnchorCollisionVec3 *target,
                                 const AnchorCollisionBody *actors, int count,
                                 AnchorCollisionVec3 *out)
{
    (void)moving;
    (void)actors;
    (void)count;
    *out = *target;
    return 1;
}

void anchor_remote_animation_reset(AnchorRemoteAnimationState *state)
{
    memset(state, 0, sizeof(*state));
}

void anchor_remote_animation_step(AnchorRemoteAnimationState *state,
                                  const AnchorRemoteAnimationInput *input,
                                  AnchorRemoteAnimationOutput *output)
{
    (void)state;
    (void)input;
    memset(output, 0, sizeof(*output));
}

void anchor_remote_appearance_apply_hurt(void *object, int appearance_flags,
                                         unsigned short native_frame)
{
    (void)object;
    (void)appearance_flags;
    (void)native_frame;
}

unsigned char *func_800145B4_151B4(unsigned int resource_id, void *dst)
{
    (void)resource_id;
    return (unsigned char *)dst;
}

int func_80014698_15298(unsigned int resource_id, void *rom_address_out)
{
    (void)resource_id;
    (void)rom_address_out;
    return 0;
}

float func_8001B5AC_1C1AC(void *object)
{
    (void)object;
    return 1.0f;
}

int func_8001C3E0_1CFE0(void *object, unsigned int model_ptr,
                        const void *replacement_table)
{
    (void)object;
    (void)model_ptr;
    (void)replacement_table;
    return 0;
}

int mnsg_array_reserve(void **data, int *capacity, int needed,
                       unsigned int element_size)
{
    (void)data;
    (void)capacity;
    (void)needed;
    (void)element_size;
    return 0;
}

int mnsg_texture_cache_find(const MnsgTextureCacheEntry *entries, int count,
                            unsigned int resource)
{
    (void)entries;
    (void)count;
    (void)resource;
    return -1;
}

int mnsg_texture_cache_victim(const MnsgTextureCacheEntry *entries, int count,
                              unsigned short frame)
{
    (void)entries;
    (void)count;
    (void)frame;
    return -1;
}

void mnsg_texture_cache_release(MnsgTextureCacheEntry *entry,
                                unsigned short frame)
{
    (void)entry;
    (void)frame;
}

int anchor_remote_model_pool_contains(const void *pointer)
{
    uintptr_t p = (uintptr_t)pointer;
    uintptr_t owner_begin = (uintptr_t)owner_task.task_storage.bytes;
    uintptr_t owner_end = owner_begin + sizeof(owner_task.task_storage.bytes);
    uintptr_t owner_link_begin = (uintptr_t)owner_task.backlink_storage.bytes;
    uintptr_t owner_link_end = owner_link_begin + sizeof(owner_task.backlink_storage.bytes);
    uintptr_t remote_begin = (uintptr_t)remote_task.task_storage.bytes;
    uintptr_t remote_end = remote_begin + sizeof(remote_task.task_storage.bytes);
    uintptr_t remote_link_begin = (uintptr_t)remote_task.backlink_storage.bytes;
    uintptr_t remote_link_end = remote_link_begin + sizeof(remote_task.backlink_storage.bytes);
    uintptr_t child_begin = (uintptr_t)owned_child_task.task_storage.bytes;
    uintptr_t child_end = child_begin + sizeof(owned_child_task.task_storage.bytes);
    uintptr_t child_link_begin = (uintptr_t)owned_child_task.backlink_storage.bytes;
    uintptr_t child_link_end = child_link_begin + sizeof(owned_child_task.backlink_storage.bytes);
    uintptr_t object_begin = (uintptr_t)local_object_storage.bytes;
    uintptr_t object_end = object_begin + sizeof(local_object_storage.bytes);

    return (p >= owner_begin && p < owner_end) ||
           (p >= owner_link_begin && p < owner_link_end) ||
           (p >= remote_begin && p < remote_end) ||
           (p >= remote_link_begin && p < remote_link_end) ||
           (p >= child_begin && p < child_end) ||
           (p >= child_link_begin && p < child_link_end) ||
           (p >= object_begin && p < object_end);
}

static void store_pointer(void *base, unsigned int offset, const void *value)
{
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static void link_task(TestLinkedTask *fixture, const void *callback)
{
    memset(fixture, 0, sizeof(*fixture));

    /* Shifting the synthetic task by four bytes keeps its native +0x04 and
     * +0x0c pointer fields aligned on a 64-bit host. */
    fixture->task = fixture->task_storage.bytes + 4;
    fixture->backlink = fixture->backlink_storage.bytes;
    store_pointer(fixture->task, 0x04, fixture->backlink);
    store_pointer(fixture->backlink, 0, fixture->task);
    store_pointer(fixture->task, 0x0c, callback);
}

static void reset_sound_slot(void)
{
    memset(&sound_slot, 0, sizeof(sound_slot));
    memset(&local_object_storage, 0, sizeof(local_object_storage));
    link_task(&owner_task, 0);
    link_task(&remote_task, remote_model_task_update);
    link_task(&owned_child_task, 0);

    D_800C7AB2 = 12;
    D_801FC604_5B8514 = owner_task.task;
    D_801FC60C_5B851C = local_object_storage.bytes;
    store_pointer(owner_task.task, 0x18, D_801FC60C_5B851C);
    store_pointer(owned_child_task.task, 0x5c, owner_task.task);
    store_pointer(remote_task.task, 0x5c, owner_task.task);
    s_owner_task = owner_task.task;
    s_slots = &sound_slot;
    s_slot_capacity = 1;

    sound_slot.active = 1;
    sound_slot.cid = 37;
    sound_slot.pending_valid = 1;
    sound_slot.pending_room = D_800C7AB2;
    sound_slot.pending_remote.interaction_session = 901;
    sound_slot.pending_remote.player_epoch = 14;
    sound_slot.pending_remote.x = 10.5f;
    sound_slot.pending_remote.y = -20.25f;
    sound_slot.pending_remote.z = 30.75f;
    sound_slot.collision_body.position.x = 100.5f;
    sound_slot.collision_body.position.y = -200.25f;
    sound_slot.collision_body.position.z = 300.75f;
    sound_slot.task = remote_task.task;
}

static void assert_position(float expected_x, float expected_y, float expected_z)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    assert(anchor_player_models_get_sound_position(37, 901, 14, &x, &y, &z));
    assert(x == expected_x);
    assert(y == expected_y);
    assert(z == expected_z);
}

static void test_scripted_position_does_not_require_collision(void)
{
    reset_sound_slot();
    sound_slot.collision_ready = 0;
    assert_position(10.5f, -20.25f, 30.75f);
}

static void test_collision_position_wins_when_ready(void)
{
    reset_sound_slot();
    sound_slot.collision_ready = 1;
    assert_position(100.5f, -200.25f, 300.75f);
}

static void test_identity_mismatch_is_rejected_without_writing(void)
{
    float x = 1.0f;
    float y = 2.0f;
    float z = 3.0f;

    reset_sound_slot();
    assert(!anchor_player_models_get_sound_position(99, 901, 14, &x, &y, &z));
    assert(!anchor_player_models_get_sound_position(37, 902, 14, &x, &y, &z));
    assert(!anchor_player_models_get_sound_position(37, 901, 15, &x, &y, &z));
    assert(x == 1.0f && y == 2.0f && z == 3.0f);
}

static void test_stale_or_unlinked_tasks_are_rejected(void)
{
    float x;
    float y;
    float z;

    reset_sound_slot();
    D_801FC604_5B8514 = remote_task.task;
    assert(!anchor_player_models_get_sound_position(37, 901, 14, &x, &y, &z));

    reset_sound_slot();
    store_pointer(owner_task.backlink, 0, 0);
    assert(!anchor_player_models_get_sound_position(37, 901, 14, &x, &y, &z));

    reset_sound_slot();
    store_pointer(remote_task.backlink, 0, 0);
    assert(!anchor_player_models_get_sound_position(37, 901, 14, &x, &y, &z));

    reset_sound_slot();
    store_pointer(remote_task.task, 0x0c, 0);
    assert(!anchor_player_models_get_sound_position(37, 901, 14, &x, &y, &z));
}

static void test_local_sound_task_ownership(void)
{
    reset_sound_slot();
    assert(anchor_player_models_is_local_sound_task(owner_task.task));
    assert(anchor_player_models_is_local_sound_task(owned_child_task.task));

    /* The remote renderer is also parented to the real player, but its
     * synthetic task must never be attributed as a local sound source. */
    assert(!anchor_player_models_is_local_sound_task(remote_task.task));

    store_pointer(owned_child_task.task, 0x5c, remote_task.task);
    assert(!anchor_player_models_is_local_sound_task(owned_child_task.task));

    reset_sound_slot();
    store_pointer(owned_child_task.backlink, 0, 0);
    assert(!anchor_player_models_is_local_sound_task(owned_child_task.task));

    reset_sound_slot();
    D_801FC60C_5B851C = 0;
    assert(!anchor_player_models_is_local_sound_task(owner_task.task));
}

int main(void)
{
    test_scripted_position_does_not_require_collision();
    test_collision_position_wins_when_ready();
    test_identity_mismatch_is_rejected_without_writing();
    test_stale_or_unlinked_tasks_are_rejected();
    test_local_sound_task_ownership();
    return 0;
}
