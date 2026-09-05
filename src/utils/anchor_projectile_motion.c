#include "anchor_projectile_motion.h"

static int bomb(int kind)
{
    return kind >= 0x0e && kind <= 0x10;
}

static int rocket(int kind)
{
    return kind >= 0x17 && kind <= 0x19;
}

int anchor_projectile_kind_supported(int kind)
{
    return kind == 1 || kind == 2 || kind == 0x0c || kind == 0x0d ||
           bomb(kind) || rocket(kind) || kind == 0x1a || kind == 0x1b;
}

static AnchorCollisionVec3 next_velocity(const AnchorProjectileMotion *motion)
{
    AnchorCollisionVec3 velocity = motion->velocity;
    float length_squared;
    velocity.x += motion->acceleration.x;
    velocity.y += motion->acceleration.y;
    velocity.z += motion->acceleration.z;
    if (rocket(motion->kind))
    {
        length_squared = velocity.x * velocity.x + velocity.y * velocity.y +
                         velocity.z * velocity.z;
        if (length_squared > 36.0f)
        {
            float scale = 6.0f / __builtin_sqrtf(length_squared);
            velocity.x *= scale;
            velocity.y *= scale;
            velocity.z *= scale;
        }
    }
    return velocity;
}

int anchor_projectile_motion_init(AnchorProjectileMotion *motion,
                                  const AnchorProjectileSpawn *spawn)
{
    float length_squared;
    if (!motion || !spawn || !anchor_projectile_kind_supported(spawn->kind) ||
        spawn->scale100000 <= 0 || spawn->scale100000 > 1000000)
        return 0;
    motion->position.x = (float)spawn->x100 / 100.0f;
    motion->position.y = (float)spawn->y100 / 100.0f;
    motion->position.z = (float)spawn->z100 / 100.0f;
    motion->velocity.x = (float)spawn->vx100 / 100.0f;
    motion->velocity.y = (float)spawn->vy100 / 100.0f;
    motion->velocity.z = (float)spawn->vz100 / 100.0f;
    motion->acceleration.x = motion->acceleration.y = motion->acceleration.z = 0;
    motion->kind = spawn->kind;
    motion->phase = ANCHOR_SHOT_FLIGHT;
    motion->alive = 1;
    motion->age = motion->phase_age = 0;
    motion->alpha = 255;
    motion->gray = 128;
    motion->scale = (float)spawn->scale100000 / 100000.0f;
    motion->frame = 0;
    motion->clip_frames = 0; /* Filled by the native model frame-count reader. */
    /* Events are captured after the first native update. Bomb initialization
     * and camera activation do not consume a flight tick; other types do. */
    motion->ttl = 119;
    if (bomb(spawn->kind))
    {
        motion->ttl = 90;
        motion->acceleration.y = -0.6666666269302368f; /* D_8020BA74 */
    }
    else if (spawn->kind == 0x1a || spawn->kind == 0x1b)
        motion->ttl = 59;
    else if (rocket(spawn->kind))
    {
        motion->ttl = 89;
        length_squared = motion->velocity.x * motion->velocity.x +
                         motion->velocity.y * motion->velocity.y +
                         motion->velocity.z * motion->velocity.z;
        if (length_squared > 0.0001f)
        {
            float scale = 1.0f / __builtin_sqrtf(length_squared);
            motion->acceleration.x = motion->velocity.x * scale;
            motion->acceleration.y = motion->velocity.y * scale;
            motion->acceleration.z = motion->velocity.z * scale;
        }
    }
    else if (spawn->kind == 2)
    {
        /* Native constructor acceleration is -initial_velocity/6. The first
         * captured update has already applied it: velocity is now 5/6. */
        motion->acceleration.x = -motion->velocity.x / 5.0f;
        motion->acceleration.y = -motion->velocity.y / 5.0f;
        motion->acceleration.z = -motion->velocity.z / 5.0f;
        motion->ttl = 12;
    }
    else if (spawn->kind == 0x0c || spawn->kind == 0x0d)
        motion->phase = ANCHOR_SHOT_FLASH;
    return 1;
}

void anchor_projectile_motion_target(const AnchorProjectileMotion *motion,
                                     AnchorCollisionVec3 *target)
{
    AnchorCollisionVec3 velocity;
    *target = motion->position;
    if (!motion->alive || (motion->phase != ANCHOR_SHOT_FLIGHT &&
                           motion->phase != ANCHOR_SHOT_RETURN))
        return;
    velocity = next_velocity(motion);
    target->x += velocity.x;
    target->y += velocity.y;
    target->z += velocity.z;
}

static void begin_impact(AnchorProjectileMotion *motion)
{
    motion->phase_age = 0;
    motion->frame = 0;
    motion->alpha = 255;
    motion->clip_frames = 0;
    if (motion->kind == 2)
    {
        motion->phase = ANCHOR_SHOT_RETURN;
        motion->ttl = (motion->age > 0 ? motion->age : 1) + 1;
        motion->velocity.x = -motion->velocity.x;
        motion->velocity.y = -motion->velocity.y;
        motion->velocity.z = -motion->velocity.z;
    }
    else if (bomb(motion->kind))
    {
        motion->phase = ANCHOR_SHOT_GROW;
        motion->scale = 0.025f; /* D_8020BA88; native impact begins small. */
    }
    else
    {
        motion->phase = ANCHOR_SHOT_IMPACT;
        motion->scale = motion->kind == 1 || rocket(motion->kind) ? 0.3f : 0.15f;
    }
}

void anchor_projectile_motion_step(AnchorProjectileMotion *motion,
                                   const AnchorCollisionVec3 *impact)
{
    if (!motion || !motion->alive)
        return;
    ++motion->age;
    if (motion->phase == ANCHOR_SHOT_FLIGHT || motion->phase == ANCHOR_SHOT_RETURN)
    {
        AnchorCollisionVec3 target;
        if (--motion->ttl <= 0)
        {
            motion->alive = 0;
            return;
        }
        anchor_projectile_motion_target(motion, &target);
        motion->velocity = next_velocity(motion);
        motion->position = impact ? *impact : target;
        if (impact && motion->phase == ANCHOR_SHOT_FLIGHT)
            begin_impact(motion);
        else if (motion->kind == 2 && motion->age == 5)
            motion->phase = ANCHOR_SHOT_RETURN;
        return;
    }
    ++motion->phase_age;
    if (motion->phase == ANCHOR_SHOT_GROW)
    {
        float growth_time = (float)motion->phase_age * 0.3f;
        motion->scale += 0.01f * growth_time * growth_time;
        if (motion->scale > 0.15f)
        {
            motion->scale = 0.15f;
            motion->phase = ANCHOR_SHOT_FADE;
        }
        return;
    }
    if (motion->kind == 1 && motion->phase == ANCHOR_SHOT_IMPACT)
    {
        motion->frame += 1.5f;
        if ((motion->clip_frames > 1.0f && motion->frame >= motion->clip_frames - 1.0f) ||
            motion->phase_age >= 30)
            motion->alive = 0;
        return;
    }
    motion->alpha -= (motion->kind == 0x1a || motion->kind == 0x1b) ? 12 : 24;
    if (motion->alpha <= 0)
    {
        motion->alpha = 0;
        motion->alive = 0;
        return;
    }
    if (motion->phase == ANCHOR_SHOT_FADE)
    {
        motion->scale += 0.002f;
        motion->gray = motion->gray >= 12 ? motion->gray - 12 : 0;
    }
    else if (motion->phase == ANCHOR_SHOT_IMPACT)
    {
        motion->frame += 1.0f;
        if (rocket(motion->kind) && motion->clip_frames > 1.0f &&
            motion->frame >= motion->clip_frames - 1.0f)
            motion->alive = 0;
    }
}
