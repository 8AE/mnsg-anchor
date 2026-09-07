#include <stdio.h>
#include <string.h>

#define ANCHOR_MIRACLE_MOON_HOST_TEST
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
#define MOON_READ_POINTER(p, off) read_pointer(p, off)
#include "../src/anchor_miracle_moon.c"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 1; \
} } while (0)

unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4;
static union { void *alignment; unsigned char bytes[2048]; } s_storage;
static unsigned char *moon;
static unsigned char *other;
static int s_native_calls;

void func_0800532C_6C4A7C(void *actor, void *object)
{
    (void)actor; (void)object; ++s_native_calls;
}
void func_080053A4_6C4AF4(void *actor, void *object)
{
    (void)actor; (void)object; ++s_native_calls;
}

static void make_actor(unsigned char *actor)
{
    write_pointer(actor, 4, actor + 512);
    write_pointer(actor + 512, 0, actor);
    write_pointer(actor, 0x18, actor + 256);
    MOON_HALF(actor, 0x5c) = 7;
    MOON_HALF(actor, 0x5e) = MOON_ENTITY;
    MOON_BYTE(actor, 0x74) = 3;
    MOON_WORD(actor, 0x68) = 0x1000u;
}

static void reset_case(void)
{
    anchor_miracle_moon_reset();
    memset(&s_storage, 0, sizeof(s_storage));
    moon = s_storage.bytes;
    other = s_storage.bytes + 1024;
    make_actor(moon);
    make_actor(other);
    D_800C7AB2 = MOON_ROOM;
    D_8016DAB4_16E6B4 = moon;
    s_native_calls = 0;
}

static int test_remote_completion_before_and_after_binding(void)
{
    int before;
    for (before = 0; before <= 1; ++before)
    {
        reset_case();
        if (before) anchor_miracle_moon_remote_completed(MOON_ROOM);
        anchor_miracle_moon_after_constructor();
        CHECK(!anchor_miracle_moon_local_pickup_active());
        if (!before) anchor_miracle_moon_remote_completed(MOON_ROOM);
        anchor_miracle_moon_before_finalizer(other, 0);
        CHECK(MOON_WORD(other, 0x68) == 0x1000u);
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1002u);
        CHECK(!s_moon && !s_remote_completed);
        CHECK(D_8016DAB4_16E6B4 == moon);
    }
    return 0;
}

static int test_local_pickup_owns_cleanup(void)
{
    int before;
    for (before = 0; before <= 1; ++before)
    {
        reset_case();
        anchor_miracle_moon_after_constructor();
        if (before) anchor_miracle_moon_remote_completed(MOON_ROOM);
        anchor_miracle_moon_before_pickup(moon, 0);
        CHECK(anchor_miracle_moon_local_pickup_active());
        if (!before) anchor_miracle_moon_remote_completed(MOON_ROOM);
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1000u);
        anchor_miracle_moon_after_pickup();
        CHECK(s_local_pickup && s_moon == moon);
        D_8016DAB4_16E6B4 = other;
        MOON_WORD(other, 0x68) |= 2u;
        anchor_miracle_moon_after_pickup();
        CHECK(s_local_pickup && s_moon == moon);
        D_8016DAB4_16E6B4 = moon;
        MOON_WORD(moon, 0x68) |= 2u; /* Native dialogue completion. */
        anchor_miracle_moon_after_pickup();
        CHECK(!s_local_pickup && !s_moon && !s_remote_completed);
        CHECK(!anchor_miracle_moon_local_pickup_active());
    }
    reset_case();
    anchor_miracle_moon_after_constructor();
    anchor_miracle_moon_before_pickup(other, 0);
    anchor_miracle_moon_remote_completed(MOON_ROOM);
    anchor_miracle_moon_before_finalizer(moon, 0);
    CHECK(MOON_WORD(moon, 0x68) == 0x1002u);
    return 0;
}

static int test_selected_pickup_precedes_entry_hook(void)
{
    unsigned long callbacks[2];
    int i, disabled;
    callbacks[0] = (unsigned long)func_0800532C_6C4A7C;
    callbacks[1] = (unsigned long)func_080053A4_6C4AF4;
    for (i = 0; i < 2; ++i)
    for (disabled = 0; disabled <= 1; ++disabled)
    {
        reset_case();
        anchor_miracle_moon_after_constructor();
        anchor_miracle_moon_remote_completed(MOON_ROOM);
        /* Native idle callback has selected the pickup. Its entry hook has
         * not run yet, matching the end-of-frame item packet drain. */
        write_pointer(moon, 0x0c, (void *)(callbacks[i] |
                      (disabled ? MOON_CALLBACK_DISABLED : 0)));
        CHECK(!s_local_pickup);
        CHECK(anchor_miracle_moon_local_pickup_active());
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1000u);
        CHECK(s_native_calls == 0);
        /* A scheduler-selected callback on another actor proves nothing
         * about the Moon's ownership. */
        write_pointer(moon, 0x0c, 0);
        write_pointer(other, 0x0c, (void *)callbacks[i]);
        CHECK(!anchor_miracle_moon_local_pickup_active());
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1002u);
    }
    return 0;
}

static int test_wrong_room_entity_and_recycled_identity(void)
{
    int changed;
    reset_case();
    D_800C7AB2 = 0x28;
    anchor_miracle_moon_after_constructor();
    anchor_miracle_moon_remote_completed(0x28);
    anchor_miracle_moon_before_finalizer(moon, 0);
    CHECK(!s_moon && MOON_WORD(moon, 0x68) == 0x1000u);
    reset_case();
    MOON_HALF(moon, 0x5e) = 0x35f;
    anchor_miracle_moon_after_constructor();
    anchor_miracle_moon_remote_completed(MOON_ROOM);
    anchor_miracle_moon_before_finalizer(moon, 0);
    CHECK(!s_moon && MOON_WORD(moon, 0x68) == 0x1000u);
    for (changed = 0; changed < 6; ++changed)
    {
        reset_case();
        anchor_miracle_moon_after_constructor();
        anchor_miracle_moon_remote_completed(MOON_ROOM);
        if (changed == 0) MOON_HALF(moon, 0x5e) = 0x350 + 1;
        if (changed == 1) ++MOON_HALF(moon, 0x5c);
        if (changed == 2) ++MOON_BYTE(moon, 0x74);
        if (changed == 3) write_pointer(moon, 0x18, other + 256);
        if (changed == 4) write_pointer(moon + 512, 0, other);
        if (changed == 5) D_800C7AB2 = 0x28;
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1000u);
    }
    return 0;
}

static int test_valid_task_relink_stays_bound(void)
{
    unsigned char *new_backlink;

    reset_case();
    anchor_miracle_moon_after_constructor();
    anchor_miracle_moon_remote_completed(MOON_ROOM);

    /* Native task insertion/deletion may replace actor+4 while the same task,
       object, descriptor and generation remain alive. */
    new_backlink = moon + 640;
    write_pointer(new_backlink, 0, moon);
    write_pointer(moon, 0x04, new_backlink);
    anchor_miracle_moon_before_finalizer(moon, 0);
    CHECK(MOON_WORD(moon, 0x68) == 0x1002u);
    CHECK(!s_moon && !s_remote_completed);
    return 0;
}

static int test_room_change_reset_and_no_remote_event(void)
{
    int reset;
    for (reset = 0; reset <= 1; ++reset)
    {
        reset_case();
        anchor_miracle_moon_after_constructor();
        anchor_miracle_moon_remote_completed(MOON_ROOM);
        if (reset) anchor_miracle_moon_reset();
        else anchor_miracle_moon_update_room(0x28);
        anchor_miracle_moon_update_room(MOON_ROOM);
        anchor_miracle_moon_after_constructor();
        anchor_miracle_moon_before_finalizer(moon, 0);
        CHECK(MOON_WORD(moon, 0x68) == 0x1000u);
        CHECK(!s_remote_completed && !s_local_pickup);
    }
    reset_case();
    anchor_miracle_moon_after_constructor();
    anchor_miracle_moon_update_room(MOON_ROOM);
    anchor_miracle_moon_before_finalizer(moon, 0);
    CHECK(MOON_WORD(moon, 0x68) == 0x1000u);
    return 0;
}

int main(void)
{
    if (test_remote_completion_before_and_after_binding() ||
        test_local_pickup_owns_cleanup() ||
        test_selected_pickup_precedes_entry_hook() ||
        test_wrong_room_entity_and_recycled_identity() ||
        test_valid_task_relink_stays_bound() ||
        test_room_change_reset_and_no_remote_event())
        return 1;
    puts("Miracle Moon lifecycle tests passed.");
    return 0;
}
