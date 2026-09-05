#include "anchor_remote_collision.h"

AnchorCollisionVec3 anchor_collision_push(const AnchorCollisionBody *recipient,
                                         const AnchorCollisionBody *peer,
                                         float drive_x, float drive_z)
{
    AnchorCollisionVec3 push = {0.0f, 0.0f, 0.0f};
    float x;
    float z;
    float distance;
    float radius;
    float pressure;
    float limit;
    if (!recipient || !peer || !(recipient->radius > 0.0f) ||
        !(peer->radius > 0.0f) ||
        recipient->position.y + recipient->height <= peer->position.y + 0.02f ||
        peer->position.y + peer->height <= recipient->position.y + 0.02f ||
        !(drive_x >= -10.0f && drive_x <= 10.0f) ||
        !(drive_z >= -10.0f && drive_z <= 10.0f))
        return push;
    x = recipient->position.x - peer->position.x;
    z = recipient->position.z - peer->position.z;
    distance = x * x + z * z;
    radius = recipient->radius + peer->radius;
    /* The solid solver leaves a skin between touching bodies. Do not turn
     * near misses or vertical contacts into horizontal pushes. */
    if (!(distance > 0.0001f && distance <= (radius + 0.15f) * (radius + 0.15f)))
        return push;
    distance = __builtin_sqrtf(distance);
    x /= distance;
    z /= distance;
    pressure = (drive_x * x + drive_z * z) * 0.5f;
    if (!(pressure > 0.0f))
        return push;
    limit = recipient->radius * 0.3f;
    if (limit > 1.5f)
        limit = 1.5f;
    if (pressure > limit)
        pressure = limit;
    push.x = x * pressure;
    push.z = z * pressure;
    return push;
}
