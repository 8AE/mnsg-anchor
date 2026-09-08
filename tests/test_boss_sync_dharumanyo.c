#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile the production state machine directly while replacing only its
 * platform/network/save boundaries. */
#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
void recomp_free(void *memory);
int recomp_printf(const char *format, ...);

#include "../src/boss_sync.c"

typedef union TestActor
{
    void *alignment;
    unsigned char bytes[0x200];
} TestActor;

unsigned short D_800C7AB2;
static unsigned char flags[0x200];
static int save_loaded;
static int dharumanyo_shared;
static int dharumanyo_owner;
static int packet_calls;
static int announce_calls;
static int commit_calls;
static int flag_send_calls;
static int terminal_finish_calls;

int anchor_miracle_star_local_scene_active(void) { return 0; }
void anchor_tsurami_native_finish_terminal(void) {}
int anchor_congo_damage_is_shared(void) { return 0; }
int anchor_congo_damage_is_owner(void) { return 0; }
int anchor_tsurami_damage_is_shared(void) { return 0; }
int anchor_tsurami_damage_is_owner(void) { return 0; }
int anchor_dharumanyo_damage_is_shared(void) { return dharumanyo_shared; }
int anchor_dharumanyo_damage_is_owner(void) { return dharumanyo_owner; }
int item_sync_save_is_loaded(void) { return save_loaded; }
void item_sync_apply_benkei_postfight_state(void) {}
void anchor_dharumanyo_native_finish_terminal(void)
{
    ++terminal_finish_calls;
}

void item_sync_mark_boss_defeat_announced(const char *flag_name)
{
    assert(mnsg_string_equal(flag_name, "fl_dharmanyo"));
    ++announce_calls;
}

void item_sync_commit_boss_completion(const char *flag_name)
{
    assert(mnsg_string_equal(flag_name, "fl_dharmanyo"));
    assert(flags[DARUMANYO_KILL_FLAG]);
    ++commit_calls;
}

char *anchor_get_team_id(void)
{
    char *team = malloc(5);
    assert(team);
    memcpy(team, "team", 5);
    return team;
}

int anchor_send_custom_packet(const char *packet_type, const char *payload_json,
                              const char *target_team_id,
                              unsigned int target_client_id, int add_to_queue)
{
    assert(mnsg_string_equal(packet_type, "MNSG_BOSS_DEFEAT"));
    assert(mnsg_string_equal(payload_json, "{\"flag\":\"fl_dharmanyo\"}"));
    assert(mnsg_string_equal(target_team_id, "team"));
    assert(!target_client_id && !add_to_queue);
    ++packet_calls;
    return 1;
}

int anchor_send_flag(const char *flag_name, int value, int add_to_queue)
{
    assert(flag_name && flag_name[0]);
    assert(value == 1 && add_to_queue == 1);
    ++flag_send_calls;
    return 1;
}

void func_80024088_24C88(int flag_id)
{
    assert(flag_id >= 0 && flag_id < (int)sizeof(flags));
    flags[flag_id] = 0;
}

int func_800240DC_24CDC(int flag_id)
{
    assert(flag_id >= 0 && flag_id < (int)sizeof(flags));
    return flags[flag_id] != 0;
}

void func_80034EF8_35AF8(void *actor) { (void)actor; }
void recomp_free(void *memory) { free(memory); }
int recomp_printf(const char *format, ...)
{
    (void)format;
    return 0;
}

static void reset_fixture(void)
{
    save_loaded = 0;
    boss_sync_reset();
    memset(flags, 0, sizeof(flags));
    save_loaded = 1;
    dharumanyo_shared = 1;
    dharumanyo_owner = 0;
    packet_calls = 0;
    announce_calls = 0;
    commit_calls = 0;
    flag_send_calls = 0;
    terminal_finish_calls = 0;
    D_800C7AB2 = DARUMANYO_ROOM;
}

static void remote_reward_send_gate_test(void)
{
    static const char *reward_progress[] = {
        "mi_flower", "cs_dhrm_1", "cs_dhrm_2", "cs_dhrm_3", "cs_dhrm_4"
    };
    TestActor carrier = {0};
    unsigned int i;

    reset_fixture();
    ACTOR_ENTITY_ID(carrier.bytes) = ENTITY_DARUMANYO;
    ACTOR_STATUS(carrier.bytes) = ACTOR_STATUS_ACTIVE;
    ACTOR_HEALTH(carrier.bytes) = 10;
    DARUMANYO_LIVES(carrier.bytes) = 12;
    boss_sync_capture_darumanyo_setup(carrier.bytes);
    assert(boss_sync_queue_darumanyo_shared_terminal());

    for (i = 0; i < sizeof(reward_progress) / sizeof(reward_progress[0]); ++i)
    {
        assert(boss_sync_send_local_progress(reward_progress[i], 1, 1) ==
               BOSS_SYNC_PROGRESS_SUPPRESSED);
    }
    assert(flag_send_calls == 0);
    assert(boss_sync_send_local_progress("ki_triton", 1, 1) ==
           BOSS_SYNC_PROGRESS_SENT);
    assert(flag_send_calls == 1);

    reset_fixture();
    dharumanyo_owner = 1;
    for (i = 0; i < sizeof(reward_progress) / sizeof(reward_progress[0]); ++i)
    {
        assert(boss_sync_send_local_progress(reward_progress[i], 1, 1) ==
               BOSS_SYNC_PROGRESS_SENT);
    }
    assert(flag_send_calls == 5);
}

static void legacy_packet_test(void)
{
    reset_fixture();
    assert(!boss_sync_send_defeat("fl_dharmanyo"));
    assert(packet_calls == 0 && announce_calls == 0);

    dharumanyo_owner = 1;
    assert(boss_sync_send_defeat("fl_dharmanyo"));
    assert(packet_calls == 1 && announce_calls == 1);
}

static void shared_terminal_and_reward_test(void)
{
    TestActor carrier = {0};
    TestActor terminal_controller = {0};
    TestActor reward_controller = {0};

    reset_fixture();
    ACTOR_ENTITY_ID(carrier.bytes) = ENTITY_DARUMANYO;
    ACTOR_STATUS(carrier.bytes) = ACTOR_STATUS_ACTIVE;
    ACTOR_HEALTH(carrier.bytes) = 10;
    DARUMANYO_LIVES(carrier.bytes) = 12;
    DARUMANYO_HIT_TIMER(carrier.bytes) = 0;
    flags[DARUMANYO_KILL_FLAG] = 1;
    boss_sync_capture_darumanyo_setup(carrier.bytes);

    assert(boss_sync_queue_darumanyo_shared_terminal());
    assert(!flags[DARUMANYO_KILL_FLAG]);
    assert(s_darumanyo_state.lethal_hit_pending);
    assert(s_darumanyo_state.remote_defeat_in_progress);

    boss_sync_apply_darumanyo_native_hit(carrier.bytes);
    assert(DARUMANYO_LIVES(carrier.bytes) == 1);
    assert(ACTOR_HEALTH(carrier.bytes) == 2);
    assert(ACTOR_STATUS(carrier.bytes) & ACTOR_STATUS_DAMAGE_PENDING);
    assert(s_darumanyo_state.lethal_hit_armed);

    /* Model the original inner routine accepting the synthetic one-point hit. */
    ACTOR_HEALTH(carrier.bytes) = 1;
    boss_sync_check_darumanyo_native_hit();
    assert(!(ACTOR_STATUS(carrier.bytes) & ACTOR_STATUS_DAMAGE_PENDING));
    assert(!s_darumanyo_state.lethal_hit_pending);
    assert(!s_darumanyo_state.lethal_hit_armed);

    boss_sync_start_darumanyo_native_death(carrier.bytes);
    assert(s_darumanyo_state.local_defeat_started);
    assert(packet_calls == 0);

    boss_sync_track_darumanyo_terminal_controller(terminal_controller.bytes);
    boss_sync_finish_darumanyo_native_death(terminal_controller.bytes);
    assert(s_darumanyo_state.victory_complete);
    assert(s_darumanyo_state.remote_defeat_in_progress);
    assert(commit_calls == 0);
    assert(boss_sync_has_active_encounter("fl_dharmanyo"));

    ACTOR_ENTITY_ID(reward_controller.bytes) =
        ENTITY_DARUMANYO_REWARD_CONTROLLER;
    ACTOR_STATUS(reward_controller.bytes) = ACTOR_STATUS_ACTIVE;
    boss_sync_observe_darumanyo_reward_controller(reward_controller.bytes);
    flags[DARUMANYO_KILL_FLAG] = 1; /* Native controller state D0. */
    boss_sync_finish_darumanyo_reward_controller();
    assert(commit_calls == 1);
    assert(terminal_finish_calls == 1);
    assert(!s_darumanyo_state.remote_defeat_in_progress);
    assert(!boss_sync_has_active_encounter("fl_dharmanyo"));
    assert(!boss_sync_has_local_encounter("fl_dharmanyo"));

    boss_sync_finish_darumanyo_reward_controller();
    assert(commit_calls == 1);
    assert(terminal_finish_calls == 1);
}

int main(void)
{
    legacy_packet_test();
    remote_reward_send_gate_test();
    shared_terminal_and_reward_test();
    puts("Dharumanyo boss-sync terminal and reward state machine passed");
    return 0;
}
