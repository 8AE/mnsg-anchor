#ifndef ANCHOR_PROJECTILE_SOURCE_H
#define ANCHOR_PROJECTILE_SOURCE_H

#include "anchor_projectiles.h"
#define ANCHOR_PROJECTILE_SOURCE_MAX 64
#define ANCHOR_PROJECTILE_SOURCE_TTL 23u

typedef struct AnchorProjectilePendingSpawn
{
    AnchorProjectileSpawn spawn;
    unsigned int tick;
} AnchorProjectilePendingSpawn;

typedef struct AnchorProjectileSourceState
{
    const void *tasks[ANCHOR_PROJECTILE_SOURCE_MAX];
    AnchorProjectilePendingSpawn pending[ANCHOR_PROJECTILE_SOURCE_MAX];
    int next_id;
    unsigned int head, count;
} AnchorProjectileSourceState;

void anchor_projectile_source_reset(AnchorProjectileSourceState *state);
void anchor_projectile_source_clear_pending(AnchorProjectileSourceState *state);
/* Remembers the native lifetime even when publishing is disabled, preventing
 * a connection made mid-flight from treating an old shot as a new throw. */
int anchor_projectile_source_capture(AnchorProjectileSourceState *state,
                                     const void *task, const AnchorProjectileSpawn *spawn,
                                     unsigned int tick, int publish);
void anchor_projectile_source_forget_task(AnchorProjectileSourceState *state,
                                         const void *task);
const AnchorProjectileSpawn *anchor_projectile_source_peek(
    AnchorProjectileSourceState *state, unsigned int tick);
void anchor_projectile_source_ack(AnchorProjectileSourceState *state);

#endif
