/* Link the real invitation coordinator, guarded world transfer and Mermaid
 * adapter. Native substitutes model only verified File69 phase boundaries.
 * Pointer slots translate the32-bit native layout on64-bit host compilers. */
#include "impact_test_pointers.h"
#include "core/anchor_dialog.h"
#include "bosses/anchor_boss_invite_world.h"
#include <stdio.h>
#include <stdlib.h>
void anchor_boss_invite_world_begin_load(void);
void anchor_boss_invite_world_finish_load(void);
#ifdef ANCHOR_YAE_BEFORE_FIX
int anchor_minigame_invites_start_yae(void);
#endif
#ifdef ANCHOR_YAE_DIRECT_BEFORE
int anchor_minigame_invites_yae_eligible(void);
#endif

extern unsigned int D_8015C5D8_15D1D8[];
unsigned int *launch_character_word(void);
#define D_8015C5DC_15D1DC (*launch_character_word())
#define ANCHOR_MINIGAME_YAE_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define YP(p,o) TP(p,o)
#define YAE_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#ifndef YAE_ADAPTER_SOURCE
#define YAE_ADAPTER_SOURCE "../src/world/anchor_minigame_yae.c"
#endif
#include YAE_ADAPTER_SOURCE
#include "../src/world/anchor_minigame_codec.c"
#undef D_8015C5DC_15D1DC

unsigned short D_800C7AB2;
unsigned int D_8015C5D8_15D1D8[12];
short D_8006B780_6C380[0x226*5];
static unsigned int system_words[0xd0000u/4u];
unsigned char *D_8015C5C8_15D1C8=(unsigned char *)system_words;
static unsigned int root[64],root_object[64],local[16],player[64],player_object[64],player_work[64];
static unsigned int timer[64],proxy[64],proxy_object[64],assets[3][8],camera[8],group[8];
void *D_801FC604_5B8514=player,*D_801FC60C_5B851C=player_object,*D_8016DAB4_16E6B4;
void *D_801FC628_5B8538;
static unsigned int fixture_visit;
static int saved=1,connected=1,health=16,vm,modal,flags[256],warps,locks,clear_calls;
static int broadcasts,announced_game,prompt_count,prepare_calls,peer_current=1,peer_ready=1;
static int peer_session=202,peer_visit=17,prepared_cid,prepared_session,prepared_visit;
static int native_intro,native_preparations,hud_builds,proxy_builds;
static int fade_builds,fade_alpha,fade_target,proxy_initializations,proxy_gameplay_callbacks,music_calls,camera_ease_ticks;
static int native_constructor_steps[4][2],native_constructor_count;
static int vi_black,vi_black_calls;
static int debug_request;
static unsigned short destination;
static short destination_xyz[3];
static const AnchorMinigameAdapter *bound;
static AnchorDialogResult choice=ANCHOR_DIALOG_PENDING;
static const char *invitation;
int recomp_printf(const char *format,...) { (void)format;return 0; }

unsigned int *launch_character_word(void) { return &D_8015C5D8_15D1D8[1]; }
/* Only the production boss-world TU maps its host memcpy reads through this
 * translator. All readiness predicates, system offsets and guards stay real. */
void *launch_native_pointer_copy(void *out,const void *in,size_t size)
{
    if(size==sizeof(void *)) {
        const unsigned int offsets[]={4,0x18,0x5c};
        for(unsigned int i=0;i<3;++i) if(in==(unsigned char *)player+offsets[i]) {
            void *pointer=TP(player,offsets[i]);return memcpy(out,&pointer,size);
        }
    }
    return memcpy(out,in,size);
}
int item_sync_save_is_loaded(void) { return saved; }
unsigned int item_sync_local_player_health(void) { return (unsigned int)health; }
int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return 0; }
int anchor_dialog_busy(void) { return modal; }
int anchor_boss_invites_active(void) { return 0; }
int anchor_castle_return_sign_pending(void) { return 0; }
int anchor_boss_arena_for_room(unsigned short room) { (void)room;return 0; }
int anchor_boss_arena_for_impact_stage(unsigned int stage) { (void)stage;return 0; }
int anchor_boss_arena_room(int arena) { (void)arena;return -1; }
void anchor_player_skin_clear_for_character_change(void) { }
void func_801DD5C0_5994D0(void *task,unsigned char character)
{ assert(task==player);YB(player,0x60)=character;*launch_character_word()=character; }
int func_800240DC_24CDC(unsigned short flag) { assert(flag<256);return flags[flag]; }
void func_80024088_24C88(int flag)
{ assert(flag==0x1d || flag==0x1e);flags[flag]=0;++clear_calls; }
void func_80221F70_5DD440(void) { ++locks; }
int func_8003F1D8_3FDD8(void) { return vm; }
void *func_800141C4_14DC4(unsigned int file) { assert(file==69);return (void *)1; }
void func_80038BC8_397C8(unsigned short cue) { assert(cue==0x292);++music_calls; }
void osViBlack(unsigned char active) { vi_black=active!=0;++vi_black_calls; }
static void native_unblack(int fade)
{
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    if(fade) { TP(D_8015C5C8_15D1C8,0x3b014)=assets[0];anchor_minigame_yae_world_fade(); }
#endif
    osViBlack(0);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    if(fade) anchor_minigame_yae_world_fade_return();
    else anchor_minigame_yae_world_reset_return();
#else
    (void)fade;
#endif
}
void func_80038B98_39798(unsigned short cue) { (void)cue; }
void func_08000AB4_72C304(void *fade,float value)
{ assert(fade==TP(local,0x1c));fade_alpha=(int)value; }
void func_08000AC8_72C318(void *fade,float value)
{ assert(fade==TP(local,0x1c));fade_target=(int)value; }
void func_80034EF8_35AF8(void *task) { anchor_minigame_yae_delete(task); }
void *func_08000000_72B850(void *parent,YaeCallback callback,unsigned char priority)
{ (void)parent;(void)callback;(void)priority;assert(0);return 0; }
void func_08000E1C_72C66C(void *task,void *object) { (void)task;(void)object;assert(0); }
void func_0800107C_72C8CC(void *task,void *object)
{
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    anchor_minigame_yae_swimmer(task,object);
#endif
    if(!YB(task,0xd0)) {
        ++proxy_initializations;YB(task,0xd0)=1;YH(task,0x5c)=YH(task,0x5e)=0x33a;
        YB(player,0xcc)=0xcb;YB(player,0x30)&=(unsigned char)~1u;
    } else ++proxy_gameplay_callbacks;
    YF(player_object,8)=YF(object,8);YF(player_object,0x0c)=YF(object,0x0c)+15.f;
    YF(player_object,0x10)=YF(object,0x10)+16.f;
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    anchor_minigame_yae_swimmer_return();
#endif
}
void func_080002B4_72BB04(void *task)
{
    anchor_minigame_yae_hud(task);if(YI(task,0x84)>0) --YI(task,0x84);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    anchor_minigame_yae_hud_return();
#endif
}
unsigned int anchor_minigame_sync_visit(void) { return fixture_visit; }
unsigned int anchor_minigame_sync_local_ack(int game) { (void)game;return 0; }
int anchor_minigame_sync_participant_current(int game,int cid) { (void)game;(void)cid;return 0; }
/* This launch-only fixture has no remote display roster. Rendering behavior
 * and native constructor arguments are exercised by the adapter fixture. */
unsigned int anchor_minigame_sync_participants(int game,int *out,unsigned int capacity)
{ (void)game;(void)out;(void)capacity;return 0; }
int anchor_player_models_get_display_position(int cid,float *x,float *y,float *z)
{ (void)cid;(void)x;(void)y;(void)z;assert(0);return 0; }
int anchor_remote_model_pool_contains(const void *pointer) { (void)pointer;return 0; }
void *func_8000DBF0_E7F0(void *task,unsigned int model,unsigned int animation,
    float x,float y,float z,short rx,short ry,short rz,
    float sx,float sy,float sz,short file8,short file9)
{
    (void)task;(void)model;(void)animation;(void)x;(void)y;(void)z;
    (void)rx;(void)ry;(void)rz;(void)sx;(void)sy;(void)sz;(void)file8;(void)file9;
    assert(0);return 0;
}
void func_800115E0_121E0(void *object,int x,int y)
{ (void)object;(void)x;(void)y;assert(0); }
void func_8001136C_11F6C(void *task) { (void)task;assert(0); }
void *func_80036158_36D58(void *task,void *object,unsigned char count)
{ (void)task;(void)object;(void)count;assert(0);return 0; }
void anchor_minigame_sync_begin_load(void)
{ if(bound) reset();bound=0; }
void anchor_minigame_sync_finish_load(void) { ++fixture_visit; }
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *adapter)
{ bound=adapter;role(connected,1,1);return 1; }
int anchor_minigame_sync_game(void) { return bound ? 4:0; }
int anchor_minigame_sync_returning(void) { return bound && return_pending(); }
char *anchor_update_minigame(int game,int visit,const char *state)
{ assert(!game && !visit && !strcmp(state,"null"));return 0; }
int anchor_update_minigame_active(int game,int visit)
{ (void)visit;++broadcasts;announced_game=game;return connected; }
int anchor_minigame_prepare_join(int game,int cid,int session,int visit)
{
    assert(game==4);++prepare_calls;prepared_cid=cid;prepared_session=session;prepared_visit=visit;
    return !cid || (peer_current && peer_ready && session==peer_session && visit==peer_visit);
}
char *anchor_get_transfer_target_json(unsigned int cid)
{
    if(cid!=2 || !peer_current) return 0;
    char value[160];snprintf(value,sizeof(value),
        "{\"cid\":2,\"room\":340,\"game\":4,\"session\":%d,\"gameVisit\":%d}",peer_session,peer_visit);
    char *copy=malloc(strlen(value)+1);assert(copy);strcpy(copy,value);return copy;
}
char *anchor_get_minigame_invitation_json(void)
{
    if(!invitation || !peer_current) return 0;
    char *copy=malloc(strlen(invitation)+1);assert(copy);strcpy(copy,invitation);return copy;
}
int anchor_minigame_invitation_is_current(int cid,int session,int sequence)
{ assert(cid==2 && session==202 && sequence==7);return peer_current; }
void anchor_dismiss_minigame_invitation(int cid,int session,int sequence)
{ assert(cid==2 && session==202 && sequence==7);invitation=0; }
void recomp_free(void *memory) { free(memory); }
int anchor_dialog_begin_minigame(const char *name,const char *game)
{ assert(name && game && !modal);++prompt_count;modal=1;choice=ANCHOR_DIALOG_PENDING;return 1; }
AnchorDialogResult anchor_dialog_poll_for(AnchorDialogOwner owner)
{
    assert(owner==ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
    if(choice!=ANCHOR_DIALOG_PENDING) modal=0;
    return choice;
}
void anchor_dialog_cancel_for(AnchorDialogOwner owner)
{ assert(owner==ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);modal=0;choice=ANCHOR_DIALOG_CANCELLED; }
void func_8000607C_6C7C(unsigned short room,short x,short y,short z,
                      short camera_rot,short player_rot,short field90,int field91)
{
    (void)camera_rot;(void)player_rot;
    assert(!field90 && !field91);destination=room;++warps;
    destination_xyz[0]=x;destination_xyz[1]=y;destination_xyz[2]=z;
    D_8015C5C8_15D1C8[0x3ae29]=1;
}
void func_80003728_4328(unsigned char step)
{ assert(step==12);D_8015C5C8_15D1C8[0x3add4]=step; }
void func_800061B0_6DB0(unsigned char index,unsigned short field90,unsigned int field91)
{ (void)index;(void)field90;(void)field91;assert(0); }
void func_80006140_6D40(unsigned char index,unsigned short field90,unsigned int field91)
{ (void)index;(void)field90;(void)field91;assert(0); }

static void install_player(int character)
{
    void *backlink=player;TP(player,4)=group;memcpy(group,&backlink,sizeof(backlink));
    TP(player,0x18)=player_object;TP(player,0x5c)=player_work;
    YB(player,0x60)=(unsigned char)character;*launch_character_word()=(unsigned int)character;
}
static void load(unsigned short room)
{
    anchor_minigame_sync_begin_load();anchor_boss_invite_world_begin_load();anchor_minigame_invites_begin_load();
    D_800C7AB2=room;anchor_minigame_sync_finish_load();
    anchor_boss_invite_world_finish_load();anchor_minigame_invites_finish_load();
    native_unblack(0);
}
static void idle_world(void)
{
    D_8015C5C8_15D1C8[0x3add4]=13;D_8015C5C8_15D1C8[0x3adde]=1;
    D_8015C5C8_15D1C8[0x3ae29]=0;YB(player_work,0x69)=0;YB(player,0xcd)=0;
}
static void fixture(void)
{
    saved=0;anchor_minigame_invites_update();reset();saved=1;test_ptr_count=0;memset(test_ptrs,0,sizeof(test_ptrs));bound=0;
    memset(system_words,0,sizeof(system_words));memset(root,0,sizeof(root));
    memset(root_object,0,sizeof(root_object));memset(local,0,sizeof(local));
    memset(player,0,sizeof(player));memset(player_work,0,sizeof(player_work));
    memset(timer,0,sizeof(timer));memset(proxy,0,sizeof(proxy));memset(proxy_object,0,sizeof(proxy_object));
    memset(flags,0,sizeof(flags));memset(D_8015C5D8_15D1D8,0,sizeof(D_8015C5D8_15D1D8));
    vm=modal=warps=locks=clear_calls=prompt_count=prepare_calls=0;peer_current=peer_ready=1;health=16;
    connected=1;debug_request=0;
    peer_session=202;peer_visit=17;prepared_cid=prepared_session=prepared_visit=0;
    native_intro=native_preparations=hud_builds=proxy_builds=0;invitation=0;
    fade_builds=fade_alpha=fade_target=proxy_initializations=proxy_gameplay_callbacks=music_calls=camera_ease_ticks=0;
    native_constructor_count=vi_black=vi_black_calls=0;
    install_player(1);idle_world();load(0x42);anchor_minigame_invites_update();
    YF(player_object,8)=17.25f;YF(player_object,0x0c)=-81.5f;YF(player_object,0x10)=328.75f;
    assert(anchor_boss_invite_world_loaded_player_active());
    assert(anchor_boss_invite_world_can_prompt());
}
static void construct_root(void)
{
    memset(root,0,sizeof(root));memset(root_object,0,sizeof(root_object));memset(local,0,sizeof(local));
    TP(root,0x18)=root_object;TP(root,0xd0)=local;TP(root,0x0c)=(void *)func_080014E8_72CD38;
    YH(root,0x28)=69;YH(root,0x5e)=0x33a;YB(root,0x74)=7;D_8016DAB4_16E6B4=root;
    func_080014E8_72CD38(root,root_object);
}
void func_080014E8_72CD38(void *task,void *object)
{
    int phase=YB(local,0),frame=YS(local,4);anchor_minigame_yae_root(task,object);
    if((phase==4 && (frame==1 || frame==20)) || (phase==6 && frame==1)) {
        assert(native_constructor_count<4);
        native_constructor_steps[native_constructor_count][0]=phase;
        native_constructor_steps[native_constructor_count++][1]=frame;
    }
    switch(phase) {
    case 0:YH(task,0x5e)=0x327;TP(local,0x14)=group;YB(local,0)=1;break;
    case 1:break;
    case 2:
        if(YB(player,0xcd)==3) { YB(player,0xcd)=4;func_80221F70_5DD440();vm=1;YB(local,0)=3; }
        break;
    case 3:
        if(!vm) {
            YW(task,0x68)=(YW(task,0x68)&~0x100u)|0x4400u;YS(local,4)=0;
            YB(local,0)=(flags[0x15] && !flags[0x1e] && *launch_character_word()==3 && flags[0x7a]) ? 4:12;
        }
        break;
    case 4:
        if(frame==1) { ++fade_builds;fade_alpha=255;TP(local,0x1c)=assets[0]; }
        if(frame==20) {
            assert(fade_builds==1 && fade_alpha==255);
            ++native_preparations;TP(local,0x0c)=camera;TP(camera,0x6c)=assets[2];
            YF(player_object,8)=0.f;YF(player_object,0x0c)=-454.f;YF(player_object,0x10)=190.f;
            fade_target=0;
        }
        if(frame==40) { ++native_intro;vm=1;YB(local,0)=5; }
        ++YS(local,4);break;
    case 5:if(!vm) { YB(local,0)=6;YS(local,4)=1; }break;
    case 6:
        ++camera_ease_ticks;
        if(frame==1) {
            ++hud_builds;++proxy_builds;TP(local,0x10)=timer;TP(local,0x18)=proxy;
            TP(timer,0x0c)=(void *)func_080002B4_72BB04;
            TP(timer,0x78)=assets[0];TP(timer,0x7c)=assets[1];TP(timer,0x80)=assets[2];YI(timer,0x84)=1500;
            TP(proxy,0x18)=proxy_object;TP(proxy,0x0c)=(void *)func_0800107C_72C8CC;YH(proxy,0x28)=69;
            YF(proxy_object,8)=0.f;YF(proxy_object,0x0c)=-350.f;YF(proxy_object,0x10)=264.f;
            D_8015C5C8_15D1C8[0x3ae20]=1;
            D_8015C5C8_15D1C8[0xcf8a2]=1;
        }
        if(frame==20) { YB(local,1)=2;YB(local,0)=7; }
        ++YS(local,4);break;
    case 7:break;
    case 12:if(!vm) { YB(local,0)=1;YB(player,0xcd)=0; }break;
    default:assert(0);
    }
    anchor_minigame_yae_root_return();
}
static void native_frame_end(void)
{
#ifndef ANCHOR_YAE_BEFORE_FIX
    anchor_minigame_yae_frame();
#endif
}
static void frame_end(void) { anchor_minigame_invites_update();native_frame_end(); }
static int debug_start(void)
{
#ifdef ANCHOR_YAE_BEFORE_FIX
    int started=anchor_minigame_invites_start_yae();
#else
    int started=anchor_minigame_invites_debug_start_yae();
#endif
    if(started) debug_request=1;
    return started;
}
static void arrive_pending(int root_exists)
{
    assert(destination==0x154 && D_8015C5C8_15D1C8[0x3add4]==12);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    if(!debug_request) assert(vi_black);
#endif
    load(0x154);install_player((int)*launch_character_word());
    D_8015C5C8_15D1C8[0x3add4]=13;D_8015C5C8_15D1C8[0x3adde]=0;
    D_8015C5C8_15D1C8[0x3ae29]=0;
    YB(player_work,0x69)=1;YB(player,0xcc)=0xba;
    if(root_exists) construct_root();
    frame_end();
    assert(!bound && YB(local,0)==(root_exists ? 1:0));
    idle_world();
}
static void arrive_requested(void) { arrive_pending(1);frame_end(); }
static void assert_trial_running(void)
{
    assert(bound && YB(local,0)==4 && !anchor_minigame_invites_yae_requested());
    int entered=0;
    for(int i=0;i<64 && !entered;++i) {
        root_callback(root,root_object);
        assert(!vm && !native_intro);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
        /* Once the callback commits and reveals phase7, its next HUD draw is
         * the first gameplay tick, rather than hidden setup time. */
        if(YB(local,0)==7 && !vi_black) { entered=1;break; }
#endif
        if(TP(local,0x18)) {
            YaeCallback callback=(YaeCallback)TP(proxy,0x0c);callback(proxy,proxy_object);
        }
        if(TP(local,0x10)) {
            if(((unsigned long)TP(timer,0x0c)&~YAE_DISABLED)==
               ((unsigned long)(void *)func_080002B4_72BB04&~YAE_DISABLED))
                func_080002B4_72BB04(timer);
            else ((YaeCallback)TP(timer,0x0c))(timer,0);
            assert(YI(timer,0x84)==1500);
        }
        native_unblack(1);native_frame_end();
#ifndef ANCHOR_YAE_DIRECT_BEFORE
        assert(vi_black || (YB(local,0)==7 && hud_live() && swimmer_object()));
        entered=YB(local,0)==7 && !vi_black;
#else
        entered=YB(local,0)==7;
#endif
    }
    assert(YB(local,0)==7 && native_preparations==1 && hud_builds==1 && proxy_builds==1);
    assert(YI(timer,0x84)==1500 && YB(player,0xcc)==0xcb && proxy_initializations==1 && !flags[0x7a]);
    assert(native_constructor_count==3 && fade_builds==1 && !native_intro && !vm);
    assert(native_constructor_steps[0][0]==4 && native_constructor_steps[0][1]==1);
    assert(native_constructor_steps[1][0]==4 && native_constructor_steps[1][1]==20);
    assert(native_constructor_steps[2][0]==6 && native_constructor_steps[2][1]==1);
    assert(proxy_gameplay_callbacks==0 && music_calls==1);
    assert(camera_ease_ticks==20 && fade_target==0);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    assert(!vi_black);
#endif
}
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
static void assert_debug_tutorial_then_running(void)
{
    assert(bound && YB(local,0)==4 && !anchor_minigame_invites_yae_requested());
    for(int tick=0;tick<50 && !vm && YB(local,0)==4;++tick) root_callback(root,root_object);
    assert(vm && native_intro==1 && YB(local,0)==5 && !hud_builds && !proxy_builds);
    assert(!vi_black && !music_calls);
    for(int tick=0;tick<5;++tick) root_callback(root,root_object);
    assert(vm && native_intro==1 && !hud_builds && !proxy_builds && !music_calls);
    /* Only the native scenario-completion edge permits HUD/proxy setup. */
    func_80038BC8_397C8(0x292);vm=0;
    for(int tick=0;tick<30 && YB(local,0)!=7;++tick) {
        root_callback(root,root_object);
        if(TP(local,0x18)) ((YaeCallback)TP(proxy,0x0c))(proxy,proxy_object);
        if(TP(local,0x10)) {
            if(TP(timer,0x0c)==(void *)func_080002B4_72BB04) func_080002B4_72BB04(timer);
            else ((YaeCallback)TP(timer,0x0c))(timer,0);
        }
    }
    assert(YB(local,0)==7 && native_intro==1 && hud_builds==1 && proxy_builds==1);
    assert(proxy_initializations==1 && music_calls==1 && !vi_black);
}
#endif
static void offer(void)
{
    invitation="{\"cid\":2,\"session\":202,\"seq\":7,\"game\":4,\"gameVisit\":17,\"name\":\"Host\"}";
    anchor_minigame_invites_update();
}
static void test_explicit_debug_full_load(void)
{
    fixture();flags[0x1d]=flags[0x1e]=1;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    assert(debug_start());assert(*launch_character_word()==3 && YB(player,0x60)==1);
    arrive_pending(1);
    const unsigned int gates[]={0x3ae16,0x3ae26,0x3ae29,0xcf8a2,0x3ae20,0x3ae22,0x3ae23};
    for(unsigned int i=0;i<sizeof(gates)/sizeof(gates[0]);++i) {
        D_8015C5C8_15D1C8[gates[i]]=1;native_frame_end();
        assert(!bound && anchor_minigame_invites_yae_requested());
        D_8015C5C8_15D1C8[gates[i]]=0;
    }
    YB(player,0xcd)=1;native_frame_end();assert(!bound);YB(player,0xcd)=0;
    YB(player,0x60)=1;assert(anchor_boss_invite_world_can_prompt());native_frame_end();
    assert(!bound);YB(player,0x60)=3;
    vm=1;native_frame_end();assert(!bound);vm=0;modal=1;native_frame_end();assert(!bound);modal=0;
    frame_end();assert_debug_tutorial_then_running();
    assert(!flags[0x15] && !flags[0x1d] && !flags[0x1e] && clear_calls==2);
#else
    assert(!debug_start() && !warps && !anchor_minigame_invites_yae_requested());
#endif
}
static void test_offline_debug_retains_tutorial(void)
{
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    fixture();connected=0;assert(debug_start());arrive_requested();assert_debug_tutorial_then_running();
#endif
}
static void test_join_eligibility(void)
{
    for(int story=0;story<2;++story) for(int previous=0;previous<2;++previous)
        for(int via_list=0;via_list<2;++via_list) {
        fixture();flags[0x15]=story;flags[0x1d]=flags[0x1e]=previous;flags[0x1f]=1;
        if(via_list) {
            int result=anchor_minigame_invites_transfer_to_yae_client(2);
            assert(result);
        } else {
            offer();assert(modal);choice=ANCHOR_DIALOG_YES;anchor_minigame_invites_update();
        }
        assert(warps==1 && prepared_cid==2 && prepared_session==202 && prepared_visit==17);
        int before_prepare=prepare_calls;
        arrive_requested();assert_trial_running();
        assert(prepare_calls>before_prepare && prepared_cid==2 && prepared_session==202 && prepared_visit==17);
        assert(clear_calls==2 && flags[0x15]==story && flags[0x1f]);
        assert(!flags[0x1d] && !flags[0x1e]);
    }
    fixture();peer_current=0;
    assert(!anchor_minigame_invites_transfer_to_yae_client(2) && !warps);
    assert(!anchor_minigame_invites_transfer_to_yae_client(0) && !warps);
    frame_end();assert(!bound && !anchor_minigame_invites_yae_requested());

    fixture();flags[0x1e]=1;offer();assert(modal);peer_ready=0;
    choice=ANCHOR_DIALOG_YES;anchor_minigame_invites_update();
    assert(!warps && anchor_minigame_invites_active() && *launch_character_word()==1);
    peer_ready=1;anchor_minigame_invites_update();assert(warps==1);
    arrive_requested();assert_trial_running();
}
static void test_ordinary_sign_predicates_and_refusal(void)
{
    for(int story=0;story<2;++story) for(int completed=0;completed<2;++completed)
        for(int accepted=0;accepted<2;++accepted) {
            fixture();load(0x154);install_player(3);idle_world();construct_root();frame_end();
            assert(!bound && !anchor_minigame_invites_yae_requested());
            flags[0x15]=story;flags[0x1e]=completed;flags[0x7a]=accepted;
            /* Native phase2 waits for the interacting playerCD3, then starts
             * scenario176. Its own accepted3->4 branch owns every gate. */
            YB(local,0)=2;YB(player,0xcd)=3;func_080014E8_72CD38(root,root_object);
            assert(YB(local,0)==3 && vm && !bound);frame_end();vm=0;
            func_080014E8_72CD38(root,root_object);
            int enters=story && !completed && accepted;
            assert((bound!=0)==enters && !clear_calls);
            assert(flags[0x15]==story && flags[0x1e]==completed);
            if(!enters) {
                assert(YB(local,0)==12);frame_end();func_080014E8_72CD38(root,root_object);frame_end();
                assert(YB(local,0)==1 && !bound && !anchor_minigame_invites_yae_requested());
            } else {
                /* Entering through the real sign retains its instruction
                 * conversation and ordinary native constructor timing. */
                for(int i=0;i<50 && !vm;++i) root_callback(root,root_object);
                assert(vm && native_intro==1 && YB(local,0)==5 && !hud_builds);
                for(int i=0;i<3;++i) root_callback(root,root_object);
                assert(vm && !hud_builds && native_intro==1);
                func_80038BC8_397C8(0x292);vm=0;
                for(int i=0;i<30 && YB(local,0)!=7;++i) root_callback(root,root_object);
                assert(YB(local,0)==7 && native_intro==1 && hud_builds==1 && music_calls==1);
            }
        }
}
static int request_origin_for_test(int debug)
{
    if(debug) return debug_start();
    flags[0x15]=1;return anchor_minigame_invites_transfer_to_yae_client(2);
}
static void assert_original_source_return(void)
{
    assert(warps==2 && destination==0x42 && *launch_character_word()==1);
    assert(destination_xyz[0]==17 && destination_xyz[1]==-81 && destination_xyz[2]==328);
    assert(!anchor_minigame_invites_yae_requested() && !bound && !clear_calls);
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    assert(!vi_black);
#endif
    int count=warps;frame_end();frame_end();assert(warps==count);
}
static void test_manual_interaction_consumes_pending_permission(void)
{
    for(int debug=0;debug<2;++debug) for(int phase_index=0;phase_index<3;++phase_index) {
#if !defined(DEBUG_BUTTON_ENABLED) || !DEBUG_BUTTON_ENABLED
        if(debug) continue;
#endif
        const int manual_phases[]={2,3,12};
        fixture();assert(request_origin_for_test(debug));arrive_pending(1);
        flags[0x15]=0;YB(local,0)=(unsigned char)manual_phases[phase_index];
        YB(player,0xcd)=3;vm=phase_index ? 1:0;
        D_8015C5C8_15D1C8[0x3ae20]=1;D_8015C5C8_15D1C8[0x3ae22]=1;
        func_080014E8_72CD38(root,root_object);frame_end();
        assert(!anchor_minigame_invites_yae_requested() && !bound && warps==1 && !clear_calls);
        assert(vm && D_8015C5C8_15D1C8[0x3ae20]==1 && D_8015C5C8_15D1C8[0x3ae22]==1);
        vm=0;flags[0x7a]=1;
        if(YB(local,0)==3) func_080014E8_72CD38(root,root_object);
        if(YB(local,0)==12) func_080014E8_72CD38(root,root_object);
        assert(YB(local,0)==1 && !bound);frame_end();assert(warps==1);
        D_8015C5C8_15D1C8[0x3ae20]=D_8015C5C8_15D1C8[0x3ae22]=0;
        frame_end();assert_original_source_return();
    }
}
static void test_revoked_join_and_missing_actor_cleanup(void)
{
    fixture();assert(request_origin_for_test(0));arrive_pending(1);
    peer_current=0;frame_end();assert(!bound && !clear_calls);
    frame_end();assert_original_source_return();
    for(int no_root=0;no_root<2;++no_root) {
        fixture();assert(request_origin_for_test(0));arrive_pending(!no_root);
        if(no_root) for(int i=0;i<602;++i) frame_end();
        else { anchor_minigame_yae_delete(root);frame_end(); }
        assert_original_source_return();
    }
    for(int changed_session=0;changed_session<2;++changed_session) {
        fixture();assert(request_origin_for_test(0));arrive_pending(1);
        if(changed_session) peer_session=303;else peer_visit=18;
        frame_end();assert(!bound && !clear_calls && prepared_session==202 && prepared_visit==17);
        frame_end();assert_original_source_return();
    }
    for(int zero_session=0;zero_session<2;++zero_session) {
        fixture();if(zero_session) peer_session=0;else peer_visit=0;
        assert(!anchor_minigame_invites_transfer_to_yae_client(2));
        assert(!warps && !prepare_calls && !anchor_minigame_invites_yae_requested());
    }
}
static void test_debug_retry_preserves_new_request(void)
{
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    fixture();assert(debug_start());arrive_pending(1);
    assert(debug_start() && warps==2);
    /* The retiring old room's candidate deletion must not cancel the newly
     * accepted transfer, whose new origin is consumed by the next load. */
    anchor_minigame_yae_delete(root);arrive_requested();assert_debug_tutorial_then_running();
    unsigned short source;float xyz[3];
    assert(anchor_minigame_invites_source_waypoint(&source,xyz));
    assert(source==0x42 && xyz[0]==17.25f && xyz[1]==-81.5f && xyz[2]==328.75f);
#endif
}
static void test_direct_black_cleanup(void)
{
#ifndef ANCHOR_YAE_DIRECT_BEFORE
    for(int teardown=0;teardown<7;++teardown) {
        fixture();assert(request_origin_for_test(0));arrive_requested();
        for(int slot=0;slot<3;++slot) {
            root_callback(root,root_object);
            if(TP(local,0x18)) ((YaeCallback)TP(proxy,0x0c))(proxy,proxy_object);
            if(TP(local,0x10)) {
                if(TP(timer,0x0c)==(void *)func_080002B4_72BB04) func_080002B4_72BB04(timer);
                else ((YaeCallback)TP(timer,0x0c))(timer,0);
            }
        }
        assert(vi_black && YB(local,0)==6 && D_8015C5C8_15D1C8[0xcf8a2]==1);
        if(teardown<=1) {
            if(teardown) YW(root,0x68)|=2u; /* Deferred deletion marks first. */
            anchor_minigame_yae_delete(root);assert(!vi_black);
        } else if(teardown==2) {
            anchor_minigame_yae_reuse(root);assert(!vi_black);
        } else if(teardown==3) {
            ++YB(root,0x74);anchor_minigame_yae_reuse(root);
            native_unblack(0);assert(!vi_black);
        } else if(teardown==4) {
            load(0x155);idle_world();D_8015C5C8_15D1C8[0xcf8a2]=0;frame_end();assert(!vi_black);
        } else {
            if(teardown==5) D_8015C5C8_15D1C8[0x3add4]=3;
            else { saved=0;anchor_minigame_invites_update(); }
            anchor_minigame_yae_direct_load_cancel();assert(vi_black);
            /* A no-record fade call never requests native unblank. Its return
             * must not consume a pending handoff or override external black. */
            TP(D_8015C5C8_15D1C8,0x3b014)=0;
            anchor_minigame_yae_world_fade();anchor_minigame_yae_world_fade_return();assert(vi_black);
            native_unblack(0);assert(!vi_black);
        }
    }
#endif
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--debug-only")) { test_explicit_debug_full_load();return 0; }
    if(argc==2 && !strcmp(argv[1],"--join-only")) { test_join_eligibility();return 0; }
    assert(argc==1);
    test_explicit_debug_full_load();test_join_eligibility();
    test_offline_debug_retains_tutorial();
    test_ordinary_sign_predicates_and_refusal();test_manual_interaction_consumes_pending_permission();
    test_revoked_join_and_missing_actor_cleanup();
    test_debug_retry_preserves_new_request();
    test_direct_black_cleanup();
    puts("Yae integrated launch: real world guards, full-load request, native initialization, frame-end entry and debug/join eligibility passed");
    return 0;
}
