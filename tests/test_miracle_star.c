#include <stdio.h>
#include <string.h>

#define ANCHOR_MIRACLE_STAR_HOST_TEST
static void *read_pointer(const void *record, unsigned int offset)
{
    void *pointer;
    memcpy(&pointer, (const unsigned char *)record + offset, sizeof(pointer));
    return pointer;
}
static void write_pointer(void *record, unsigned int offset, void *pointer)
{
    memcpy((unsigned char *)record + offset, &pointer, sizeof(pointer));
}
#define STAR_READ_POINTER(p, off) read_pointer(p, off)
#include "../src/anchor_miracle_star.c"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 1; \
} } while (0)

unsigned short D_800C7AB2;
static union { void *alignment; unsigned char bytes[4096]; } storage;
static unsigned char *controller;
static unsigned char *scene;
static int native_completed;
static int completion_flag;
static int active_encounter;
static int bound_root;

int func_800240DC_24CDC(int flag_id)
{
    return flag_id == 0x40 && completion_flag;
}
void func_80024038_24C38(unsigned short flag_id)
{
    if (flag_id == 0x40) completion_flag = 1;
}
void func_80024088_24C88(int flag_id)
{
    if (flag_id == 0x40) completion_flag = 0;
}
int boss_sync_has_active_encounter(const char *flag_name)
{
    return strcmp(flag_name, "fl_tsurami") == 0 && active_encounter;
}
void *anchor_tsurami_native_root_task(void)
{
    return bound_root ? controller + 2048 : 0;
}
void boss_sync_finish_tsurami_reward_scene(void)
{
    ++native_completed;
}

static void reset_case(void)
{
    anchor_miracle_star_reset();
    memset(&storage, 0, sizeof(storage));
    controller = storage.bytes;
    scene = storage.bytes + 1024;
    write_pointer(controller, 4, controller + 512);
    write_pointer(controller + 512, 0, controller);
    write_pointer(controller, 0x18, controller + 256);
    write_pointer(controller, 0xd0, scene);
    STAR_HALF(controller, 0x5c) = 7;
    STAR_HALF(controller, 0x5e) = 0x358;
    STAR_BYTE(controller, 0x74) = 3;
    STAR_WORD(controller, 0x68) = 0x1000;
    D_800C7AB2 = STAR_ROOM;
    native_completed = completion_flag = active_encounter = bound_root = 0;
}

static void construct(void)
{
    anchor_miracle_star_before_constructor(controller, controller + 256);
    anchor_miracle_star_after_constructor();
}

static int test_native_reward_owns_child_and_control(void)
{
    unsigned char before[4096];
    unsigned int phase;
    reset_case();
    construct();
    for (phase = 0; phase <= 23; ++phase)
    {
        STAR_WORD(scene, 0) = phase;
        STAR_WORD(scene, 4) = 1;
        memcpy(before, storage.bytes, sizeof(before));
        CHECK(anchor_miracle_star_local_scene_active());
        anchor_miracle_star_before_controller(controller, 0);
        anchor_miracle_star_after_controller();
        CHECK(native_completed == 0);
        CHECK(memcmp(before, storage.bytes, sizeof(before)) == 0);
    }
    /* Timer one is not completion; native decrements it and returns first. */
    STAR_WORD(scene, 4) = 0;
    anchor_miracle_star_before_controller(controller, 0);
    CHECK(anchor_miracle_star_local_scene_active());
    /* Model native room teardown: no post-return dereference is permitted. */
    write_pointer(controller, 0xd0, 0);
    anchor_miracle_star_after_controller();
    CHECK(!anchor_miracle_star_local_scene_active());
    CHECK(native_completed == 1);
    anchor_miracle_star_after_controller();
    CHECK(native_completed == 1);
    return 0;
}

static int test_identity_and_room_lifetime(void)
{
    int change;
    for (change = 0; change < 8; ++change)
    {
        reset_case();
        construct();
        STAR_WORD(scene, 0) = 23;
        if (change == 0) ++STAR_HALF(controller, 0x5c);
        if (change == 1) ++STAR_HALF(controller, 0x5e);
        if (change == 2) ++STAR_BYTE(controller, 0x74);
        if (change == 3) write_pointer(controller, 0x18, controller + 768);
        if (change == 4) write_pointer(controller, 0xd0, controller + 1280);
        if (change == 5) write_pointer(controller + 512, 0, controller + 1280);
        if (change == 6) STAR_WORD(controller, 0x68) |= 2;
        if (change == 7) D_800C7AB2 = 0x147;
        CHECK(!anchor_miracle_star_local_scene_active());
        anchor_miracle_star_before_controller(controller, 0);
        anchor_miracle_star_after_controller();
        CHECK(!native_completed);
    }
    reset_case();
    construct();
    anchor_miracle_star_update_room(0x147);
    anchor_miracle_star_update_room(STAR_ROOM);
    CHECK(!anchor_miracle_star_local_scene_active());
    construct();
    CHECK(anchor_miracle_star_local_scene_active());
    anchor_miracle_star_reset();
    CHECK(!anchor_miracle_star_local_scene_active());
    reset_case();
    write_pointer(controller, 0x18, 0);
    construct();
    CHECK(anchor_miracle_star_local_scene_active());
    return 0;
}

static int test_skipped_constructor_and_terminal_phase(void)
{
    reset_case();
    completion_flag = 1;
    anchor_miracle_star_before_constructor(controller, 0);
    /* Native immediately destroys the task when durable flag 0x40 is set. */
    controller = 0;
    anchor_miracle_star_after_constructor();
    CHECK(!anchor_miracle_star_local_scene_active());
    CHECK(!native_completed);
    reset_case();
    construct();
    STAR_WORD(scene, 0) = 24;
    CHECK(!anchor_miracle_star_local_scene_active());
    anchor_miracle_star_before_controller(controller, 0);
    anchor_miracle_star_after_controller();
    CHECK(!native_completed);
    return 0;
}

static int test_stored_progress_preserves_an_existing_encounter(void)
{
    int role;
    for (role = 0; role < 2; ++role)
    {
        reset_case();
        completion_flag = 1;
        active_encounter = role == 0;
        bound_root = role == 1;
        anchor_miracle_star_before_constructor(controller, 0);
        CHECK(completion_flag == 0);
        anchor_miracle_star_after_constructor();
        CHECK(completion_flag == 1);
        CHECK(anchor_miracle_star_local_scene_active());
        CHECK(!native_completed);
    }
    return 0;
}

int main(void)
{
    if (test_native_reward_owns_child_and_control() ||
        test_identity_and_room_lifetime() ||
        test_skipped_constructor_and_terminal_phase() ||
        test_stored_progress_preserves_an_existing_encounter())
        return 1;
    puts("Miracle Star lifecycle tests passed.");
    return 0;
}
