#include "impact_test_pointers.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define ANCHOR_MINIGAME_GOEMON_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define GP(p,o) TP(p,o)
#define GOEMON_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#ifndef GOEMON_ADAPTER_SOURCE
#define GOEMON_ADAPTER_SOURCE "../src/world/anchor_minigame_goemon.c"
#endif
#include GOEMON_ADAPTER_SOURCE
#include "../src/world/anchor_minigame_codec.c"

unsigned short D_800C7AB2=GT_ROOM;
void *D_8016DAB4_16E6B4;
static unsigned int root[64],object[64],work[16],targets[6][64],objects[6][64],children[6][64];
static unsigned int child_objects[6][64];
static unsigned int timer[64],handles[3],cube[64],cube_object[64],clones[18][64],clone_objects[18][64];
static int vm,missing_resource,root_calls,target_calls,draws,intros,builds,carried,throw_button,throws;
static int clone_count,clone_fail,model_binds,returns;
static int music_calls,gameplay_music_calls,music_cue;
static int character_ready=1;
static unsigned int visit=1,fixture_ack;
static int rng_calls,hit_sounds,pop_signals,pop_effects,pop_particles,pop_debris,root_epilogues,terminal_cleanups;
static int target_return_mutation;
static float fixture_launch_velocity[3]={1.25f,4.1f,-2.2f};
unsigned int anchor_minigame_sync_local_ack(int game) { assert(game==1);return fixture_ack; }
float func_80003E10_4A10(unsigned int angle) { return sinf((float)(angle&1023u)*0.00613592315f); }
static const AnchorMinigameAdapter *bound;
unsigned int anchor_minigame_sync_visit(void) { return visit; }
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *a)
{ bound=a;role(1,0,0);return 1; }
unsigned int anchor_get_client_id(void) { return 7; }
int anchor_get_projectile_session(void) { return 9; }
int anchor_minigame_invites_character_motion_ready(unsigned short room)
{ assert(room==GT_ROOM);return character_ready; }
void *func_800141C4_14DC4(unsigned int file)
{ assert(file==48);return missing_resource ? (void *)(unsigned long)0xffffffffu:(void *)1; }
int func_8003F1D8_3FDD8(void) { return vm; }
void func_80038BC8_397C8(unsigned short cue)
{
    assert(cue==0x0292 || cue==0x0052);
    ++music_calls;music_cue=cue;
    if(cue==0x0052) ++gameplay_music_calls;
}
static void finish_dialogue(void)
{
    assert(vm && intros);
    /* Scenario 0x0173 queues gameplay audio before its END command. */
    func_80038BC8_397C8(0x0052);vm=0;
}
void func_80216E1C_5D22EC(void *task,unsigned int variant)
{
    assert(variant==2 && task==clones[model_binds]);++model_binds;
    assert(GW(task,0x48)==0 && GB(task,0x4c)==0 && GW(task,0x60)==0x20);
    GF(GP(task,0x18),0x1c)=GF(GP(task,0x18),0x20)=GF(GP(task,0x18),0x24)=.1f;
}
void *func_08000000_707A40(void *parent,GoemonCallback callback,
                          unsigned char kind,unsigned short file)
{
    assert(parent==root && callback==visual_callback && kind==0 && file==48);
    if(clone_fail) return 0;
    assert(clone_count<18);void *task=clones[clone_count],*o=clone_objects[clone_count++];
    GP(task,0x18)=o;GP(task,0x0c)=(void *)callback;GB(task,0x74)=1;
    GH(task,0x28)=48;GW(task,0x48)=0xffffffffu;GB(task,0x4c)=1;GW(task,0x60)=0x2c007e0u;
    return task;
}
void func_080005D0_708010(void *task,void *context)
{ (void)context;++draws;if(GI(task,0x84)>0) --GI(task,0x84); }
void func_0800126C_708CAC(void *task)
{ anchor_minigame_goemon_child_terminal(task);++GB(task,0xd0);++pop_signals; }
void func_08000CC8_708708(void *task,void *context)
{ (void)task;(void)context; }
static void fixture_balloon_slot(void *task,void *o)
{
    if(GB(task,0xd0)==0) {
        GW(task,0x60)=0x20;GF(o,0xc)=150.f;
        GF(o,0x1c)=GF(o,0x20)=GF(o,0x24)=0.5f;GB(task,0xd0)=1;
    } else if(GB(task,0xd0)==2) {
        ++pop_effects;pop_particles+=5;pop_debris+=3;
        anchor_minigame_goemon_delete(task);GW(task,0x68)|=2u;
    }
}
void func_0800127C_708CBC(void *task,void *context)
{
    void *o=context;
    float speed=GF(task,0xe4),height=GF(task,0xdc),desired=GF(task,0xe0);
    ++target_calls;
    if((double)speed>0.001) {
        float distance=fabsf(desired-height);
        if(distance<speed) { GF(task,0xdc)=desired;GF(task,0xe4)=0.f; }
        else GF(task,0xdc)+=desired<height?-speed:speed;
    }
    if(GB(task,0xd1)==3) GB(task,0xd0)=7;
    if(GB(task,0xd1)==4) { func_0800126C_708CAC(GP(task,0xe8));GB(task,0xd0)=7; }
    switch(GB(task,0xd0)) {
    case 0: {
        int i;
        for(i=0;i<6;++i) if(task==targets[i]) break;
        assert(i<6);GP(task,0xe8)=children[i];GB(children[i],0xd0)=0;
        GP(children[i],0x18)=child_objects[i];GH(children[i],0x28)=48;GB(children[i],0x74)=4;
        GF(child_objects[i],8)=GF(objects[i],8);GF(child_objects[i],0x10)=GF(objects[i],0x10);
        GB(task,0xd0)=1;GB(task,0xd1)=0;GF(task,0xdc)=-130.f;
        GF(o,0x1c)=GF(o,0x20)=GF(o,0x24)=1.f;
        break;
    }
    case 1:
        if(GW(task,0x68)&0x80u) {
            GF(task,0xdc)-=8.f;if(GF(task,0xdc)<-130.f) GF(task,0xdc)=-130.f;
            GS(task,0xd6)=0;GB(task,0xd0)=3;++hit_sounds;
        }
        GF(task,0xdc)=(float)((double)GF(task,0xdc)+(GS(task,0xd8)<GS(task,0xd6)?0.4:0.05));
        if(GS(task,0xd8)<GS(task,0xd6) && GS(task,0xd8)+GS(task,0xda)<GS(task,0xd6)) {
            rng_calls+=2;GS(task,0xd8)=(short)(GS(task,0xd6)+17);GS(task,0xda)=27;
        }
        break;
    case 3:case 4: {
        int phase_before=GB(task,0xd0),frame;
        GB(task,0xd1)=0;
        if(GW(task,0x68)&0x80u) {
            GF(task,0xdc)=phase_before==3?GF(task,0xdc)-16.f:-130.f;
            GS(task,0xd6)=0;GB(task,0xd0)=4;++hit_sounds;
        }
        frame=GS(task,0xd6);
        if(frame<60) {
            float q=(float)(60-frame)/60.f;
            float value=(float)(1.0+(double)func_80003E10_4A10((unsigned short)((frame*0x3c00)/60))*0.1*(double)q*(double)q);
            GF(o,0x1c)=GF(o,0x20)=GF(o,0x24)=value;
        }
        if(frame>60) GB(task,0xd0)=1;
        break;
    }
    default:break;
    }
    if(GF(task,0xdc)>-10.f) GB(task,0xd1)=1;
    GF(o,0xc)=GF(task,0xdc);++GS(task,0xd6);
    /* The target leaves contact80 for the engine's damage finalizer. With
     * no native attacker at +38, that finalizer cannot retire this bit. */
    if(task==targets[2]) {
        GW(task,0x68)|=0x1000u;
        if(target_return_mutation==1) ++GB(task,0x74);
        if(target_return_mutation==2) GP(task,0x18)=objects[1];
        if(target_return_mutation==3) GP(task,0xc)=(void *)func_0800127C_708CBC;
        if(target_return_mutation==4) ++visit;
        if(target_return_mutation==5) anchor_minigame_goemon_reuse(task);
        if(target_return_mutation==6) anchor_minigame_goemon_delete(task);
    }
}
static void setup_graph(void)
{
    ++builds;
    GP(work,0x14)=timer;GP(timer,0x0c)=(void *)func_080005D0_708010;
    for(int i=0;i<3;++i) GP(timer,0x78+i*4)=&handles[i];
    GI(timer,0x84)=3000;anchor_minigame_goemon_timer(timer,0);
    for(int i=0;i<6;++i) {
        GP(work,0x18+i*4)=targets[i];GP(targets[i],0x18)=objects[i];
        GP(targets[i],0x38)=0;
        GP(targets[i],0x0c)=(void *)func_0800127C_708CBC;
        GH(targets[i],0x28)=48;GB(targets[i],0x74)=2;
        GF(objects[i],8)=(float)i*10;GF(objects[i],0x10)=(float)i*-10;
        anchor_minigame_goemon_target(targets[i],objects[i]);
        func_0800127C_708CBC(targets[i],objects[i]);anchor_minigame_goemon_target_return();
        anchor_minigame_goemon_child(children[i],child_objects[i]);
        fixture_balloon_slot(children[i],child_objects[i]);anchor_minigame_goemon_child_return();
    }
    GB(work,0)=1;
}
void func_080018E8_709328(void *task,void *context)
{
    int active=GB(work,0)==1 || GB(work,0)==2;
    assert(task==root && context==object);++root_calls;
    if(GP(work,0x30) && !GB(GP(work,0x30),0xd1)) {
        GP(work,0x30)=0;carried=0;
    }
    switch(GB(work,0)) {
    case 0:GB(work,0)=3;break;
    case 3:++intros;vm=1;func_80038BC8_397C8(0x0292);GB(work,0)=4;break;
    case 4:if(!vm) setup_graph();break;
    case 1:
        if(!carried) {
            GP(work,0x30)=cube;GP(cube,0x18)=cube_object;
            GP(cube,0x0c)=(void *)func_08000CC8_708708;GH(cube,0x28)=48;GB(cube,0x74)=3;
            GB(cube,0xd0)=GB(cube,0xd1)=1;GW(cube,0x68)|=0x8000u;
            GF(cube_object,8)=15.f;GF(cube_object,0x0c)=20.f;carried=1;
        }
        if(throw_button && carried) GB(work,0)=2;
        break;
    case 2:
        ++throws;carried=0;throw_button=0;GW(cube,0x68)&=~0x8000u;GB(work,0)=1;
        for(int i=0;i<3;++i) { GF(cube,0x78+4*i)=fixture_launch_velocity[i];GF(cube_object,8+4*i)=20.f+10.f*(float)i; }
        break;
    case 5:case 6:
        if(GS(work,4)==1) { ++terminal_cleanups;GP(work,0x30)=0;carried=0; }
        if(GS(work,4)>=(GB(work,0)==5?89:119)) { GB(work,0)=7;GS(work,4)=0; }
        break;
    case 7:if(GS(work,4)==20) { assert(return_pending());++returns; }break;
    }
    if(active) {
        if(GI(timer,0x84)/30==0) { GB(work,0)=5;GS(work,4)=0; }
        for(int i=0;i<6;++i) if(GB(targets[i],0xd1)==1) {
            for(int j=0;j<6;++j) GB(targets[j],0xd1)=3;
            GB(targets[i],0xd1)=4;GB(work,0)=6;GS(work,4)=0;
        }
        GH(work,2)=(unsigned short)((GH(work,2)+1)&1023u);GW(work,8)=(unsigned int)throw_button;
    }
    ++GS(work,4);++root_epilogues;
}
static void fixture(void)
{
    reset();test_ptr_count=0;
    memset(root,0,sizeof(root));memset(object,0,sizeof(object));memset(work,0,sizeof(work));
    memset(targets,0,sizeof(targets));memset(objects,0,sizeof(objects));memset(children,0,sizeof(children));
    memset(child_objects,0,sizeof(child_objects));
    memset(timer,0,sizeof(timer));memset(cube,0,sizeof(cube));memset(cube_object,0,sizeof(cube_object));
    memset(clones,0,sizeof(clones));memset(clone_objects,0,sizeof(clone_objects));
    vm=missing_resource=root_calls=target_calls=draws=intros=builds=carried=throw_button=throws=0;
    clone_count=clone_fail=model_binds=returns=0;D_800C7AB2=GT_ROOM;bound=0;
    music_calls=gameplay_music_calls=music_cue=0;
    character_ready=1;fixture_ack=0;
    rng_calls=hit_sounds=pop_signals=pop_effects=pop_particles=pop_debris=root_epilogues=terminal_cleanups=0;
    target_return_mutation=0;
    GP(root,0x18)=object;GH(root,0x5c)=GT_ENTITY;GB(root,0x74)=3;
    D_8016DAB4_16E6B4=root;
    anchor_minigame_goemon_init(root,object);
    GP(root,0xd0)=work;GP(root,0x0c)=(void *)func_080018E8_709328;
    anchor_minigame_goemon_init_return();assert(bound==&s_adapter);
    root_callback(root,object);assert(phase()==3);
}
static AnchorMinigameState state(void)
{
    AnchorMinigameState s;memset(&s,0,sizeof(s));assert(capture(&s));return s;
}
static void start(void)
{ role(1,1,1);root_callback(root,object);assert(vm && intros==1);vm=0;root_callback(root,object);assert(graph()); }
static void test_skipped_intro_music(void)
{
    AnchorMinigameState host,intro,terminal;
    fixture();start();host=state();
    fixture();role(1,1,1);root_callback(root,object);intro=state();
    assert(intro.controller[1]==4 && vm);
    /* A direct phase4 adoption bypasses the local rules but waits for the
     * authority's gameplay phase even after native graph construction. */
    fixture();assert(apply(&intro) && phase()==4 && !music_calls);
    root_callback(root,object);assert(graph() && builds==1 && !music_calls);
    assert(apply(&intro));root_callback(root,object);assert(!music_calls);
    assert(apply(&host));root_callback(root,object);
    assert(music_calls==1 && gameplay_music_calls==1 && music_cue==0x0052 && !intros);
    role(1,0,1);
    for(int i=0;i<8;++i) { assert(apply(&host));root_callback(root,object); }
    throw_button=1;root_callback(root,object);assert(phase()==2 && music_calls==1);
    root_callback(root,object);assert(phase()==1 && music_calls==1);
    /* Running-state joins use the scheduler's separate phase3 bypass. A
     * complete local graph may be built before coordinator role readiness. */
    fixture();assert(!apply(&host));root_callback(root,object);
    assert(phase()==4 && !music_calls);
    root_callback(root,object);
    assert(graph() && music_calls==1 && gameplay_music_calls==1 && !intros);
    assert(apply(&host));root_callback(root,object);assert(music_calls==1);
    /* Native VM, physical targets and the timer must all be ready. */
    fixture();assert(apply(&intro));root_callback(root,object);assert(graph());
    assert(apply(&host));vm=1;root_callback(root,object);assert(!music_calls);
    vm=0;missing_resource=1;root_callback(root,object);assert(!music_calls);
    missing_resource=0;void *saved_timer=GP(work,0x14);GP(work,0x14)=0;
    root_callback(root,object);assert(!music_calls);GP(work,0x14)=saved_timer;
    void *saved_target=GP(work,0x18);GP(work,0x18)=0;
    root_callback(root,object);assert(!music_calls);GP(work,0x18)=saved_target;
    root_callback(root,object);assert(music_calls==1 && gameplay_music_calls==1);
    /* A participant already throwing still enters the same gameplay track. */
    fixture();assert(apply(&intro));root_callback(root,object);assert(apply(&host));
    GB(work,0)=2;root_callback(root,object);assert(music_calls==1 && phase()==2);
    /* A skipped guest that becomes owner (or continues offline) still needs
     * the skipped scenario's audio, once, after native setup. */
    fixture();assert(apply(&intro));role(1,1,1);root_callback(root,object);
    assert(graph() && music_calls==1 && gameplay_music_calls==1 && !intros);
    root_callback(root,object);assert(music_calls==1);
    fixture();assert(!apply(&host));root_callback(root,object);role(0,0,0);
    root_callback(root,object);assert(graph() && music_calls==1 && !intros);
    root_callback(root,object);assert(music_calls==1);
    /* Fresh owners/offline players retain the native scenario audio. */
    for(int offline=0;offline<2;++offline) {
        fixture();role(!offline,!offline,!offline);root_callback(root,object);
        assert(vm && intros==1 && music_calls==1 && music_cue==0x0292);
        finish_dialogue();root_callback(root,object);root_callback(root,object);
        assert(graph() && music_calls==2 && gameplay_music_calls==1 && music_cue==0x0052);
    }
    /* Result hints may precede local setup; never insert gameplay BGM then. */
    terminal=host;terminal.controller[1]=5;terminal.controller[4]=terminal.result=2;terminal.running=0;
    fixture();assert(!apply(&terminal));root_callback(root,object);root_callback(root,object);
    assert(graph() && !music_calls);assert(apply(&terminal));root_callback(root,object);
    assert(phase()==5 && !music_calls);
    fixture();assert(apply(&intro));root_callback(root,object);assert(apply(&host));
    result(2);root_callback(root,object);assert(!music_calls);
    /* Stale room/visit state must neither queue music nor consume the latch. */
    fixture();assert(!apply(&host));root_callback(root,object);
    D_800C7AB2=0x40;root_callback(root,object);assert(!music_calls);
    D_800C7AB2=GT_ROOM;++visit;root_callback(root,object);assert(!music_calls);
    --visit;root_callback(root,object);assert(music_calls==1);
    fixture();assert(!apply(&host));root_callback(root,object);root_callback(root,object);
    assert(music_calls==1 && gameplay_music_calls==1);
    puts("Goemon skipped intro music: phase4 and running joins, native owners, VM/graph gates, takeover and lifecycle passed");
}
static void test_bootstrap_and_atomic_graph(void)
{
    fixture();start();AnchorMinigameState host=state();host.controller[3]=1745;
    host.actors[2][11]=777*1024;
    host.actors[2][20]=778*1024;
    fixture();assert(!apply(&host));root_callback(root,object);
    assert(phase()==4 && intros==0 && builds==0);
    root_callback(root,object);assert(graph() && builds==1 && intros==0);
    assert(apply(&host));assert(GI(timer,0x84)==1745 && GF(objects[2],8)==777.f);
    assert(GF(child_objects[2],8)==778.f);
    host.actors[4][4]=100;
    assert(!apply(&host));assert(GF(objects[2],8)==777.f && GI(timer,0x84)==1745);
    host.actors[4][4]=1;void *child=GP(targets[4],0xe8);GP(targets[4],0xe8)=0;
    host.actors[2][11]=666*1024;assert(!apply(&host));assert(GF(objects[2],8)==777.f);
    GP(targets[4],0xe8)=child;missing_resource=1;assert(!apply(&host));missing_resource=0;
    assert(apply(&host));assert(GF(objects[2],8)==666.f);
    /* A retired visual child is not a missing physical target or a readable
     * stale pointer. Its canonical end phase survives task-slot reuse. */
    GB(targets[0],0xd1)=1;anchor_minigame_goemon_delete(children[0]);
    assert(graph());AnchorMinigameState after=state();
    assert(after.actors[0][18]==2 && after.actors[0][19]==0);
    GH(children[0],0x28)=0;GP(children[0],0x18)=0;
    after=state();assert(after.actors[0][19]==0);
    fixture();start();host=state();host.actors[0][18]=2;host.actors[0][19]=0;
    host.actors[0][5]=1;host.actors[1][11]=555*1024;
    s_child_phase[0]=0;s_children[0].initialized=0;GB(children[0],0xd0)=0;
    assert(!apply(&host) && GF(objects[1],8)!=555.f && GB(children[0],0xd0)==0);
    s_child_phase[0]=1;s_children[0].initialized=1;GB(children[0],0xd0)=1;
    assert(apply(&host) && GB(children[0],0xd0)==2);
    assert(apply(&host) && GB(children[0],0xd0)==2); /* Increment exactly once. */
    host.actors[1][5]=-1;assert(!validate(&host));
}
static void test_character_bootstrap(void)
{
    fixture();GB(work,0)=0;character_ready=0;int calls=root_calls;
    root_callback(root,object);assert(phase()==0 && root_calls==calls);
    character_ready=1;root_callback(root,object);
    assert(phase()==3 && root_calls==calls+1);
}
static void test_local_control_and_shared_hits(void)
{
    fixture();start();AnchorMinigameState host=state();assert(apply(&host));role(1,0,1);
    GI(timer,0x84)=0;for(int i=0;i<6;++i) GB(targets[i],0xd1)=1;
    root_callback(root,object);assert(phase()==1 && carried && !s_terminal);
    unsigned int cube_generation=s_cube_generation[0];
    ++GB(cube,0x74);cube_poses();assert(s_cube_generation[0]==cube_generation+1);
    assert(GI(timer,0x84)==0 && GB(targets[0],0xd1)==1);
    throw_button=1;root_callback(root,object);assert(phase()==2);
    root_callback(root,object);assert(throws==1 && phase()==1 && !carried);
    int before=target_calls;GB(targets[2],0xd0)=1;GW(targets[2],0x68)|=0x80u;
    target_callback(targets[2],objects[2]);assert(target_calls==before && s_hits[2][0]);
    assert(!(GW(targets[2],0x68)&0x80u) && GB(targets[2],0xd0)==1);
    int hit[8]={8,10,1,s_hits[2][0],GT_INPUT_HIT,2,1,1};
    assert(!input(hit,8));role(1,1,1);assert(input(hit,8));
    target_callback(targets[2],objects[2]);assert(GB(targets[2],0xd0)==3);
    assert(input(hit,8));assert(!(GW(targets[2],0x68)&0x80u));
    GB(targets[2],0xd0)=3;assert(input(hit,8));assert(!(GW(targets[2],0x68)&0x80u));
    GB(targets[2],0xd0)=4;hit[7]=4;
#ifdef ANCHOR_GOEMON_ACTION_V2
    hit[6]=(int)s_action[2];
#endif
    assert(input(hit,8));
    assert(!(GW(targets[2],0x68)&0x80u));
    GB(targets[2],0xd0)=5;hit[7]=5;assert(input(hit,8));
    assert(!(GW(targets[2],0x68)&0x80u));
    role(1,0,1);s_hits[2][0]=0;GB(targets[2],0xd0)=4;GW(targets[2],0x68)|=0x80u;
    target_callback(targets[2],objects[2]);assert(s_hits[2][4]==4);
    role(1,1,1);
    hit[5]=9;assert(input(hit,8));hit[5]=2;hit[6]=99;assert(input(hit,8));
    int tick=GI(timer,0x84);role(1,0,1);timer_callback(timer,0);
    assert(draws==1 && GI(timer,0x84)==tick);
}
static void test_visual_safety_and_optional_capacity(void)
{
    fixture();start();role(1,1,1);
    int pose[11]={8,10,1,1,(1<<6)|GT_INPUT_POSE|16|32,100*1024,20*1024,0,0,512,0};
    assert(input(pose,11));assert(s_visuals[0].row[3]==8);
    visuals_update();assert(clone_count==1 && GW(clones[0],0x48)==0 && GB(clones[0],0x4c)==0);
    visual_callback(clones[0],clone_objects[0]);assert(model_binds==1);
    assert(GF(clone_objects[0],8)==100.f && GH(clone_objects[0],0x16)==512);
    assert(GW(clones[0],0x60)==0x20 && !(GW(clones[0],0x68)&0x8000u));
    for(int cid=9;cid<14;++cid) { pose[0]=cid;pose[3]=cid;assert(input(pose,11)); }
    pose[0]=14;pose[3]=14;assert(input(pose,11));assert(visual_slot(14,10,1,0,0)<0);
    clone_fail=1;AnchorMinigameState snapshot=state();assert(validate(&snapshot));
    assert(apply(&snapshot) && graph()); /* Visual allocation failure is optional. */
    __builtin_memcpy(snapshot.actors[12],snapshot.actors[6],sizeof(snapshot.actors[12]));
    snapshot.actor_words[12]=GT_VISUAL_WORDS;snapshot.actor_count=13;
    snapshot.actors[12][1]=6;snapshot.actors[12][3]=14;
    assert(!validate(&snapshot)); /* A seventh distinct visual peer is invalid. */
    pose[0]=8;pose[2]=2;pose[3]=1;assert(input(pose,11));
    assert(s_visuals[0].row[15]==2 && s_visuals[0].row[14]==1);
    assert(GW(clones[0],0x68)&2u); /* Same session, fresh visit retires old clone. */
    for(int i=0;i<GT_VISUAL_TTL+1;++i) visuals_update();
    assert(GW(clones[0],0x68)&2u);
    assert(s_visuals[0].row[3]==0);
}
static void test_result_and_lifecycle(void)
{
    fixture();start();result(2);assert(phase()==5);
    for(int i=0;i<90;++i) root_callback(root,object);
    assert(phase()==7 && !return_pending());
    for(int i=0;i<19;++i) root_callback(root,object);
    assert(return_pending());D_8016DAB4_16E6B4=targets[0];assert(!return_pending());
    D_8016DAB4_16E6B4=root;root_callback(root,object);assert(returns==1 && !return_pending());
    anchor_minigame_goemon_reuse(targets[1]);assert(!target_live(1));
    anchor_minigame_goemon_delete(root);assert(!return_pending() && !graph());
    fixture();start();reset();assert(callback_is(GP(root,0x0c),func_080018E8_709328));
    assert(callback_is(GP(targets[0],0x0c),func_0800127C_708CBC));
}
static void target_row(AnchorMinigameState *s,int slot,int p,int frame,float height)
{
    int *r=s->actors[slot];r[4]=p;r[5]=0;r[6]=frame;r[7]=2;r[8]=1000;
    r[9]=r[12]=(int)(height*1024.f);r[10]=0;r[29]=(int)(height*1024.f);r[30]=1;
    r[15]=r[16]=r[17]=1024;
}
static void test_action_target_motion(void)
{
    AnchorMinigameState host;
    unsigned int ref_task[64]={0},ref_object[64]={0};
    float y,scale;
    int before,rng_before;
    fixture();start();host=state();target_row(&host,2,1,0,-100.f);
    host.adopted_generation=1;host.full_tick=1;host.source_tick=1;
    assert(apply(&host));role(1,0,1);
    GP(ref_task,0x18)=ref_object;GB(ref_task,0xd0)=1;
    GF(ref_task,0xdc)=-100.f;GS(ref_task,0xd8)=2;GS(ref_task,0xda)=1000;
    for(int j=0;j<3;++j) GF(ref_object,0x1c+4*j)=1.f;
    before=target_calls;rng_before=rng_calls;
    for(int i=0;i<12;++i) {
        func_0800127C_708CBC(ref_task,ref_object);
        target_callback(targets[2],objects[2]);
        fprintf(stderr,"Goemon action motion frame%d nativeY%.6f followerY%.6f\n",i,GF(ref_object,0xc),GF(objects[2],0xc));
        assert(fabsf(GF(objects[2],0xc)-GF(ref_object,0xc))<0.0001f);
        assert(GB(targets[2],0xd1)==0);
    }
    assert(target_calls==before+12 && rng_calls==rng_before && !hit_sounds);
    y=GF(objects[2],0xc);host.source_tick=2;host.local_ack=50;assert(apply(&host));
    assert(GF(objects[2],0xc)==y); /* Omitted ACTION row + ACK cannot rewind/re-anchor. */
    for(int i=12;i<60;++i) target_callback(targets[2],objects[2]);
    y=GF(objects[2],0xc);assert(apply(&host));target_callback(targets[2],objects[2]);assert(GF(objects[2],0xc)==y);
    ++host.full_tick;assert(apply(&host));target_callback(targets[2],objects[2]);assert(GF(objects[2],0xc)>y);
    /* Same-action full checkpoint converges rather than jumping. */
    host.actors[2][6]=GS(targets[2],0xd6);host.actors[2][9]=host.actors[2][12]=(int)((GF(objects[2],0xc)+8.f)*1024.f);
    ++host.full_tick;y=GF(objects[2],0xc);assert(apply(&host));assert(GF(objects[2],0xc)==y);
    for(int i=0;i<8;++i) target_callback(targets[2],objects[2]);
    assert(fabsf(GF(objects[2],0xc)-(y+8.f+8.f*0.4f))<0.001f);
    host.adopted_generation=2;host.actors[2][9]=host.actors[2][12]=-70*1024;assert(apply(&host));
    assert(GF(objects[2],0xc)==-70.f);
    /* Strict interpolation and native wobble/recovery boundaries. */
    for(int f=59;f<=61;++f) {
        target_row(&host,2,3,f,-50.f);++host.adopted_generation;assert(apply(&host));
        scale=GF(objects[2],0x1c);target_callback(targets[2],objects[2]);
        if(f==59) assert(GF(objects[2],0x1c)!=scale);
        else assert(GF(objects[2],0x1c)==scale);
        assert(GB(targets[2],0xd0)==(f>60?1:3) && GS(targets[2],0xd6)==f+1);
    }
    target_row(&host,2,3,0,-50.f);++host.adopted_generation;assert(apply(&host));
    GF(objects[2],0x1c)=GF(objects[2],0x20)=GF(objects[2],0x24)=1.1f;
    host.actors[2][6]=4;++host.full_tick;assert(apply(&host));
    assert(GF(objects[2],0x1c)==1.1f);
    for(int j=0;j<8;++j) {
        int f=4+j;float q=(float)(60-f)/60.f;
        float base=(float)(1.0+(double)func_80003E10_4A10((unsigned short)((f*0x3c00)/60))*0.1*(double)q*(double)q);
        target_callback(targets[2],objects[2]);
        assert(fabsf(GF(objects[2],0x1c)-(base+0.1f*(float)(7-j)/8.f))<0.0001f);
    }
    target_row(&host,2,2,10,-40.f);host.actors[2][29]=-38*1024;host.actors[2][10]=2*1024;
    ++host.adopted_generation;assert(apply(&host));target_callback(targets[2],objects[2]);
    assert(GF(targets[2],0xdc)==-38.f && GF(targets[2],0xe4)==2.f && GB(targets[2],0xd0)==2);
    target_callback(targets[2],objects[2]);assert(GF(targets[2],0xe4)==0.f && !GB(targets[2],0xd1));
    target_row(&host,2,1,1,-9.f);++host.adopted_generation;assert(apply(&host));
    target_callback(targets[2],objects[2]);assert(GB(targets[2],0xd1)==0); /* No predicted completion. */
    puts("Goemon scalar rise, strict schedules, wobble, interpolation, freshness and correction passed");
}
static void test_six_result_controls_and_pop(void)
{
    AnchorMinigameState host;
    int calls;
    fixture();start();host=state();assert(apply(&host));role(1,0,1);
    for(int winner=0;winner<6;++winner) {
        for(int i=0;i<6;++i) GB(targets[i],0xd1)=(unsigned char)(i==winner);
        GI(timer,0x84)=0;calls=root_calls;root_callback(root,object);
        assert(root_calls==calls+1 && phase()==1 && !s_terminal && GI(timer,0x84)==0);
        for(int i=0;i<6;++i) assert(GB(targets[i],0xd1)==(i==winner));
        assert(!terminal_cleanups);
    }
    /* Owner native ANY predicate still admits one result and its next-slot cleanup. */
    role(1,1,1);GI(timer,0x84)=100;root_callback(root,object);
    assert(phase()==6 && GB(targets[5],0xd1)==4 && GB(targets[0],0xd1)==3);
    root_callback(root,object);assert(terminal_cleanups==1);
    fixture();start();host=state();host.controller[1]=6;host.controller[4]=host.result=1;host.running=0;
    host.actors[2][5]=4;host.actors[2][18]=2;
    assert(apply(&host));role(1,0,1);assert(pop_signals==1 && GB(children[2],0xd0)==2);
    assert(apply(&host));target_callback(targets[2],objects[2]);
    assert(pop_signals==1 && GB(children[2],0xd0)==2);
    fixture_balloon_slot(children[2],child_objects[2]);
    assert(pop_effects==1 && pop_particles==5 && pop_debris==3 && !child_live(2));
    assert(apply(&host) && pop_signals==1); /* Cached terminal receipt succeeds without repeating its cue. */
    anchor_minigame_goemon_delete(targets[3]);GP(targets[3],0x18)=0;
    assert(apply(&host)); /* Terminal graph may retain a missing target's last canonical row. */
    puts("Goemon six native ANY controls masked, owner cleanup and one scheduled balloon pop passed");
}
static void test_action_hit_epochs(void)
{
    AnchorMinigameState host;
    int hit[8]={8,9,1,1,GT_INPUT_HIT,2,1,4};
    fixture();start();GB(targets[2],0xd0)=4;GS(targets[2],0xd6)=19;GF(targets[2],0xdc)=-70.f;
    assert(input(hit,8));target_callback(targets[2],objects[2]);
    assert(GB(targets[2],0xd0)==4 && GS(targets[2],0xd6)==1 && hit_sounds==1);
    host=state();(void)host;
#ifdef ANCHOR_GOEMON_ACTION_V2
    assert(host.actors[2][30]==2);assert(input(hit,8));assert(!(GW(targets[2],0x68)&0x80u));
    hit[6]=2;assert(input(hit,8));target_callback(targets[2],objects[2]);assert(s_action[2]==3 && hit_sounds==2);
    host=state();assert(apply(&host));role(1,0,1);GW(targets[2],0x68)|=0x80u;
    target_callback(targets[2],objects[2]);assert(s_hits[2][0] && s_hits[2][3]==3);
    host.local_ack=(unsigned int)s_hits[2][0];assert(apply(&host));assert(!s_hits[2][0]);
    GW(targets[2],0x68)|=0x80u;target_callback(targets[2],objects[2]);assert(s_hits[2][0]);
    ++host.actors[2][30];assert(apply(&host));assert(!s_hits[2][0]);
#endif
    puts("Goemon same-phase hit action epochs and receipt retirement passed");
}
static void test_hit_recovery(void)
{
    AnchorMinigameState host;
    unsigned int ref_task[64],ref_object[64];
    for(int p=3;p<=4;++p) {
        fixture();start();host=state();target_row(&host,2,p,59,-50.f);
        host.adopted_generation=1;host.full_tick=1;
        assert(apply(&host));role(1,0,1);
        memset(ref_task,0,sizeof(ref_task));memset(ref_object,0,sizeof(ref_object));
        GB(ref_task,0xd0)=(unsigned char)p;GS(ref_task,0xd6)=59;
        GS(ref_task,0xd8)=2;GS(ref_task,0xda)=1000;GF(ref_task,0xdc)=-50.f;
        for(int j=0;j<3;++j) GF(ref_object,0x1c+4*j)=1.f;
        for(int f=59;f<=65;++f) {
            func_0800127C_708CBC(ref_task,ref_object);
            target_callback(targets[2],objects[2]);
            assert(GB(targets[2],0xd0)==GB(ref_task,0xd0));
            assert(GS(targets[2],0xd6)==GS(ref_task,0xd6));
            assert(fabsf(GF(objects[2],0xc)-GF(ref_object,0xc))<0.0001f);
            assert(!GB(targets[2],0xd1));
        }
        assert(GB(targets[2],0xd0)==1 && GF(objects[2],0xc)>-50.f);
        assert(!hit_sounds && !rng_calls && !pop_effects);
    }
    /* One ordinary full checkpoint renews the stale-motion horizon while
     * ACK-only cached states must not restart the post-hit recovery. */
    fixture();start();host=state();target_row(&host,2,3,1,-50.f);
    host.adopted_generation=1;host.full_tick=1;
    assert(apply(&host));role(1,0,1);
    memset(ref_task,0,sizeof(ref_task));memset(ref_object,0,sizeof(ref_object));
    GB(ref_task,0xd0)=3;GS(ref_task,0xd6)=1;GF(ref_task,0xdc)=-50.f;
    GS(ref_task,0xd8)=2;GS(ref_task,0xda)=1000;
    for(int f=0;f<67;++f) {
        if(f==30) { host.actors[2][6]=GS(ref_task,0xd6);++host.full_tick; }
        ++host.source_tick;++host.local_ack;assert(apply(&host));
        func_0800127C_708CBC(ref_task,ref_object);target_callback(targets[2],objects[2]);
        assert(GB(targets[2],0xd0)==GB(ref_task,0xd0));
        assert(GS(targets[2],0xd6)==GS(ref_task,0xd6));
        assert(fabsf(GF(objects[2],0xc)-GF(ref_object,0xc))<0.0001f);
    }
    assert(GB(targets[2],0xd0)==1 && GF(objects[2],0xc)>-50.f);
    assert(!rng_calls && !hit_sounds && !pop_effects && !GB(targets[2],0xd1));
    puts("Goemon follower hit phases recover at the native boundary and resume rising passed");
}
static void test_owner_hit_once(void)
{
    int hit[8]={8,9,1,1,GT_INPUT_HIT,2,1,1};
    for(int p=1;p<=4;++p) {
        if(p==2) continue;
        fixture();start();GB(targets[2],0xd0)=(unsigned char)p;
        GS(targets[2],0xd6)=19;GS(targets[2],0xd8)=1000;GS(targets[2],0xda)=1000;
        GF(targets[2],0xdc)=-70.f;GW(targets[2],0x68)=0x400u;hit[7]=p;
        assert(!GP(targets[2],0x38));assert(input(hit,8));assert(input(hit,8));
        assert(GW(targets[2],0x68)==0x400u);
        target_callback(targets[2],objects[2]);
        assert(hit_sounds==1 && s_action[2]==2 && !(GW(targets[2],0x68)&0x80u));
        assert((GW(targets[2],0x68)&0x1400u)==0x1400u);
        assert(input(hit,8)); /* Old action retransmission is harmless. */
        float knocked_down=GF(targets[2],0xdc);
        for(int f=0;f<70;++f) target_callback(targets[2],objects[2]);
        assert(hit_sounds==1 && s_action[2]==2 && GB(targets[2],0xd0)==1);
        assert(GF(targets[2],0xdc)>knocked_down && !GB(targets[2],0xd1));
    }
    /* Genuine contacts retain their bit and attacker for native finalization;
     * a simultaneous remote contact is coalesced into that one native hit. */
    fixture();start();hit[7]=1;assert(input(hit,8));
    GW(targets[2],0x68)|=0x80u;GP(targets[2],0x38)=cube;
    target_callback(targets[2],objects[2]);
    assert(hit_sounds==1 && s_action[2]==2 && (GW(targets[2],0x68)&0x80u));
    assert(GP(targets[2],0x38)==cube);
    GW(targets[2],0x68)&=~0x80u;GP(targets[2],0x38)=0;
    target_callback(targets[2],objects[2]);assert(hit_sounds==1 && s_action[2]==2);
    hit[6]=2;hit[7]=3;assert(input(hit,8));target_callback(targets[2],objects[2]);
    assert(hit_sounds==2 && s_action[2]==3 && GB(targets[2],0xd0)==4);
    hit[6]=3;hit[7]=4;assert(input(hit,8));target_callback(targets[2],objects[2]);
    assert(hit_sounds==3 && s_action[2]==4 && GF(targets[2],0xdc)==-130.f && GS(targets[2],0xd6)==1);
    target_callback(targets[2],objects[2]);assert(hit_sounds==3 && s_action[2]==4);
    puts("Goemon remote contact is consumed once, recovers, and preserves genuine native contacts passed");
}
static void test_pending_hit_lifetime(void)
{
    int hit[8]={8,9,1,1,GT_INPUT_HIT,2,1,1};
    for(int mode=0;mode<8;++mode) {
        fixture();start();assert(input(hit,8));
        assert(!(GW(targets[2],0x68)&0x80u));
        if(mode==0) role(1,0,1);
        if(mode==1) ++s_action[2];
        if(mode==2) GB(targets[2],0xd0)=3;
        if(mode==3) GB(work,0)=5;
        if(mode==4) anchor_minigame_goemon_reuse(targets[2]);
        if(mode==5) anchor_minigame_goemon_delete(targets[2]);
        if(mode==6) { GB(targets[2],0xd0)=0;anchor_minigame_goemon_target(targets[2],objects[2]); }
        if(mode==7) GB(targets[2],0xd1)=3;
        target_callback(targets[2],objects[2]);
        assert(!hit_sounds && !(GW(targets[2],0x68)&0x80u));
    }
    fixture();start();assert(input(hit,8));role(0,0,0);
    target_callback(targets[2],objects[2]);target_callback(targets[2],objects[2]);
    assert(hit_sounds==1 && s_action[2]==2 && !(GW(targets[2],0x68)&0x80u));
    /* Never clear a bit on a task that changed identity during native code. */
    for(int mutation=1;mutation<=6;++mutation) {
        fixture();start();assert(input(hit,8));target_return_mutation=mutation;
        target_callback(targets[2],objects[2]);
        assert(hit_sounds==1 && (GW(targets[2],0x68)&0x80u));
        assert(GW(targets[2],0x68)&0x1000u);
    }
    fixture();start();assert(input(hit,8));
    int calls=target_calls;target_callback(targets[2],objects[1]);
    assert(target_calls==calls && !(GW(targets[2],0x68)&0x80u));
    target_callback(targets[2],objects[2]);assert(hit_sounds==1);
    puts("Goemon pending hits revalidate phase/epoch, stop at ownership/lifecycle changes and guard native return writes passed");
}
static void pose_input_from_local(int out[11],int cid)
{
    out[0]=cid;out[1]=9;out[2]=(int)visit;out[3]=s_poses[0][0];
    memcpy(out+4,s_poses[0]+1,sizeof(int)*7);
}
static void test_box_action_flight(void)
{
    int packet[11],seq,launch,physical_calls;
    float expected[3],v[3],before;
    AnchorMinigameState host,got;(void)got;
    fixture();start();root_callback(root,object);assert(s_poses[0][0] && (s_poses[0][1]&32));
    seq=s_poses[0][0];throw_button=1;root_callback(root,object);root_callback(root,object);
    assert(throws==1 && s_poses[0][0]!=seq && !(s_poses[0][1]&32));
    for(int i=0;i<3;++i) {
        assert(GF(cube,0x78+4*i)==fixture_launch_velocity[i]);
        assert(s_poses[0][5+i]==(int)(fixture_launch_velocity[i]*1024.f));
        expected[i]=GF(cube_object,8+4*i);v[i]=(float)s_poses[0][5+i]/1024.f;
    }
    pose_input_from_local(packet,8);assert(input(packet,11));
    visuals_update();assert(clone_count==1);physical_calls=target_calls;
    for(int tick=0;tick<12;++tick) {
        v[1]=(float)((double)v[1]-.2);if(v[1]<-3.f) v[1]=-3.f;
        for(int j=0;j<3;++j) expected[j]+=2.f*v[j];
        visual_callback(clones[0],clone_objects[0]);
        fprintf(stderr,"Goemon flight tick%d expectedX%.3f cloneX%.3f\n",tick,expected[0],GF(clone_objects[0],8));
        for(int j=0;j<3;++j) assert(fabsf(GF(clone_objects[0],8+4*j)-expected[j])<0.001f);
        assert(GW(clones[0],0x60)==0x20 && !(GW(clones[0],0x68)&0x8000u));
    }
    assert(target_calls==physical_calls && model_binds==1);
    host=state();before=GF(clone_objects[0],8);assert(apply(&host));role(1,0,1);
    visual_callback(clones[0],clone_objects[0]);assert(GF(clone_objects[0],8)>before);
#ifdef ANCHOR_GOEMON_ACTION_V2
    launch=s_visuals[0].row[19];assert(launch==packet[3]);
    assert(host.actors[6][16]==(int)(v[0]*1024.f) && host.actors[6][8]==(int)(before*1024.f));
    before=s_visuals[0].xyz[0];assert(apply(&host));assert(s_visuals[0].xyz[0]==before);
    for(int i=0;i<GT_VISUAL_WORDS;++i) assert(s_visuals[0].net[i]==host.actors[6][i] || i==8 || i==9 || i==10 || i==16 || i==17 || i==18);
    /* Authoritative flight checkpoint retains launch serial and corrects smoothly. */
    host.actors[6][8]=(int)((before+8.f)*1024.f);host.actors[6][9]=300*1024;
    host.actors[6][16]=1024;host.actors[6][17]=0;host.actors[6][18]=0;
    host.full_tick=1;assert(apply(&host));assert(s_visuals[0].row[19]==launch);
    before=s_visuals[0].xyz[0];visual_callback(clones[0],clone_objects[0]);assert(s_visuals[0].xyz[0]>before);
    assert(model_binds==1);host=state();assert(capture(&got));
    /* Freeflight checkpoints retain launch identity; no six-frame freeflight stream. */
    role(1,1,1);seq=s_poses[0][0];s_frames=s_cube_emitted[0]+29;cube_poses();assert(s_poses[0][0]==seq);
    ++s_frames;GF(cube,0x7c)=-1.5f;cube_poses();assert(s_poses[0][0]!=seq);
    pose_input_from_local(packet,8);assert(input(packet,11));assert(s_visuals[0].row[19]==launch);
    /* The owner exports actual physical self state and never constructs its own visual. */
    pose_input_from_local(packet,7);assert(input(packet,11));visual_prepare();assert(clone_count==1);
    GF(cube_object,8)=321.f;GF(cube,0x78)=3.25f;host=state();
    assert(host.actors[7][3]==7 && host.actors[7][8]==321*1024 && host.actors[7][16]==3328);
    GB(cube,0xd1)=0;++s_frames;cube_poses();seq=s_poses[0][0];assert(!(s_poses[0][1]&16));
    GP(work,0x30)=0;++s_frames;cube_poses();assert(s_poses[0][0]==seq);
    /* Foreign reused clone storage cannot be modified or retired by old ownership. */
    for(int bad=0;bad<3;++bad) {
        GoemonNode saved=s_visuals[0].clone;
        GW(clones[0],0x60)=0x12345678u;
        if(bad==0) GP(clones[0],0xc)=(void *)func_08000CC8_708708;
        if(bad==1) GH(clones[0],0x28)=20;
        if(bad==2) GH(clones[0],0x5c)=1;
        visual_callback(clones[0],clone_objects[0]);visual_retire(&s_visuals[0]);
        assert(GW(clones[0],0x60)==0x12345678u && !(GW(clones[0],0x68)&2u));
        s_visuals[0].clone=saved;GP(clones[0],0xc)=(void *)visual_callback;
        GH(clones[0],0x28)=48;GH(clones[0],0x5c)=GT_ENTITY;
    }
#else
    launch=0;(void)launch;
#endif
    puts("Goemon immediate world launch, gravity and2v once, cached flight, correction and clone ownership passed");
}
static void test_box_hold_watchdog_and_terminal(void)
{
    int pose[11]={8,9,1,1,(1<<6)|GT_INPUT_POSE|16|32,100*1024,1000*1024,0,0,512,0};
    AnchorMinigameState snapshot,stopped;
    float before;
    fixture();start();assert(input(pose,11));visuals_update();visual_callback(clones[0],clone_objects[0]);
    pose[3]=2;pose[5]=108*1024;assert(input(pose,11));
    before=GF(clone_objects[0],8);visual_callback(clones[0],clone_objects[0]);assert(GF(clone_objects[0],8)==before+1.f);
    for(int i=1;i<8;++i) visual_callback(clones[0],clone_objects[0]);assert(GF(clone_objects[0],8)==108.f);
    pose[3]=3;pose[4]&=~32;pose[8]=1024;pose[9]=pose[10]=0;assert(input(pose,11));snapshot=state();
    for(int i=0;i<60;++i) visual_callback(clones[0],clone_objects[0]);
    before=GF(clone_objects[0],8);assert(apply(&snapshot));role(1,0,1);
    visual_callback(clones[0],clone_objects[0]);assert(GF(clone_objects[0],8)==before);
    ++snapshot.source_tick;assert(apply(&snapshot));visual_callback(clones[0],clone_objects[0]);
    assert(GF(clone_objects[0],8)==before); /* ACTION omitted this row: no watchdog renewal. */
    ++snapshot.full_tick;assert(apply(&snapshot));visual_callback(clones[0],clone_objects[0]);
    assert(GF(clone_objects[0],8)==before+2.f && model_binds==1);
    /* Accepted result promptly stops optional boxes, before native physical cleanup. */
    role(1,1,1);root_callback(root,object);assert(GP(work,0x30));
    result(2);stopped=state();assert(stopped.actors[6][6]==0 && (GW(clones[0],0x68)&2u));
    ++pose[3];assert(input(pose,11));visuals_update();assert(clone_count==1 && state().actors[6][6]==0);
    root_callback(root,object);root_callback(root,object);
    assert(terminal_cleanups==1 && !GP(work,0x30) && s_poses[0][0] && !(s_poses[0][1]&16));
    assert(apply(&stopped));visuals_update();assert(clone_count==1 && state().actors[6][6]==0);
    puts("Goemon held interpolation, flight watchdog, omitted ACTION and prompt result stop passed");
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--music")) { test_skipped_intro_music();return 0; }
    if(argc==2 && !strcmp(argv[1],"--hit-recovery")) { test_hit_recovery();return 0; }
    if(argc==2 && !strcmp(argv[1],"--owner-hit")) { test_owner_hit_once();test_pending_hit_lifetime();return 0; }
    if(argc==2 && !strcmp(argv[1],"--target-motion")) { test_action_target_motion();return 0; }
    if(argc==2 && !strcmp(argv[1],"--box-flight")) { test_box_action_flight();return 0; }
    if(argc==2 && !strcmp(argv[1],"--six-controls")) { test_six_result_controls_and_pop();return 0; }
    if(argc==2 && !strcmp(argv[1],"--export")) {
        char encoded[ANCHOR_MINIGAME_JSON_SIZE];fixture();start();
        GI(timer,0x84)=1745;GB(targets[2],0xd0)=3;GF(objects[2],8)=77.125f;
        int pose[11]={8,9,1,1,(1<<6)|GT_INPUT_POSE|16|32,100*1024,50*1024,0,0,512,0};
        assert(input(pose,11));pose[3]=2;pose[4]&=~32;pose[8]=1024;pose[9]=2048;pose[10]=-512;
        assert(input(pose,11));
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
        assert(incoming.has_state && incoming.state.running && incoming.state.ready);
        fixture();assert(validate(&incoming.state));assert(!apply(&incoming.state));
        root_callback(root,object);root_callback(root,object);
        assert(graph() && builds==1 && intros==0 && apply(&incoming.state));
        assert(GI(timer,0x84)==1745 && GB(targets[2],0xd0)==3 && GF(objects[2],8)==77.125f);
        assert(incoming.state.actor_count==7 && incoming.state.actor_words[6]==20);
#ifdef ANCHOR_GOEMON_ACTION_V2
        assert(s_visuals[0].row[16]==1024 && s_visuals[0].row[17]==2048 && s_visuals[0].row[19]==2);
#endif
        role(1,0,1);visual_callback(clones[0],clone_objects[0]);
        assert(GF(clone_objects[0],8)==102.f && fabsf(GF(clone_objects[0],0xc)-53.6f)<0.001f && model_binds==1);
        puts("Goemon Python status -> production codec -> native bootstrap/apply passed");return 0;
    }
    assert(argc==1);
    test_skipped_intro_music();
    test_hit_recovery();
    test_owner_hit_once();test_pending_hit_lifetime();
    test_action_target_motion();test_six_result_controls_and_pop();test_action_hit_epochs();test_box_action_flight();
    test_box_hold_watchdog_and_terminal();
    test_character_bootstrap();test_bootstrap_and_atomic_graph();test_local_control_and_shared_hits();
    test_visual_safety_and_optional_capacity();test_result_and_lifecycle();
    puts("Goemon native adapter: native bootstrap, atomic targets/timer, local carry/throw, semantic hits, visual safety, result and lifecycle passed");
    return 0;
}
