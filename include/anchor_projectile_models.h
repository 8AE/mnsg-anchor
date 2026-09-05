#ifndef ANCHOR_PROJECTILE_MODELS_H
#define ANCHOR_PROJECTILE_MODELS_H

#include "anchor_projectiles.h"

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
void anchor_projectile_models_load_resources(void);

#endif
