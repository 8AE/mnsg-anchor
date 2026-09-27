#ifndef ANCHOR_PROJECTILE_MODELS_H
#define ANCHOR_PROJECTILE_MODELS_H

#include "combat/anchor_projectiles.h"

/* Native Fire Ryo kind 0x12 leaves four independently fading particles. */
#define ANCHOR_FIRE_RYO_TRAIL_COUNT 4
typedef struct AnchorFireRyoTrail
{
    float x, y, z, scale;
    unsigned short rx, ry, rz;
    int alpha;
} AnchorFireRyoTrail;

void anchor_projectile_fire_ryo_trail_step(
    AnchorFireRyoTrail trails[ANCHOR_FIRE_RYO_TRAIL_COUNT],
    int flight, int native_tick, float x, float y, float z,
    unsigned short rx, unsigned short ry, unsigned short rz);

/* Pure native-recipe/material validation, also exercised by host tests. */
int anchor_projectile_recipe_id(unsigned int model, int family);
int anchor_projectile_material_decode(unsigned int context,
                                      const unsigned int *commands,
                                      int *material, unsigned int *prim_rgb,
                                      unsigned int *env_rgba);
int anchor_projectile_material_build(int material, unsigned int prim_rgb,
                                     unsigned int env_rgba,
                                     unsigned int commands[8]);

/* Spawn events are consumed once; native display tasks then simulate locally.
 * A zero return requests retry while the owner/resources are becoming ready. */
int anchor_projectile_models_spawn(const AnchorProjectileRemote *remote, void *owner);
void anchor_projectile_models_tick(void *owner);
void anchor_projectile_models_reset(void);
void anchor_projectile_models_stop(int cid, int session, int epoch, int event_id);
void anchor_projectile_models_load_resources(void);

#endif
