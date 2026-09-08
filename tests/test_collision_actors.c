#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anchor_collision_actors.h"

/* Native pointer words retain their 32-bit layout on a 64-bit host. */
unsigned char D_8006D328_6DF28[0x20] __attribute__((aligned(16)));
static unsigned char root[0xf0] __attribute__((aligned(16)));
static unsigned char tasks[128][0xf0] __attribute__((aligned(16)));
static unsigned char objects[128][0x98] __attribute__((aligned(16)));
static void *pointers[1024];
static unsigned int pointer_count;
static int fail_alloc;

void *recomp_alloc(unsigned long size) { return fail_alloc ? 0 : malloc(size); }
void recomp_free(void *pointer) { free(pointer); }
void *anchor_collision_test_pointer(unsigned int value)
{
    return value && value <= pointer_count ? pointers[value - 1] : 0;
}
int anchor_collision_test_valid(const void *pointer)
{
    unsigned int i;
    for (i = 0; i < pointer_count; ++i)
        if (pointers[i] == pointer)
            return 1;
    return 0;
}
static void pointer_at(void *record, int offset, void *pointer)
{
    unsigned int i;
    unsigned int value = 0;
    if (pointer)
    {
        for (i = 0; i < pointer_count && pointers[i] != pointer; ++i) {}
        if (i == pointer_count)
            pointers[pointer_count++] = pointer;
        value = i + 1;
    }
    memcpy((unsigned char *)record + offset, &value, sizeof(value));
}
static void u16_at(void *record, int offset, unsigned short value)
{
    memcpy((unsigned char *)record + offset, &value, sizeof(value));
}
static void f32_at(void *record, int offset, float value)
{
    memcpy((unsigned char *)record + offset, &value, sizeof(value));
}
void func_80033898_34498(unsigned short rx, unsigned short ry, unsigned short rz,
                        float *x, float *y, float *z)
{
    float old_x = *x, old_y = *y, old_z = *z;
    assert(rx == 0 && rz == 0 && (ry == 0 || ry == 256));
    if (ry == 256)
    {
        *x = old_z;
        *z = -old_x;
    }
    *y = old_y;
}
static void fixture(int count)
{
    int i;
    void *previous = root;
    memset(root, 0, sizeof(root));
    memset(tasks, 0, sizeof(tasks));
    memset(objects, 0, sizeof(objects));
    memset(D_8006D328_6DF28, 0, sizeof(D_8006D328_6DF28));
    pointer_count = 0;
    pointer_at(D_8006D328_6DF28, 0, root);
    u16_at(D_8006D328_6DF28, 0x0c, (unsigned short)count);
    for (i = 0; i < count; ++i)
    {
        pointer_at(previous, 0, tasks[i]);
        pointer_at(tasks[i], 4, previous);
        pointer_at(tasks[i], 0x18, objects[i]);
        tasks[i][0x30] = 1;
        u16_at(tasks[i], 0x5c, 0x100);
        u16_at(tasks[i], 0x3c, 50);
        u16_at(tasks[i], 0x3e, 100);
        objects[i][4] = 2;
        f32_at(objects[i], 8, (float)i * 20.0f);
        f32_at(objects[i], 0x1c, 0.1f);
        f32_at(objects[i], 0x20, 0.1f);
        f32_at(objects[i], 0x24, 0.1f);
        previous = tasks[i];
    }
}

int main(void)
{
    AnchorCollisionBody *bodies = 0;
    int capacity = 0, count;
    /* The same live actor storage may be reused as an enemy, a prop or an
     * arena boss. Classification must be refreshed on every collection,
     * including IDs at both ends of the table and gaps between enemy IDs. */
    static const struct { unsigned short id; int collides; } visits[] = {
        {0x0cc, 1}, {0x34f, 0}, {0x0cb, 1}, {0x0cd, 0},
        {0x0fa, 1}, {0x0f9, 0}, {0x100, 1}, {0x101, 0},
        {0x1a6, 1}, {0x1b6, 0}, {0x323, 1}, {0x324, 0},
        {0x3da, 0}, {0, 0}, {0xffff, 0}, {0x0cc, 1},
    };
    unsigned int visit;
    fixture(1);
    for (visit = 0; visit < sizeof(visits) / sizeof(visits[0]); ++visit)
    {
        u16_at(tasks[0], 0x5c, visits[visit].id);
        count = anchor_collision_append_enemies(&bodies, &capacity, 0);
        assert(count == visits[visit].collides);
    }
    fixture(7);
    u16_at(tasks[0], 0x5c, 0x3da); /* Boulder/prop must not become solid. */
    u16_at(tasks[1], 0x5c, 0x34f); /* Reward controller. */
    u16_at(tasks[2], 0x5c, 0x1b6); /* Spawner, not the spawned enemy. */
    tasks[3][0x30] = 0;           /* Disabled native body. */
    objects[4][4] = 0x82;        /* Retired display record. */
    tasks[5][0x68] = 2;          /* Removal pending (host endianness). */
    count = anchor_collision_append_enemies(&bodies, &capacity, 0);
    assert(count == 1 && bodies[0].position.x == 120.0f);
    assert(bodies[0].radius == 5.0f && bodies[0].height == 10.0f);

    fixture(1);
    u16_at(tasks[0], 0x5c, 0xcc); /* Dharumanyo native combat carrier. */
    u16_at(tasks[0], 0x40, (unsigned short)-20);
    u16_at(tasks[0], 0x42, 30);
    u16_at(objects[0], 0x16, 256);
    count = anchor_collision_append_enemies(&bodies, &capacity, 0);
    assert(count == 1 && bodies[0].position.x == 3.0f);
    assert(bodies[0].position.y == -2.0f && bodies[0].position.z == 0.0f);
    u16_at(tasks[0], 0x3e, 0); /* Native sphere has no height. */
    count = anchor_collision_append_enemies(&bodies, &capacity, 0);
    assert(count == 1 && bodies[0].height == 10.0f && bodies[0].position.y == -7.0f);

    fixture(128);
    count = anchor_collision_append_enemies(&bodies, &capacity, 0);
    assert(count == 128 && bodies[127].position.x == 2540.0f);
    pointer_at(tasks[127], 4, 0); /* Unlinked last task cannot be retained. */
    count = anchor_collision_append_enemies(&bodies, &capacity, 0);
    assert(count == 127);
    fixture(1);
    f32_at(objects[0], 0x20, NAN);
    assert(anchor_collision_append_enemies(&bodies, &capacity, 0) == 0);
    fixture(1);
    u16_at(D_8006D328_6DF28, 0x0c, 0); /* Empty/retired scene. */
    assert(anchor_collision_append_enemies(&bodies, &capacity, 0) == 0);
    free(bodies);
    bodies = 0;
    capacity = 0;
    fixture(1);
    fail_alloc = 1;
    assert(anchor_collision_append_enemies(&bodies, &capacity, 0) == -1);
    assert(!bodies && !capacity);
    puts("native enemy-only collision collection tests passed");
    return 0;
}
