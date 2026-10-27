/* Exercise the production File69 adapter using native-width scalar records.
 * The substitutes model the verified scheduled constructor/result boundaries;
 * this is a host regression fixture, not live game or two-client evidence. */
#include "impact_test_pointers.h"
#include <stdio.h>
#include <stdlib.h>
#ifndef DEBUG_BUTTON_ENABLED
#define DEBUG_BUTTON_ENABLED 1
#endif
#define ANCHOR_MINIGAME_YAE_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define YP(p,o) TP(p,o)
#define YAE_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#include "../src/world/anchor_minigame_yae.c"
#include "../src/world/anchor_minigame_codec.c"

unsigned short D_800C7AB2;
/* Native code loads this alias with lw; declaring it as a byte would make
 * character 3 read zero on the big-endian target (00 00 00 03). */
unsigned int D_8015C5DC_15D1DC;
void *D_801FC604_5B8514,*D_801FC60C_5B851C,*D_8016DAB4_16E6B4;
static unsigned int full_system[0xd0000u/4u];
unsigned char *D_8015C5C8_15D1C8=(unsigned char *)full_system;
static unsigned int root[64],root_object[64],work[16],player_task[64],player_object[64];
static unsigned int timer_task[64],swimmer_task[64],swimmer_obj[64],camera[8],fade[8];
enum { TEST_CHILD_CAPACITY=Y_COUNT+8 };
static unsigned int children[TEST_CHILD_CAPACITY][64],child_objects[TEST_CHILD_CAPACITY][64],hud_assets[3][8];
static unsigned int visit=1,ack;
static int requested,cancelled,started,finished,missing_resource,vm,modal,prompt_safe=1;
static int request_kind,active_attempt,save_loaded=1,host_available=1,vi_black,fade_current,fade_target;
static int native_roots,camera_ticks,hud_draws,hud_builds,proxy_builds,child_allocations;
static int obstacle_calls,obstacle_initializers,child_deletes,locks,clear_flags,fail_alloc;
static int fail_after=-1;
static int intro_vms,reward_vms,rewards,native_failure_warps,cleanups;
static int music_calls,last_music,warning_calls,result_audio;
static int obstacle_sfx;
static int flags[256],participant_ok=1;
static const AnchorMinigameAdapter *bound;

unsigned int anchor_minigame_sync_visit(void) { return visit; }
unsigned int anchor_minigame_sync_local_ack(int game) { assert(game==4);return ack; }
int anchor_minigame_sync_participant_current(int game,int cid)
{ assert(game==4);return participant_ok && cid==9; }
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *adapter)
{ assert(adapter->game==4);bound=adapter;role(1,0,0);return 1; }
int anchor_minigame_invites_yae_requested(void) { return requested; }
int anchor_minigame_invites_yae_request_origin(void)
{ return requested ? request_kind:ANCHOR_YAE_LAUNCH_NONE; }
int anchor_minigame_invites_yae_validate_join(void) { return requested && request_kind==2 && host_available; }
int anchor_minigame_invites_yae_direct_pending(void) { return requested; }
int anchor_minigame_invites_yae_black_scope(void)
{ return save_loaded && D_800C7AB2==Y_ROOM && (requested || active_attempt); }
int item_sync_save_is_loaded(void) { return save_loaded; }
void osViBlack(unsigned char active) { vi_black=active!=0; }
void anchor_minigame_invites_yae_cancel_request(void)
{ if(requested) { requested=0;cancelled=1;anchor_minigame_yae_direct_load_cancel(); } }
int anchor_minigame_invites_yae_cancel_pending(void) { return cancelled; }
void anchor_minigame_invites_yae_started(void)
{ ++started;requested=cancelled=0;active_attempt=1;++visit; }
void anchor_minigame_invites_yae_finished(void)
{ ++finished;cancelled=active_attempt=0;reset();bound=0; }
int anchor_boss_invite_world_can_prompt(void) { return prompt_safe; }
int anchor_dialog_busy(void) { return modal; }
void *func_800141C4_14DC4(unsigned int file)
{ assert(file==69);return missing_resource ? (void *)(unsigned long)0xffffffffu:(void *)1; }
int func_8003F1D8_3FDD8(void) { return vm; }
void func_80221F70_5DD440(void) { ++locks; }
void func_80024088_24C88(int flag)
{ assert(flag==0x1d || flag==0x1e);flags[flag]=0;++clear_flags; }
void func_80038BC8_397C8(unsigned short cue)
{ assert(cue==0x292);++music_calls;last_music=cue; }
void func_80038B98_39798(unsigned short cue)
{ if(cue==0x0b) ++warning_calls;else if(cue==0x13a || cue==0x39) ++result_audio; }
void func_08000AB4_72C304(void *task,float value) { assert(task==fade);fade_current=(int)value; }
void func_08000AC8_72C318(void *task,float value) { assert(task==fade);fade_target=(int)value; }
void func_80034EF8_35AF8(void *task)
{
    ++child_deletes;YW(task,0x68)|=2u;anchor_minigame_yae_delete(task);
}
void *func_08000000_72B850(void *parent,YaeCallback callback,unsigned char priority)
{
    assert(parent==root && callback==func_08000E1C_72C66C && priority==5);
    if(fail_alloc || (fail_after>=0 && child_allocations>=fail_after) ||
       child_allocations>=TEST_CHILD_CAPACITY) return 0;
    int i=child_allocations++;
    void *task=children[i];
    memset(task,0,sizeof(children[i]));memset(child_objects[i],0,sizeof(child_objects[i]));
    YP(task,0x18)=child_objects[i];YP(task,0x0c)=(void *)callback;
    YP(task,0x14)=work;YH(task,0x28)=69;YB(task,0x74)=(unsigned char)(i+3);
    return task;
}
void func_08000E1C_72C66C(void *task,void *object)
{
    anchor_minigame_yae_obstacle(task,object);++obstacle_calls;
    if(YB(task,0xd0)==0) {
        ++obstacle_initializers;YH(task,0x5c)=YH(task,0x5e)=0x33a;
        YB(task,0x75)=10;YB(task,0x4c)=1;YW(task,0x60)=0x00c006e1u;
        YW(task,0x64)|=0x4000u;YB(task,0xd1)=1;YB(task,0xd0)=1;
        YH(object,0x16)=0x200;
    } else if(YB(task,0xd0)==1) {
        YH(object,0x14)=(unsigned short)(YH(object,0x14)+8);
        ++YH(object,0x16);
        if(YW(task,0x68)&0x2200u) {
            ++obstacle_sfx; /* Verified native positional cue13B. */
            YB(task,0xd1)=0;YB(task,0xd0)=2;YS(task,0xd4)=0;
        }
        if(YF(object,0x0c)<-468.f) { func_80034EF8_35AF8(task);return; }
    } else if(YB(task,0xd0)==2) {
        if(YS(task,0xd4)==1) { YF(task,0x80)=-4.f;YF(task,0x7c)=4.f; }
        if(YS(task,0xd4)==30) { func_80034EF8_35AF8(task);return; }
    }
    ++YS(task,0xd4);anchor_minigame_yae_obstacle_return();
}
void func_0800107C_72C8CC(void *task,void *object)
{
    anchor_minigame_yae_swimmer(task,object);
    if(!YB(task,0xd0)) {
        YB(task,0xd0)=1;YH(task,0x5c)=YH(task,0x5e)=0x33a;
        YB(player_task,0x30)&=(unsigned char)~1u;YB(player_task,0xcc)=0xcb;
    }
    YF(player_object,8)=YF(object,8);YF(player_object,0x0c)=YF(object,0x0c)+15.f;
    YF(player_object,0x10)=YF(object,0x10)+16.f;
    anchor_minigame_yae_swimmer_return();
}
void func_080002B4_72BB04(void *task)
{
    anchor_minigame_yae_hud(task);++hud_draws;
    if(YI(task,0x84)>0) --YI(task,0x84);
    anchor_minigame_yae_hud_return();
}
static void build_trial_graph(void)
{
    ++hud_builds;++proxy_builds;memset(timer_task,0,sizeof(timer_task));
    YP(work,0x10)=timer_task;YP(timer_task,0x0c)=(void *)func_080002B4_72BB04;
    YP(timer_task,0x78)=hud_assets[0];YP(timer_task,0x7c)=hud_assets[1];
    YP(timer_task,0x80)=hud_assets[2];YI(timer_task,0x84)=50*30;
    memset(swimmer_task,0,sizeof(swimmer_task));memset(swimmer_obj,0,sizeof(swimmer_obj));
    YP(work,0x18)=swimmer_task;YP(swimmer_task,0x18)=swimmer_obj;
    YP(swimmer_task,0x0c)=(void *)func_0800107C_72C8CC;YH(swimmer_task,0x28)=69;
    YF(swimmer_obj,8)=0.f;YF(swimmer_obj,0x0c)=-350.f;YF(swimmer_obj,0x10)=264.f;
    YP(camera,0x6c)=hud_assets[2];D_8015C5C8_15D1C8[0xcf8a2]=1;
}
void func_080014E8_72CD38(void *task,void *object)
{
    int phase=YB(work,0),frame=YS(work,4);
    anchor_minigame_yae_root(task,object);++native_roots;
    switch(phase) {
    case 0:YH(task,0x5e)=0x327;YP(work,0x14)=work;YB(work,0)=1;break;
    case 1:break;
    case 3:if(!vm) {
        YB(work,0)=(flags[0x15] && !flags[0x1e] && D_8015C5DC_15D1DC==3 && flags[0x7a]) ? 4:12;
        YS(work,4)=0;
    }break;
    case 4:
        if(frame==1) YP(work,0x1c)=fade;
        if(frame==20) YP(work,0x0c)=camera;
        if(frame==40) { ++intro_vms;vm=1;YB(work,0)=5; }
        ++YS(work,4);break;
    case 5:if(!vm) { YB(work,0)=6;YS(work,4)=1; }break;
    case 6:
        if(frame==1) build_trial_graph();
        if(frame==20) { YB(work,0)=7;YB(work,1)=2; }
        ++YS(work,4);break;
    case 7:
        ++camera_ticks;
        if(frame%40==0) for(int i=0;i<2;++i) {
            void *child=func_08000000_72B850(task,func_08000E1C_72C66C,5);
            if(child) { void *o=YP(child,0x18);YF(o,8)=i ? 53.f:-53.f;YF(o,0x0c)=400.f;YF(o,0x10)=264.f; }
        }
        if(YP(work,0x10)) {
            YI(timer_task,0x90)=(int)(YF(swimmer_obj,0x0c)+440.f);
            if(YI(timer_task,0x84)/30<=0 || YF(swimmer_obj,0x0c)<-440.f) {
                YB(work,0)=10;YS(work,4)=0;
            }
            if(YF(swimmer_obj,0x0c)>380.f) { YB(work,0)=9;YS(work,4)=0; }
        }
        ++YS(work,4);break;
    case 9:
        if(frame==1) { flags[0x1e]=1;func_80038B98_39798(0x13a);func_80038B98_39798(0x39);YB(swimmer_task,0xd0)=0xff; }
        if(frame==40) { YB(work,0)=11;YS(work,4)=0; }
        ++YS(work,4);break;
    case 10:
        if(frame==1) { flags[0x1d]=1;YB(swimmer_task,0xd0)=0xfe; }
        if(frame==60) { YB(work,0)=11;YS(work,4)=0; }
        ++YS(work,4);break;
    case 11:
        if(frame==20) {
            ++cleanups;YP(work,0x0c)=0;YP(work,0x18)=0;YP(work,0x10)=0;
            YB(player_task,0x30)|=1;YB(player_task,0xcc)=0;
            D_8015C5C8_15D1C8[0xcf8a2]=D_8015C5C8_15D1C8[0x3ae20]=0;
            if(!flags[0x1e]) ++native_failure_warps;
        }
        if(frame==40 && flags[0x1e]) { ++reward_vms;vm=1;YB(work,0)=12;YS(work,4)=0; }
        ++YS(work,4);break;
    case 12:if(!vm) { YB(work,0)=1;YS(work,4)=0; }break;
    default:assert(0);
    }
    anchor_minigame_yae_root_return();
}
static void tick_hud(void)
{
    if(!YP(work,0x10)) return;
    if(callback_is(YP(timer_task,0x0c),hud_callback)) hud_callback(timer_task,0);
    else func_080002B4_72BB04(timer_task);
}
static void tick_children(void)
{
    for(int i=0;i<child_allocations;++i) if(!(YW(children[i],0x68)&2u)) {
        YaeCallback callback=(YaeCallback)YP(children[i],0x0c);
        callback(children[i],child_objects[i]);
    }
}
static void finish_intro(void)
{
    assert(vm && intro_vms);func_80038BC8_397C8(0x292);vm=0;
}
static void fixture(int request)
{
    reset();test_ptr_count=0;memset(test_ptrs,0,sizeof(test_ptrs));
    memset(root,0,sizeof(root));memset(root_object,0,sizeof(root_object));memset(work,0,sizeof(work));
    memset(player_task,0,sizeof(player_task));memset(player_object,0,sizeof(player_object));
    memset(children,0,sizeof(children));memset(child_objects,0,sizeof(child_objects));
    memset(timer_task,0,sizeof(timer_task));memset(flags,0,sizeof(flags));
    requested=request!=0;request_kind=request;active_attempt=0;
    cancelled=started=finished=missing_resource=vm=modal=0;prompt_safe=participant_ok=1;
    save_loaded=host_available=1;vi_black=fade_current=fade_target=0;memset(full_system,0,sizeof(full_system));
    D_8015C5C8_15D1C8[0x3add4]=13;D_8015C5C8_15D1C8[0x3adde]=1;
    native_roots=camera_ticks=hud_draws=hud_builds=proxy_builds=child_allocations=0;
    obstacle_calls=obstacle_initializers=child_deletes=locks=clear_flags=fail_alloc=0;
    fail_after=-1;
    intro_vms=reward_vms=rewards=native_failure_warps=cleanups=0;
    music_calls=last_music=warning_calls=result_audio=obstacle_sfx=0;ack=0;bound=0;
    D_800C7AB2=Y_ROOM;D_8015C5DC_15D1DC=3;
    D_801FC604_5B8514=player_task;D_801FC60C_5B851C=player_object;
    D_8016DAB4_16E6B4=root;YP(root,0x18)=root_object;YP(root,0xd0)=work;
    YP(root,0x0c)=(void *)func_080014E8_72CD38;YH(root,0x28)=69;YH(root,0x5e)=0x33a;
    YB(root,0x74)=7;YB(player_task,0x60)=3;
    if(requested && request_kind==ANCHOR_YAE_LAUNCH_JOIN) anchor_minigame_yae_direct_load_begin();
    func_080014E8_72CD38(root,root_object);
    anchor_minigame_yae_frame();
}
static void tick_swimmer(void)
{
    if(!YP(work,0x18)) return;
    YaeCallback callback=(YaeCallback)YP(swimmer_task,0x0c);callback(swimmer_task,swimmer_obj);
}
static void begin_running(int owner)
{
    fixture(2);assert(bound==&s_adapter && started==1 && YB(work,0)==4);
    role(1,owner,1);
    if(!owner) s_host_phase=7;
    for(int i=0;i<64 && vi_black;++i) {
        root_callback(root,root_object);
        assert(!vm && !intro_vms);
        if(!vi_black) break;
        tick_swimmer();tick_hud();
    }
    assert(YB(work,0)==7 && !vi_black && hud_builds==1 && proxy_builds==1);
    tick_hud();assert(hud_live() && swimmer_object()==swimmer_obj);
    role(1,owner,1);
}
static AnchorMinigameState state(void)
{ AnchorMinigameState value={0};assert(capture(&value));return value; }
static void spawn_pair(void)
{
    role(1,1,1);YS(work,4)=40;root_callback(root,root_object);
    assert(child_allocations==2);tick_children();assert(obstacle_initializers==2);
}
static void test_launch_and_bootstrap(void)
{
    fixture(0);assert(!bound && !started && YB(work,0)==1 && YH(root,0x5e)==0x327);
    for(int i=0;i<4;++i) func_080014E8_72CD38(root,root_object);
    assert(!bound && !locks && !clear_flags);
    requested=1;request_kind=2;anchor_minigame_yae_direct_load_begin();
    flags[0x1d]=flags[0x1e]=flags[0x1f]=1;flags[0x15]=0;
    D_8015C5DC_15D1DC=0;func_080014E8_72CD38(root,root_object);anchor_minigame_yae_frame();assert(!bound);
    D_8015C5DC_15D1DC=3;modal=1;func_080014E8_72CD38(root,root_object);anchor_minigame_yae_frame();assert(!bound);
    modal=0;func_080014E8_72CD38(root,root_object);anchor_minigame_yae_frame();
    assert(bound && YB(work,0)==4 && clear_flags==2 && locks==1 && !flags[0x1d] && !flags[0x1e]);
    assert(flags[0x1f] && !flags[0x15]);
    AnchorMinigameState initial=state();assert(initial.controller[1]==4 && initial.controller[3]==-1);
    role(1,0,0);
    for(int i=0;i<40;++i) { root_callback(root,root_object);tick_swimmer();tick_hud(); }
    assert(YB(work,0)==7 && vi_black && !intro_vms && !vm && hud_builds==1 && proxy_builds==1);
    assert(YI(timer_task,0x84)==1500 && !music_calls && fade_current==255);
    /* A local graph can be complete while its authority is still pending.
     * Reveal and start the clock only after the real owner-ready edge. */
    role(1,1,1);root_callback(root,root_object);
    assert(!vi_black && music_calls==1 && last_music==0x292 && YI(timer_task,0x84)==1500);
    /* The renderer still runs while the shared clock is held awaiting ACK. */
    role(1,0,0);int before=YI(timer_task,0x84);for(int i=0;i<10;++i) tick_hud();
    assert(YI(timer_task,0x84)==before);

    /* The normal sign still runs instructionVM177 and waits for its native
     * completion; direct arrival is the only path that omits that dialogue. */
    fixture(0);flags[0x15]=flags[0x7a]=1;YB(work,0)=3;
    func_080014E8_72CD38(root,root_object);assert(bound && YB(work,0)==4);
    role(1,1,1);for(int i=0;i<50 && !vm;++i) root_callback(root,root_object);
    assert(vm && intro_vms==1 && !hud_builds);finish_intro();
    for(int i=0;i<30 && YB(work,0)!=7;++i) { root_callback(root,root_object);tick_swimmer();tick_hud(); }
    assert(YB(work,0)==7 && hud_builds==1 && music_calls==1);
}
static void test_timer_graph_and_inputs(void)
{
    begin_running(1);spawn_pair();AnchorMinigameState host=state();
    assert(host.actor_count==2 && host.actor_words[0]==15 && host.controller[3]==1499);
    int calls=obstacle_calls,allocs=child_allocations;
    host.controller[3]=999;host.actors[0][6]=27*1024;host.actors[0][12]=1024;
    YB(work,1)=1;YF(swimmer_obj,8)=19.f;YF(swimmer_obj,0x0c)=-217.f;
    assert(apply(&host));assert(YI(timer_task,0x84)==999 && YF(child_objects[0],8)==27.f);
    assert(YB(work,1)==1 && YF(swimmer_obj,8)==19.f && YF(swimmer_obj,0x0c)==-217.f);
    assert(obstacle_calls==calls && child_allocations==allocs);
    role(1,0,1);int previous_draws=hud_draws;for(int i=0;i<12;++i) tick_hud();
    assert(YI(timer_task,0x84)==999-Y_PREDICT && hud_draws==previous_draws+12);
    YW(children[0],0x68)|=0x200u;obstacle_callback(children[0],child_objects[0]);
    AnchorMinigameState hit=state();assert(hit.input_count==1 && hit.inputs[0][1]==Y_HIT);
    assert(YB(children[0],0xd0)==2);YW(children[0],0x68)&=~0x200u;
    YF(swimmer_obj,0x0c)=390.f;root_callback(root,root_object);host=state();
    assert(host.input_count==2 && host.inputs[1][1]==Y_GOAL && !s_terminal && YB(work,0)==7);
    ack=(unsigned int)host.inputs[0][0];host=state();assert(host.input_count==1 && host.inputs[0][1]==Y_GOAL);
    int input_words[8]={9,909,17,1,Y_GOAL,390*1024,19*1024,264*1024};
    assert(!input(input_words,8));role(1,1,1);participant_ok=0;
    assert(input(input_words,8) && !s_terminal);participant_ok=1;
    input_words[5]=379*1024;assert(input(input_words,8) && !s_terminal);
    input_words[5]=390*1024;assert(input(input_words,8) && s_terminal && s_outcome==1);
    assert(input(input_words,8) && s_terminal); /* Idempotent native edge. */
}
static void test_reconstruction_and_atomicity(void)
{
    begin_running(1);spawn_pair();AnchorMinigameState host=state(),bad;
    host.controller[3]=876;host.actors[0][6]=77*1024;
    begin_running(0);int initial_timer=YI(timer_task,0x84);fail_after=1;
    assert(!apply(&host) && child_allocations==1 && !obstacle_initializers);
    assert(YI(timer_task,0x84)==initial_timer && YF(child_objects[0],0x0c)==30000.f);
    AnchorMinigameState incomplete={0};assert(!capture(&incomplete));
    tick_children();assert(!apply(&host) && YI(timer_task,0x84)==initial_timer);
    assert(YF(child_objects[0],0x0c)==30000.f && !capture(&incomplete));
    fail_after=-1;assert(!apply(&host) && child_allocations==2);
    tick_children();assert(obstacle_initializers==2);assert(apply(&host));
    assert(YI(timer_task,0x84)==876 && YF(child_objects[0],8)==77.f);
    int calls=obstacle_calls;assert(apply(&host) && obstacle_calls==calls);
    bad=host;bad.actors[1][6]=0x7fffffff;
    assert(!validate(&bad) && !apply(&bad) && YI(timer_task,0x84)==876);
    bad=host;bad.actors[1][1]=bad.actors[0][1];assert(!apply(&bad));
    ++YB(children[1],0x74);fail_alloc=1;host.controller[3]=600;
    assert(!apply(&host) && YI(timer_task,0x84)==876 && YF(child_objects[0],8)==77.f);
    --YB(children[1],0x74);fail_alloc=0;missing_resource=1;assert(!apply(&host));missing_resource=0;
    D_800C7AB2=0x153;assert(!apply(&host));D_800C7AB2=Y_ROOM;
    ++visit;assert(!apply(&host));--visit;
    assert(apply(&host));
    int words[7]={9,909,17,1,Y_HIT,(int)s_obstacles[0].id,1};role(1,1,1);
    assert(input(words,7) && YB(children[0],0xd0)==1);
    obstacle_callback(children[0],child_objects[0]);
    assert(YB(children[0],0xd0)==2 && YB(children[0],0xd1)==0);
    short broken_frame=YS(children[0],0xd4);
    assert(input(words,7) && YS(children[0],0xd4)==broken_frame);
}
static void test_results_personal_fall_and_lifecycle(void)
{
    AnchorMinigameState absent={0};
    begin_running(1);YF(swimmer_obj,0x0c)=-441.f;root_callback(root,root_object);
    assert(s_personal_failed && !s_outcome && !s_terminal && !capture(&absent));
    for(int i=0;i<100 && !finished;++i) root_callback(root,root_object);
    assert(finished==1 && cleanups==1 && native_failure_warps==1 && !bound);
    begin_running(0);YF(swimmer_obj,0x0c)=-441.f;root_callback(root,root_object);
    assert(s_personal_failed && !capture(&absent));
    begin_running(1);YI(timer_task,0x84)=29;root_callback(root,root_object);
    assert(s_terminal && s_outcome==2 && !s_personal_failed);
    AnchorMinigameState failed=state();assert(failed.result==2);
    begin_running(0);result(1);result(1);assert(s_terminal && YB(work,0)==9);
    for(int i=0;i<100 && YB(work,0)!=12;++i) root_callback(root,root_object);
    assert(cleanups==1 && reward_vms==1 && !finished && vm && flags[0x1e]);
    assert(!YP(work,0x0c) && !YP(work,0x10) && !YP(work,0x18));
    /* The real scenario176 grants these before phase12 returns to sign idle. */
    flags[0x1f]=1;++rewards;vm=0;root_callback(root,root_object);
    assert(finished==1 && rewards==1 && result_audio==2 && YB(work,0)==1);
    begin_running(1);spawn_pair();anchor_minigame_yae_reuse(children[0]);
    assert(!s_obstacles[0].task);anchor_minigame_yae_reuse(timer_task);assert(!hud_live());
    begin_running(1);anchor_minigame_yae_delete(root);assert(!capture(&absent));
    begin_running(1);reset();assert(callback_is(YP(root,0x0c),func_080014E8_72CD38));
}
static void test_full_graph_replacement_and_contact_once(void)
{
    begin_running(1);
    for(int pair=0;pair<Y_COUNT/2;++pair) {
        YS(work,4)=40;root_callback(root,root_object);tick_children();
    }
    AnchorMinigameState host=state();assert(host.actor_count==Y_COUNT);
    begin_running(0);assert(!apply(&host) && child_allocations==Y_COUNT);
    tick_children();assert(apply(&host));
    /* A full graph must be able to retire one ID and stage its replacement;
     * readiness must not require a free65th registry slot first. */
    for(int i=0;i<Y_COUNT-1;++i)
        memcpy(host.actors[i],host.actors[i+1],sizeof(host.actors[i]));
    host.actors[Y_COUNT-1][1]=Y_COUNT+1;host.controller[6]=Y_COUNT+1;
    int before=YI(timer_task,0x84);role(1,0,0);
    assert(!apply(&host) && child_deletes==1 && child_allocations==Y_COUNT+1);
    assert(YI(timer_task,0x84)==before && YF(child_objects[Y_COUNT],0x0c)==30000.f);
    tick_children();assert(apply(&host));role(1,0,1);
    AnchorMinigameState adopted=state();assert(adopted.actor_count==Y_COUNT);
    assert(adopted.actors[0][1]==2 && adopted.actors[Y_COUNT-1][1]==Y_COUNT+1);

    begin_running(1);spawn_pair();host=state();role(1,0,1);
    YW(children[0],0x68)|=0x200u;
    for(int i=0;i<12;++i) {
        if(i==4 || i==8) assert(apply(&host)); /* Older phase1 checkpoint. */
        obstacle_callback(children[0],child_objects[0]);
    }
    AnchorMinigameState pending=state();
    assert(obstacle_sfx==1 && pending.input_count==1 && pending.inputs[0][1]==Y_HIT);
    assert(YB(children[0],0xd0)==2);
}
static void test_entry_identity_and_takeover_music(void)
{
    fixture(0);requested=1;request_kind=2;anchor_minigame_yae_direct_load_begin();
    anchor_minigame_yae_root(root,root_object);++YB(root,0x74);
    anchor_minigame_yae_root_return();assert(!bound);--YB(root,0x74);
    anchor_minigame_yae_root_return();assert(!bound);
    D_8016DAB4_16E6B4=swimmer_task;
    func_080014E8_72CD38(root,root_object);anchor_minigame_yae_frame();assert(bound && started==1);
    /* The original arguments captured at ENTRY identify the root, regardless
     * of a scheduler-global task value at the matching RETURN boundary. */
    role(1,0,0);s_host_phase=7;
    for(int i=0;i<30;++i) { root_callback(root,root_object);tick_swimmer();tick_hud(); }
    assert(YB(work,0)==7 && vi_black && !intro_vms && !music_calls);
    role(1,1,1);root_callback(root,root_object);
    assert(hud_builds==1 && music_calls==1 && last_music==0x292);
    for(int i=0;i<5;++i) root_callback(root,root_object);
    assert(music_calls==1);
}
static void test_saturated_contact_queue_retries_after_ack(void)
{
    begin_running(1);
    for(int pair=0;pair<17;++pair) {
        YS(work,4)=40;root_callback(root,root_object);tick_children();
    }
    AnchorMinigameState baseline=state();assert(baseline.actor_count==34);role(1,0,1);
    for(int i=0;i<33;++i) {
        YW(children[i],0x68)|=0x200u;obstacle_callback(children[i],child_objects[i]);
    }
    AnchorMinigameState full=state();assert(full.input_count==Y_INPUTS && obstacle_sfx==33);
    /* The next full predates the33rd contact. It must neither replay impact
     * sound nor erase the unsent semantic request while the queue is full. */
    assert(apply(&baseline));obstacle_callback(children[32],child_objects[32]);
    assert(obstacle_sfx==33);
    ack=(unsigned int)full.inputs[0][0];AnchorMinigameState space=state();
    assert(space.input_count==Y_INPUTS-1);
    obstacle_callback(children[32],child_objects[32]);
    AnchorMinigameState retry=state();assert(retry.input_count==Y_INPUTS);
    assert(retry.inputs[Y_INPUTS-1][1]==Y_HIT && retry.inputs[Y_INPUTS-1][2]==33);
    assert(obstacle_sfx==33);
}
/* Fresh native disassembly verifies entity postprocessing runs AFTER the
 * scheduled +0C callback: gravity8021C444, then integrate8021B808. File69
 * obstacles have gravity byte+75=10, subtracting0.2 fromY velocity, followed
 * by XYZ +=2*velocity. Merely zeroing velocity at the callback cannot holdY. */
static void verified_obstacle_post(void *task,void *object)
{
    if(YW(task,0x60)&0x00800000u) {
        assert(YB(task,0x75)==10);YF(task,0x7c)-=(float)YB(task,0x75)/50.f;
        float terminal=(YW(task,0x64)&0x1000u) ? -6.f:-3.f;
        if(YF(task,0x7c)<terminal) YF(task,0x7c)=terminal;
    }
    if(YW(task,0x60)&0x200u)
        for(int i=0;i<3;++i) YF(object,8+4*i)+=2.f*YF(task,0x78+4*i);
}
static void scheduled_obstacle_step(int index)
{
    obstacle_callback(children[index],child_objects[index]);
    verified_obstacle_post(children[index],child_objects[index]);
}
static void test_postphysics_prediction_hold_and_restore(void)
{
    const unsigned int movement=0x00800200u;
    begin_running(1);spawn_pair();AnchorMinigameState host=state();
    host.actors[0][12]=1024;host.actors[0][13]=-1024;host.actors[0][14]=2048;
    assert(apply(&host));role(1,0,1);
    YW(children[0],0x60)|=0x10000000u;
    YW(children[0],0x64)|=0x8000u;YW(children[0],0x68)|=0x10000000u;
    unsigned int native_flags=YW(children[0],0x60);
    unsigned int collision_flags=YW(children[0],0x64),status_flags=YW(children[0],0x68);
    float initial_y=YF(child_objects[0],0x0c);
    for(int i=0;i<Y_PREDICT;++i) scheduled_obstacle_step(0);
    assert(YF(child_objects[0],0x0c)<initial_y);
    float held[3];for(int i=0;i<3;++i) held[i]=YF(child_objects[0],8+4*i);
    for(int i=0;i<24;++i) scheduled_obstacle_step(0);
    for(int i=0;i<3;++i) assert(YF(child_objects[0],8+4*i)==held[i]);
    assert((YW(children[0],0x60)&movement)==0);
    assert((YW(children[0],0x60)&~movement)==(native_flags&~movement));
    assert(YW(children[0],0x64)==collision_flags && YW(children[0],0x68)==status_flags);

    assert(apply(&host));assert(YW(children[0],0x60)==native_flags);
    initial_y=YF(child_objects[0],0x0c);scheduled_obstacle_step(0);
    assert(YF(child_objects[0],0x0c)<initial_y);
    for(int i=0;i<Y_PREDICT+1;++i) scheduled_obstacle_step(0);
    assert(!(YW(children[0],0x60)&movement));
    role(1,1,1);initial_y=YF(child_objects[0],0x0c);scheduled_obstacle_step(0);
    assert(YW(children[0],0x60)==native_flags && YF(child_objects[0],0x0c)<initial_y);

    role(1,0,1);assert(apply(&host));
    for(int i=0;i<Y_PREDICT+1;++i) scheduled_obstacle_step(0);
    assert(!(YW(children[0],0x60)&movement));
    reset();assert(YW(children[0],0x60)==native_flags);
    assert(callback_is(YP(children[0],0x0c),func_08000E1C_72C66C));

    begin_running(1);spawn_pair();host=state();assert(apply(&host));role(1,0,1);
    native_flags=YW(children[0],0x60);
    for(int i=0;i<Y_PREDICT+1;++i) scheduled_obstacle_step(0);
    assert(!(YW(children[0],0x60)&movement));
    anchor_minigame_yae_reuse(children[0]);
    assert(YW(children[0],0x60)==native_flags && !s_obstacles[0].task);

    begin_running(1);spawn_pair();host=state();assert(apply(&host));role(1,0,1);
    for(int i=0;i<Y_PREDICT+1;++i) scheduled_obstacle_step(0);
    assert(!(YW(children[0],0x60)&movement));
    /* Allocator reuse retires the cache; it must never restore an old
     * generation's motion mask into a newly initialized task at that address. */
    ++YB(children[0],0x74);YW(children[0],0x60)=0x20000020u;
    anchor_minigame_yae_reuse(children[0]);reset();
    assert(YW(children[0],0x60)==0x20000020u && !s_obstacles[0].task);
}
static AnchorMinigameState natural_host_prelude(int phase)
{
    fixture(0);flags[0x15]=flags[0x7a]=1;YB(work,0)=3;
    func_080014E8_72CD38(root,root_object);role(1,1,1);
    if(phase==5) {
        for(int i=0;i<50 && !vm;++i) root_callback(root,root_object);
        assert(vm && intro_vms==1);
    }
    AnchorMinigameState prelude=state();assert(prelude.controller[1]==phase && !prelude.running);
    return prelude;
}
static void hidden_join(const AnchorMinigameState *prelude)
{
    fixture(2);assert(bound && !flags[0x15]);assert(apply(prelude));role(1,0,1);
    for(int i=0;i<35;++i) {
        root_callback(root,root_object);tick_swimmer();tick_hud();
        assert(!vm && !intro_vms && vi_black);
    }
    assert(YB(work,0)==7 && hud_live() && YI(timer_task,0x84)==1500);
    assert(YF(swimmer_obj,0x0c)==-350.f && !music_calls && !s_input_count);
}
static void test_join_waits_for_running_host_and_preserves_timer(void)
{
    begin_running(1);AnchorMinigameState running=state();running.controller[3]=1200;
    assert(validate(&running));
    for(int phase=4;phase<=5;++phase) {
        AnchorMinigameState prelude=natural_host_prelude(phase);hidden_join(&prelude);
        for(int frame=0;frame<700;++frame) {
            if(frame%30==0) {
                prelude.adopted_generation=1;prelude.source_tick=(unsigned int)frame+1;
                assert(apply(&prelude));
            }
            root_callback(root,root_object);tick_swimmer();tick_hud();
            assert(vi_black && !finished && YI(timer_task,0x84)==1500 && !music_calls);
        }
        assert(apply(&running));tick_hud();
        assert(vi_black && YI(timer_task,0x84)==1200 && !music_calls);
        root_callback(root,root_object);
        assert(!vi_black && YI(timer_task,0x84)==1200 && music_calls==1);
        for(int i=0;i<5;++i) root_callback(root,root_object);
        assert(music_calls==1);tick_hud();assert(YI(timer_task,0x84)==1199);
    }
    AnchorMinigameState stalled=natural_host_prelude(5);hidden_join(&stalled);
    stalled.adopted_generation=1;stalled.source_tick=1;assert(apply(&stalled));
    for(int frame=0;frame<700 && !finished;++frame) {
        if(frame%30==0) assert(apply(&stalled)); /* Duplicate cannot renew. */
        root_callback(root,root_object);tick_swimmer();tick_hud();
    }
    assert(finished==1 && !vi_black);
    begin_running(1);result(1);root_callback(root,root_object);root_callback(root,root_object);
    AnchorMinigameState won=state();assert(won.result==1);
    AnchorMinigameState prelude=natural_host_prelude(5);hidden_join(&prelude);
    assert(apply(&won));root_callback(root,root_object);
    assert(!vi_black && !music_calls && s_terminal);
    for(int i=0;i<100 && YB(work,0)!=12;++i) root_callback(root,root_object);
    assert(cleanups==1 && reward_vms==1 && !intro_vms);
    vm=0;root_callback(root,root_object);assert(finished==1 && !vi_black);
}
static void export_native(void)
{
    char json[ANCHOR_MINIGAME_JSON_SIZE];begin_running(1);spawn_pair();
    AnchorMinigameState value=state();assert(anchor_minigame_state_encode(&value,json,sizeof(json)));puts(json);
}
static void test_debug_tutorial_for_owner_offline_and_follower(void)
{
    begin_running(1);AnchorMinigameState running=state();running.controller[3]=1200;
    for(int mode=0;mode<3;++mode) {
        fixture(1);assert(bound && YB(work,0)==4 && !vi_black && !flags[0x15]);
        int follower=mode==2;
        role(mode!=1,!follower,!follower);
        if(follower) assert(!apply(&running));
        for(int frame=0;frame<50 && !vm;++frame) {
            if(follower) assert(!apply(&running));
            root_callback(root,root_object);
        }
        assert(vm && intro_vms==1 && YB(work,0)==5 && !hud_builds && !proxy_builds);
        assert(!vi_black && !music_calls);
        for(int frame=0;frame<5;++frame) {
            if(follower) assert(!apply(&running));
            root_callback(root,root_object);
        }
        assert(vm && !hud_builds && !proxy_builds && intro_vms==1 && !music_calls);
        finish_intro();
        for(int frame=0;frame<30 && YB(work,0)!=7;++frame) {
            root_callback(root,root_object);tick_swimmer();tick_hud();
        }
        assert(YB(work,0)==7 && hud_builds==1 && proxy_builds==1 && !vi_black);
        assert(music_calls==1 && last_music==0x292 && !vm && intro_vms==1);
        if(follower) {
            assert(apply(&running));role(1,0,1);
            assert(YI(timer_task,0x84)==1200);
        }
        for(int frame=0;frame<5;++frame) root_callback(root,root_object);
        assert(music_calls==1);
    }
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--export")) { export_native();return 0; }
    if(argc==3 && !strcmp(argv[1],"--validate")) {
        char json[ANCHOR_MINIGAME_JSON_SIZE];AnchorMinigameStatus incoming;
        FILE *stream=fopen(argv[2],"rb");assert(stream);
        size_t count=fread(json,1,sizeof(json)-1,stream);assert(!ferror(stream) && feof(stream));
        fclose(stream);json[count]=0;assert(anchor_minigame_status_decode(json,&incoming));
        assert(incoming.has_state && incoming.state.controller[1]==7 && validate(&incoming.state));
        incoming.state.adopted_generation=incoming.generation;incoming.state.source_tick=incoming.tick;
        fixture(2);assert(bound && YB(work,0)==4);assert(!apply(&incoming.state));
        for(int i=0;i<64 && YB(work,0)!=7;++i) {
            root_callback(root,root_object);tick_swimmer();tick_hud();
        }
        assert(YB(work,0)==7 && !intro_vms && hud_builds==1 && proxy_builds==1);
        tick_hud();assert(!apply(&incoming.state));tick_children();assert(apply(&incoming.state));
        assert(vi_black);role(1,0,incoming.ready);root_callback(root,root_object);
        assert(vi_black==!incoming.ready && music_calls==incoming.ready);
        if(incoming.ready) assert(last_music==0x292);
        AnchorMinigameState adopted=state();assert(adopted.actor_count==incoming.state.actor_count);
        assert(adopted.controller[3]==incoming.state.controller[3]);
        puts("Yae Python status -> production codec -> native prelude and scheduled obstacle reconstruction/apply passed");return 0;
    }
    assert(argc==1);test_launch_and_bootstrap();test_timer_graph_and_inputs();
    test_reconstruction_and_atomicity();test_results_personal_fall_and_lifecycle();
    test_full_graph_replacement_and_contact_once();test_entry_identity_and_takeover_music();
    test_saturated_contact_queue_retries_after_ack();
    test_postphysics_prediction_hold_and_restore();
    test_join_waits_for_running_host_and_preserves_timer();
    test_debug_tutorial_for_owner_offline_and_follower();
    puts("Yae native adapter: scoped native launch, scheduled bootstrap, shared timer/obstacles, goal/hit inputs, result cleanup and lifecycle passed");
    return 0;
}
