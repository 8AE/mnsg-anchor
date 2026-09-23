#ifndef ANCHOR_IMPACT_STAGE_H
#define ANCHOR_IMPACT_STAGE_H

#include "bosses/anchor_boss_arenas.h"

/* Boss rush advances the stage after each fight: 0x260 Kashiwagi,
 * 0x261 Taisamba 2, 0x262 Balberra, 0x263 D'Etoile. FUN_80037000
 * returns to 0x25F after the last fight. These stages are battle scopes,
 * separate from the story stages that support arena invitations. */
#define ANCHOR_IMPACT_RUSH_FIRST 0x0260u
#define ANCHOR_IMPACT_RUSH_LAST 0x0263u

static inline int anchor_impact_stage_valid(unsigned int stage)
{
    return ANCHOR_BOSS_IMPACT_STAGE_VALID(stage) ||
           (stage >= ANCHOR_IMPACT_RUSH_FIRST && stage <= ANCHOR_IMPACT_RUSH_LAST);
}

/* During a rush handoff the stage changes before the previous root/selector
 * is replaced. That old graph is no longer a snapshot or gameplay target. */
static inline int anchor_impact_stage_matches(unsigned int stage, unsigned int encounter)
{
    return anchor_impact_stage_valid(stage) &&
           (stage < ANCHOR_IMPACT_RUSH_FIRST || stage > ANCHOR_IMPACT_RUSH_LAST ||
            encounter == stage - ANCHOR_IMPACT_RUSH_FIRST + 1u);
}

#endif
