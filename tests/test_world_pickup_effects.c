#include <assert.h>
#include <stddef.h>
#include <string.h>

#define ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
#include "../src/world/anchor_world_pickup_effects.c"

static unsigned char root_task[0xf0];
static unsigned char child_tasks[10][0xf0];
static unsigned char child_objects[10][0xa0];
static unsigned char local_object[0xa0];
static const void *encoded[256];
static unsigned int encoded_count;
static int helper_calls, retire_calls;
static int next_child;
void *D_8015CCBC = root_task;
short D_800C7A72_C8672;
unsigned short D_800C7AB2 = 10;

unsigned int pickup_test_encode_pointer(const void *pointer)
{
    unsigned int i;
    if (!pointer)
        return 0;
    for (i = 1; i <= encoded_count; ++i)
        if (encoded[i] == pointer)
            return i;
    assert(encoded_count + 1 < 256);
    encoded[++encoded_count] = pointer;
    return encoded_count;
}

void *pickup_test_decode_pointer(unsigned int value)
{
    assert(value <= encoded_count);
    return (void *)encoded[value];
}

static void put_pointer(void *record, unsigned int offset, const void *pointer)
{
    unsigned int encoded_pointer = pickup_test_encode_pointer(pointer);
    memcpy((unsigned char *)record + offset, &encoded_pointer, 4);
}

int anchor_remote_model_pool_contains(const void *pointer)
{
    unsigned long value = (unsigned long)pointer;
    return (value >= (unsigned long)child_tasks &&
            value < (unsigned long)child_tasks + sizeof(child_tasks)) ||
           (value >= (unsigned long)child_objects &&
            value < (unsigned long)child_objects + sizeof(child_objects));
}

void func_80211EF8_5CD3C8(void *task, void *object)
{ (void)task; (void)object; }
void func_80211F70_5CD440(void *task, void *object)
{ (void)task; (void)object; }
void func_80212020_5CD4F0(void *task, void *object)
{ (void)task; (void)object; }
void func_80218F30_5D4400(void *task) { (void)task; }

static void link_child(int index)
{
    unsigned char *child = child_tasks[index];
    unsigned char *object = child_objects[index];
    void *first = read_pointer(root_task, 0);
    memset(child, 0, 0xf0);
    memset(object, 0, 0xa0);
    put_pointer(child, 0, first);
    put_pointer(child, 4, root_task);
    put_pointer(child, 0x0c, func_80211EF8_5CD3C8);
    put_pointer(child, 0x10, func_80218F30_5D4400);
    put_pointer(child, 0x18, object);
    *(unsigned short *)(child + 0x20) = 1;
    if (first)
        put_pointer(first, 4, child);
    put_pointer(root_task, 0, child);
}

void *func_8021804C_5D351C(void *actor, int variant)
{
    int index = next_child++;
    assert(actor && variant == 0 && index < 10);
    ++helper_calls;
    link_child(index);
    return child_tasks[index];
}

void func_80034EF8_35AF8(void *task)
{
    void *previous = read_pointer(task, 4);
    void *next = read_pointer(task, 0);
    assert(previous && read_pointer(previous, 0) == task);
    put_pointer(previous, 0, next);
    if (next)
        put_pointer(next, 4, previous);
    put_pointer(task, 0, 0);
    put_pointer(task, 4, 0);
    ++retire_calls;
}

static void setup(void)
{
    memset(root_task, 0, sizeof(root_task));
    memset(child_tasks, 0, sizeof(child_tasks));
    memset(child_objects, 0, sizeof(child_objects));
    memset(local_object, 0, sizeof(local_object));
    memset(encoded, 0, sizeof(encoded));
    encoded_count = 0;
    next_child = helper_calls = retire_calls = 0;
    D_800C7A72_C8672 = 0;
    D_800C7AB2 = 10;
    anchor_world_pickup_effects_load_resources();
}

static void test_expanded_child_material_tracks_native_fade_and_bank(void)
{
    unsigned char actor[0xf0] = {0};
    unsigned char *child, *object, *bank0, *bank1;
    int i;
    setup();
    memset(s_material_arena, 0xa5, PICKUP_ARENA_BYTES);
    anchor_world_pickup_effects_spawn(actor, 10);
    assert(helper_calls == 1 && s_visuals[0].task == child_tasks[0]);
    child = child_tasks[0];
    object = child_objects[0];
    assert(anchor_remote_model_pool_contains(object));
    assert(!low_graphics_pointer(object));
    assert(read_pointer(child, 0x10) == (void *)func_80218F30_5D4400);
    anchor_world_pickup_effects_before_draw(object);
    assert(*(unsigned int *)(object + 0x30) == 0); /* Init pending. */
    put_pointer(child, 0x0c, func_80211F70_5CD440);
    for (i = 0; i < 24; ++i)
        object[0x80 + i] = (unsigned char)(i + 1);
    anchor_world_pickup_effects_before_draw(object);
    bank0 = s_material_arena;
    bank1 = bank0 + PICKUP_MATERIAL_BYTES;
    assert(!memcmp(bank0, object + 0x80, 24));
    assert(bank1[0] == 0xa5);
    assert(*(unsigned int *)(object + 0x30) ==
           ((unsigned int)(unsigned long)bank0 | 0x60000000u));
    put_pointer(child, 0x0c, func_80212020_5CD4F0);
    object[0x80 + 15] = 0x20; /* Native alpha changed this tick. */
    D_800C7A72_C8672 = 1;
    anchor_world_pickup_effects_before_draw(object);
    assert(bank1[15] == 0x20 && bank0[15] == 16);
    assert(*(unsigned int *)(object + 0x30) ==
           ((unsigned int)(unsigned long)bank1 | 0x60000000u));
    anchor_world_pickup_effects_before_draw(local_object);
    assert(*(unsigned int *)(local_object + 0x30) == 0);
    anchor_world_pickup_effects_reset();
    assert(retire_calls == 1 && !s_visuals[0].task);
}

static void test_room_exit_and_reused_task_do_not_rebind(void)
{
    unsigned char actor[0xf0] = {0};
    unsigned char *child, *object;
    setup();
    anchor_world_pickup_effects_spawn(actor, 10);
    child = child_tasks[0];
    object = child_objects[0];
    put_pointer(child, 0x0c, func_80211F70_5CD440);
    *(unsigned int *)(object + 0x2c) = 0xb4;
    D_800C7AB2 = 11;
    anchor_world_pickup_effects_before_draw(object);
    assert(*(unsigned int *)(object + 0x2c) == 0);
    assert(s_visuals[0].task == child); /* Frame-end reset still owns it. */
    anchor_world_pickup_effects_reset();
    assert(retire_calls == 1);
    D_800C7AB2 = 10;
    *(unsigned int *)(object + 0x30) = 0;
    link_child(0); /* Native pool reused the same address for a local child. */
    put_pointer(child, 0x0c, func_80211F70_5CD440);
    anchor_world_pickup_effects_before_draw(object);
    assert(*(unsigned int *)(object + 0x30) == 0);
    anchor_world_pickup_effects_reset();
    assert(retire_calls == 1); /* Never retire an unregistered local child. */
}

static void test_native_task_reset_forgets_remote_identity_before_local_reuse(void)
{
    unsigned char actor[0xf0] = {0};
    unsigned char *child, *object;
    setup();
    anchor_world_pickup_effects_spawn(actor, 10);
    child = child_tasks[0];
    object = child_objects[0];
    assert(s_visuals[0].task == child);
    /* Native allocator is about to reset this same task/model slot. */
    put_pointer(root_task, 0, 0);
    put_pointer(child, 4, 0);
    anchor_world_pickup_effects_task_reset(child);
    assert(!s_visuals[0].task);
    link_child(0);
    put_pointer(child, 0x0c, func_80211F70_5CD440);
    memset(object + 0x80, 0x77, 24);
    anchor_world_pickup_effects_before_draw(object);
    assert(*(unsigned int *)(object + 0x30) == 0);
    anchor_world_pickup_effects_reset();
    assert(retire_calls == 0); /* Local reuse cannot be owned by the bridge. */
}

static void test_cap_and_missing_arena_skip_native_constructor(void)
{
    unsigned char actor[0xf0] = {0};
    int i;
    setup();
    s_material_arena = 0;
    anchor_world_pickup_effects_spawn(actor, 10);
    assert(helper_calls == 0);
    anchor_world_pickup_effects_load_resources();
    for (i = 0; i < PICKUP_SLOT_LIMIT; ++i)
        anchor_world_pickup_effects_spawn(actor, 10);
    assert(helper_calls == PICKUP_SLOT_LIMIT);
    anchor_world_pickup_effects_spawn(actor, 10);
    assert(helper_calls == PICKUP_SLOT_LIMIT);
    anchor_world_pickup_effects_reset();
    assert(retire_calls == PICKUP_SLOT_LIMIT);
}

int main(void)
{
    test_expanded_child_material_tracks_native_fade_and_bank();
    test_room_exit_and_reused_task_do_not_rebind();
    test_native_task_reset_forgets_remote_identity_before_local_reuse();
    test_cap_and_missing_arena_skip_native_constructor();
    return 0;
}
