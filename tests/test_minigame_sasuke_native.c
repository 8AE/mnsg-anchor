/* Runs the production File47 adapter with native-sized pointer slots.
 * Native substitutes below model the audited scalar/control side effects;
 * this proves adapter boundaries, not game runtime or two-client behavior. */
#include "impact_test_pointers.h"
#include <stdio.h>
#include <stdlib.h>
#define ANCHOR_MINIGAME_SASUKE_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define SP(p,o) TP(p,o)
#define SASUKE_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#include "../src/world/anchor_minigame_sasuke.c"
#include "../src/world/anchor_minigame_codec.c"

unsigned short D_800C7AB2=SJ_ROOM;
void *D_801FC60C_5B851C;
void *D_8016DAB4_16E6B4;
static unsigned int root[64], root_object[64], tasks[SJ_COUNT][64], objects[SJ_COUNT][64];
static unsigned int hud[64], player[64];
static unsigned int visit=1;
static int vm, missing_resource, native_root_calls, native_platform_calls, hud_draws;
static int dialogue_calls, hud_builds, hud_deletes, native_returns, camera_calls;
static int instruction_flag;
static int music_calls, gameplay_music_calls, music_cue;
static const AnchorMinigameAdapter *bound;
unsigned int anchor_minigame_sync_visit(void) { return visit; }
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *a)
{ bound=a; role(1,0,0); return 1; }
void *func_800141C4_14DC4(unsigned int file)
{ assert(file==47);return missing_resource ? (void *)(unsigned long)0xffffffffu:(void *)1; }
int func_8003F1D8_3FDD8(void) { return vm; }
void func_80038BC8_397C8(unsigned short cue)
{
    assert(cue==0x0292 || cue==0x0052);
    ++music_calls;music_cue=cue;
    if(cue==0x0052) ++gameplay_music_calls;
}
static void finish_dialogue(void)
{
    assert(vm && dialogue_calls);
    /* Scenario 179's final audio bytecode precedes its VM completion. */
    func_80038BC8_397C8(0x0052);vm=0;
}
void func_80034EF8_35AF8(void *task)
{ assert(task==hud);++hud_deletes;anchor_minigame_sasuke_delete(task); }
void func_080002B4_7060A4(void *task,void *object)
{ (void)object;++hud_draws;if(SI(task,0x80)>0) --SI(task,0x80); }
void func_080006F8_7064E8(void *task,void *object)
{
    unsigned char phase=SB(task,0xd0);
    void *callback=SP(task,0x0c);
    anchor_minigame_sasuke_orbit(task,object);
    assert(SB(task,0xd0)==phase && SP(task,0x0c)==callback);
    ++native_platform_calls;
    if(!SB(task,0xd0)) {
        SH(task,0x5c)=SH(task,0x5e)=SJ_ENTITY;SB(task,0xd0)=1;
        SW(task,0x64)|=0x4000u; /* Native platform collision init. */
    } else { ++SS(task,0xd6);SF(object,8)+=1.f; }
    SF(object,0x0c)=SF(task,0xdc);
    anchor_minigame_sasuke_orbit_return();
}
void func_08000980_706770(void *task,void *object)
{
    unsigned char phase=SB(task,0xd0);
    void *callback=SP(task,0x0c);
    anchor_minigame_sasuke_spin(task,object);
    assert(SB(task,0xd0)==phase && SP(task,0x0c)==callback);
    ++native_platform_calls;
    if(!SB(task,0xd0)) {
        SH(task,0x5c)=SH(task,0x5e)=SJ_ENTITY;SB(task,0xd0)=1;SW(task,0x64)|=0x4000u;
    } else SH(object,0x16)=(unsigned short)(SH(object,0x16)+SS(task,0xd2));
    SF(object,0x0c)=SF(task,0xd4);
    anchor_minigame_sasuke_spin_return();
}
static void construct_hud(void)
{
    ++hud_builds;memset(hud,0,sizeof(hud));
    SP(root,0xe4)=hud;SP(hud,0x0c)=(void *)func_080002B4_7060A4;
    SP(hud,0x78)=player;SP(hud,0x7c)=root_object;SI(hud,0x80)=3960;
    anchor_minigame_sasuke_hud(hud,0);
}
void func_08000AD4_7068C4(void *task,void *object)
{
    unsigned char phase=SB(task,0xd0);
    void *callback=SP(task,0x0c);
    anchor_minigame_sasuke_root(task,object);
    assert(SB(task,0xd0)==phase && SP(task,0x0c)==callback);
    (void)object;++native_root_calls;
    switch(SB(task,0xd0)) {
    case 0:
        if(SS(task,0xd4)<2) break;
        SH(task,0x5c)=SH(task,0x5e)=SJ_ENTITY;
        SP(task,0xdc)=root_object;SP(task,0xe0)=player;
        instruction_flag=1;SB(task,0xd0)=1;break;
    case 1:
        if(SS(task,0xd4)>=6) {
            ++dialogue_calls;vm=1;func_80038BC8_397C8(0x0292);SB(task,0xd0)=2;
        }break;
    case 2:
        if(!vm) { construct_hud();instruction_flag=0;SB(task,0xd0)=3; }break;
    case 3:
        ++camera_calls;
        if(SF(player,0x0c)>620.f &&
           SF(player,8)*SF(player,8)+SF(player,0x10)*SF(player,0x10)>2000.f) {
            if(SP(task,0xe4)) {
                func_80034EF8_35AF8(SP(task,0xe4));SP(task,0xe4)=0;
            }
            SB(task,0xd0)=6;SS(task,0xd4)=0;
        } else if(SP(task,0xe4) && SI(SP(task,0xe4),0x80)<=0) {
            SB(task,0xd0)=5;SS(task,0xd4)=0;
        }
        break;
    case 5:case 6:
        if(++SS(task,0xd4)>=60) { SB(task,0xd0)=7;SS(task,0xd4)=1; }break;
    case 7:
        if(SS(task,0xd4)==10) ++native_returns;
        ++SS(task,0xd4);break;
    }
    if(phase<=2) ++SS(task,0xd4); /* Native shared epilogue, after phase checks. */
    anchor_minigame_sasuke_root_return();
}
static void fixture_root(void)
{
    reset();memset(root,0,sizeof(root));memset(root_object,0,sizeof(root_object));
    memset(tasks,0,sizeof(tasks));memset(objects,0,sizeof(objects));memset(player,0,sizeof(player));
    test_ptr_count=0;vm=missing_resource=0;D_800C7AB2=SJ_ROOM;D_801FC60C_5B851C=player;
    native_root_calls=native_platform_calls=hud_draws=dialogue_calls=hud_builds=0;
    hud_deletes=native_returns=camera_calls=instruction_flag=0;bound=0;
    music_calls=gameplay_music_calls=0;music_cue=0x0292;
    SP(root,0x18)=root_object;SP(root,0x0c)=(void *)func_08000AD4_7068C4;
    D_8016DAB4_16E6B4=root;
    SB(root,0x74)=3;
    func_08000AD4_7068C4(root,root_object);
    assert(!bound && !s_root.initialized && SB(root,0xd0)==0 && SS(root,0xd4)==1);
    assert(callback_is(SP(root,0x0c),func_08000AD4_7068C4));
    func_08000AD4_7068C4(root,root_object);
    assert(!bound && !s_root.initialized && SB(root,0xd0)==0 && SS(root,0xd4)==2);
    assert(callback_is(SP(root,0x0c),func_08000AD4_7068C4));
    func_08000AD4_7068C4(root,root_object);
    assert(bound==&s_adapter);
    assert(native_root_calls==3 && s_root.initialized && SB(root,0xd0)==1);
    assert(SS(root,0xd4)==3 && instruction_flag==1);
    assert(callback_is(SP(root,0x0c),root_callback));
}
static void fixture_platforms(void)
{
    for(int i=0;i<SJ_COUNT;++i) {
        const SasukeConfig *c=&s_config[i];void *t=tasks[i],*o=objects[i];
        SP(t,0x18)=o;SH(t,0x28)=47;SB(t,0x74)=4;
        SH(o,0x16)=(unsigned short)c->yaw;
        SP(t,0x0c)=(void *)(c->kind==SJ_ORBIT ? func_080006F8_7064E8:func_08000980_706770);
        if(c->kind==SJ_ORBIT) {
            SF(t,0xdc)=(float)c->y;SS(t,0xd2)=c->yaw;SS(t,0xd6)=c->parameter;
            /* Native first four orbit constructors use 360; later ones 240. */
            SS(t,0xd4)=(short)(i<4 ? 360:240);SF(t,0xd8)=30.f;SF(t,0xe0)=20.f;
            func_080006F8_7064E8(t,o);
        } else {
            SF(t,0xd4)=(float)c->y;SS(t,0xd2)=c->parameter;
            func_08000980_706770(t,o);
        }
        assert(s_platforms[i].task==t && platform_live(i));
        assert(SW(t,0x64)&0x4000u);
    }
}
static void fixture(void)
{
    fixture_root();fixture_platforms();
}
static AnchorMinigameState state(void)
{
    AnchorMinigameState s={0};assert(capture(&s));return s;
}
static void start(void)
{
    s_host_phase=3;root_callback(root,root_object);assert(SB(root,0xd0)==2);
    root_callback(root,root_object);assert(SB(root,0xd0)==3 && hud_live());
    role(1,0,1);
}
static void test_bootstrap(void)
{
    fixture();assert(graph());
    root_callback(root,root_object);assert(SB(root,0xd0)==1 && !dialogue_calls);
    AnchorMinigameState host=state();host.controller[1]=3;host.controller[6]=2200;
    host.running=1;assert(validate(&host));assert(!apply(&host));
    assert(SB(root,0xd0)==1 && !hud_builds);
    vm=1;root_callback(root,root_object);assert(SB(root,0xd0)==1);
    vm=0;root_callback(root,root_object);assert(SB(root,0xd0)==2 && !dialogue_calls);
    root_callback(root,root_object);assert(hud_builds==1 && SB(root,0xd0)==3);
    assert(apply(&host));assert(SI(hud,0x80)==2200 && !dialogue_calls && !instruction_flag);
    fixture();role(0,1,1);
    for(int i=0;i<6;++i) root_callback(root,root_object);
    assert(dialogue_calls==1 && vm && !hud_builds);
    root_callback(root,root_object);assert(!hud_builds);
    finish_dialogue();root_callback(root,root_object);assert(hud_builds==1);
    assert(music_calls==2 && gameplay_music_calls==1 && music_cue==0x0052);
}
static void test_fresh_owner_entry(void)
{
    AnchorMinigameState initial={0};
    fixture_root();
    assert(!capture(&initial)); /* Platform callbacks have not run yet. */
    root_callback(root,root_object);
    assert(native_root_calls==3 && SB(root,0xd0)==1 && SS(root,0xd4)==3);
    fixture_platforms();
    assert(capture(&initial) && validate(&initial) && initial.ready);
    assert(initial.controller[1]==1 && initial.controller[6]==-1);
    /* The bridge elects the fresh local owner from this complete phase1 graph.
     * The introductory VM and HUD do not form a readiness prerequisite. */
    role(1,1,1);
    for(int i=0;i<3;++i) root_callback(root,root_object);
    assert(!dialogue_calls && SB(root,0xd0)==1 && SS(root,0xd4)==6);
    root_callback(root,root_object);
    assert(dialogue_calls==1 && vm && SB(root,0xd0)==2 && !hud_builds && instruction_flag);
    root_callback(root,root_object);assert(!hud_builds);
    finish_dialogue();root_callback(root,root_object);
    assert(SB(root,0xd0)==3 && hud_live() && hud_builds==1 && !instruction_flag);
    assert(SI(hud,0x80)==3960);
    assert(music_calls==2 && gameplay_music_calls==1 && music_cue==0x0052);
}
static void test_skipped_intro_music(void)
{
    AnchorMinigameState host;
    /* A guest may first adopt phase 2 directly while the owner's VM runs. */
    fixture();host=state();host.controller[1]=2;
    assert(apply(&host) && SB(root,0xd0)==2 && !music_calls);
    root_callback(root,root_object);assert(!hud_builds && !music_calls);
    host.controller[1]=3;host.controller[6]=2200;host.running=1;
    assert(!apply(&host));vm=1;
    root_callback(root,root_object);assert(!hud_builds && !music_calls);
    vm=0;root_callback(root,root_object);
    assert(hud_live() && music_cue==0x0052 && gameplay_music_calls==1 && music_calls==1);
    assert(apply(&host));role(1,0,1);
    for(int i=0;i<8;++i) { assert(apply(&host));root_callback(root,root_object); }
    assert(gameplay_music_calls==1 && music_calls==1 && !dialogue_calls);
    /* A direct running-state join takes the native HUD bootstrap path. */
    fixture();host=state();host.controller[1]=3;host.controller[6]=1800;host.running=1;
    assert(!apply(&host));root_callback(root,root_object);
    assert(SB(root,0xd0)==2 && !music_calls);
    root_callback(root,root_object);
    assert(hud_live() && gameplay_music_calls==1 && music_cue==0x0052);
    assert(apply(&host));role(1,0,1);root_callback(root,root_object);
    assert(music_calls==1 && !dialogue_calls);
    /* Ownership or offline native continuation after the skip also starts BGM. */
    fixture();host=state();host.controller[1]=2;assert(apply(&host));
    role(1,1,1);root_callback(root,root_object);
    assert(hud_live() && music_calls==1 && gameplay_music_calls==1);
    root_callback(root,root_object);assert(music_calls==1 && !dialogue_calls);
    /* Result hints arrive before HUD bootstrap too; do not start gameplay BGM. */
    fixture();host=state();host.controller[1]=5;host.controller[7]=2;host.result=2;
    assert(!apply(&host));root_callback(root,root_object);
    assert(SB(root,0xd0)==2 && !music_calls);
    root_callback(root,root_object);assert(hud_live() && !music_calls);
    assert(apply(&host));root_callback(root,root_object);
    assert(SB(root,0xd0)==5 && !music_calls && !dialogue_calls);
    /* A stale room/visit must not consume the next game's one-shot transition. */
    fixture();host=state();host.controller[1]=3;host.controller[6]=1800;host.running=1;
    assert(!apply(&host));root_callback(root,root_object);
    D_800C7AB2=0x40;root_callback(root,root_object);assert(!music_calls);
    D_800C7AB2=SJ_ROOM;++visit;root_callback(root,root_object);assert(!music_calls);
    fixture();start();assert(music_calls==1 && gameplay_music_calls==1);
}
static void test_atomic_graph(void)
{
    fixture();start();AnchorMinigameState host=state(),bad;
    host.controller[6]=1600;host.actors[0][7]=123456;host.actors[16][6]=271;
    SB(root,0xd1)=2;SH(root,0xd6)=17;
    for(int i=0;i<SJ_COUNT;++i) {
        bad=host;bad.actors[i][1]=(i+1)%SJ_COUNT;
        assert(!validate(&bad) && !apply(&bad));assert(SI(hud,0x80)==3960);
    }
    bad=host;bad.actors[16][8]=2147483647;
    assert(!apply(&bad) && SF(objects[0],8)==0.f);
    bad=host;bad.actors[4][6]=3;assert(!validate(&bad));
    bad=host;bad.result=bad.controller[7]=1;assert(!validate(&bad));
    ++SB(tasks[16],0x74);assert(!apply(&host) && SI(hud,0x80)==3960);
    --SB(tasks[16],0x74);missing_resource=1;assert(!apply(&host));missing_resource=0;
    SS(tasks[0],0xd4)=0;assert(!apply(&host));SS(tasks[0],0xd4)=360;
    SS(tasks[9],0xd4)=360;assert(!apply(&host));SS(tasks[9],0xd4)=240;
    assert(apply(&host) && SI(hud,0x80)==1600);
    assert(SB(root,0xd1)==2 && SH(root,0xd6)==17);
    assert(SF(objects[0],8)==123456.f/1024.f && SS(tasks[16],0xd6)==271);
    D_800C7AB2=0x40;assert(!apply(&host));D_800C7AB2=SJ_ROOM;
    ++visit;assert(!capture(&bad));--visit;
}
static void test_prediction_and_goal(void)
{
    fixture();start();AnchorMinigameState host=state();assert(apply(&host));
    for(int i=0;i<12;++i) {
        platform_callback(tasks[0],objects[0]);hud_callback(hud,0);
    }
    assert(SS(tasks[0],0xd6)==6 && SI(hud,0x80)==3960-6);
    assert(SW(tasks[0],0x64)&0x4000u && hud_draws==12);
    assert(apply(&host) && SS(tasks[0],0xd6)==0 && SI(hud,0x80)==3960);
    SF(player,0x0c)=700.f;SF(player,8)=60.f;
    root_callback(root,root_object);
    assert(camera_calls==1 && s_goal_sent && SB(root,0xd0)==3 && !hud_deletes && hud_live());
    host=state();assert(host.input_count==1 && host.inputs[0][1]==SJ_GOAL_INPUT);
    int words[8]={9,10,2,1,SJ_GOAL_INPUT,700*1024,60*1024,0};
    assert(!input(words,8));role(1,1,1);
    assert(input(words,7) && !s_terminal); /* Old prefix cannot award a win. */
    words[5]=600*1024;assert(input(words,8) && !s_terminal);
    words[5]=700*1024;assert(input(words,8) && s_terminal);
    root_callback(root,root_object);assert(SB(root,0xd0)==6 && hud_deletes==1);
    assert(!return_pending());
    for(int i=0;i<59;++i) root_callback(root,root_object);
    assert(!return_pending());
    for(int i=0;i<9;++i) root_callback(root,root_object);
    assert(return_pending());D_8016DAB4_16E6B4=tasks[0];assert(!return_pending());
    D_8016DAB4_16E6B4=root;root_callback(root,root_object);assert(!return_pending());
    assert(native_returns==1);
}
static void test_result_and_lifecycle(void)
{
    fixture();start();AnchorMinigameState host=state();
    host.controller[1]=5;host.controller[4]=49;host.controller[7]=2;host.result=2;host.running=0;
    assert(apply(&host));assert(SB(root,0xd0)==5 && SS(root,0xd4)==0);
    root_callback(root,root_object);assert(hud_deletes==0 && SS(root,0xd4)==1);
    assert(apply(&host));assert(SS(root,0xd4)==1); /* Do not replay end sequence. */
    for(int i=0;i<59;++i) root_callback(root,root_object);
    assert(!return_pending());
    for(int i=0;i<9;++i) root_callback(root,root_object);
    assert(return_pending());D_800C7AB2=0x40;assert(!return_pending());D_800C7AB2=SJ_ROOM;
    anchor_minigame_sasuke_delete(root);assert(!return_pending());
    fixture();start();anchor_minigame_sasuke_reuse(hud);assert(!hud_live());
    fixture();start();anchor_minigame_sasuke_reuse(tasks[2]);assert(!graph());
    fixture();reset();assert(callback_is(SP(root,0x0c),func_08000AD4_7068C4));
    assert(callback_is(SP(tasks[0],0x0c),func_080006F8_7064E8));
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--export")) {
        char encoded[ANCHOR_MINIGAME_JSON_SIZE];fixture();start();
        AnchorMinigameState native=state();
        assert(anchor_minigame_state_encode(&native,encoded,sizeof(encoded)));
        puts(encoded);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--validate")) {
        char json[ANCHOR_MINIGAME_JSON_SIZE];AnchorMinigameStatus incoming;
        FILE *input_file=fopen(argv[2],"rb");assert(input_file);
        size_t n=fread(json,1,sizeof(json)-1,input_file);assert(!ferror(input_file));
        assert(feof(input_file));fclose(input_file);json[n]=0;
        assert(anchor_minigame_status_decode(json,&incoming));
        assert(incoming.has_state && incoming.state.running==1 && incoming.state.controller[1]==3);
        fixture();assert(validate(&incoming.state));assert(!apply(&incoming.state));
        root_callback(root,root_object);assert(SB(root,0xd0)==2 && !dialogue_calls);
        root_callback(root,root_object);assert(hud_live() && !dialogue_calls);
        assert(apply(&incoming.state));AnchorMinigameState applied=state();
        assert(applied.running && applied.actor_count==SJ_COUNT);
        assert(applied.controller[6]==incoming.state.controller[6]);
        puts("Sasuke Python status -> production codec -> native bootstrap/apply passed");return 0;
    }
    assert(argc==1);
    test_bootstrap();test_fresh_owner_entry();test_skipped_intro_music();test_atomic_graph();
    test_prediction_and_goal();test_result_and_lifecycle();
    puts("Sasuke native adapter: 17-platform graph, native bootstrap, timer prediction, participant goal, result and lifecycle passed");
    return 0;
}
