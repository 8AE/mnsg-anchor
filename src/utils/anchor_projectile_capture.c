#include "anchor_projectile_capture.h"

int anchor_projectile_capture_first_update(const void *pointer)
{
    const unsigned char *task = pointer;
    unsigned short timer;
    if (!task || task[0x60] != 0)
        return 0;
    timer = *(const unsigned short *)(task + 0x62);
    switch (task[0x64])
    {
        case 1: case 2:
            return timer == 0;
        case 0xc: case 0xd: case 0xe: case 0xf: case 0x10:
            return 1;
        case 0x17: case 0x18: case 0x19:
            return timer == 90;
        case 0x1a: case 0x1b:
            return timer == 60;
        default:
            return 0;
    }
}

static int quantize(const void *pointer, float scale, int limit, int *out)
{
    union { float value; unsigned int bits; } number;
    float value;
    number.bits = *(const unsigned int *)pointer;
    /* Keep malformed native floats out even with the mod's fast-math flags. */
    if ((number.bits & 0x7f800000u) == 0x7f800000u)
        return 0;
    value = number.value;
    if (!(value >= -(float)limit / scale && value <= (float)limit / scale))
        return 0;
    value *= scale;
    if (!(value >= -(float)limit && value <= (float)limit))
        return 0;
    *out = (int)value;
    return 1;
}

int anchor_projectile_capture_fields(const void *task_pointer,
                                      const void *object_pointer,
                                      AnchorProjectileSpawn *out)
{
    const unsigned char *task = task_pointer;
    const unsigned char *object = object_pointer;
    int stationary;
    if (!task || !object || !out || object[4] != 2 ||
        (signed char)object[0x65] < 0)
        return 0;
    /* Native kind-2 defaults +0x65 to 1; FUN_80016C44 accepts all
     * nonnegative values. Bit 7 of +4 instead marks a retired record. */
    out->kind = task[0x64];
    switch (out->kind)
    {
        case 1: case 2: case 0x17: case 0x18: case 0x19:
        case 0x1a: case 0x1b:
            if (task[0x60] != 0)
                return 0; /* The first update already changed into impact. */
            break;
        case 0xc: case 0xd: case 0xe: case 0xf: case 0x10:
            if (task[0x60] != 1)
                return 0; /* Pose initialized, with no immediate impact. */
            break;
        default:
            return 0;
    }
    stationary = out->kind == 0xc || out->kind == 0xd;
    out->id = 1;
    if (!quantize(object + 8, 100.0f, 1000000000, &out->x100) ||
        !quantize(object + 0xc, 100.0f, 1000000000, &out->y100) ||
        !quantize(object + 0x10, 100.0f, 1000000000, &out->z100) ||
        !quantize(object + 0x1c, 100000.0f, 1000000, &out->scale100000))
        return 0;
    out->vx100 = out->vy100 = out->vz100 = 0;
    if (!stationary &&
        (!quantize(task + 0x6c, 100.0f, 1000000, &out->vx100) ||
         !quantize(task + 0x70, 100.0f, 1000000, &out->vy100) ||
         !quantize(task + 0x74, 100.0f, 1000000, &out->vz100)))
        return 0;
    out->rx = *(const short *)(object + 0x14);
    out->ry = *(const short *)(object + 0x16);
    out->rz = *(const short *)(object + 0x18);
    return anchor_projectile_spawn_valid(out);
}
