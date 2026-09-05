#include "anchor_projectile_capture.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef union NativeRecord
{
    unsigned int alignment;
    float float_alignment;
    unsigned char bytes[0xf0];
} NativeRecord;

static void set_float(NativeRecord *record, unsigned int offset, float value)
{
    memcpy(record->bytes + offset, &value, sizeof(value));
}

static void set_short(NativeRecord *record, unsigned int offset, short value)
{
    memcpy(record->bytes + offset, &value, sizeof(value));
}

static void native_coin(NativeRecord *task, NativeRecord *object)
{
    memset(task, 0, sizeof(*task));
    memset(object, 0, sizeof(*object));
    task->bytes[0x64] = 1;
    object->bytes[4] = 2;
    /* The native reset template at 8005B974 sets +0x65 to 1. This exact
     * ordinary visible state was discarded by the former snapshot filter. */
    object->bytes[0x65] = 1;
    set_float(object, 8, -123.25f);
    set_float(object, 0xc, 42.5f);
    set_float(object, 0x10, 789.0f);
    set_float(object, 0x1c, 0.1f);
    set_float(task, 0x6c, -6.0f);
    set_float(task, 0x70, 0.5f);
    set_float(task, 0x74, 1.25f);
    set_short(object, 0x14, -32768);
    set_short(object, 0x16, 512);
    set_short(object, 0x18, 32767);
}

static void test_native_visible_record_and_signed_rotations(void)
{
    NativeRecord task, object, original_task, original_object;
    AnchorProjectileSpawn spawn;
    native_coin(&task, &object);
    original_task = task;
    original_object = object;
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(spawn.id == 1 && spawn.kind == 1);
    assert(spawn.x100 == -12325 && spawn.y100 == 4250 && spawn.z100 == 78900);
    assert(spawn.vx100 == -600 && spawn.vy100 == 50 && spawn.vz100 == 125);
    assert(spawn.rx == -32768 && spawn.ry == 512 && spawn.rz == 32767);
    assert(spawn.scale100000 == 10000);
    assert(memcmp(&task, &original_task, sizeof(task)) == 0);
    assert(memcmp(&object, &original_object, sizeof(object)) == 0);
    object.bytes[0x65] = 0;
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    object.bytes[0x65] = 127;
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    object.bytes[0x65] = 128;
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    object.bytes[0x65] = 255;
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    object.bytes[0x65] = 1;
    object.bytes[4] = 0x82; /* Removal marker is not a live kind-2 record. */
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    object.bytes[4] = 1;
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(!anchor_projectile_capture_fields(0, object.bytes, &spawn));
    assert(!anchor_projectile_capture_fields(task.bytes, 0, &spawn));
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, 0));
}

static void test_initialized_native_kinds(void)
{
    static const unsigned char ready[] = {1, 2, 0x17, 0x18, 0x19, 0x1a, 0x1b};
    static const unsigned char delayed[] = {0xc, 0xd, 0xe, 0xf, 0x10};
    NativeRecord task, object;
    AnchorProjectileSpawn spawn;
    unsigned int i;
    native_coin(&task, &object);
    for (i = 0; i < sizeof(ready); ++i)
    {
        task.bytes[0x64] = ready[i];
        task.bytes[0x60] = 0;
        assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        task.bytes[0x60] = 1;
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    }
    for (i = 0; i < sizeof(delayed); ++i)
    {
        task.bytes[0x64] = delayed[i];
        task.bytes[0x60] = 0;
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        task.bytes[0x60] = 1;
        assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        task.bytes[0x60] = 255;
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        task.bytes[0x60] = 2;
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    }
    for (i = 0; i < 256; ++i)
    {
        int supported = i == 1 || i == 2 || (i >= 0xc && i <= 0x10) ||
                        (i >= 0x17 && i <= 0x1b);
        task.bytes[0x64] = (unsigned char)i;
        task.bytes[0x60] = i >= 0xc && i <= 0x10;
        assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn) == supported);
    }
}

static void test_first_update_native_lifecycle(void)
{
    static const unsigned char kinds[] = {1, 2, 0xc, 0xd, 0xe, 0xf, 0x10,
                                         0x17, 0x18, 0x19, 0x1a, 0x1b};
    NativeRecord task, object;
    AnchorProjectileSpawn spawn;
    unsigned int i;
    assert(!anchor_projectile_capture_first_update(0));
    for (i = 0; i < sizeof(kinds); ++i)
    {
        int delayed = kinds[i] >= 0xc && kinds[i] <= 0x10;
        short timer = kinds[i] >= 0x1a ? 60 : kinds[i] >= 0x17 ? 90 : 0;
        native_coin(&task, &object);
        task.bytes[0x64] = kinds[i];
        set_short(&task, 0x62, timer);
        assert(anchor_projectile_capture_first_update(task.bytes));
        if (delayed)
        {
            /* Some initializers wait in state zero. Never decode that pose. */
            assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
            set_short(&task, 0x62, 255);
            assert(anchor_projectile_capture_first_update(task.bytes));
            task.bytes[0x60] = 1;
        }
        else
        {
            set_short(&task, 0x62, timer ? timer - 1 : 1);
            assert(!anchor_projectile_capture_first_update(task.bytes));
        }
        assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        assert(!anchor_projectile_capture_first_update(task.bytes));
        task.bytes[0x60] = delayed ? 2 : 1;
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        assert(!anchor_projectile_capture_first_update(task.bytes));
        task.bytes[0x60] = 0;
        if (!delayed)
        {
            set_short(&task, 0x62, timer + 1);
            assert(!anchor_projectile_capture_first_update(task.bytes));
            set_short(&task, 0x62, -1); /* Unsigned native timer must stay exact. */
            assert(!anchor_projectile_capture_first_update(task.bytes));
        }
    }
    task.bytes[0x64] = 0;
    assert(!anchor_projectile_capture_first_update(task.bytes));
    task.bytes[0x64] = 0xff;
    assert(!anchor_projectile_capture_first_update(task.bytes));
}

static void test_stationary_ebisumaru_ignores_unrelated_work_floats(void)
{
    NativeRecord task, object;
    AnchorProjectileSpawn spawn;
    native_coin(&task, &object);
    task.bytes[0x60] = 1;
    set_float(&task, 0x6c, NAN);
    set_float(&task, 0x70, INFINITY);
    set_float(&task, 0x74, -INFINITY);
    task.bytes[0x64] = 0xc;
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(spawn.vx100 == 0 && spawn.vy100 == 0 && spawn.vz100 == 0);
    task.bytes[0x64] = 0xd;
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(spawn.vx100 == 0 && spawn.vy100 == 0 && spawn.vz100 == 0);
    task.bytes[0x64] = 0xe;
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
}

static void test_finite_quantization_bounds(void)
{
    NativeRecord task, object;
    AnchorProjectileSpawn spawn;
    static const float bad[] = {NAN, INFINITY, -INFINITY};
    unsigned int i;
    native_coin(&task, &object);
    set_float(&object, 8, -10000000.0f);
    set_float(&object, 0xc, 10000000.0f);
    set_float(&task, 0x6c, -10000.0f);
    set_float(&task, 0x70, 10000.0f);
    set_float(&object, 0x1c, 10.0f);
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(spawn.x100 == -1000000000 && spawn.y100 == 1000000000);
    assert(spawn.vx100 == -1000000 && spawn.vy100 == 1000000);
    assert(spawn.scale100000 == 1000000);
    set_float(&object, 8, -10000001.0f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 8, 0.0f);
    set_float(&object, 0xc, 10000001.0f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 0xc, 0.0f);
    set_float(&task, 0x6c, -10001.0f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&task, 0x6c, 0.0f);
    set_float(&task, 0x70, 10001.0f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&task, 0x70, 0.0f);
    set_float(&object, 0x1c, 10.0001f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 0x1c, 0.0f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 0x1c, -0.1f);
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 0x1c, 0.000005f); /* Truncates below the positive wire minimum. */
    assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    set_float(&object, 0x1c, 0.00001f);
    assert(anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    assert(spawn.scale100000 == 1);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        native_coin(&task, &object);
        set_float(&object, 0x10, bad[i]);
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        native_coin(&task, &object);
        set_float(&task, 0x74, bad[i]);
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
        native_coin(&task, &object);
        set_float(&object, 0x1c, bad[i]);
        assert(!anchor_projectile_capture_fields(task.bytes, object.bytes, &spawn));
    }
}

int main(void)
{
    test_native_visible_record_and_signed_rotations();
    test_initialized_native_kinds();
    test_first_update_native_lifecycle();
    test_stationary_ebisumaru_ignores_unrelated_work_floats();
    test_finite_quantization_bounds();
    puts("Projectile native-field capture tests passed");
    return 0;
}
