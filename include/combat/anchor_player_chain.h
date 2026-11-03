#ifndef ANCHOR_PLAYER_CHAIN_H
#define ANCHOR_PLAYER_CHAIN_H

struct AnchorPlayerAttackSample;
struct AnchorPlayerHitTarget;

/* Observe copied overlaps only. Native collision results retain precedence. */
void anchor_player_chain_observe(const struct AnchorPlayerAttackSample *sample,
                                  const struct AnchorPlayerHitTarget *target);
int anchor_player_chain_is_native(const void *task);
/* Observe each native collision result. Grapples commit only at the next
 * chain-head update, after all native victim groups have been tested. */
void anchor_player_chain_sphere_begin(void *task);
void anchor_player_chain_sphere_end(void);
/* Copied native endpoint for ordinary player presentation; no native pointers. */
int anchor_player_chain_visual_snapshot(int out[4]);

#endif
