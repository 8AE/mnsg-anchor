#ifndef ANCHOR_DHARUMANYO_DAMAGE_H
#define ANCHOR_DHARUMANYO_DAMAGE_H

typedef struct AnchorDharumanyoHit
{
    int sequence;
    int amount;
} AnchorDharumanyoHit;

/* Dharumanyo consumes one real life for every accepted physical hit,
 * independent of the local attack-strength byte. */
void anchor_dharumanyo_damage_set_context(int active, int owner, int paused,
                                           unsigned int encounter);
void anchor_dharumanyo_damage_bind(void *root, void *carrier);
void anchor_dharumanyo_damage_reset(void);
int anchor_dharumanyo_damage_take_local_hit(AnchorDharumanyoHit *out);
int anchor_dharumanyo_damage_apply(int amount);
int anchor_dharumanyo_damage_is_shared(void);
int anchor_dharumanyo_damage_is_owner(void);
int anchor_dharumanyo_damage_is_carrier(const void *actor);

#endif
