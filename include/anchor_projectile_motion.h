#ifndef ANCHOR_PROJECTILE_MOTION_H
#define ANCHOR_PROJECTILE_MOTION_H

#include "anchor_projectiles.h"
#include "anchor_remote_collision.h"

enum {
    ANCHOR_SHOT_FLIGHT, ANCHOR_SHOT_IMPACT, ANCHOR_SHOT_GROW,
    ANCHOR_SHOT_FADE, ANCHOR_SHOT_FLASH, ANCHOR_SHOT_RETURN
};

typedef struct AnchorProjectileMotion
{
    AnchorCollisionVec3 position;
    AnchorCollisionVec3 velocity;
    AnchorCollisionVec3 acceleration;
    int kind, phase, alive, age, phase_age, ttl;
    int alpha, gray;
    float scale, frame, clip_frames;
} AnchorProjectileMotion;

int anchor_projectile_kind_supported(int kind);
int anchor_projectile_motion_init(AnchorProjectileMotion *motion,
                                  const AnchorProjectileSpawn *spawn);
void anchor_projectile_motion_target(const AnchorProjectileMotion *motion,
                                     AnchorCollisionVec3 *target);
void anchor_projectile_motion_step(AnchorProjectileMotion *motion,
                                   const AnchorCollisionVec3 *impact);

#endif
