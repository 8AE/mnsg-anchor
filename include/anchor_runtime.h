#ifndef ANCHOR_RUNTIME_H
#define ANCHOR_RUNTIME_H

void anchor_runtime_init_defaults(void);
void anchor_runtime_set_damage_sync_enabled(int enabled);
int anchor_runtime_damage_sync_enabled(void);
void anchor_runtime_set_no_hit_enabled(int enabled);
int anchor_runtime_no_hit_enabled(void);
void anchor_runtime_set_one_life_enabled(int enabled);
int anchor_runtime_one_life_enabled(void);
void anchor_runtime_set_enemy_multiplier(int multiplier);
int anchor_runtime_enemy_multiplier(void);

void anchor_startup_menu_finish(void);
int anchor_startup_menu_is_complete(void);
void anchor_startup_menu_open(void);
void anchor_startup_multiplayer_open(void);
void anchor_startup_multiplayer_open_for_race(void);
void anchor_startup_race_open(void);
void anchor_race_lobby_open(void);
int anchor_race_start_from_lobby(void);
const char *anchor_race_get_config_json(void);
void anchor_race_apply_config_json(const char *json);

#endif
