#ifndef ANCHOR_IMPACT_DAMAGE_H
#define ANCHOR_IMPACT_DAMAGE_H

/* One physical Impact boss hit. The amount is the native HP delta observed on
 * a follower and applied verbatim by the elected authority. */
typedef struct AnchorImpactHit
{
    int sequence;
    int amount;
} AnchorImpactHit;

void anchor_impact_damage_set_context(int active, int owner, int paused,
                                      unsigned int encounter);
void anchor_impact_damage_reset(void);
int anchor_impact_damage_take_local_hit(AnchorImpactHit *out);
int anchor_impact_damage_apply(int amount);
int anchor_impact_damage_is_shared(void);
int anchor_impact_damage_is_owner(void);

#endif
