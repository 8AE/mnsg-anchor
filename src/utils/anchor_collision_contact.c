#include "anchor_remote_collision.h"

#define ESCAPE_SKIN 0.04f
#define OVERLAP_TOLERANCE 0.0001f

static int overlaps(const AnchorCollisionBody *moving,
                    const AnchorCollisionVec3 *position,
                    const AnchorCollisionBody *peer)
{
    float x = position->x - peer->position.x;
    float z = position->z - peer->position.z;
    float radius = moving->radius + peer->radius;
    if (position->y + moving->height <= peer->position.y ||
        position->y >= peer->position.y + peer->height)
        return 0;
    return x * x + z * z < radius * radius - OVERLAP_TOLERANCE;
}

static int clear_of_peers(const AnchorCollisionBody *moving,
                           const AnchorCollisionVec3 *position,
                           const AnchorCollisionBody *peers, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        if (overlaps(moving, position, &peers[i]))
            return 0;
    return 1;
}

static int same_position(const AnchorCollisionVec3 *a,
                           const AnchorCollisionVec3 *b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z;
}

static int clear_at_origin(const AnchorCollisionBody *peer, const void *context)
{
    const AnchorCollisionBody *moving = context;
    return !overlaps(moving, &moving->position, peer);
}

/* A second world sweep is only needed when a peer changed the target. The
 * final overlap check is essential: a wall can reject the peer separation. */
static void resolve_candidate(const AnchorCollisionBody *moving,
                               const AnchorCollisionVec3 *target, float scale,
                               const AnchorCollisionBody *peers, int count,
                               AnchorCollisionPeerFilter include,
                               AnchorCollisionVec3 *out)
{
    AnchorCollisionVec3 contact;
    anchor_collision_world_move(&moving->position, target, scale, out);
    if (!count)
        return;
    anchor_collision_move_peers_filtered(moving, out, peers, count, include,
                                         moving, &contact);
    if (!same_position(&contact, out))
        anchor_collision_world_move(&moving->position, &contact, scale, out);
}

int anchor_collision_move_body(const AnchorCollisionBody *moving,
                               const AnchorCollisionVec3 *target, float scale,
                               const AnchorCollisionBody *peers, int count,
                               AnchorCollisionVec3 *out)
{
    static const float directions[8][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        {0.70710678f, 0.70710678f}, {-0.70710678f, 0.70710678f},
        {0.70710678f, -0.70710678f}, {-0.70710678f, -0.70710678f},
    };
    AnchorCollisionVec3 resolved;
    AnchorCollisionVec3 best;
    float best_distance = 0.0f;
    int path_count = 0;
    int found = 0;
    int i;
    int direction;

    if (!moving || !target || !out)
        return 0;
    *out = moving->position;
    if (count < 0 || (count && !peers))
        return 0;

    resolve_candidate(moving, target, scale, peers, count, 0, &resolved);
    if (clear_of_peers(moving, &resolved, peers, count))
    {
        *out = resolved;
        return 1;
    }

    /* Preserve the previous position when it still clears live world and
     * peer geometry. This avoids an unnecessary escape for ordinary contact. */
    anchor_collision_world_move(&moving->position, &moving->position, scale,
                                &resolved);
    if (clear_of_peers(moving, &resolved, peers, count))
    {
        *out = resolved;
        return 1;
    }

    /* A newly coincident spawn has no valid starting side. Allow it to leave
     * only peers already overlapping its origin. Every other body remains
     * a swept obstacle, so an escape cannot jump across a third player. */
    for (i = 0; i < count; ++i)
        if (clear_at_origin(&peers[i], moving))
            ++path_count;
    if (path_count == count)
        return 0;

    for (direction = 0; direction < 8; ++direction)
    {
        AnchorCollisionVec3 candidate = moving->position;
        float dx = directions[direction][0];
        float dz = directions[direction][1];
        float distance = 0.0f;
        float distance_squared;
        for (i = 0; i < count; ++i)
        {
            float x;
            float z;
            float radius;
            float along;
            float exit_distance;
            if (!overlaps(moving, &moving->position, &peers[i]))
                continue;
            x = moving->position.x - peers[i].position.x;
            z = moving->position.z - peers[i].position.z;
            radius = moving->radius + peers[i].radius + ESCAPE_SKIN;
            along = x * dx + z * dz;
            exit_distance = -along + __builtin_sqrtf(
                along * along + radius * radius - x * x - z * z);
            if (exit_distance > distance)
                distance = exit_distance;
        }
        candidate.x += dx * distance;
        candidate.z += dz * distance;
        resolve_candidate(moving, &candidate, scale, peers,
                           path_count ? count : 0,
                           clear_at_origin, &resolved);
        if (!clear_of_peers(moving, &resolved, peers, count))
            continue;
        dx = resolved.x - moving->position.x;
        dz = resolved.z - moving->position.z;
        distance_squared = dx * dx + dz * dz;
        if (!found || distance_squared < best_distance)
        {
            best = resolved;
            best_distance = distance_squared;
            found = 1;
        }
    }
    if (found)
        *out = best;
    return found;
}
