#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __MODDING_H__
#define __RECOMPUTILS_H__
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
void recomp_free(void *memory);
int recomp_printf(const char *format, ...);
#include "../src/boss_sync.c"

typedef union TestActor { void *align; unsigned char bytes[0x200]; } TestActor;
unsigned short D_800C7AB2;
static unsigned char flags[0x200];
static int loaded, shared, owner, sent, committed;
int anchor_miracle_star_local_scene_active(void) { return 0; }
void anchor_tsurami_native_finish_terminal(void) {}
int anchor_congo_damage_is_shared(void) { return 0; }
int anchor_congo_damage_is_owner(void) { return 0; }
int anchor_dharumanyo_damage_is_shared(void) { return 0; }
int anchor_dharumanyo_damage_is_owner(void) { return 0; }
int anchor_tsurami_damage_is_shared(void) { return shared; }
int anchor_tsurami_damage_is_owner(void) { return owner; }
int item_sync_save_is_loaded(void) { return loaded; }
void item_sync_apply_benkei_postfight_state(void) {}
void anchor_dharumanyo_native_finish_terminal(void) {}
void item_sync_mark_boss_defeat_announced(const char *flag) { assert(!strcmp(flag,"fl_tsurami")); }
void item_sync_commit_boss_completion(const char *flag)
{ assert(!strcmp(flag,"fl_tsurami")); ++committed; flags[TSURAMI_KILL_FLAG]=1; }
char *anchor_get_team_id(void) { char *s=malloc(5); memcpy(s,"team",5); return s; }
int anchor_send_custom_packet(const char *type,const char *payload,const char *team,
                             unsigned int target,int queue)
{
    assert(!strcmp(type,"MNSG_BOSS_DEFEAT"));
    assert(!strcmp(payload,"{\"flag\":\"fl_tsurami\"}"));
    assert(!strcmp(team,"team")&&!target&&!queue); ++sent; return 1;
}
int anchor_send_flag(const char *flag,int value,int queue)
{ (void)flag;(void)value;(void)queue; return 1; }
void recomp_free(void *memory) { free(memory); }
int recomp_printf(const char *format,...) { (void)format; return 0; }
void func_80024088_24C88(int flag) { assert(flag>=0&&flag<0x200);flags[flag]=0; }
int func_800240DC_24CDC(int flag) { assert(flag>=0&&flag<0x200);return flags[flag]; }
void func_80034EF8_35AF8(void *actor) { (void)actor; }

static void start(TestActor *actor)
{
    loaded=0; boss_sync_reset(); loaded=1;
    memset(actor,0,sizeof(*actor)); memset(flags,0,sizeof(flags));
    D_800C7AB2=TSURAMI_ROOM; shared=1; owner=0; sent=committed=0;
    ACTOR_HEALTH(actor)=12; ACTOR_STATUS(actor)=ACTOR_STATUS_ACTIVE;
    boss_sync_capture_tsurami_root_setup(actor);
    boss_sync_track_tsurami_root(actor);
}
int main(void)
{
    TestActor root, child;
    start(&root);
    assert(boss_sync_has_active_encounter("fl_tsurami"));
    assert(boss_sync_has_local_encounter("fl_tsurami"));
    assert(boss_sync_apply_remote_defeat("fl_tsurami"));
    assert(!s_tsurami_state.lethal_hit_pending&&ACTOR_HEALTH(&root)==12);
    flags[TSURAMI_KILL_FLAG]=1;
    boss_sync_track_tsurami_root(&root);
    assert(!s_tsurami_state.lethal_hit_pending);
    assert(boss_sync_queue_tsurami_shared_terminal());
    assert(!flags[TSURAMI_KILL_FLAG]);
    assert(s_tsurami_state.lethal_hit_pending);
    memset(&child,0,sizeof(child)); ACTOR_HEALTH(&child)=12;
    boss_sync_apply_common_lethal_hit(&child);
    assert(ACTOR_HEALTH(&child)==12&&s_tsurami_state.lethal_hit_pending);
    boss_sync_apply_common_lethal_hit(&root);
    assert(ACTOR_HEALTH(&root)==2);
    assert(ACTOR_STATUS(&root)&ACTOR_STATUS_DAMAGE_PENDING);
    /* Native common damage consumes one HP and invokes the custom reaction. */
    ACTOR_HEALTH(&root)=1;
    ACTOR_STATUS(&root)&=~ACTOR_STATUS_DAMAGE_PENDING;
    boss_sync_check_common_lethal_hit();
    boss_sync_start_tsurami_native_death(&root);
    assert(!sent&&boss_sync_has_active_encounter("fl_tsurami"));
    assert(boss_sync_queue_tsurami_shared_terminal());
    assert(!s_tsurami_state.lethal_hit_pending);
    boss_sync_finish_tsurami_native_death(&root);
    assert(committed==0&&boss_sync_has_active_encounter("fl_tsurami"));
    boss_sync_finish_tsurami_reward_scene();
    assert(committed==1&&flags[TSURAMI_KILL_FLAG]);
    assert(!boss_sync_has_active_encounter("fl_tsurami"));
    boss_sync_finish_tsurami_native_death(&root);assert(committed==1);

    start(&root);owner=1;
    ACTOR_HEALTH(&root)=1;boss_sync_start_tsurami_native_death(&root);
    assert(sent==1);boss_sync_start_tsurami_native_death(&root);assert(sent==1);

    start(&root);assert(boss_sync_queue_tsurami_shared_terminal());
    D_800C7AB2=0;boss_sync_reset();
    assert(s_tsurami_state.remote_defeat_in_progress);
    D_800C7AB2=TSURAMI_ROOM;
    boss_sync_capture_tsurami_root_setup(&root);
    boss_sync_track_tsurami_root(&root);
    assert(s_tsurami_state.lethal_hit_pending);

    start(&root);shared=0;
    assert(boss_sync_apply_remote_defeat("fl_tsurami"));
    assert(s_tsurami_state.lethal_hit_pending);
    assert(!boss_sync_queue_tsurami_shared_terminal());
    puts("Tsurami terminal progression tests passed");
    return 0;
}
