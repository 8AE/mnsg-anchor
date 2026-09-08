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
enum BossSyncProgressSendResult
{
    BOSS_SYNC_PROGRESS_SEND_FAILED = 0,
    BOSS_SYNC_PROGRESS_SENT = 1,
    BOSS_SYNC_PROGRESS_SUPPRESSED = 2
};
/* Remote Dharumanyo replicas run the native reward scripts locally. Route
 * their reward-owned save changes through this gate so only the original
 * simulator publishes duplicate durable deltas. Other progress is unchanged. */
int boss_sync_is_darumanyo_reward_progress(const char *flag_name);
int boss_sync_is_tsurami_reward_progress(const char *flag_name);
int boss_sync_send_local_progress(const char *flag_name, int value,
                                  int add_to_queue);
/* A shared Dharumanyo terminal checkpoint enters the same verified native
 * last-life path used by the terminal compatibility packet. */
int boss_sync_queue_darumanyo_shared_terminal(void);
/* Tsurami's HP-one reaction starts the native destruction sequence. */
int boss_sync_queue_tsurami_shared_terminal(void);
/* File73 has released pickup controls and requested the native room exit. */
void boss_sync_finish_tsurami_reward_scene(void);
void boss_sync_reset(void);

#endif
