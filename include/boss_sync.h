#ifndef MNSG_BOSS_SYNC_H
#define MNSG_BOSS_SYNC_H

/* Boss completion is split into two network events:
 *
 *  - MNSG_BOSS_DEFEAT is transient and tells a client that is currently in
 *    the encounter to apply a lethal hit through the game's native damage
 *    routine before the boss-specific death update runs.
 *  - SET_FLAG remains durable progression for clients that are elsewhere or
 *    offline.
 *
 * Keeping those responsibilities separate prevents a save bit from starting
 * a victory cutscene while the local boss actor is still alive.
 */
int boss_sync_is_completion_flag(const char *flag_name);
int boss_sync_should_defer_flag(const char *flag_name);
int boss_sync_send_defeat(const char *flag_name);
int boss_sync_apply_remote_defeat(const char *flag_name);
int boss_sync_has_active_encounter(const char *flag_name);
int boss_sync_has_local_encounter(const char *flag_name);
void boss_sync_reset(void);

#endif
