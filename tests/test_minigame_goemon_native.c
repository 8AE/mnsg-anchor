#include "impact_test_pointers.h"
#include <stdio.h>
#include <stdlib.h>
#define ANCHOR_MINIGAME_GOEMON_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define GP(p,o) TP(p,o)
#define GOEMON_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#include "../src/world/anchor_minigame_goemon.c"
#include "../src/world/anchor_minigame_codec.c"

unsigned short D_800C7AB2=GT_ROOM;
void *D_8016DAB4_16E6B4;
static unsigned int root[64],object[64],work[16],targets[6][64],objects[6][64],children[6][64];
static unsigned int child_objects[6][64];
static unsigned int timer[64],handles[3],cube[64],cube_object[64],clones[18][64],clone_objects[18][64];
static int vm,missing_resource,root_calls,target_calls,draws,intros,builds,carried,throw_button,throws;
static int clone_count,clone_fail,model_binds,returns;
static int character_ready=1;
static unsigned int visit=1;
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
{ anchor_minigame_goemon_child_terminal(task);++GB(task,0xd0); }
void func_08000CC8_708708(void *task,void *context)
{ (void)task;(void)context; }
void func_0800127C_708CBC(void *task,void *context)
{
    (void)context;++target_calls;
    if(GB(task,0xd0)==0) {
        int i;
        for(i=0;i<6;++i) if(task==targets[i]) break;
        assert(i<6);GP(task,0xe8)=children[i];GB(children[i],0xd0)=1;
        GP(children[i],0x18)=child_objects[i];GH(children[i],0x28)=48;GB(children[i],0x74)=4;
        GF(child_objects[i],8)=GF(objects[i],8);GF(child_objects[i],0x10)=GF(objects[i],0x10);
        GF(child_objects[i],0x1c)=GF(child_objects[i],0x20)=GF(child_objects[i],0x24)=.1f;
        GB(task,0xd0)=1;GB(task,0xd1)=0;
        GF(task,0xdc)=GF(task,0xe4)=0;
        GF(GP(task,0x18),0x1c)=GF(GP(task,0x18),0x20)=GF(GP(task,0x18),0x24)=.1f;
    } else if((GB(task,0xd0)==1 || GB(task,0xd0)==3 || GB(task,0xd0)==4) && (GW(task,0x68)&0x80u)) {
        GB(task,0xd0)=2;GS(task,0xd6)=0;GW(task,0x68)&=~0x80u;
    } else if(GB(task,0xd0)==2 && ++GS(task,0xd6)>5) {
        GB(task,0xd0)=7;GB(task,0xd1)=1;GB(GP(task,0xe8),0xd0)=2;
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
        GP(targets[i],0x0c)=(void *)func_0800127C_708CBC;
        GH(targets[i],0x28)=48;GB(targets[i],0x74)=2;
        GF(objects[i],8)=(float)i*10;GF(objects[i],0x10)=(float)i*-10;
        anchor_minigame_goemon_target(targets[i],objects[i]);
        func_0800127C_708CBC(targets[i],objects[i]);anchor_minigame_goemon_target_return();
        anchor_minigame_goemon_child(children[i],child_objects[i]);
        anchor_minigame_goemon_child_return();
    }
    GB(work,0)=1;
}
void func_080018E8_709328(void *task,void *context)
{
    assert(task==root && context==object);++root_calls;
    switch(GB(work,0)) {
    case 0:GB(work,0)=3;break;
    case 3:++intros;vm=1;GB(work,0)=4;break;
    case 4:if(!vm) setup_graph();break;
    case 1:
        if(!carried) {
            GP(work,0x30)=cube;GP(cube,0x18)=cube_object;
            GP(cube,0x0c)=(void *)func_08000CC8_708708;GH(cube,0x28)=48;GB(cube,0x74)=3;
            GB(cube,0xd0)=GB(cube,0xd1)=1;GW(cube,0x68)|=0x8000u;
            GF(cube_object,8)=15.f;GF(cube_object,0x0c)=20.f;carried=1;
        }
        if(throw_button && carried) GB(work,0)=2;
        if(GI(timer,0x84)/30==0) { GB(work,0)=5;GS(work,4)=0; }
        else {
            int all=1;
            for(int i=0;i<6;++i) if(GB(targets[i],0xd1)!=1) all=0;
            if(all) { GB(work,0)=6;GS(work,4)=0; }
        }
        break;
    case 2:++throws;carried=0;throw_button=0;GW(cube,0x68)&=~0x8000u;GB(work,0)=1;break;
    case 5:if(++GS(work,4)>=90) { GB(work,0)=7;GS(work,4)=1; }break;
    case 6:if(++GS(work,4)>=120) { GB(work,0)=7;GS(work,4)=1; }break;
    case 7:if(GS(work,4)==20) { assert(return_pending());++returns; }++GS(work,4);break;
    }
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
    character_ready=1;
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
    target_callback(targets[2],objects[2]);assert(GB(targets[2],0xd0)==2);
    assert(input(hit,8));assert(!(GW(targets[2],0x68)&0x80u));
    GB(targets[2],0xd0)=3;assert(input(hit,8));assert(!(GW(targets[2],0x68)&0x80u));
    GB(targets[2],0xd0)=4;hit[7]=4;assert(input(hit,8));
    assert(GW(targets[2],0x68)&0x80u);GW(targets[2],0x68)&=~0x80u;
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
    int pose[11]={8,10,1,1,(1<<6)|GT_INPUT_POSE|16,100*1024,20*1024,0,0,512,0};
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
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--export")) {
        char encoded[ANCHOR_MINIGAME_JSON_SIZE];fixture();start();
        GI(timer,0x84)=1745;GB(targets[2],0xd0)=3;GF(objects[2],8)=77.125f;
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
        puts("Goemon Python status -> production codec -> native bootstrap/apply passed");return 0;
    }
    assert(argc==1);
    test_character_bootstrap();test_bootstrap_and_atomic_graph();test_local_control_and_shared_hits();
    test_visual_safety_and_optional_capacity();test_result_and_lifecycle();
    puts("Goemon native adapter: native bootstrap, atomic targets/timer, local carry/throw, semantic hits, visual safety, result and lifecycle passed");
    return 0;
}
