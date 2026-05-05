#ifndef ANCHOR_RUNTIME_H
#define ANCHOR_RUNTIME_H

void anchor_runtime_init_defaults(void);
void anchor_runtime_set_damage_sync_enabled(int enabled);
int anchor_runtime_damage_sync_enabled(void);

void anchor_startup_menu_finish(void);
int anchor_startup_menu_is_complete(void);
void anchor_startup_menu_open(void);
void anchor_startup_multiplayer_open(void);
void anchor_startup_multiplayer_open_for_race(void);
void anchor_startup_race_open(void);

#endif
