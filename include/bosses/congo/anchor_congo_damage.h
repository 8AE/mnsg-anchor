#ifndef ANCHOR_CONGO_DAMAGE_H
#define ANCHOR_CONGO_DAMAGE_H

typedef struct AnchorCongoHit
{
    int sequence;
    int amount;
} AnchorCongoHit;

/* active includes the connected state waiting for its first checkpoint.
 * encounter changes on a new encounter, not an authority handoff. */
void anchor_congo_damage_set_context(int active, int owner, int paused,
                                     unsigned int encounter);
void anchor_congo_damage_bind_root(void *actor);
void anchor_congo_damage_reset(void);
int anchor_congo_damage_take_local_hit(AnchorCongoHit *out);
int anchor_congo_damage_apply(int amount);

/* The terminal-only compatibility path must not independently kill a shared
 * follower; the native snapshot layer starts that client's victory instead. */
int anchor_congo_damage_is_shared(void);
int anchor_congo_damage_is_owner(void);
int anchor_congo_damage_is_root(const void *actor);

#endif
