#include "anchor_collision_actors.h"
#include "anchor_remote_model_pool.h"
#include "utils/array_utils.h"

extern unsigned char D_8006D328_6DF28[];
extern void func_80033898_34498(unsigned short rx, unsigned short ry,
                               unsigned short rz, float *x, float *y, float *z);

#ifdef ANCHOR_COLLISION_ACTORS_HOST_TEST
extern void *anchor_collision_test_pointer(unsigned int value);
extern int anchor_collision_test_valid(const void *pointer);
#endif

static void *read_pointer(const void *record, unsigned int offset)
{
    unsigned int value = *(const unsigned int *)((const unsigned char *)record + offset);
#ifdef ANCHOR_COLLISION_ACTORS_HOST_TEST
    return anchor_collision_test_pointer(value);
#else
    return (void *)(unsigned long)value;
#endif
}

static int valid_pointer(const void *pointer)
{
#ifdef ANCHOR_COLLISION_ACTORS_HOST_TEST
    return anchor_collision_test_valid(pointer);
#else
    unsigned long address = (unsigned long)pointer;
    return pointer && !(address & 3u) &&
           ((address >= 0x80001000u && address < 0x80800000u) ||
            anchor_remote_model_pool_contains(pointer));
#endif
}

static int live_task(const void *task)
{
    void *backlink;
    if (!valid_pointer(task))
        return 0;
    backlink = read_pointer(task, 4);
    return valid_pointer(backlink) && read_pointer(backlink, 0) == task;
}

/* Explicit native enemy identities. Actor-manager categories alone are not
 * sufficient: the same lists also contain elevators, doors and hazards.
 * IDs are the shared native reference's enemy table plus the verified boss
 * carriers. Deliberately exclude spawners, projectiles and level machinery. */
static int enemy_id(unsigned short id)
{
    /* Keep this as sorted data and comparisons. LLVM turned the former
     * switch into a hoisted-base jump table that the live MIPS recompiler
     * treated as an indirect function call to an interior block. Visiting
     * Dharumanyo then dispatched an unregistered address and exited. */
    static const unsigned short ids[] = {
        0x0cb, 0x0cc, /* Tsurami / Dharumanyo damage carriers. */
        0x0fa, 0x0fb, 0x0fc, 0x0fd, 0x0fe,
        0x0ff, 0x100, 0x103, 0x104, 0x105,
        0x106, 0x107, 0x108, 0x109, 0x10a,
        0x10b, 0x10c, 0x110,
        0x12c, 0x12d, 0x12e, 0x12f, 0x130,
        0x131, 0x132, 0x133, 0x136, 0x13a,
        0x13b, 0x13c, 0x13d, 0x13e, 0x13f,
        0x140, 0x141, 0x144, 0x145, 0x147,
        0x148, 0x190, 0x1a6, 0x1b0, 0x2f6,
        0x323, /* Congo. */
    };
    unsigned int low = 0;
    unsigned int high = sizeof(ids) / sizeof(ids[0]);
    while (low < high)
    {
        unsigned int middle = low + (high - low) / 2u;
        if (ids[middle] < id)
            low = middle + 1u;
        else
            high = middle;
    }
    return low < sizeof(ids) / sizeof(ids[0]) && ids[low] == id;
}

static int finite_coordinate(float value)
{
    return value >= -10000000.0f && value <= 10000000.0f;
}

/* FUN_80033688 reads task +3C/+3E as unsigned radius/height and +40/+42
 * as signed Y/Z offsets, with display scales +1C/+20/+24. A zero height is
 * a sphere (FUN_80033BDC). These are combat-body dimensions, never a mesh.
 * The upright contact envelope encloses rotated native cylinders. */
static int enemy_body(const unsigned char *task, AnchorCollisionBody *body)
{
    const unsigned char *object;
    float radius, height, axis_x = 0.0f, axis_y, axis_z = 0.0f;
    float offset_x = 0.0f, offset_y, offset_z;
    float sx, sy, sz, horizontal_axis, vertical_radius;
    unsigned short rx, ry, rz;
    if (!enemy_id(*(const unsigned short *)(task + 0x5c)) ||
        !(task[0x30] & 1u) || (task[0x30] & 4u) ||
        (*(const unsigned int *)(task + 0x68) & 2u))
        return 0;
    object = read_pointer(task, 0x18);
    if (!valid_pointer(object) || object[4] != 2)
        return 0;
    sx = *(const float *)(object + 0x1c);
    sy = *(const float *)(object + 0x20);
    sz = *(const float *)(object + 0x24);
    if (!(sx > 0.0f && sx <= 10.0f && sy > 0.0f && sy <= 10.0f &&
          sz > 0.0f && sz <= 10.0f))
        return 0;
    radius = (float)*(const unsigned short *)(task + 0x3c) * sx;
    height = (float)*(const unsigned short *)(task + 0x3e) * sy;
    if (!(radius > 0.0f && radius <= 10000.0f && height <= 10000.0f))
        return 0;
    offset_y = (float)*(const short *)(task + 0x40) * sy;
    offset_z = (float)*(const short *)(task + 0x42) * sz;
    rx = *(const unsigned short *)(object + 0x14) & 0x3ff;
    ry = *(const unsigned short *)(object + 0x16) & 0x3ff;
    rz = *(const unsigned short *)(object + 0x18) & 0x3ff;
    func_80033898_34498(rx, ry, rz, &offset_x, &offset_y, &offset_z);
    axis_y = 1.0f;
    func_80033898_34498(rx, ry, rz, &axis_x, &axis_y, &axis_z);
    horizontal_axis = __builtin_sqrtf(axis_x * axis_x + axis_z * axis_z);
    vertical_radius = height == 0.0f ? radius : radius * horizontal_axis;
    body->position.x = *(const float *)(object + 8) + offset_x + axis_x * height * 0.5f;
    body->position.z = *(const float *)(object + 0x10) + offset_z + axis_z * height * 0.5f;
    body->position.y = *(const float *)(object + 0xc) + offset_y - vertical_radius;
    if (axis_y < 0.0f)
        body->position.y += axis_y * height;
    body->radius = radius + horizontal_axis * height * 0.5f;
    body->height = (axis_y < 0.0f ? -axis_y : axis_y) * height + 2.0f * vertical_radius;
    return finite_coordinate(body->position.x) && finite_coordinate(body->position.y) &&
           finite_coordinate(body->position.z);
}

int anchor_collision_append_enemies(AnchorCollisionBody **bodies, int *capacity,
                                    int count)
{
    const unsigned char *task;
    unsigned int remaining;
    if (!bodies || !capacity || count < 0 || count > *capacity ||
        (*capacity && !*bodies))
        return -1;
    task = read_pointer(D_8006D328_6DF28, 0);
    if (!valid_pointer(task))
        return count;
    /* Native active count bounds corrupt traversal without imposing any
     * multiplayer-roster limit. Skip the scheduler's separate root record. */
    remaining = *(const unsigned short *)(D_8006D328_6DF28 + 0x0c);
    task = read_pointer(task, 0);
    while (remaining-- && live_task(task))
    {
        AnchorCollisionBody body;
        if (enemy_body(task, &body))
        {
            if (!mnsg_array_reserve((void **)bodies, capacity, count + 1,
                                    sizeof(**bodies)))
                return -1;
            (*bodies)[count++] = body;
        }
        task = read_pointer(task, 0);
    }
    return count;
}
