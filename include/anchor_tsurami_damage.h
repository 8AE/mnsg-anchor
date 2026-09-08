#ifndef ANCHOR_TSURAMI_DAMAGE_H
#define ANCHOR_TSURAMI_DAMAGE_H

typedef struct AnchorTsuramiHit {
    int sequence;
    int amount;
    unsigned int target; /* 0 is the root, otherwise a stable reflectable projectile ID. */
} AnchorTsuramiHit;

void anchor_tsurami_damage_set_context(int active, int owner, int paused,
                                       unsigned int encounter);
void anchor_tsurami_damage_bind_root(void *actor);
void anchor_tsurami_damage_reset(void);
int anchor_tsurami_damage_take_local_hit(AnchorTsuramiHit *out);
/* Bounded delivery queue; flush only in the root's mapped native pre callback
 * after pending checkpoint adoption, with the world running. */
int anchor_tsurami_damage_apply(int amount, unsigned int target);
void anchor_tsurami_damage_flush(void);
/* A checkpoint may acknowledge delivered hits only after mapped processing. */
int anchor_tsurami_damage_pending(void);
/* Terminal adoption rejects outstanding combat intents before publication. */
void anchor_tsurami_damage_discard_pending(void);
int anchor_tsurami_damage_is_shared(void);
int anchor_tsurami_damage_is_owner(void);
int anchor_tsurami_damage_is_root(const void *actor);
#endif
