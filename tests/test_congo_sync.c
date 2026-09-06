#include "anchor_congo_sync.h"
#include "anchor_congo_native.h"
#include "anchor_congo_damage.h"
#include "anchor_player_models.h"
#include "utils/anchor_congo_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned short D_800C7AB2=0x16;
static unsigned short system_words[0x3af00/2];
void *D_8015C5C8_15D1C8=system_words;
static int connected=1, loaded=1, ready=1, root=1, local_pause;
static int native_active,native_owner,native_paused,damage_active,damage_owner,damage_paused;
static int apply_ok=1, applied, hits, target_cleared, supplied_state, updates, exits;
static int send_ok=1, sends, epoch=1, native_script_pause;
static unsigned int visit=11;
static int defer_apply,pending,allow_adoption=1;
static AnchorCongoNativeSnapshot pending_state;
static AnchorCongoHit queued;
static AnchorCongoNativeSnapshot local, incoming;
static char response[8192],wire_state[4096];
int anchor_is_connected(void){return connected;}
int anchor_is_disabled(void){return 0;}
int item_sync_save_is_loaded(void){return loaded;}
int anchor_dialog_world_paused(void){return local_pause;}
int anchor_congo_native_world_paused(void){return native_script_pause;}
void recomp_free(void *p){free(p);}
void anchor_congo_native_tick(void)
{if(pending&&allow_adoption){local=pending_state;pending=0;applied++;}}
int anchor_congo_native_ready(void){return ready;}
unsigned int anchor_congo_native_visit(void){return visit;}
int anchor_player_models_get_epoch(void){return epoch;}
void *anchor_congo_native_root_task(void){return root?&root:0;}
void anchor_congo_native_set_role(int active,int owner,int paused)
{native_active=active;native_owner=owner;native_paused=paused;}
int anchor_congo_native_capture(AnchorCongoNativeSnapshot *out){*out=local;return ready&&!pending;}
int anchor_congo_native_apply(const AnchorCongoNativeSnapshot *s)
{if(!apply_ok)return 0;if(defer_apply){pending_state=*s;pending=1;return 1;}local=*s;applied++;return 1;}
void anchor_congo_damage_set_context(int active,int owner,int paused,unsigned int encounter)
{(void)encounter;damage_active=active;damage_owner=owner;damage_paused=paused;}
int anchor_congo_damage_take_local_hit(AnchorCongoHit *out)
{if(!queued.sequence)return 0;*out=queued;queued.sequence=0;return 1;}
int anchor_congo_damage_apply(int amount)
{assert(damage_active&&damage_owner&&!damage_paused);hits++;local.root[CONGO_HP]-=(unsigned)amount;return 1;}
int anchor_player_models_get_boss_target(int current,int rotate,AnchorBossTarget *out)
{(void)current;(void)rotate;(void)out;return 0;}
void anchor_congo_native_set_target(float x,float y,float z){(void)x;(void)y;(void)z;}
void anchor_congo_native_clear_target(void){target_cleared++;}
int anchor_send_congo_hit(int sequence,int amount)
{assert(sequence>0&&amount>0);sends++;return send_ok;}
char *anchor_congo_update(int r,unsigned int visit,int paused,const char *state)
{
    char *copy=malloc(strlen(response)+1);
    (void)paused;
    updates++;supplied_state=strcmp(state,"null")!=0;
    if(!r&&!visit)exits++;
    strcpy(copy,response);return copy;
}
static void status(int role,int owner,unsigned int term,unsigned int revision,int paused,
                   int encounter_visit,int with_state,const char *hit_json)
{
    assert(anchor_congo_state_encode(&incoming,wire_state,sizeof(wire_state)));
    snprintf(response,sizeof(response),
      "{\"role\":%d,\"owner\":%d,\"term\":%u,\"revision\":%u,\"paused\":%d,"
      "\"encounter\":[10,20,%d],\"state\":%s,\"hits\":%s}",
      role,owner,term,revision,paused,encounter_visit,with_state?wire_state:"null",hit_json);
}
int main(void)
{
    local.root[CONGO_HP]=30;local.root[CONGO_PHASE]=1;incoming=local;
    strcpy(response,"{\"role\":0,\"owner\":0,\"term\":0,\"revision\":0,\"paused\":0,\"encounter\":[0,0,0],\"state\":null,\"hits\":[]}");
    anchor_congo_sync_frame();
    assert(native_active&&!native_owner&&native_paused&&supplied_state);
    status(1,10,1,1,0,11,1,"[]");
    anchor_congo_sync_frame();
    assert(applied==1&&native_owner&&!native_paused&&target_cleared);
    local.root[CONGO_HP]=28;
    status(1,10,1,2,0,11,1,"[[22,33,44,55,2]]");
    anchor_congo_sync_frame();
    assert(applied==1&&local.root[CONGO_HP]==26&&hits==1&&supplied_state);
    queued=(AnchorCongoHit){101,1};send_ok=0;
    status(1,10,1,3,0,11,0,"[]");
    anchor_congo_sync_frame();assert(sends==1);
    send_ok=1;anchor_congo_sync_frame();assert(sends==2);
    anchor_congo_sync_frame();assert(sends==2);
    incoming.root[CONGO_HP]=20;
    status(2,9,2,4,0,11,1,"[]");
    anchor_congo_sync_frame();
    assert(applied==2&&!native_owner&&!native_paused&&local.root[CONGO_HP]==20);
    anchor_congo_sync_frame();assert(applied==2&&!supplied_state);
    local_pause=1;incoming.root[CONGO_HP]=17;
    status(2,9,2,5,0,11,1,"[]");anchor_congo_sync_frame();
    assert(applied==2&&native_paused&&local.root[CONGO_HP]==20);
    local_pause=0;anchor_congo_sync_frame();
    assert(applied==3&&local.root[CONGO_HP]==17&&!native_paused);
    status(1,10,3,6,0,11,1,"[]");anchor_congo_sync_frame();
    assert(applied==4&&native_owner&&!supplied_state);
    status(1,10,3,6,0,11,0,"[[22,33,44,56,1]]");anchor_congo_sync_frame();
    assert(supplied_state&&local.root[CONGO_HP]==16&&hits==2);
    /* Native Start pauses the same timeline even without our invite dialog. */
    system_words[0x3ae26/2]=1;
    status(1,10,3,7,1,11,0,"[]");anchor_congo_sync_frame();
    assert(native_paused&&damage_paused);
    system_words[0x3ae26/2]=0;
    native_script_pause=1;anchor_congo_sync_frame();assert(native_paused);
    native_script_pause=0;
    status(2,8,4,8,0,11,1,"[]");apply_ok=0;anchor_congo_sync_frame();
    assert(native_paused&&!native_owner&&applied==4);
    apply_ok=1;anchor_congo_sync_frame();assert(applied==5&&!supplied_state);
    /* Parsing failures never partially mutate native state. */
    strcpy(response,"{\"role\":1}");anchor_congo_sync_frame();
    assert(native_paused&&applied==5);
    connected=0;anchor_congo_sync_frame();
    assert(!native_active&&!damage_active&&exits==1);
    anchor_congo_sync_frame();assert(exits==1);
    connected=1;incoming.root[CONGO_HP]=30;
    status(1,10,1,1,0,12,1,"[]");anchor_congo_sync_frame();
    assert(applied==6&&local.root[CONGO_HP]==30&&native_owner);
    queued=(AnchorCongoHit){102,1};send_ok=0;
    anchor_congo_sync_frame();assert(sends==3);
    epoch++;send_ok=1;anchor_congo_sync_frame();assert(sends==3);
    queued=(AnchorCongoHit){103,1};send_ok=0;
    anchor_congo_sync_frame();assert(sends==4);
    visit++;send_ok=1;anchor_congo_sync_frame();
    assert(sends==4&&applied==7);
    /* The real native adapter queues adoption at the file-bound root pre.
     * Failed/preempted adoption must keep capture null, which the Python
     * transport uses to withhold hit delivery and checkpoint acknowledgments. */
    defer_apply=1;allow_adoption=0;incoming.root[CONGO_HP]=9;
    status(1,10,2,2,0,12,1,"[]");anchor_congo_sync_frame();
    assert(pending&&applied==7&&local.root[CONGO_HP]==30);
    anchor_congo_sync_frame();
    assert(pending&&!supplied_state&&hits==2);
    anchor_congo_sync_frame();assert(!supplied_state&&hits==2);
    allow_adoption=1;
    status(1,10,2,2,0,12,1,"[[22,33,44,57,2]]");
    anchor_congo_sync_frame();
    assert(!pending&&supplied_state&&applied==8&&local.root[CONGO_HP]==7&&hits==3);
    D_800C7AB2=0x1a;anchor_congo_sync_frame();assert(exits==2&&!native_active);
    puts("Congo coordinator tests passed");
}
