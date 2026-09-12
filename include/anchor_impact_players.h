#ifndef ANCHOR_IMPACT_PLAYERS_H
#define ANCHOR_IMPACT_PLAYERS_H

/* Call after the shared Impact coordinator. Reticles and received shots use
 * display-only children. Shared combat inputs execute through the owner's
 * native interpreter so all participants use one mech and ammunition pool. */
void anchor_impact_players_tick(int connected);
void anchor_impact_players_reset(void);
void anchor_impact_players_hide_shots(void);
void anchor_impact_players_set_authority(unsigned int owner, unsigned int term);

#endif
