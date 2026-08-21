#ifndef MNSG_ITEM_SYNC_H
#define MNSG_ITEM_SYNC_H

int item_sync_save_is_loaded(void);
void item_sync_apply_benkei_postfight_state(void);
void item_sync_mark_boss_defeat_announced(const char *flag_name);
void item_sync_commit_boss_completion(const char *flag_name);

void item_sync_force_flag(const char *name);
void item_sync_force_flag_val(const char *name, int value);
int item_sync_write_local_flag_val(const char *name, int value);

#endif
