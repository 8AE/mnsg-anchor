#ifndef ANCHOR_RENDER_SCRATCH_H
#define ANCHOR_RENDER_SCRATCH_H

typedef struct AnchorRenderBudget
{
    unsigned int commands;
    unsigned int matrices;
} AnchorRenderBudget;

/* Resolve an encoded native model address and a readable byte range. */
typedef const void *(*AnchorRenderResolve)(unsigned int address,
                                          unsigned int bytes,
                                          const void *context);

/* Bounds the native case-6 clothed-player path before it can write graphics
 * memory. The resolver must reject ranges outside the object's bound assets.
 * Limits are available storage, not a count of players. */
int anchor_render_player_budget(const void *object,
                                AnchorRenderResolve resolve,
                                const void *context,
                                const AnchorRenderBudget *available,
                                AnchorRenderBudget *out);

int anchor_render_scratch_plan(unsigned int cursor, unsigned int *start,
                               unsigned int *bank_bytes, unsigned int *end);

/* Call after all remote character, expression and projectile resources have
 * been staged. Scratch is retained until the native scene registry resets. */
void anchor_render_scratch_load_resources(void);

#endif
