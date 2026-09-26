#include "combat/anchor_collision_cube.h"

#define CUBE_CONTACT_SKIN 0.02f
#define CUBE_EPSILON 0.000001f
#define CUBE_SIDE_PASSES 4

static float clamp(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int coordinate(float value)
{
    return value >= -10000000.0f && value <= 10000000.0f;
}

static int valid_cube(const AnchorCollisionCube *cube)
{
    return cube && coordinate(cube->min.x) && coordinate(cube->min.y) &&
           coordinate(cube->min.z) && coordinate(cube->max.x) &&
           coordinate(cube->max.y) && coordinate(cube->max.z) &&
           cube->min.x < cube->max.x && cube->min.y < cube->max.y &&
           cube->min.z < cube->max.z;
}

static int vertical_overlap(const AnchorCollisionBody *moving, float feet,
                            const AnchorCollisionCube *cube)
{
    /* Native floor contact can settle a fraction below the upper plane.
     * Treat that narrow band as support, not side penetration, including
     * stationary frames where there is no descending top crossing. */
    return feet < cube->max.y - CUBE_CONTACT_SKIN &&
           feet + moving->height > cube->min.y;
}

static float distance_to_box_squared(float x, float z,
                                     const AnchorCollisionCube *cube)
{
    float dx = x - clamp(x, cube->min.x, cube->max.x);
    float dz = z - clamp(z, cube->min.z, cube->max.z);
    return dx * dx + dz * dz;
}

int anchor_collision_cube_side_overlaps(const AnchorCollisionBody *moving,
                                        const AnchorCollisionVec3 *position,
                                        const AnchorCollisionCube *cube)
{
    if (!moving || !position || !valid_cube(cube) ||
        !(moving->radius > 0.0f && moving->height > 0.0f) ||
        !vertical_overlap(moving, position->y, cube))
        return 0;
    return distance_to_box_squared(position->x, position->z, cube) <
           moving->radius * moving->radius - CUBE_EPSILON;
}

static int separate_start(const AnchorCollisionBody *moving,
                          const AnchorCollisionCube *cube,
                          AnchorCollisionVec3 *position)
{
    float near_x, near_z, dx, dz, distance;
    float radius = moving->radius + CUBE_CONTACT_SKIN;
    if (!anchor_collision_cube_side_overlaps(moving, position, cube))
        return 0;
    near_x = clamp(position->x, cube->min.x, cube->max.x);
    near_z = clamp(position->z, cube->min.z, cube->max.z);
    dx = position->x - near_x;
    dz = position->z - near_z;
    distance = __builtin_sqrtf(dx * dx + dz * dz);
    if (distance > CUBE_EPSILON)
    {
        position->x += dx / distance * (radius - distance);
        position->z += dz / distance * (radius - distance);
    }
    else
    {
        float left = position->x - (cube->min.x - radius);
        float right = cube->max.x + radius - position->x;
        float back = position->z - (cube->min.z - radius);
        float front = cube->max.z + radius - position->z;
        if (left <= right && left <= back && left <= front)
            position->x -= left;
        else if (right <= back && right <= front)
            position->x += right;
        else if (back <= front)
            position->z -= back;
        else
            position->z += front;
    }
    return 1;
}

static void consider_hit(float time, float nx, float nz,
                         const AnchorCollisionBody *moving,
                         AnchorCollisionVec3 start, AnchorCollisionVec3 delta,
                         const AnchorCollisionCube *cube,
                         float *best_time, float *best_nx, float *best_nz)
{
    if (time < 0.0f || time > 1.0f || time >= *best_time ||
        delta.x * nx + delta.z * nz >= -CUBE_EPSILON ||
        !vertical_overlap(moving, start.y + delta.y * time, cube))
        return;
    *best_time = time;
    *best_nx = nx;
    *best_nz = nz;
}

static void face_hits(const AnchorCollisionBody *moving,
                      AnchorCollisionVec3 start, AnchorCollisionVec3 delta,
                      const AnchorCollisionCube *cube,
                      float *best_time, float *best_nx, float *best_nz)
{
    float time, other;
    if (delta.x > CUBE_EPSILON)
    {
        time = (cube->min.x - moving->radius - start.x) / delta.x;
        other = start.z + delta.z * time;
        if (other >= cube->min.z && other <= cube->max.z)
            consider_hit(time, -1.0f, 0.0f, moving, start, delta, cube,
                         best_time, best_nx, best_nz);
    }
    else if (delta.x < -CUBE_EPSILON)
    {
        time = (cube->max.x + moving->radius - start.x) / delta.x;
        other = start.z + delta.z * time;
        if (other >= cube->min.z && other <= cube->max.z)
            consider_hit(time, 1.0f, 0.0f, moving, start, delta, cube,
                         best_time, best_nx, best_nz);
    }
    if (delta.z > CUBE_EPSILON)
    {
        time = (cube->min.z - moving->radius - start.z) / delta.z;
        other = start.x + delta.x * time;
        if (other >= cube->min.x && other <= cube->max.x)
            consider_hit(time, 0.0f, -1.0f, moving, start, delta, cube,
                         best_time, best_nx, best_nz);
    }
    else if (delta.z < -CUBE_EPSILON)
    {
        time = (cube->max.z + moving->radius - start.z) / delta.z;
        other = start.x + delta.x * time;
        if (other >= cube->min.x && other <= cube->max.x)
            consider_hit(time, 0.0f, 1.0f, moving, start, delta, cube,
                         best_time, best_nx, best_nz);
    }
}

static void corner_hits(const AnchorCollisionBody *moving,
                        AnchorCollisionVec3 start, AnchorCollisionVec3 delta,
                        const AnchorCollisionCube *cube,
                        float *best_time, float *best_nx, float *best_nz)
{
    float a = delta.x * delta.x + delta.z * delta.z;
    int i;
    if (a <= CUBE_EPSILON)
        return;
    for (i = 0; i < 4; ++i)
    {
        float cx = (i & 1) ? cube->max.x : cube->min.x;
        float cz = (i & 2) ? cube->max.z : cube->min.z;
        float x = start.x - cx, z = start.z - cz;
        float b = x * delta.x + z * delta.z;
        float c = x * x + z * z - moving->radius * moving->radius;
        float discriminant = b * b - a * c;
        float time, hx, hz, length;
        if (discriminant < 0.0f)
            continue;
        time = (-b - __builtin_sqrtf(discriminant)) / a;
        if (time < 0.0f || time > 1.0f)
            continue;
        hx = x + delta.x * time;
        hz = z + delta.z * time;
        if (((i & 1) ? hx < 0.0f : hx > 0.0f) ||
            ((i & 2) ? hz < 0.0f : hz > 0.0f))
            continue;
        length = __builtin_sqrtf(hx * hx + hz * hz);
        if (length <= CUBE_EPSILON)
            continue;
        consider_hit(time, hx / length, hz / length, moving, start, delta,
                     cube, best_time, best_nx, best_nz);
    }
}

int anchor_collision_cube_push_intent(const AnchorCollisionBody *moving,
                                      const AnchorCollisionVec3 *target,
                                      const AnchorCollisionCube *cube,
                                      float *intent_x, float *intent_z)
{
    AnchorCollisionVec3 delta;
    float best_time = 2.0f, nx = 0.0f, nz = 0.0f;
    float blocked;
    if (!moving || !target || !intent_x || !intent_z || !valid_cube(cube) ||
        !coordinate(moving->position.x) ||
        !coordinate(moving->position.y) ||
        !coordinate(moving->position.z) ||
        !coordinate(target->x) || !coordinate(target->y) ||
        !coordinate(target->z) ||
        !(moving->radius > 0.0f && moving->radius <= 10000.0f &&
          moving->height > 0.0f && moving->height <= 10000.0f) ||
        anchor_collision_cube_side_overlaps(moving, &moving->position, cube))
        return 0;
    delta.x = target->x - moving->position.x;
    delta.y = target->y - moving->position.y;
    delta.z = target->z - moving->position.z;
    /* Match a native movement step, never a teleport or contact correction. */
    if (!(delta.x * delta.x + delta.z * delta.z > CUBE_EPSILON &&
          delta.x * delta.x + delta.z * delta.z <= 144.0f))
        return 0;
    face_hits(moving, moving->position, delta, cube,
              &best_time, &nx, &nz);
    corner_hits(moving, moving->position, delta, cube,
                &best_time, &nx, &nz);
    if (best_time > 1.0f)
        return 0;
    blocked = -(delta.x * nx + delta.z * nz) * (1.0f - best_time);
    if (blocked <= CUBE_CONTACT_SKIN)
        return 0;
    if (blocked > 12.0f)
        blocked = 12.0f;
    *intent_x = -nx * blocked;
    *intent_z = -nz * blocked;
    return 1;
}

int anchor_collision_cube_move_sides(const AnchorCollisionBody *moving,
                                     const AnchorCollisionVec3 *target,
                                     const AnchorCollisionCube *cubes, int count,
                                     AnchorCollisionVec3 *out)
{
    AnchorCollisionVec3 position, delta;
    int pass, i;
    if (!moving || !target || !out || count < 0 || (count && !cubes) ||
        !(moving->radius > 0.0f && moving->height > 0.0f) ||
        !coordinate(target->x) || !coordinate(target->y) ||
        !coordinate(target->z))
        return 0;
    position = moving->position;
    *out = position;
    if (!coordinate(position.x) || !coordinate(position.y) ||
        !coordinate(position.z))
        return 0;
    for (i = 0; i < count; ++i)
        if (!valid_cube(&cubes[i]))
            return 0;
    delta.x = target->x - position.x;
    delta.y = target->y - position.y;
    delta.z = target->z - position.z;
    for (pass = 0; pass < CUBE_SIDE_PASSES; ++pass)
    {
        int separated = 0;
        for (i = 0; i < count; ++i)
            separated |= separate_start(moving, &cubes[i], &position);
        if (!separated)
            break;
    }
    for (pass = 0; pass < CUBE_SIDE_PASSES; ++pass)
    {
        float best_time = 2.0f, nx = 0.0f, nz = 0.0f;
        for (i = 0; i < count; ++i)
        {
            face_hits(moving, position, delta, &cubes[i],
                      &best_time, &nx, &nz);
            corner_hits(moving, position, delta, &cubes[i],
                        &best_time, &nx, &nz);
        }
        if (best_time > 1.0f)
        {
            position.x += delta.x;
            position.y += delta.y;
            position.z += delta.z;
            delta.x = delta.y = delta.z = 0.0f;
            break;
        }
        position.x += delta.x * best_time + nx * CUBE_CONTACT_SKIN;
        position.y += delta.y * best_time;
        position.z += delta.z * best_time + nz * CUBE_CONTACT_SKIN;
        delta.x *= 1.0f - best_time;
        delta.y *= 1.0f - best_time;
        delta.z *= 1.0f - best_time;
        {
            float into = delta.x * nx + delta.z * nz;
            if (into < 0.0f)
            {
                delta.x -= nx * into;
                delta.z -= nz * into;
            }
        }
    }
    for (i = 0; i < count; ++i)
        if (anchor_collision_cube_side_overlaps(moving, &position, &cubes[i]))
        {
            float top;
            /* Top landing belongs to the native ground selector. Do not
             * turn a descent through the top into horizontal separation. */
            if (!anchor_collision_cube_top_contact(moving, &position,
                                                   &cubes[i], &top))
                return 0;
        }
    *out = position;
    return 1;
}

int anchor_collision_cube_top_contact(const AnchorCollisionBody *moving,
                                      const AnchorCollisionVec3 *target,
                                      const AnchorCollisionCube *cube,
                                      float *top_y)
{
    float top, time, x, z;
    if (!moving || !target || !top_y || !valid_cube(cube) ||
        !(moving->radius > 0.0f && moving->height > 0.0f))
        return 0;
    top = cube->max.y;
    if (!(moving->position.y >= top - CUBE_CONTACT_SKIN &&
          target->y < moving->position.y && target->y <= top))
        return 0;
    time = (moving->position.y - top) /
           (moving->position.y - target->y);
    time = clamp(time, 0.0f, 1.0f);
    x = moving->position.x + (target->x - moving->position.x) * time;
    z = moving->position.z + (target->z - moving->position.z) * time;
    if (distance_to_box_squared(x, z, cube) >
        moving->radius * moving->radius)
        return 0;
    *top_y = top;
    return 1;
}

#ifndef ANCHOR_COLLISION_CUBE_GEOMETRY_ONLY
#include "combat/anchor_player_freeze_visual.h"
#ifndef ANCHOR_COLLISION_CUBE_HOST_TEST
#include "platform/modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;
extern int anchor_is_connected(void);
extern int item_sync_save_is_loaded(void);
extern int anchor_player_models_get_epoch(void);

#define CUBE_SCOPE_STACK 4
#define CUBE_RAY_STACK 8
#define CUBE_WORLD_HIT 0x7fffu

typedef struct NativeCubeRayHit
{
    float origin[3];
    float direction[3];
    float delta[3];
    float normal[3];
    unsigned char opaque[8];
    unsigned short hit;
    unsigned short padding;
    float distance_squared;
    unsigned int dynamic_object;
    unsigned int dynamic_task;
} NativeCubeRayHit;

_Static_assert(sizeof(NativeCubeRayHit) == 0x48,
               "Native ray hit record is 0x48 bytes");

typedef struct CubeSelectorScope
{
    void *task;
    unsigned short room;
    int epoch;
    unsigned int sequence;
} CubeSelectorScope;

typedef struct CubeRayOverride
{
    NativeCubeRayHit *out;
    void *task;
    unsigned short room;
    int epoch;
    int cid, session, target_epoch;
    float dx, dy, dz, distance;
    int floor;
} CubeRayOverride;

static CubeSelectorScope s_floor_scopes[CUBE_SCOPE_STACK];
static CubeSelectorScope s_ceiling_scopes[CUBE_SCOPE_STACK];
static CubeSelectorScope s_ground_scopes[CUBE_SCOPE_STACK];
static unsigned int s_floor_depth, s_ceiling_depth, s_ground_depth;
static unsigned int s_ray_depth, s_scope_sequence;
static CubeRayOverride s_ray_overrides[CUBE_RAY_STACK];

static int local_selector(void *task)
{
    return task && task == D_801FC604_5B8514 &&
           D_801FC60C_5B851C &&
           *(void **)((unsigned char *)task + 0x18) == D_801FC60C_5B851C &&
           anchor_is_connected() && item_sync_save_is_loaded() &&
           !anchor_remote_collision_is_scripted();
}

static int ray_output_pointer(const void *pointer)
{
#ifdef ANCHOR_COLLISION_CUBE_HOST_TEST
    return pointer != 0;
#else
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return physical >= 0x1000u && physical <= 0x800000u - 0x48u;
#endif
}

static void begin_scope(CubeSelectorScope *scopes, unsigned int *depth,
                        void *task)
{
    unsigned int index = (*depth)++;
    if (index >= CUBE_SCOPE_STACK)
        return;
    scopes[index].task = 0;
    scopes[index].sequence = ++s_scope_sequence;
    if (!local_selector(task))
        return;
    scopes[index].task = task;
    scopes[index].room = D_800C7AB2;
    scopes[index].epoch = anchor_player_models_get_epoch();
}

static void end_scope(CubeSelectorScope *scopes, unsigned int *depth)
{
    if (*depth)
    {
        unsigned int index = --(*depth);
        if (index < CUBE_SCOPE_STACK)
            scopes[index].task = 0;
    }
}

static CubeSelectorScope *current_scope(int *floor)
{
    CubeSelectorScope *ground = s_ground_depth &&
                                s_ground_depth <= CUBE_SCOPE_STACK ?
                                &s_ground_scopes[s_ground_depth - 1] : 0;
    CubeSelectorScope *upper = s_floor_depth &&
                                s_floor_depth <= CUBE_SCOPE_STACK ?
                                &s_floor_scopes[s_floor_depth - 1] : 0;
    CubeSelectorScope *lower = s_ceiling_depth &&
                                s_ceiling_depth <= CUBE_SCOPE_STACK ?
                                &s_ceiling_scopes[s_ceiling_depth - 1] : 0;
    CubeSelectorScope *latest = 0;
    *floor = 1;
    if (ground)
        latest = ground;
    if (upper && (!latest || upper->sequence > latest->sequence))
        latest = upper;
    if (lower && (!latest || lower->sequence > latest->sequence))
    {
        latest = lower;
        *floor = 0;
    }
    return latest;
}

static int scope_current(const CubeSelectorScope *scope)
{
    return scope && local_selector(scope->task) &&
           scope->room == D_800C7AB2 &&
           scope->epoch == anchor_player_models_get_epoch();
}

/* A floor ray meets the upper plane; an upward ceiling ray meets the lower
 * plane. The native selector retains control of grounded and ceiling flags. */
static void find_cube_ray(float x, float y, float z,
                          float dx, float dy, float dz, float range,
                          int floor, CubeRayOverride *override)
{
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    float nearest = range + 1.0f;
    int count, i;
    if (!(range > 0.0f && range <= 10000.0f) ||
        !coordinate(x) || !coordinate(y) || !coordinate(z) ||
        !coordinate(dx) || !coordinate(dy) || !coordinate(dz) ||
        !(dx * dx + dy * dy + dz * dz >= 0.95f &&
          dx * dx + dy * dy + dz * dz <= 1.05f) ||
        (floor ? dy >= -0.1f : dy <= 0.1f))
        return;
    count = anchor_player_freeze_visual_get_cubes(cubes,
                                                  ANCHOR_FREEZE_VISUAL_MAX);
    for (i = 0; i < count; ++i)
    {
        const AnchorFreezeCubeCollision *cube = &cubes[i];
        float plane, distance, hit_x, hit_z;
        if (cube->cid <= 0) /* The playable never collides with its own ice. */
            continue;
        plane = floor ? cube->cube.max.y : cube->cube.min.y;
        distance = (plane - y) / dy;
        if (distance < 0.0f || distance > range || distance >= nearest)
            continue;
        hit_x = x + dx * distance;
        hit_z = z + dz * distance;
        if (hit_x < cube->cube.min.x || hit_x > cube->cube.max.x ||
            hit_z < cube->cube.min.z || hit_z > cube->cube.max.z)
            continue;
        nearest = distance;
        override->cid = cube->cid;
        override->session = cube->session;
        override->target_epoch = cube->epoch;
        override->distance = distance;
        override->dx = dx;
        override->dy = dy;
        override->dz = dz;
    }
}

RECOMP_HOOK("func_801CEEA8_58ADB8")
void anchor_collision_cube_floor_begin(void *task)
{
    begin_scope(s_floor_scopes, &s_floor_depth, task);
}

RECOMP_HOOK_RETURN("func_801CEEA8_58ADB8")
void anchor_collision_cube_floor_end(void)
{
    end_scope(s_floor_scopes, &s_floor_depth);
}

/* Native 801D0C64 is the stable ground selector. Its accepted down-ray hit
 * sets grounded state and the proposed world feet position itself. */
RECOMP_HOOK("func_801D0C64_58CB74")
void anchor_collision_cube_ground_begin(void *task, void *primary)
{
    begin_scope(s_ground_scopes, &s_ground_depth,
                primary == D_801FC60C_5B851C ? task : 0);
}

RECOMP_HOOK_RETURN("func_801D0C64_58CB74")
void anchor_collision_cube_ground_end(void)
{
    end_scope(s_ground_scopes, &s_ground_depth);
}

RECOMP_HOOK("func_801D1358_58D268")
void anchor_collision_cube_ceiling_begin(void *task)
{
    begin_scope(s_ceiling_scopes, &s_ceiling_depth, task);
}

RECOMP_HOOK_RETURN("func_801D1358_58D268")
void anchor_collision_cube_ceiling_end(void)
{
    end_scope(s_ceiling_scopes, &s_ceiling_depth);
}

/* MIPS o32: a0 is out, a1/a2/a3 hold XYZ, and the four float direction/range
 * arguments are the caller's stack arguments 5..8. Match the native ABI. */
RECOMP_HOOK("func_8002C9D4_2D5D4")
void anchor_collision_cube_ray_begin(void *out, float x, float y, float z,
                                     float dx, float dy, float dz, float range)
{
    CubeSelectorScope *scope;
    CubeRayOverride *override;
    unsigned int index = s_ray_depth++;
    int floor = 0;
    if (index >= CUBE_RAY_STACK)
        return;
    override = &s_ray_overrides[index];
    override->out = 0;
    scope = current_scope(&floor);
    if (!scope_current(scope) || !ray_output_pointer(out) ||
        ((unsigned long)out & 3u))
        return;
    override->cid = 0;
    find_cube_ray(x, y, z, dx, dy, dz, range, floor, override);
    if (override->cid <= 0)
        return;
    override->out = out;
    override->task = scope->task;
    override->room = scope->room;
    override->epoch = scope->epoch;
    override->floor = floor;
}

RECOMP_HOOK_RETURN("func_8002C9D4_2D5D4")
void anchor_collision_cube_ray_end(void)
{
    CubeRayOverride *override;
    CubeSelectorScope *scope;
    NativeCubeRayHit *hit;
    float distance_squared;
    unsigned int index, i;
    int floor = 0;
    if (!s_ray_depth)
        return;
    index = --s_ray_depth;
    if (index >= CUBE_RAY_STACK)
        return;
    override = &s_ray_overrides[index];
    hit = override->out;
    override->out = 0;
    scope = current_scope(&floor);
    if (!hit || !scope_current(scope) || floor != override->floor ||
        scope->task != override->task || scope->room != override->room ||
        scope->epoch != override->epoch ||
        !anchor_player_freeze_visual_has_cube(
            override->cid, override->session, override->target_epoch))
        return;
    distance_squared = override->distance * override->distance;
    if (hit->hit == CUBE_WORLD_HIT &&
        hit->distance_squared >= 0.0f &&
        hit->distance_squared <= distance_squared + CUBE_EPSILON)
        return; /* Static or dynamic native collision is already nearer. */
    hit->delta[0] = override->dx * override->distance;
    hit->delta[1] = override->dy * override->distance;
    hit->delta[2] = override->dz * override->distance;
    hit->normal[0] = 0.0f;
    hit->normal[1] = floor ? 1.0f : -1.0f;
    hit->normal[2] = 0.0f;
    for (i = 0; i < sizeof(hit->opaque); ++i)
        hit->opaque[i] = 0;
    hit->hit = CUBE_WORLD_HIT;
    hit->padding = 0;
    hit->distance_squared = distance_squared;
    hit->dynamic_object = hit->dynamic_task = 0;
}
#endif
