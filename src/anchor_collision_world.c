#include "anchor_remote_collision.h"

/* The generic scene queries work on a caller-owned 0x48-byte record. They do
 * not run the playable task's input, action, camera, or damage callbacks.
 * Verified against FUN_8002C9D4, FUN_8002EB10, FUN_80030730 and their native
 * player caller FUN_801D04F8. Positions returned at +0x18 are displacement
 * from the query origin, not absolute world coordinates. */
typedef struct NativeCollisionQuery
{
    float origin[3];
    float direction[3];
    float delta[3];
    float normal[3];
    unsigned int surface_data;
    unsigned char surface_type;
    unsigned char padding_35;
    unsigned short surface_flags;
    unsigned short hit;
    unsigned short padding_3a;
    float distance_squared;
    unsigned int object;
    unsigned int task;
} NativeCollisionQuery;

_Static_assert(sizeof(NativeCollisionQuery) == 0x48,
               "Native scene query record must retain its N64 layout");

/* Nearest ray hit among the static map and live actor collision meshes.
 * Direction must be normalized, and range is in world units. */
extern void *func_8002C9D4_2D5D4(void *out, float x, float y, float z,
                               float dx, float dy, float dz, float range);

/* Native radial wall depenetration at two vertical sample heights. Both
 * ignore floor-like normals and return an X/Z correction. The first uses
 * static map geometry; the second uses live actor collision meshes. */
extern void *func_8002EB10_2F710(void *out, float x, float low_y,
                               float high_y, float z, float radius);
extern void *func_80030730_31330(void *out, float x, float low_y,
                               float high_y, float z, float radius);

#define WORLD_HIT 0x7fff
#define WORLD_EPSILON 0.0001f
#define WORLD_CONTACT_SKIN 0.01f
#define WORLD_SLIDE_PASSES 3
#define WORLD_WALL_PASSES 2
#define WORLD_RAY_SEGMENT 32.0f
#define WORLD_MAX_MOVE_DISTANCE 512.0f

static float vector_length(const AnchorCollisionVec3 *v)
{
    return __builtin_sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static int valid_coordinate(float value)
{
    return value >= -10000000.0f && value <= 10000000.0f;
}

static int valid_position(const AnchorCollisionVec3 *position)
{
    return position && valid_coordinate(position->x) &&
           valid_coordinate(position->y) && valid_coordinate(position->z);
}

static int valid_hit(const NativeCollisionQuery *query)
{
    return query->hit == WORLD_HIT &&
           valid_coordinate(query->delta[0]) &&
           valid_coordinate(query->delta[1]) &&
           valid_coordinate(query->delta[2]) &&
           query->normal[0] >= -1.01f && query->normal[0] <= 1.01f &&
           query->normal[1] >= -1.01f && query->normal[1] <= 1.01f &&
           query->normal[2] >= -1.01f && query->normal[2] <= 1.01f;
}

static void query_ray(NativeCollisionQuery *query,
                      const AnchorCollisionVec3 *origin,
                      const AnchorCollisionVec3 *direction, float range)
{
    float travelled = 0.0f;

    /* FUN_80032320 seeds its nearest dynamic-mesh distance squared with
     * 2000, regardless of the requested range (800323D4..800323D8). A single
     * long ray can therefore miss an actor farther than sqrt(2000). Keep
     * each query below that limit, including packet corrections. */
    do
    {
        float segment = range - travelled;
        if (segment > WORLD_RAY_SEGMENT)
            segment = WORLD_RAY_SEGMENT;
        query->hit = 0;
        func_8002C9D4_2D5D4(query,
                           origin->x + direction->x * travelled,
                           origin->y + direction->y * travelled,
                           origin->z + direction->z * travelled,
                           direction->x, direction->y, direction->z, segment);
        if (query->hit == WORLD_HIT)
        {
            query->delta[0] += direction->x * travelled;
            query->delta[1] += direction->y * travelled;
            query->delta[2] += direction->z * travelled;
            return;
        }
        travelled += segment;
    } while (travelled < range);
}

static void depenetrate_walls(AnchorCollisionVec3 *position, float scale)
{
    NativeCollisionQuery query;
    float radius = 70.0f * scale;
    int pass;

    /* These dimensions are the normal native player's wall envelope in
     * FUN_801D04F8, including Mini's live object scale. */
    for (pass = 0; pass < WORLD_WALL_PASSES; ++pass)
    {
        query.hit = 0;
        func_8002EB10_2F710(&query, position->x,
                           position->y + 50.0f * scale,
                           position->y + 165.0f * scale,
                           position->z, radius);
        if (valid_hit(&query) &&
            query.delta[0] * query.delta[0] +
                query.delta[2] * query.delta[2] <= 4.0f * radius * radius)
        {
            position->x += query.delta[0];
            position->z += query.delta[2];
        }

        query.hit = 0;
        func_80030730_31330(&query, position->x,
                           position->y + 50.0f * scale,
                           position->y + 165.0f * scale,
                           position->z, radius);
        if (valid_hit(&query) &&
            query.delta[0] * query.delta[0] +
                query.delta[2] * query.delta[2] <= 4.0f * radius * radius)
        {
            position->x += query.delta[0];
            position->z += query.delta[2];
        }
    }
}

static void consider_sweep_ray(const AnchorCollisionVec3 *origin,
                                const AnchorCollisionVec3 *direction,
                                float length, int vertical_only, float *distance,
                                AnchorCollisionVec3 *normal)
{
    NativeCollisionQuery query;
    float along;
    float approach;

    query_ray(&query, origin, direction, length + WORLD_CONTACT_SKIN);
    if (!valid_hit(&query))
        return;
    if (vertical_only && query.normal[1] > -0.5f &&
        query.normal[1] < 0.5f)
        return;

    approach = query.normal[0] * direction->x +
               query.normal[1] * direction->y +
               query.normal[2] * direction->z;
    /* A face that we are leaving or sliding along must not pin the actor. */
    if (approach >= -WORLD_EPSILON)
        return;

    along = query.delta[0] * direction->x +
            query.delta[1] * direction->y +
            query.delta[2] * direction->z - WORLD_CONTACT_SKIN;
    if (along < 0.0f)
        along = 0.0f;
    if (along < *distance)
    {
        *distance = along;
        normal->x = query.normal[0];
        normal->y = query.normal[1];
        normal->z = query.normal[2];
    }
}

static void sweep_body(const AnchorCollisionVec3 *from,
                       const AnchorCollisionVec3 *movement, float scale,
                       AnchorCollisionVec3 *out)
{
    AnchorCollisionVec3 remaining = *movement;
    AnchorCollisionVec3 position = *from;
    float radius = 70.0f * scale;
    int pass;

    for (pass = 0; pass < WORLD_SLIDE_PASSES; ++pass)
    {
        AnchorCollisionVec3 direction;
        AnchorCollisionVec3 normal = {0.0f, 0.0f, 0.0f};
        AnchorCollisionVec3 origin;
        float length = vector_length(&remaining);
        float distance = length;
        float horizontal;
        float hx = 0.0f;
        float hz = 0.0f;
        float into;
        int height;
        int side;

        if (length < WORLD_EPSILON)
            break;
        direction.x = remaining.x / length;
        direction.y = remaining.y / length;
        direction.z = remaining.z / length;
        horizontal = __builtin_sqrtf(direction.x * direction.x +
                                     direction.z * direction.z);
        if (horizontal > WORLD_EPSILON)
        {
            hx = direction.x / horizontal;
            hz = direction.z / horizontal;
        }

        /* Sweep the leading edge and both shoulders at the same heights as
         * the native wall query. This catches a thin wall even when a packet
         * step ends beyond it. The native radial pass then resolves corners. */
        for (height = 0; height < 2; ++height)
        {
            for (side = -1; side <= 1; ++side)
            {
                origin = position;
                origin.y += (height ? 165.0f : 50.0f) * scale;
                if (side == 0)
                {
                    origin.x += hx * radius;
                    origin.z += hz * radius;
                }
                else
                {
                    origin.x += (float)side * -hz * radius;
                    origin.z += (float)side * hx * radius;
                }
                consider_sweep_ray(&origin, &direction, length, 0,
                                   &distance, &normal);
            }
        }
        /* Feet and head rays preserve incoming jumps and falls while
         * preventing a fast sample from tunneling through a floor/ceiling. */
        origin = position;
        origin.y += WORLD_CONTACT_SKIN;
        consider_sweep_ray(&origin, &direction, length, 1, &distance, &normal);
        origin = position;
        origin.y += 185.0f * scale - WORLD_CONTACT_SKIN;
        consider_sweep_ray(&origin, &direction, length, 1, &distance, &normal);

        position.x += direction.x * distance;
        position.y += direction.y * distance;
        position.z += direction.z * distance;
        if (distance >= length)
            break;

        remaining.x = direction.x * (length - distance);
        remaining.y = direction.y * (length - distance);
        remaining.z = direction.z * (length - distance);
        into = remaining.x * normal.x + remaining.y * normal.y +
               remaining.z * normal.z;
        if (into < 0.0f)
        {
            remaining.x -= normal.x * into;
            remaining.y -= normal.y * into;
            remaining.z -= normal.z * into;
        }
    }
    *out = position;
}

static int depenetrate_vertical(AnchorCollisionVec3 *position, float scale)
{
    NativeCollisionQuery query;
    AnchorCollisionVec3 origin = *position;
    AnchorCollisionVec3 direction = {0.0f, -1.0f, 0.0f};
    /* Rise only as far as the lowest native wall probe. Higher ledges are
     * stopped by the swept body. The stock floor callback casts from higher
     * up; its separate 30*scale allowance is downward snapping, not a limit
     * on climbing, and is deliberately unnecessary for received motion. */
    float step_height = 50.0f * scale;
    float correction;
    float minimum_y = position->y;
    int floor_found = 0;

    /* Only correct actual penetration. Received airborne height remains
     * authoritative, so these queries never add local gravity or floor-snap
     * a remote who is still jumping on their own client. */
    origin.y += step_height;
    query_ray(&query, &origin, &direction,
              step_height + WORLD_CONTACT_SKIN);
    if (valid_hit(&query) && query.normal[1] > 0.5f)
    {
        correction = step_height + query.delta[1];
        minimum_y = position->y + correction;
        floor_found = 1;
        if (correction > 0.0f && correction <= step_height)
            position->y += correction;
    }

    origin = *position;
    origin.y += 50.0f * scale;
    direction.y = 1.0f;
    query_ray(&query, &origin, &direction, 135.0f * scale);
    if (valid_hit(&query) && query.normal[1] < -0.5f)
    {
        correction = 135.0f * scale - query.delta[1];
        if (correction > 0.0f && correction <= 135.0f * scale)
        {
            /* A ledge beneath a low ceiling has no standing clearance. Do
             * not trade a head intersection for feet inside that ledge. */
            if (floor_found && position->y - correction -
                    WORLD_CONTACT_SKIN < minimum_y - WORLD_CONTACT_SKIN)
                return 0;
            position->y -= correction + WORLD_CONTACT_SKIN;
        }
    }
    return 1;
}

void anchor_collision_world_move(const AnchorCollisionVec3 *from,
                                 const AnchorCollisionVec3 *target,
                                 float scale, AnchorCollisionVec3 *out)
{
    AnchorCollisionVec3 origin;
    AnchorCollisionVec3 movement;

    if (!out || !valid_position(from))
        return;
    *out = *from;
    if (!valid_position(target) || !(scale >= 0.001f && scale <= 1.0f))
        return;

    origin = *from;
    depenetrate_walls(&origin, scale);
    depenetrate_vertical(&origin, scale);
    movement.x = target->x - origin.x;
    movement.y = target->y - origin.y;
    movement.z = target->z - origin.z;
    {
        float length = vector_length(&movement);
        /* Bound query work for an extreme correction. A new scene, first
         * pose, or scripted-state reset can establish a new origin. Large
         * ordinary network corrections keep sweeping the intervening
         * geometry over successive frames. */
        if (length > WORLD_MAX_MOVE_DISTANCE)
        {
            float ratio = WORLD_MAX_MOVE_DISTANCE / length;
            movement.x *= ratio;
            movement.y *= ratio;
            movement.z *= ratio;
        }
    }
    sweep_body(&origin, &movement, scale, out);
    depenetrate_walls(out, scale);
    if (!depenetrate_vertical(out, scale))
        *out = *from;
}
