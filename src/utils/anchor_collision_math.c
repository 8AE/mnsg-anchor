#include "anchor_remote_collision.h"

#define CONTACT_SKIN 0.02f
#define MOTION_EPSILON 0.000001f
#define CONTACT_PASSES 4

static float dot(AnchorCollisionVec3 a, AnchorCollisionVec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* Intersect a swept feet position with a cylinder expanded by the moving
 * body's radius and height. Intersecting the horizontal quadratic with the
 * vertical slab catches crossings even when neither endpoint overlaps. */
static int sweep_body(const AnchorCollisionBody *moving,
                      AnchorCollisionVec3 start, AnchorCollisionVec3 delta,
                      const AnchorCollisionBody *other,
                      float *time, AnchorCollisionVec3 *normal)
{
    float x = start.x - other->position.x;
    float z = start.z - other->position.z;
    float radius = moving->radius + other->radius + CONTACT_SKIN;
    float a = delta.x * delta.x + delta.z * delta.z;
    float b = x * delta.x + z * delta.z;
    float c = x * x + z * z - radius * radius;
    float horizontal_enter = -1000000.0f;
    float horizontal_exit = 1000000.0f;
    float vertical_enter = -1000000.0f;
    float vertical_exit = 1000000.0f;
    float bottom = other->position.y - moving->height;
    float top = other->position.y + other->height;
    float enter;
    float leave;

    if (a < MOTION_EPSILON)
    {
        if (c >= 0.0f)
            return 0;
    }
    else
    {
        float discriminant = b * b - a * c;
        float root;
        if (discriminant <= 0.0f)
            return 0;
        root = __builtin_sqrtf(discriminant);
        horizontal_enter = (-b - root) / a;
        horizontal_exit = (-b + root) / a;
    }

    if (delta.y * delta.y < MOTION_EPSILON)
    {
        if (start.y <= bottom || start.y >= top)
            return 0;
    }
    else
    {
        vertical_enter = (bottom - start.y) / delta.y;
        vertical_exit = (top - start.y) / delta.y;
        if (vertical_enter > vertical_exit)
        {
            float swap = vertical_enter;
            vertical_enter = vertical_exit;
            vertical_exit = swap;
        }
    }

    enter = horizontal_enter > vertical_enter ?
                horizontal_enter : vertical_enter;
    leave = horizontal_exit < vertical_exit ?
                horizontal_exit : vertical_exit;
    if (enter > leave || leave <= 0.0f || enter > 1.0f)
        return 0;
    if (enter < 0.0f)
        enter = 0.0f;

    normal->x = 0.0f;
    normal->y = 0.0f;
    normal->z = 0.0f;
    if (vertical_enter > horizontal_enter)
        normal->y = delta.y < 0.0f ? 1.0f : -1.0f;
    else
    {
        float nx = x + delta.x * enter;
        float nz = z + delta.z * enter;
        float length = __builtin_sqrtf(nx * nx + nz * nz);
        if (length < MOTION_EPSILON)
            return 0;
        normal->x = nx / length;
        normal->z = nz / length;
    }
    if (dot(delta, *normal) >= -MOTION_EPSILON)
        return 0; /* touching while leaving or moving along the surface */
    *time = enter;
    return 1;
}

void anchor_collision_move_peers(const AnchorCollisionBody *moving,
                                 const AnchorCollisionVec3 *target,
                                 const AnchorCollisionBody *obstacles,
                                 int count, AnchorCollisionVec3 *out)
{
    AnchorCollisionVec3 position = moving->position;
    AnchorCollisionVec3 delta;
    int pass;
    int i;

    /* A peer can spawn/reconnect at exactly the same location. Separate such
     * overlaps horizontally; do not lift a stationary player onto a head. */
    for (pass = 0; pass < CONTACT_PASSES; ++pass)
    {
        int separated = 0;
        for (i = 0; i < count; ++i)
        {
            const AnchorCollisionBody *other = &obstacles[i];
            float x = position.x - other->position.x;
            float z = position.z - other->position.z;
            float radius = moving->radius + other->radius;
            float distance_sq = x * x + z * z;
            float distance;
            if (position.y + moving->height <= other->position.y ||
                position.y >= other->position.y + other->height ||
                distance_sq >= radius * radius)
                continue;
            if (distance_sq < MOTION_EPSILON)
            {
                x = moving->position.x - target->x;
                z = moving->position.z - target->z;
                distance_sq = x * x + z * z;
                if (distance_sq < MOTION_EPSILON)
                {
                    x = 1.0f;
                    z = 0.0f;
                    distance_sq = 1.0f;
                }
                distance = __builtin_sqrtf(distance_sq);
                position.x += x / distance * (radius + CONTACT_SKIN);
                position.z += z / distance * (radius + CONTACT_SKIN);
            }
            else
            {
                distance = __builtin_sqrtf(distance_sq);
                position.x += x / distance * (radius + CONTACT_SKIN - distance);
                position.z += z / distance * (radius + CONTACT_SKIN - distance);
            }
            separated = 1;
        }
        if (!separated)
            break;
    }

    delta.x = target->x - position.x;
    delta.y = target->y - position.y;
    delta.z = target->z - position.z;
    for (pass = 0; pass < CONTACT_PASSES; ++pass)
    {
        float first_time = 1.0f;
        AnchorCollisionVec3 first_normal = {0.0f, 0.0f, 0.0f};
        int hit = 0;
        for (i = 0; i < count; ++i)
        {
            float time;
            AnchorCollisionVec3 normal;
            if (sweep_body(moving, position, delta, &obstacles[i],
                           &time, &normal) && time <= first_time)
            {
                first_time = time;
                first_normal = normal;
                hit = 1;
            }
        }
        position.x += delta.x * first_time;
        position.y += delta.y * first_time;
        position.z += delta.z * first_time;
        if (!hit)
            break;
        delta.x *= 1.0f - first_time;
        delta.y *= 1.0f - first_time;
        delta.z *= 1.0f - first_time;
        {
            float into = dot(delta, first_normal);
            delta.x -= first_normal.x * into;
            delta.y -= first_normal.y * into;
            delta.z -= first_normal.z * into;
        }
    }
    *out = position;
}
