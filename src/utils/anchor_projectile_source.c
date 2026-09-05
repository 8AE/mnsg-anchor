#include "anchor_projectile_source.h"

void anchor_projectile_source_clear_pending(AnchorProjectileSourceState *state)
{
    state->head = state->count = 0;
}

void anchor_projectile_source_reset(AnchorProjectileSourceState *state)
{
    int i;
    for (i = 0; i < ANCHOR_PROJECTILE_SOURCE_MAX; ++i)
        state->tasks[i] = 0;
    anchor_projectile_source_clear_pending(state);
    /* Keep IDs monotonic across connection/life resets. */
}

int anchor_projectile_source_capture(AnchorProjectileSourceState *state,
                                     const void *task, const AnchorProjectileSpawn *spawn,
                                     unsigned int tick, int publish)
{
    int i;
    int free_slot = -1;
    unsigned int tail;
    if (!task || !spawn)
        return 0;
    for (i = 0; i < ANCHOR_PROJECTILE_SOURCE_MAX; ++i)
    {
        if (state->tasks[i] == task)
            return 0;
        if (!state->tasks[i] && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0)
        return 0;
    state->tasks[free_slot] = task;
    if (!publish || state->count == ANCHOR_PROJECTILE_SOURCE_MAX)
        return 0;
    state->next_id = state->next_id == 0x7fffffff ? 1 : state->next_id + 1;
    tail = (state->head + state->count) % ANCHOR_PROJECTILE_SOURCE_MAX;
    state->pending[tail].spawn = *spawn;
    state->pending[tail].spawn.id = state->next_id;
    state->pending[tail].tick = tick;
    ++state->count;
    return 1;
}

void anchor_projectile_source_forget_task(AnchorProjectileSourceState *state,
                                         const void *task)
{
    int i;
    for (i = 0; i < ANCHOR_PROJECTILE_SOURCE_MAX; ++i)
        if (state->tasks[i] == task)
            state->tasks[i] = 0;
    /* The throw event survives destruction of the source projectile. */
}

void anchor_projectile_source_ack(AnchorProjectileSourceState *state)
{
    if (state->count)
    {
        state->head = (state->head + 1u) % ANCHOR_PROJECTILE_SOURCE_MAX;
        --state->count;
    }
}

const AnchorProjectileSpawn *anchor_projectile_source_peek(
    AnchorProjectileSourceState *state, unsigned int tick)
{
    while (state->count)
    {
        AnchorProjectilePendingSpawn *pending = &state->pending[state->head];
        if (tick - pending->tick <= ANCHOR_PROJECTILE_SOURCE_TTL)
            return &pending->spawn;
        anchor_projectile_source_ack(state);
    }
    return 0;
}
