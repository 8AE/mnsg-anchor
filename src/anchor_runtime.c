#include "anchor_runtime.h"

static int s_initialized = 0;
static int s_damage_sync_enabled = 0;
static int s_no_hit_enabled = 0;
static int s_one_life_enabled = 0;
static int s_ryo_sync_enabled = 0;
static int s_enemy_multiplier = 1;
static int s_startup_complete = 0;

void anchor_runtime_init_defaults(void)
{
    if (s_initialized)
        return;

    s_initialized = 1;
    s_damage_sync_enabled = 0;
    s_no_hit_enabled = 0;
    s_one_life_enabled = 0;
    s_ryo_sync_enabled = 0;
    s_enemy_multiplier = 1;
}

void anchor_runtime_set_damage_sync_enabled(int enabled)
{
    anchor_runtime_init_defaults();
    s_damage_sync_enabled = enabled ? 1 : 0;
}

int anchor_runtime_damage_sync_enabled(void)
{
    anchor_runtime_init_defaults();
    return s_damage_sync_enabled;
}

void anchor_runtime_set_no_hit_enabled(int enabled)
{
    anchor_runtime_init_defaults();
    s_no_hit_enabled = enabled ? 1 : 0;
}

int anchor_runtime_no_hit_enabled(void)
{
    anchor_runtime_init_defaults();
    return s_no_hit_enabled;
}

void anchor_runtime_set_one_life_enabled(int enabled)
{
    anchor_runtime_init_defaults();
    s_one_life_enabled = enabled ? 1 : 0;
}

int anchor_runtime_one_life_enabled(void)
{
    anchor_runtime_init_defaults();
    return s_one_life_enabled;
}

void anchor_runtime_set_ryo_sync_enabled(int enabled)
{
    anchor_runtime_init_defaults();
    s_ryo_sync_enabled = enabled ? 1 : 0;
}

int anchor_runtime_ryo_sync_enabled(void)
{
    anchor_runtime_init_defaults();
    return s_ryo_sync_enabled;
}

void anchor_runtime_set_enemy_multiplier(int multiplier)
{
    anchor_runtime_init_defaults();
    if (multiplier < 1)
        multiplier = 1;
    if (multiplier > 3)
        multiplier = 3;
    s_enemy_multiplier = multiplier;
}

int anchor_runtime_enemy_multiplier(void)
{
    anchor_runtime_init_defaults();
    return s_enemy_multiplier;
}

void anchor_startup_menu_finish(void)
{
    s_startup_complete = 1;
}

int anchor_startup_menu_is_complete(void)
{
    return s_startup_complete;
}
