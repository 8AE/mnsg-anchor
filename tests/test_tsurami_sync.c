#include "anchor_tsurami_sync.h"
#include "anchor_tsurami_native.h"
#include "anchor_tsurami_damage.h"
#include "anchor_player_models.h"
#include "utils/anchor_tsurami_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned short D_800C7AB2=0x71;
static unsigned short system_words[0x3af00/2];
void *D_8015C5C8_15D1C8=system_words;
static int connected=1, loaded=1, ready=1, root=1, local_pause, terminal_ready;
static int native_active,native_owner,native_paused,damage_active,damage_owner,damage_paused;
static int apply_ok=1, applied, hits, target_cleared, supplied_state, updates, exits;
static int send_ok=1, sends, epoch=1, native_script_pause;
static unsigned int visit=11;
static unsigned int wire_visit;
static int local_arena=3;
static int defer_apply,pending,allow_adoption=1,delivery_pending;
static AnchorTsuramiNativeSnapshot pending_state;
static AnchorTsuramiHit queued;
static AnchorTsuramiNativeSnapshot local, incoming;
static char response[12288],wire_state[7680];
int anchor_is_connected(void){return connected;}
int anchor_is_disabled(void){return 0;}
int item_sync_save_is_loaded(void){return loaded;}
int anchor_dialog_world_paused(void){return local_pause;}
int anchor_tsurami_native_world_paused(void){return native_script_pause;}
void recomp_free(void *p){free(p);}
void anchor_tsurami_native_tick(void)
{if(pending&&allow_adoption){local=pending_state;pending=0;applied++;}}
int anchor_tsurami_native_ready(void){return ready;}
int anchor_tsurami_native_snapshot_ready(void){return root||terminal_ready;}
unsigned int anchor_tsurami_native_visit(void){return visit;}
int anchor_player_models_get_epoch(void){return epoch;}
int anchor_boss_invite_world_arena(void){return local_arena;}
void *anchor_tsurami_native_root_task(void){return root?&root:0;}
void anchor_tsurami_native_set_role(int active,int owner,int paused)
{native_active=active;native_owner=owner;native_paused=paused;}
int anchor_tsurami_native_capture(AnchorTsuramiNativeSnapshot *out){*out=local;return (ready||terminal_ready)&&!pending;}
int anchor_tsurami_native_apply(const AnchorTsuramiNativeSnapshot *s)
{if(!apply_ok)return 0;if(defer_apply){pending_state=*s;pending=1;return 1;}local=*s;applied++;return 1;}
void anchor_tsurami_damage_set_context(int active,int owner,int paused,unsigned int encounter)
{(void)encounter;damage_active=active;damage_owner=owner;damage_paused=paused;}
int anchor_tsurami_damage_take_local_hit(AnchorTsuramiHit *out)
{if(!queued.sequence)return 0;*out=queued;queued.sequence=0;return 1;}
int anchor_tsurami_damage_pending(void){return delivery_pending;}
int anchor_tsurami_damage_apply(int amount,unsigned int target)
{assert(damage_active&&damage_owner&&!damage_paused);assert(target==0);hits++;local.root[TSU_HP]-=(unsigned)amount;return 1;}
int anchor_player_models_get_boss_target(int current,int rotate,AnchorBossTarget *out)
{(void)current;(void)rotate;(void)out;return 0;}
void anchor_tsurami_native_set_target(float x,float y,float z){(void)x;(void)y;(void)z;}
void anchor_tsurami_native_clear_target(void){target_cleared++;}
int anchor_send_tsurami_hit(int sequence,int amount,unsigned int target)
{assert(sequence>0&&amount>0&&target==0);sends++;return send_ok;}
char *anchor_tsurami_update(int r,unsigned int visit,int paused,const char *state)
{
    char *copy=malloc(strlen(response)+1);
    (void)paused;
    updates++;supplied_state=strcmp(state,"null")!=0;
    wire_visit=visit;
    if(!r&&!visit)exits++;
    strcpy(copy,response);return copy;
}
static void status(int role,int owner,unsigned int term,unsigned int revision,int paused,
                   int encounter_visit,int with_state,const char *hit_json)
{
    assert(anchor_tsurami_state_encode(&incoming,wire_state,sizeof(wire_state)));
    snprintf(response,sizeof(response),
      "{\"role\":%d,\"owner\":%d,\"term\":%u,\"revision\":%u,\"paused\":%d,"
      "\"encounter\":[10,20,%d],\"state\":%s,\"hits\":%s}",
      role,owner,term,revision,paused,encounter_visit,with_state?wire_state:"null",hit_json);
}
int main(void)
{
    local.root[TSU_HP]=30;local.root[TSU_PHASE]=1;incoming=local;
    strcpy(response,"{\"role\":0,\"owner\":0,\"term\":0,\"revision\":0,\"paused\":0,\"encounter\":[0,0,0],\"state\":null,\"hits\":[]}");
    anchor_tsurami_sync_frame();
    assert(native_active&&!native_owner&&native_paused&&supplied_state);
    status(1,10,1,1,0,11,1,"[]");
    anchor_tsurami_sync_frame();
    assert(applied==1&&native_owner&&!native_paused&&target_cleared);
    local.root[TSU_HP]=28;
    status(1,10,1,2,0,11,1,"[[22,33,44,55,2,0]]");
    anchor_tsurami_sync_frame();
    assert(applied==1&&local.root[TSU_HP]==26&&hits==1&&supplied_state);
    queued=(AnchorTsuramiHit){101,1,0};send_ok=0;
    status(1,10,1,3,0,11,0,"[]");
    anchor_tsurami_sync_frame();assert(sends==1);
    send_ok=1;anchor_tsurami_sync_frame();assert(sends==2);
    anchor_tsurami_sync_frame();assert(sends==2);
    incoming.root[TSU_HP]=20;
    status(2,9,2,4,0,11,1,"[]");
    anchor_tsurami_sync_frame();
    assert(applied==2&&!native_owner&&!native_paused&&local.root[TSU_HP]==20);
    anchor_tsurami_sync_frame();assert(applied==2&&!supplied_state);
    local_pause=1;incoming.root[TSU_HP]=17;
    status(2,9,2,5,0,11,1,"[]");anchor_tsurami_sync_frame();
    assert(applied==2&&native_paused&&local.root[TSU_HP]==20);
    local_pause=0;anchor_tsurami_sync_frame();
    assert(applied==3&&local.root[TSU_HP]==17&&!native_paused);
    status(1,10,3,6,0,11,1,"[]");anchor_tsurami_sync_frame();
    assert(applied==4&&native_owner&&!supplied_state);
    status(1,10,3,6,0,11,0,"[[22,33,44,56,1,0]]");anchor_tsurami_sync_frame();
    assert(supplied_state&&local.root[TSU_HP]==16&&hits==2);
    /* Native Start pauses the same timeline even without our invite dialog. */
    system_words[0x3ae26/2]=1;
    status(1,10,3,7,1,11,0,"[]");anchor_tsurami_sync_frame();
    assert(native_paused&&damage_paused);
    system_words[0x3ae26/2]=0;
    native_script_pause=1;anchor_tsurami_sync_frame();assert(native_paused);
    native_script_pause=0;
    status(2,8,4,8,0,11,1,"[]");apply_ok=0;anchor_tsurami_sync_frame();
    assert(native_paused&&!native_owner&&applied==4);
    apply_ok=1;anchor_tsurami_sync_frame();assert(applied==5&&!supplied_state);
    /* Parsing failures never partially mutate native state. */
    strcpy(response,"{\"role\":1}");anchor_tsurami_sync_frame();
    assert(native_paused&&applied==5);
    connected=0;anchor_tsurami_sync_frame();
    assert(!native_active&&!damage_active&&exits==1);
    anchor_tsurami_sync_frame();assert(exits==1);
    connected=1;incoming.root[TSU_HP]=30;
    status(1,10,1,1,0,12,1,"[]");anchor_tsurami_sync_frame();
    assert(applied==6&&local.root[TSU_HP]==30&&native_owner);
    queued=(AnchorTsuramiHit){102,1,0};send_ok=0;
    anchor_tsurami_sync_frame();assert(sends==3);
    epoch++;send_ok=1;anchor_tsurami_sync_frame();assert(sends==3);
    queued=(AnchorTsuramiHit){103,1,0};send_ok=0;
    anchor_tsurami_sync_frame();assert(sends==4);
    visit++;send_ok=1;anchor_tsurami_sync_frame();
    assert(sends==4&&applied==7);
    /* The real native adapter queues adoption at the file-bound root pre.
     * Failed/preempted adoption must keep capture null, which the Python
     * transport uses to withhold hit delivery and checkpoint acknowledgments. */
    defer_apply=1;allow_adoption=0;incoming.root[TSU_HP]=9;
    status(1,10,2,2,0,12,1,"[]");anchor_tsurami_sync_frame();
    assert(pending&&applied==7&&local.root[TSU_HP]==30);
    anchor_tsurami_sync_frame();
    assert(pending&&!supplied_state&&hits==2);
    anchor_tsurami_sync_frame();assert(!supplied_state&&hits==2);
    allow_adoption=1;
    status(1,10,2,2,0,12,1,"[[22,33,44,57,2,0]]");
    anchor_tsurami_sync_frame();
    assert(!pending&&supplied_state&&applied==8&&local.root[TSU_HP]==7&&hits==3);
    D_800C7AB2=0x1a;anchor_tsurami_sync_frame();assert(exits==2&&!native_active);
    /* A queued native delivery cannot be acknowledged by a checkpoint
     * captured while Start prevented the mapped pre callback from running. */
    D_800C7AB2=0x71;defer_apply=0;pending=0;
    connected=1;loaded=1;root=1;ready=1;local_pause=0;
    status(1,10,8,20,0,11,1,"[]");anchor_tsurami_sync_frame();
    delivery_pending=1;local_pause=1;
    anchor_tsurami_sync_frame();assert(!supplied_state);
    delivery_pending=0;anchor_tsurami_sync_frame();assert(supplied_state);
    /* The reward controller remains a checkpoint source after the root
     * has been destroyed; otherwise a late join loses the shared terminal. */
    local_pause=0;root=ready=0;terminal_ready=1;
    anchor_tsurami_sync_frame();assert(native_active&&supplied_state);
    terminal_ready=0;anchor_tsurami_sync_frame();assert(!native_active);
    /* A death/readiness gap can keep the room's resource-load visit. Peers
     * fenced the previous advertisement, so reentry needs a fresh wire visit
     * and must apply the current owner's checkpoint to the rebuilt actor. */
    root=ready=1;incoming.root[TSU_HP]=8;
    status(2,9,10,50,0,11,1,"[]");anchor_tsurami_sync_frame();
    {
        unsigned int previous_wire_visit=wire_visit;
        int previous_applied=applied;
        root=ready=0;anchor_tsurami_sync_frame();assert(!native_active);
        root=ready=1;local.root[TSU_HP]=12;incoming.root[TSU_HP]=6;
        status(2,9,10,51,0,11,1,"[]");anchor_tsurami_sync_frame();
        assert(wire_visit>previous_wire_visit);
        assert(applied==previous_applied+1&&local.root[TSU_HP]==6&&!native_owner);
        local_arena=0;anchor_tsurami_sync_frame();
        assert(!native_active&&!damage_active); /* owner remains alive elsewhere */
        local_arena=3;local.root[TSU_HP]=12;
        anchor_tsurami_sync_frame();
        assert(wire_visit>previous_wire_visit+1&&local.root[TSU_HP]==6);
    }
    puts("Tsurami coordinator tests passed");
}
