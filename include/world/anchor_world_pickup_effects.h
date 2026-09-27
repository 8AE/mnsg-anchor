#ifndef ANCHOR_WORLD_PICKUP_EFFECTS_H
#define ANCHOR_WORLD_PICKUP_EFFECTS_H

/* The returned native category-8 pickup child is independent of its source
 * actor. Keep only remote coin bursts in a bounded render-material registry. */
void anchor_world_pickup_effects_load_resources(void);
void anchor_world_pickup_effects_reset(void);
void anchor_world_pickup_effects_spawn(void *actor, unsigned short room);

#endif
