#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "player/anchor_player_models.h"
#if DEBUG_BUTTON_ENABLED
#include <stdarg.h>
static char diagnostic_log[131072];
static unsigned int diagnostic_calls, diagnostic_length;
int recomp_printf(const char *fmt, ...)
{
    va_list args;int n;
    ++diagnostic_calls;va_start(args,fmt);
    n=vsnprintf(diagnostic_log+diagnostic_length,sizeof(diagnostic_log)-diagnostic_length,fmt,args);
    va_end(args);
    if(n>0 && (unsigned int)n<sizeof(diagnostic_log)-diagnostic_length) diagnostic_length+=(unsigned int)n;
    return n;
}
#endif

typedef void (*TestCallback)(void *,void *);
static void **pointer_slot(void *,unsigned int);
static TestCallback *ai_slot(void *);
static unsigned long callback_value(TestCallback);
#define EBI_PTR(p,o) (*pointer_slot(p,o))
#define EBI_AI(p) (*ai_slot(p))
#define EBI_CALLBACK_VALUE(callback) callback_value(callback)
#define EBI_CAMERA_DEST(p) EBI_PTR(p,0x2c)
#define ANCHOR_MINIGAME_EBISUMARU_HOST_TEST
#ifndef EBI_ADAPTER_SOURCE
#define EBI_ADAPTER_SOURCE "../src/world/anchor_minigame_ebisumaru.c"
#endif
#include EBI_ADAPTER_SOURCE
#include "../src/world/anchor_minigame_codec.c"

typedef union Store { unsigned int align; unsigned char bytes[256]; } Store;
typedef struct Task {
    Store task,object,link;
    int used;
    TestCallback ai;
} Task;
typedef struct Ptr { void *base;unsigned int offset;void *value; } Ptr;
static Task tasks[128];
static Ptr pointers[4096];
static unsigned int task_count,pointer_count,visit=1;
static int allowed,allocations,removals,awards,deaths,bomb_native,camera_calls,root_calls;
static int camera_commits;
static int character_ready=1;
static int cold_registry;
static unsigned int pool_limit;
static unsigned char resources[1024];
static unsigned int resource_queries;
static unsigned int callback_bank_handles[2]={0xc03ce000u,0xc03c1000u};
static int robot_phase_transition;
static int fixture_bomb_detonate;
static unsigned int fixture_blast_initializations,fixture_blast_effects;
static void *fixture_blast_children[2];
static unsigned int fixture_ack;
static AnchorBossTarget fixture_targets[8];
static int fixture_target_count,fixture_ineligible_cid;
static int fixture_local_cid=1;
unsigned int anchor_get_client_id(void) { return (unsigned int)fixture_local_cid; }
unsigned int anchor_minigame_sync_local_ack(int game) { assert(game==2);return fixture_ack; }
int anchor_minigame_sync_participant_current(int game,int cid)
{ assert(game==2);return cid>0 && cid!=fixture_ineligible_cid; }
int anchor_player_models_get_boss_targets(AnchorBossTarget *out,int count)
{ assert(count>=fixture_target_count);memcpy(out,fixture_targets,sizeof(*out)*(unsigned int)fixture_target_count);return fixture_target_count; }
static const AnchorMinigameAdapter *bound_adapter;
static Store hud,camera_store,scene_store,camera_destination;
static unsigned char backup[48];
unsigned short D_800C7AB2=0x1e1;
void *D_8016DAB4_16E6B4,*D_801FC604_5B8514,*D_801FC628_5B8538;
void *D_801FC60C_5B851C;
unsigned char D_080032F4_6F3504;
unsigned int D_8015C5D8_15D1D8[12];
double D_080032B0_6F34C0=4.,D_080032B8_6F34C8=4.;
double D_080032C0_6F34D0=4.,D_080032C8_6F34D8=4.;

static void **pointer_slot(void *base,unsigned int offset)
{
    unsigned int i;
    for(i=0;i<pointer_count;++i)
        if(pointers[i].base==base && pointers[i].offset==offset) return &pointers[i].value;
    assert(pointer_count<4096);
    pointers[pointer_count].base=base;pointers[pointer_count].offset=offset;
    return &pointers[pointer_count++].value;
}
static Task *task_for(void *p)
{
    unsigned int i;
    for(i=0;i<task_count;++i) if(tasks[i].task.bytes==p) return &tasks[i];
    assert(0);return 0;
}
static TestCallback *ai_slot(void *p) { return &task_for(p)->ai; }
static void *make_task(unsigned short entity,TestCallback ai)
{
    Task *t=&tasks[task_count++];unsigned int i;
    assert(task_count<=128);memset(t,0,sizeof(*t));t->used=1;t->ai=ai;
    EBI_PTR(t->task.bytes,0x18)=t->object.bytes;
    EBI_PTR(t->task.bytes,4)=t->link.bytes;EBI_PTR(t->link.bytes,0)=t->task.bytes;
    EH(t->task.bytes,0x5c)=EH(t->task.bytes,0x5e)=entity;
    EB(t->task.bytes,0x74)=7;EB(t->task.bytes,0x8d)=1;
    for(i=0;i<3;++i) EF(t->object.bytes,0x1c+4*i)=0.1f;
    return t->task.bytes;
}
unsigned int anchor_minigame_sync_visit(void) { return visit; }
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *a) { bound_adapter=a;return 1; }
int anchor_minigame_sync_native_allowed(int game) { assert(game==2);return allowed; }
int anchor_minigame_invites_character_motion_ready(unsigned short room)
{ assert(room==0x1e1);return character_ready; }
void *func_800141C4_14DC4(unsigned int f)
{
    ++resource_queries;
    if(f>=1024 || !resources[f]) return (void *)(unsigned long)0xffffffffu;
    if(f==0x2a || f==0x20)
        return (void *)(unsigned long)callback_bank_handles[f==0x20];
    return &resources[f];
}
typedef struct CallbackIdentity { TestCallback host;unsigned int raw,bank; } CallbackIdentity;
static const CallbackIdentity callback_identities[]={
    {func_0800139C_6F15AC,0x0800139cu,0x2a},
    {func_08001250_6F1460,0x08001250u,0x2a},
    {func_08001720_6D0800,0x08001720u,0x20},
    {func_08000E2C_6F103C,0x08000e2cu,0x2a},
    {func_08001030_6F1240,0x08001030u,0x2a},
    {func_08000B58_6F0D68,0x08000b58u,0x2a},
    {func_08001884_6D0964,0x08001884u,0x20},
    {func_080018D0_6D09B0,0x080018d0u,0x20},
    {func_0800191C_6D09FC,0x0800191cu,0x20},
    {func_08001958_6D0A38,0x08001958u,0x20},
    {func_080019D0_6D0AB0,0x080019d0u,0x20},
    {func_08001A40_6D0B20,0x08001a40u,0x20},
    {func_08001AA4_6D0B84,0x08001aa4u,0x20},
    {func_08001AFC_6D0BDC,0x08001afcu,0x20},
    {func_08001B40_6D0C20,0x08001b40u,0x20},
    {func_08001BC8_6D0CA8,0x08001bc8u,0x20},
    {func_08001C4C_6D0D2C,0x08001c4cu,0x20},
    {func_08001CD0_6D0DB0,0x08001cd0u,0x20},
    {func_08001D7C_6D0E5C,0x08001d7cu,0x20}
};
static unsigned int fixture_loaded_callback(const CallbackIdentity *identity)
{
    return identity->raw-0x08000000u+
        (identity->bank==0x20?0x803c1000u:0x803ce000u);
}
/* Native identities are 32-bit N64 values; executable host stubs remain
 * ordinary host function pointers. Only comparisons use this accessor. */
static unsigned long callback_value(TestCallback callback)
{
    unsigned int i;
    for(i=0;i<sizeof(callback_identities)/sizeof(callback_identities[0]);++i)
        if(callback==callback_identities[i].host)
            return fixture_loaded_callback(&callback_identities[i]);
    return (unsigned long)callback;
}
/* File12 births retain segmented table callbacks until patched 1481C
 * resolves the callback at the child's first scheduler slot. */
static TestCallback segmented_callback(TestCallback callback)
{
    if(callback==func_0800139C_6F15AC) return (TestCallback)(unsigned long)0x0800139cu;
    if(callback==func_08001250_6F1460) return (TestCallback)(unsigned long)0x08001250u;
    if(callback==func_08001720_6D0800) return (TestCallback)(unsigned long)0x08001720u;
    if(callback==func_08000E2C_6F103C) return (TestCallback)(unsigned long)0x08000e2cu;
    if(callback==func_08001030_6F1240) return (TestCallback)(unsigned long)0x08001030u;
    if(callback==func_08000B58_6F0D68) return (TestCallback)(unsigned long)0x08000b58u;
    return callback;
}
static unsigned int resolver_calls;
void *func_80014840_15440(void *callback,unsigned int bank)
{
    unsigned long raw=(unsigned long)callback;
    ++resolver_calls;
    if(raw<0x08000000ul || raw>=0x09000000ul) return callback;
    assert(bank<1024 && resources[bank]); /* Native resolver faults otherwise. */
    assert(bank==0x2a || bank==0x20);
    /* Both banks use legacy segment0x11. Patched1481C uses this return only
     * as a relocation predicate; its dispatch address uses segment0x08. */
    return (void *)(unsigned long)((unsigned int)raw-0x11000000u+
        (callback_bank_handles[bank==0x20]&~0x40000000u));
}
void func_80217144_5D2614(void *t) { EB(t,0x8d)=1;EB(t,0x4c)=1; }
void func_8021664C_5D1B1C(void *t,unsigned int clip,float rate,unsigned int flags)
{
    if(cold_registry && EH(t,0x5e)==0x142) {
        /* Native descriptor142: model/animation files 1CF and173. */
        resources[0x1cf]=resources[0x173]=1;
    }
    if(cold_registry && EH(t,0x5e)==0xfb)
        resources[0x197]=resources[0x159]=1;
    anchor_minigame_ebi_clip(t,clip,rate,flags);EF(EBI_PTR(t,0x18),0x28)=0.f;
}
void func_80216E1C_5D22EC(void *t,int mode) { assert(mode==0);(void)t; }
void func_80219E70_5D5340(void *t,unsigned char a,unsigned char b)
{ assert(a==10 && b==0);EW(t,0x60)|=0x08000000u; }
void func_8021A310_5D57E0(void *t)
{ void *o=EBI_PTR(t,0x18);EH(o,0x14)=EH(o,0x16)=EH(o,0x18)=0x8000; }
void func_80218DA8_5D4278(void *t,int radius,unsigned short height,short offset)
{ EW(t,0x48)=0xffffffffu;ES(t,0x4e)=(short)(radius/10);EH(t,0x50)=height;ES(t,0x52)=offset; }
void func_80218548_5D3A18(void *t)
{
    assert(t==D_8016DAB4_16E6B4);
    if(!(EW(t,0x68)&0x40000u) && !EBI_PTR(t,0x38)) return;
    assert(EW(t,0x64)&0x8000u);++deaths;EW(t,0x68)|=2;
}
static void fixture_collision_slot(void *t)
{
    unsigned int i;
    void *o=EBI_PTR(t,0x18);D_8016DAB4_16E6B4=t;
    anchor_minigame_ebi_collision_begin();
    if(EW(t,0x60)&0x200u) for(i=0;i<3;++i) EF(o,8+4*i)+=2.f*EF(t,0x78+4*i);
    EF(o,0x28)+=0.25f;
    anchor_minigame_ebi_damage(t);
    if(EW(t,0x68)&0x40000u) {
        if(EB(t,0x8d)) --EB(t,0x8d);
        if(!EB(t,0x8d)) func_80218548_5D3A18(t);
        else { EW(t,0x68)|=1u;EB(t,0x8c)=60; }
    }
    EW(t,0x68)&=~0x40000u;
    if(EB(t,0x8c)) { --EB(t,0x8c);if(!EB(t,0x8c)) EW(t,0x68)&=~1u; }
    anchor_minigame_ebi_collision_end();
    if(EW(t,0x68)&2u) func_80034EF8_35AF8(t);
}
unsigned int func_8021ADD0_5D62A0(float x,float z)
{ return (unsigned int)((int)(atan2f(x,z)*162.97466f)&1023); }
int func_802196FC_5D4BCC(void *t,float x,float y,float z,int step)
{
    void *o=EBI_PTR(t,0x18);int desired=func_8021ADD0_5D62A0(x-EF(o,8),z-EF(o,0x10));
    int difference=(desired-(int)EH(o,0x16)+512)&1023;(void)y;
    if(abs(difference-512)<step*2) { EH(o,0x16)=(unsigned short)desired;return 0; }
    EH(o,0x16)=(unsigned short)((EH(o,0x16)+(difference>=512?step*2:-step*2))&1023);return 1;
}
int func_8021BB04_5D6FD4(void *t,float x,float z,float speed)
{
    void *o=EBI_PTR(t,0x18);float dx=x-EF(o,8),dz=z-EF(o,0x10),length=sqrtf(dx*dx+dz*dz);
    if(!length) { EF(t,0x78)=EF(t,0x80)=0.f;return 0; }
    EF(t,0x78)=dx/length*speed;EF(t,0x80)=dz/length*speed;return 1;
}
void func_80219448_5D4918(void *t)
{
    float theta,rotation,x,z,p,q;
    EH(t,0xd0)=(unsigned short)((EH(t,0xd0)+(signed char)EB(t,0xd6))&1023);
    theta=(float)EH(t,0xd0)*0.00613592315f;rotation=(float)EH(t,0xd2)*0.00613592315f;
    p=(float)EB(t,0xd4)*sinf(theta);q=(float)EB(t,0xd5)*cosf(theta);
    x=q*sinf(rotation)-cosf(rotation)*p;z=sinf(rotation)*p+q*cosf(rotation);
    EF(t,0x78)=(x-EF(t,0xd8))/2.f;EF(t,0x80)=(z-EF(t,0xdc))/2.f;
    EF(t,0xd8)=x;EF(t,0xdc)=z;
}
void *func_8021A26C_5D573C(void *t,unsigned int opacity,int r,unsigned int g,unsigned int b)
{ void *o=EBI_PTR(t,0x18);assert(!r && !g && !b);EW(o,0x80)=opacity;
  return (void *)(unsigned long)0x80001280u; }
void *func_8021DD4C_5D921C(void *t,unsigned int opacity,int r,unsigned int g,unsigned int b)
{ return func_8021A26C_5D573C(t,opacity,r,g,b); }
void func_80034EF8_35AF8(void *t)
{ anchor_minigame_ebi_delete_named(t);task_for(t)->used=0;++removals; }
static void *spawn(void *parent,unsigned short entity,TestCallback ai,int segmented)
{
    unsigned int i,count=0;
    void *t;
    for(i=0;i<task_count;++i) count+=tasks[i].used!=0;
    if(pool_limit && count>=pool_limit) return 0;
    TestCallback birth=segmented?segmented_callback(ai):ai;
    int direct=ai==func_08000B58_6F0D68;
    t=make_task(0,birth);EB(t,0x74)=0;
    EH(t,0x28)=direct?0:entity==0xfb?0x20:0x2a;
    if(direct) EBI_PTR(t,0x2c)=0;
    anchor_minigame_ebi_child(parent,t);
    /* The hook must observe, not rewrite, the native birth callback. */
    assert(EBI_AI(t)==birth);
    if(raw_find(t)) assert(raw_find(t)->entity==0 && raw_find(t)->task_generation==0);
    /* ENTRY precedes18C28's inheritance, and the constructor then assigns
     * its own entity. The live recording shows zero entity/gen at ENTRY. */
    EB(t,0x74)=EB(parent,0x74);
    EH(t,0x5c)=entity;EH(t,0x5e)=direct?EH(parent,0x5e):entity;
    if(direct) { EH(t,0x28)=0xfffe;EBI_PTR(t,0x2c)=(void *)(unsigned long)0xffffffffu; }
    ++allocations;return t;
}
void *func_80217360_5D2830(void *parent,unsigned short entity,unsigned char group)
{
    TestCallback ai=entity==0x1ae?func_0800139C_6F15AC:entity==0x1b1?func_08001250_6F1460:
        entity==0xfb?func_08001720_6D0800:entity==0x1ac?func_08000E2C_6F103C:func_08001030_6F1240;
    assert(group==(entity==0xfb || entity==0x1ac?6:entity==0x1ad?9:0));
    return spawn(parent,entity,ai,1);
}
void *func_8021DDE8_5D92B8(void *parent,EbiCallback ai,unsigned char group,
                          float x,float y,float z,int yaw)
{
    void *t,*o;unsigned int i;
    int raw=((unsigned long)ai&~0x00800000ul)==0x08000b58ul;
    assert((raw || ai==func_08000B58_6F0D68) && group<=1 && x==0 && y==0 && z==0 && yaw==0);
    t=spawn(parent,EH(parent,0x5c),func_08000B58_6F0D68,raw);
    if(!t) return 0;
    o=EBI_PTR(t,0x18);
    for(i=0;i<3;++i) { EF(o,8+4*i)=EF(EBI_PTR(parent,0x18),8+4*i);EF(o,0x1c+4*i)=0.1f; }
    return t;
}
void func_0800139C_6F15AC(void *t,void *o)
{
    if(!EB(t,0xd4)) {
        if(cold_registry) resources[0x1e3]=1;
        EW(t,0x60)=0x20;
        EB(t,0xd4)=1;EBI_PTR(t,0xd0)=func_80217360_5D2830(t,0x1b1,0);
    }
    (void)o;anchor_minigame_ebi_mechanism_initialized();
}
void func_08001250_6F1460(void *t,void *o)
{ if(cold_registry) resources[0x1e6]=1;
  EW(t,0x60)=0x21;
  EF(o,0x1c)=EF(o,0x20)=EF(o,0x24)=1.f;
  EB(t,0xd0)=1;anchor_minigame_ebi_platform_initialized(); }
void func_08001720_6D0800(void *t,void *o)
{
    EW(t,0x60)=0x02ae87e1;ES(t,0xb4)=(short)EF(o,0xc);
    func_8021664C_5D1B1C(t,0,1.f/3.f,1);
    EBI_AI(t)=func_080018D0_6D09B0;anchor_minigame_ebi_robot_initialized();
}
void func_08000E2C_6F103C(void *t,void *o)
{
    ++bomb_native;
    if(!EB(t,0xd0)) EB(t,0xd0)=1;
    else if(fixture_bomb_detonate && !(EW(t,0x68)&0x20u)) {
        unsigned int i;
        for(i=0;i<2;++i) {
            void *child=func_8021DDE8_5D92B8(t,(EbiCallback)(unsigned long)0x08000b58u,(unsigned char)i,0,0,0,0);
            fixture_blast_children[i]=child;
            if(child) { EH(child,0x28)=0x2a;EW(child,0xe8)|=i?4u:1u;EBI_PTR(child,0x2c)=func_800141C4_14DC4(0x2a); }
        }
        anchor_minigame_ebi_delete();task_for(t)->used=0;++removals;
    }
    (void)o;anchor_minigame_ebi_bomb_initialized();
}
void func_08001030_6F1240(void *t,void *o)
{
    if(!EB(t,0xd0)) { EB(t,0xd0)=1;EH(t,0x8a)=270; }
    else if(EB(t,0xd0)==2) { ++D_080032F4_6F3504;++awards;func_80034EF8_35AF8(t); }
    (void)o;anchor_minigame_ebi_sweet_initialized();
}
static void fixture_empty_blast_effect(void *t,void *o) { (void)t;(void)o; }
void func_08000B58_6F0D68(void *t,void *o)
{
    if(!EB(t,0xd0)) {
        ++fixture_blast_initializations;resources[0x191]=1;
        EW(t,0x60)=0x004006e0u;EH(t,0x5e)=0x7e;ES(t,0xd2)=90;EH(t,0x8a)=30;
        if(EW(t,0xe8)==1) {
            unsigned int j,count=0;
            EB(t,0x4c)=0;
            for(j=0;j<task_count;++j) count+=tasks[j].used!=0;
            if(!pool_limit || count<pool_limit) {
                make_task(0,fixture_empty_blast_effect);++allocations;++fixture_blast_effects;
            }
        }
        if(EW(t,0xe8)==4) EB(t,0x6d)=1;
        EB(t,0xd0)=1;
    } else {
        unsigned int old=EH(t,0x8a),i;
        ES(t,0xd2)-=2;
        for(i=0;i<3;++i) EF(o,0x1c+4*i)=(float)((double)EF(o,0x1c+4*i)+(EW(t,0xe8)==1?0.03:0.02));
        EH(o,0x16)=(unsigned short)((EH(o,0x16)+64)&1023);
        if(EW(t,0xe8)==4) { func_80218DA8_5D4278(t,1200,0,0);EB(t,0x4c)=1; }
        EH(t,0x8a)=(unsigned short)(old-1);
        if(!old) func_80034EF8_35AF8(t);
    }
    anchor_minigame_ebi_blast_initialized();
}
#define ROBOT_STUB(f) void f(void *t,void *o) { \
    if(robot_phase_transition && (f)==func_080018D0_6D09B0) \
        EBI_AI(t)=func_0800191C_6D09FC; \
    (void)o; }
EBI_ROBOT_PHASES(ROBOT_STUB)
#undef ROBOT_STUB
static unsigned int fixture_drop_warnings,fixture_drop_retractions,fixture_drop_rng,fixture_bomb_requests;
static int fixture_sweet_failure,fixture_root_terminal,fixture_corrupt_bomb,fixture_root_ineligible_on_return;
static void *fixture_last_bomb;
static float fixture_root_seen_host[3];
static int fixture_host_hidden(void)
{
    static const short pads[4][2]={{80,-49},{49,48},{-96,99},{-62,-23}};
    unsigned int i;
    float height=EF(D_801FC60C_5B851C,0xc)+15.f;
    for(i=0;i<4;++i) {
        float dx=(float)pads[i][0]-EF(D_801FC60C_5B851C,8);
        float dz=(float)pads[i][1]-EF(D_801FC60C_5B851C,0x10);
        if(sqrtf(dx*dx+dz*dz)<14.f && height< -40.f) return 1;
    }
    return 0;
}
static float fixture_scatter_x(float x)
{ if(x< -112.f) x=-112.f;if(x>112.f) x=112.f;return (float)((double)x/1.2); }
static void fixture_retract_dropper(void *root)
{
    void *mechanism=EBI_PTR(root,0xd0),*platform=EBI_PTR(mechanism,0xd0);
    ++fixture_drop_retractions;EW(root,0x60)|=0x01000000u;
    func_8021664C_5D1B1C(root,0,0.125f,1);
    EF(mechanism,0xd8)=0.f;EF(mechanism,0xdc)=fabsf(EF(EBI_PTR(mechanism,0x18),8))/20.f;
    if(platform) { EW(platform,0x60)|=0x01000000u;func_8021664C_5D1B1C(platform,0,0.5f,0); }
    EB(root,0xdc)=5;
}
void func_08001954_6F1B64(void *t,void *o)
{
    ++root_calls;
    /* Native phase0 checks the incoming frame before incrementing its common
     * tail. The first two returns precede graph construction. */
    if(EB(t,0xdc)==0 && ES(t,0xde)>=2) {
        EBI_PTR(t,0xd4)=camera_store.bytes;EBI_PTR(t,0xd8)=hud.bytes;
        EI(hud.bytes,0x80)=2970;
        if(!EBI_PTR(t,0xd0)) EBI_PTR(t,0xd0)=func_80217360_5D2830(t,0x1ae,0);
        EW(t,0x60)=0x002086e1;
        EB(t,0xdc)=1;ES(t,0xde)=0;ES(t,0xe0)=1;
    }
    else if(EB(t,0xdc)==1 && ES(t,0xe0)%201==10) {
        unsigned int count=0;
        for(unsigned int i=0;i<task_count;++i)
            count+=tasks[i].used && EH(tasks[i].task.bytes,0x5c)==0xfb;
        if(count<2) {
            void *robot=func_80217360_5D2830(t,0xfb,6);
            if(robot) {
                func_80217144_5D2614(robot);
                EW(robot,0x64)|=0x8000u; /* Native root's first-robot flags. */
            }
        }
    }
    else if(EB(t,0xdc)==4) {
        unsigned int i;
        for(i=0;i<3;++i) fixture_root_seen_host[i]=EF(D_801FC60C_5B851C,8+4*i);
        if(!fixture_host_hidden()) {
            func_8021664C_5D1B1C(t,2,0.25f,1);ES(t,0xde)=0;EB(t,0xdc)=6;
        } else {
            if(ES(t,0xde)%60==2) ++fixture_drop_warnings;
            if(ES(t,0xde)>120) fixture_retract_dropper(t);
        }
    }
    else if(EB(t,0xdc)==6) {
        int branch=ES(t,0xde)%40;
        if(ES(t,0xde)>120) fixture_retract_dropper(t);
        if(branch==1 || branch==21) {
            void *bomb;
            ++fixture_bomb_requests;
            bomb=func_80217360_5D2830(t,0x1ac,6);
            fixture_last_bomb=bomb;
            if(bomb) {
                void *object=EBI_PTR(bomb,0x18);
                func_80217144_5D2614(bomb);fixture_drop_rng+=2;
                /* Deterministic valid samples preserve the native two-call
                 * RNG schedule and caller's writes AFTER constructor return. */
                EF(object,8)=(branch==1?EF(D_801FC60C_5B851C,8):
                             fixture_scatter_x(EF(D_801FC60C_5B851C,8)))+3.f;
                EF(object,0xc)=50.f;EF(object,0x10)=EF(D_801FC60C_5B851C,0x10)-4.f;
                if(fixture_corrupt_bomb==1) ++EB(bomb,0x74);
                if(fixture_corrupt_bomb==2) EH(bomb,0x5c)=0x1ad;
                if(fixture_corrupt_bomb==3) EBI_PTR(EBI_PTR(bomb,4),0)=0;
                if(fixture_corrupt_bomb==4) func_80034EF8_35AF8(bomb);
#ifdef ANCHOR_EBI_REMOTE_DETECTOR
                if(fixture_corrupt_bomb==5) raw_find(bomb)->proxy=1;
#endif
                if(fixture_corrupt_bomb==6) ++visit;
                if(fixture_corrupt_bomb==7) ++EB(t,0x74);
                if(fixture_corrupt_bomb==8) D_8016DAB4_16E6B4=D_801FC604_5B8514;
                if(fixture_corrupt_bomb==9) EH(bomb,0x28)=0x20;
                if(fixture_corrupt_bomb==10) EBI_AI(bomb)=func_08001030_6F1240;
                if(fixture_corrupt_bomb==11) EBI_PTR(bomb,0x18)=camera_store.bytes;
                if(fixture_corrupt_bomb==12) EW(bomb,0x68)|=2u;
#ifdef ANCHOR_EBI_REMOTE_DETECTOR
                if(fixture_corrupt_bomb==13) ++raw_find(bomb)->id;
#endif
                if(fixture_corrupt_bomb==14) EH(t,0x5c)=0x1ac;
            }
        }
    }
    if(fixture_root_terminal) { EB(t,0xdc)=(unsigned char)fixture_root_terminal;ES(t,0xde)=0; }
    if(fixture_root_ineligible_on_return) fixture_ineligible_cid=fixture_root_ineligible_on_return;
    if(fixture_sweet_failure && ES(t,0xe0)%240==5) {
        void *sweet=func_80217360_5D2830(t,0x1ad,9);
        if(!sweet) { anchor_minigame_ebi_root_initialized();return; }
    }
    if(EB(t,0xdc)>=1 && EB(t,0xdc)<=6 && EBI_PTR(t,0xd8) &&
       ((int)D_8015C5D8_15D1D8[3]<=17 || EI(EBI_PTR(t,0xd8),0x80)/30<=0)) {
        EB(t,0xdc)=8;ES(t,0xde)=0;
    }
    ES(t,0xde)++;ES(t,0xe0)++;
    (void)o;anchor_minigame_ebi_root_initialized();
}
void func_800124A0_130A0(void *p,float a) { (void)p;(void)a;++camera_calls; }
void func_800124AC_130AC(void *p,float a) { (void)p;(void)a;++camera_calls; }
void func_80012628_13228(void *p,float a,float b,float c) { (void)p;(void)a;(void)b;(void)c;++camera_calls; }
void func_80012518_13118(void *p,float a,float b,float c) { (void)p;(void)a;(void)b;(void)c;++camera_calls; }
void func_80012764_13364(void *p,float a) { (void)p;(void)a;++camera_calls; }
void func_80012900_13500(void *p,void *destination)
{ assert(p==camera_store.bytes && destination==camera_destination.bytes);
  memcpy(destination,(char *)p+0x6c,0x60);++camera_commits; }
static void tick_children(int advance_wrapped)
{
    unsigned int i,n=task_count;
    for(i=0;i<n;++i) {
        Task *t=&tasks[i];
        if(!t->used || t->task.bytes==s_root.task || !t->ai ||
           (!advance_wrapped && callback_is(t->ai,ebi_actor_update))) continue;
        if((unsigned long)t->ai>=0x08000000ul && (unsigned long)t->ai<0x09000000ul) {
            unsigned int j,bank=EH(t->task.bytes,0x28);
            unsigned int raw=(unsigned int)(unsigned long)t->ai;
            unsigned int loaded=(raw&~0x00800000u)-0x08000000u+
                (callback_bank_handles[bank==0x20]&~0x40000000u);
            assert(bank==0x2a || bank==0x20);
            /* Independent patched-scheduler arithmetic, then host dispatch.
             * Never dispatch the legacy resolver's wrong return address. */
            for(j=0;j<sizeof(callback_identities)/sizeof(callback_identities[0]);++j)
                if(bank==callback_identities[j].bank &&
                   loaded==fixture_loaded_callback(&callback_identities[j])) break;
            assert(j<sizeof(callback_identities)/sizeof(callback_identities[0]));
            t->ai=callback_identities[j].host;
        }
        assert(t->ai);
        D_8016DAB4_16E6B4=t->task.bytes;t->ai(t->task.bytes,t->object.bytes);
    }
}
static void tick_initializers(void) { tick_children(0); }
static void fresh_with_children(int finish_children)
{
    void *t,*mechanism;
    reset();memset(tasks,0,sizeof(tasks));memset(pointers,0,sizeof(pointers));
    memset(resources,cold_registry?0:1,sizeof(resources));memset(backup,0x5a,sizeof(backup));
    if(cold_registry) resources[0x2a]=resources[0x20]=1;
    task_count=pointer_count=0;allowed=0;allocations=removals=awards=deaths=bomb_native=camera_calls=root_calls=0;
    camera_commits=0;character_ready=1;memset(&camera_destination,0,sizeof(camera_destination));
    robot_phase_transition=0;fixture_bomb_detonate=0;
    fixture_blast_initializations=fixture_blast_effects=0;memset(fixture_blast_children,0,sizeof(fixture_blast_children));
    fixture_local_cid=1;
    fixture_drop_warnings=fixture_drop_retractions=fixture_drop_rng=fixture_bomb_requests=0;
    fixture_sweet_failure=fixture_root_terminal=fixture_corrupt_bomb=fixture_root_ineligible_on_return=0;fixture_last_bomb=0;
    fixture_ack=0;fixture_target_count=fixture_ineligible_cid=0;
    memset(&camera_store,0x63,sizeof(camera_store));
    D_801FC628_5B8538=scene_store.bytes;
    EBI_PTR(scene_store.bytes,0x2c)=camera_destination.bytes;
    pool_limit=0;
    ++visit;D_080032F4_6F3504=0;D_8015C5D8_15D1D8[1]=1;D_8015C5D8_15D1D8[3]=20;
    t=make_task(0x142,func_08001954_6F1B64);ES(t,0xe0)=2;ES(t,0xe2)=70;
    D_8016DAB4_16E6B4=t;func_08001954_6F1B64(t,EBI_PTR(t,0x18));
    assert(bound_adapter==&s_adapter && EB(t,0xdc)==0 && ES(t,0xde)==1);
    assert(!EBI_PTR(t,0xd0) && !root_ready());
    character_ready=0;ebi_root_update(t,EBI_PTR(t,0x18));
    assert(root_calls==1 && ES(t,0xde)==1 && !EBI_PTR(t,0xd0));
    character_ready=1;ebi_root_update(t,EBI_PTR(t,0x18));
    assert(root_calls==2 && EB(t,0xdc)==0 && ES(t,0xde)==2 && !EBI_PTR(t,0xd0));
    ebi_root_update(t,EBI_PTR(t,0x18));
    mechanism=EBI_PTR(t,0xd0);assert(mechanism && raw_find(mechanism));
    assert(root_calls==3 && EB(t,0xdc)==1 && ES(t,0xde)==1);
    if(finish_children) { tick_initializers();tick_initializers();assert(root_ready()); }
    else assert(!root_ready());
    root_calls=0;
    D_801FC604_5B8514=make_task(1,0);D_801FC60C_5B851C=EBI_PTR(D_801FC604_5B8514,0x18);role(1,0,1);
}
static void fresh(void) { fresh_with_children(1); }
static void test_repeated_initializer_returns(void)
{
    static TestCallback native[]={func_0800139C_6F15AC,func_08001250_6F1460,
        func_08000E2C_6F103C,func_08001030_6F1240,func_08000B58_6F0D68};
    static const int kinds[]={EBI_MECHANISM,EBI_PLATFORM,EBI_BOMB,EBI_SWEET,EBI_BLAST};
    EbiActor *a,*sweet,*robot;
    void *children[5],*robot_task,*task;
    unsigned int i,j,removed;
    unsigned short entity;
    unsigned char generation;
    fresh();allowed=1;role(1,1,1);
    a=find(EBI_PTR(s_root.task,0xd0));assert(a && a->initialized);
    D_8016DAB4_16E6B4=a->task;
    ebi_actor_update(a->task,a->object);
    /* Stop at the first corrupted continuation, before a second slot could
     * recursively dispatch the wrapper and overflow the native C stack. */
    fprintf(stderr,"Ebi repeated return: mechanism native_is_wrapper=%d\n",
            callback_is(a->native,ebi_actor_update));
    assert(!callback_is(a->native,ebi_actor_update));
    assert(callback_is(a->native,func_0800139C_6F15AC));
    ebi_actor_update(a->task,a->object);
    assert(callback_is(a->native,func_0800139C_6F15AC));
    children[0]=a->task;children[1]=EBI_PTR(a->task,0xd0);
    children[2]=func_80217360_5D2830(s_root.task,0x1ac,6);
    children[3]=func_80217360_5D2830(s_root.task,0x1ad,9);
    { int row[ER_WORDS]={0};row[ER_KIND]=EBI_BLAST;row[ER_ID]=9;row[ER_GENERATION]=1;row[ER_MODE]=1;
      EbiActor *blast=construct(row);assert(blast);children[4]=blast->task; }
    robot_task=func_80217360_5D2830(s_root.task,0xfb,6);
    tick_initializers();
    for(i=0;i<5;++i) {
        a=find(children[i]);assert(a && a->initialized && a->kind==kinds[i]);
        entity=a->entity;generation=a->task_generation;
        for(j=0;j<3;++j) {
            D_8016DAB4_16E6B4=a->task;ebi_actor_update(a->task,a->object);
            assert(callback_is(a->native,native[i]) && EBI_AI(a->task)==ebi_actor_update);
            assert(a->entity==entity && a->task_generation==generation);
        }
        /* Repeated RETURN hooks must neither overwrite inhibited callbacks
         * nor recapture a recycled/mismatched task's identity. */
        EBI_AI(a->task)=(TestCallback)(callback_value(native[i])|0x00800000ul);
        D_8016DAB4_16E6B4=a->task;initialized(kinds[i]);
        assert((unsigned long)EBI_AI(a->task)==(callback_value(native[i])|0x00800000ul));
        assert(a->native==native[i]);
        EH(a->task,0x5c)=(unsigned short)(entity+1);EB(a->task,0x74)=(unsigned char)(generation+1);
        initialized(kinds[i]);
        assert(a->entity==entity && a->task_generation==generation && !live(a));
        EH(a->task,0x5c)=entity;EB(a->task,0x74)=generation;EBI_AI(a->task)=ebi_actor_update;
    }
    robot=find(robot_task);assert(robot && robot->initialized);
    /* The initial robot callback transitions once; a repeated return from
     * that setup function must still allow its genuine continuation. */
    robot->native=func_08001720_6D0800;D_8016DAB4_16E6B4=robot_task;
    ebi_actor_update(robot_task,robot->object);
    assert(robot->native==func_080018D0_6D09B0 && EBI_AI(robot_task)==ebi_actor_update);
    robot_phase_transition=1;ebi_actor_update(robot_task,robot->object);
    assert(robot->native==func_0800191C_6D09FC && EBI_AI(robot_task)==ebi_actor_update);
    ebi_actor_update(robot_task,robot->object);
    assert(robot->native==func_0800191C_6D09FC);robot_phase_transition=0;
    sweet=find(children[3]);assert(sweet);task=sweet->task;removed=(unsigned int)removals;
    EB(task,0xd0)=2;D_8016DAB4_16E6B4=task;ebi_actor_update(task,sweet->object);
    assert(!sweet->task && !task_for(task)->used && (unsigned int)removals==removed+1);
    assert(EBI_AI(task)==func_08001030_6F1240 && sweet->native==func_08001030_6F1240);
    puts("Ebi repeated initializer returns preserve native callbacks, transitions, aliases and deletion passed");
}
static void state_base(AnchorMinigameState *s)
{ memset(s,0,sizeof(*s));assert(capture(s));assert(validate(s)); }
static void test_cold_registry_solo_startup(void)
{
    AnchorMinigameState state;
    unsigned int robots=0;
    cold_registry=1;fresh();
    assert(!resources[0x0c]); /* File12 uses the fixed overlay loader. */
    assert(EB(s_root.task,0xdc)==1 && ES(s_root.task,0xde)==1 && ES(s_root.task,0xe0)==2);
    assert(resources[0x1e3] && resources[0x1e6]);
    assert(!resources[0x1cf] && !resources[0x173]);
    for(unsigned int frame=0;frame<40;++frame) {
        memset(&state,0,sizeof(state));
        int captured=capture(&state);
        /* Mirror solo-owner readiness without granting native execution
         * until production capture and validation accept this local graph. */
        allowed=captured && state.ready && validate(&state);
        role(1,allowed,allowed);
        D_8016DAB4_16E6B4=s_root.task;
        ebi_root_update(s_root.task,s_root.object);
        anchor_minigame_ebi_hud_begin(hud.bytes);
        --EI(hud.bytes,0x80); /* Verified stock HUD decrements every callback. */
        anchor_minigame_ebi_hud_end();
        tick_children(1); /* Include ordinary wrapped authority scheduler slots. */
    }
    for(unsigned int i=0;i<task_count;++i)
        robots+=tasks[i].used && EH(tasks[i].task.bytes,0x5c)==0xfb;
    if(!allowed) {
        /* The old registry-C preflight produces all reported symptoms. */
        assert(root_calls==0 && ES(s_root.task,0xe0)==2);
        assert(EI(hud.bytes,0x80)==2970 && robots==0);
        fprintf(stderr,"cold solo blocked: ready=%d root_calls=%d clock=%d timer=%d robots=%u registryC=%u\n",
                state.ready,root_calls,ES(s_root.task,0xe0),EI(hud.bytes,0x80),robots,resources[0x0c]);
    }
    assert(allowed);
    assert(root_calls==40 && ES(s_root.task,0xe0)==42);
    assert(EI(hud.bytes,0x80)==2930 && robots==1);
    assert(resources[0x1cf] && resources[0x173] && !resources[0x0c]);
    assert(resources[0x197] && resources[0x159]);
    memset(&state,0,sizeof(state));assert(capture(&state) && state.ready && validate(&state));
    assert(state.actor_count==3 && apply(&state)); /* Cold apply preflight too. */
    printf("Ebi cold solo: clock=%d timer=%d robots=%u registryC=%u passed\n",
           ES(s_root.task,0xe0),EI(hud.bytes,0x80),robots,resources[0x0c]);
    cold_registry=0;
}
static void append_actor(AnchorMinigameState *s,int kind,int id)
{
    int *r=s->actors[s->actor_count];unsigned int i;
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
    memset(r,0,actor_words(kind)*sizeof(*r));s->actor_words[s->actor_count++]=actor_words(kind);
#else
    memset(r,0,ER_WORDS*sizeof(*r));s->actor_words[s->actor_count++]=ER_WORDS;
#endif
    r[ER_KIND]=kind;r[ER_ID]=id;r[ER_GENERATION]=1;
    for(i=0;i<3;++i) r[ER_SX+i]=float_bits(1.f);
    r[ER_Y]=float_bits(50.f);r[ER_AI]=kind==EBI_ROBOT?1:0;
    r[ER_LIFE]=(int)(1u<<24);r[ER_FLAGS]=kind==EBI_ROBOT?0x02ae87e1:0x20;
    r[ER_FLAGS2]=0x8000;r[ER_EXTRA]=1;
    if(kind==EBI_ROBOT) { r[ER_EXTRA]=0;r[ER_EXTRA+2]=50; }
    if(kind==EBI_SWEET) r[ER_LIFE]|=270;
    if(kind==EBI_BLAST) { r[ER_MODE]=4;r[ER_LIFE]|=30;r[ER_EXTRA+1]=90;r[ER_FLAGS]=0x004006e0; }
    s->controller[EC_SERIAL]=id;
}
static void test_complete_graph_and_late_join(void)
{
    AnchorMinigameState state,got;
    unsigned int i;
    unsigned char saved[48];unsigned int stats[12];
    EbiActor *robot,*blast;
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);append_actor(&state,EBI_BOMB,4);
    append_actor(&state,EBI_SWEET,5);append_actor(&state,EBI_BLAST,6);
    state.controller[EC_PHASE]=6;state.controller[EC_PHASE_TIME]=71;
    state.controller[EC_TIME]=1217;state.controller[EC_TIMER]=601;state.controller[EC_SWEETS]=4;
    memcpy(saved,backup,sizeof(saved));memcpy(stats,D_8015C5D8_15D1D8,sizeof(stats));
    assert(validate(&state));
    for(i=0;i<8 && !apply(&state);++i) {
        assert(EI(hud.bytes,0x80)==2970 && D_080032F4_6F3504==0);
        assert(EB(s_root.task,0xdc)==1);tick_initializers();
        memset(&got,0,sizeof(got));assert(capture(&got) && !got.ready);
    }
    assert(i<8 && apply(&state));assert(bomb_native==0);
    assert(!memcmp(saved,backup,sizeof(saved)) && !memcmp(stats,D_8015C5D8_15D1D8,sizeof(stats)));
    assert(EB(s_root.task,0xdc)==6 && ES(s_root.task,0xde)==71 && ES(s_root.task,0xe0)==1217);
    assert(EI(hud.bytes,0x80)==601 && D_080032F4_6F3504==4);
    robot=identity(EBI_ROBOT,3,1);blast=identity(EBI_BLAST,6,1);assert(robot && blast);
    assert(EB(blast->task,0x4c)==0 && EB(blast->task,0x6d)==1);
    state.actors[5][ER_EXTRA+1]=48;assert(apply(&state));
    assert(EW(blast->object,0x80)==48 && ES(blast->task,0xd2)==48);
    assert(EBI_PTR(blast->object,0x30)==(void *)(unsigned long)0xa0001280u);
    for(i=0;i<13;++i) {
        state.actors[2][ER_AI]=(int)i;assert(apply(&state));
        memset(&got,0,sizeof(got));assert(capture(&got));assert(got.actors[2][ER_AI]==(int)i);
    }
    assert(got.actor_count==6 && got.ready);
    /* Missing native code/resources must leave every existing scalar intact. */
    ++state.source_tick;state.controller[EC_TIMER]=99;resources[0x20]=0;assert(!apply(&state));
    assert(EI(hud.bytes,0x80)==601);resources[0x20]=1;
    resources[0x191]=0;assert(!apply(&state));assert(EI(hud.bytes,0x80)==601);resources[0x191]=1;
    resources[0x173]=0;assert(!apply(&state));assert(EI(hud.bytes,0x80)==601);
    memset(&got,0,sizeof(got));assert(capture(&got) && !got.ready);
    resources[0x173]=1;assert(apply(&state));assert(EI(hud.bytes,0x80)==99);
    ++state.source_tick;state.controller[EC_TIMER]=601;assert(apply(&state));
    state.actors[2][ER_CLIP]=6;assert(!apply(&state));assert(EI(hud.bytes,0x80)==601);
    state.actors[2][ER_CLIP]=5;assert(apply(&state));
    state.actors[2][ER_CLIP]=0;assert(apply(&state));
    state.actors[3][ER_CLIP]=1;assert(!apply(&state));assert(EI(hud.bytes,0x80)==601);
    state.actors[3][ER_CLIP]=0;
    state.actors[3][ER_X]=(int)0x7fc00000u;assert(!apply(&state));assert(EI(hud.bytes,0x80)==601);
    state.actors[3][ER_X]=float_bits(0.f);state.actors[3][ER_EXTRA]=8;
    assert(!validate(&state));assert(EI(hud.bytes,0x80)==601);
}
static void test_claims_hits_and_collision(void)
{
    AnchorMinigameState state,got;
    EbiActor *sweet,*robot;void *attacker;
    int request[8]={9,11,41,1,EBI_COLLECT,4,1,1};
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);append_actor(&state,EBI_SWEET,4);
    while(!apply(&state)) tick_initializers();
    sweet=identity(EBI_SWEET,4,1);robot=identity(EBI_ROBOT,3,1);assert(sweet && robot);
    EW(sweet->task,0x68)|=0x200;ebi_actor_update(sweet->task,sweet->object);
    assert(awards==0 && D_080032F4_6F3504==0 && s_input_count==1);
    memset(&got,0,sizeof(got));assert(capture(&got));assert(got.input_count==1 && got.inputs[0][1]==EBI_COLLECT);
    role(1,1,1);allowed=1;
    assert(input(request,7) && EB(sweet->task,0xd0)==1);
    request[2]=0;assert(input(request,8) && EB(sweet->task,0xd0)==1);request[2]=41;
    assert(input(request,8));assert(input(request,8));
    D_8016DAB4_16E6B4=sweet->task;ebi_actor_update(sweet->task,sweet->object);
    assert(awards==1 && D_080032F4_6F3504==1);assert(input(request,8));assert(awards==1);
    role(1,0,1);allowed=0;attacker=make_task(1,0);EB(attacker,0x4c)=0x22;
    EBI_PTR(robot->task,0x38)=attacker;EW(robot->task,0x68)|=0x80;
    anchor_minigame_ebi_damage(robot->task);
    assert(EB(robot->task,0x8d)==1 && !(EW(robot->task,0x68)&0x80) && deaths==0);
    request[3]=2;request[4]=EBI_HIT;request[5]=3;request[7]=8;
    EW(robot->task,0x68)&=~1u;EB(robot->task,0x8c)=0; /* Independent owner's contact state. */
    EBI_PTR(robot->task,0x38)=0;
    role(1,1,1);allowed=1;assert(input(request,8));
    D_8016DAB4_16E6B4=robot->task;ebi_actor_update(robot->task,robot->object);
    fixture_collision_slot(robot->task);assert(deaths==1);
    assert(input(request,8));assert(deaths==1);
    /* Native collision chain is retained while replica velocity/pose is held. */
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);
    state.actors[2][ER_X]=float_bits(17.f);state.actors[2][ER_VY]=float_bits(-3.f);
    while(!apply(&state)) tick_initializers();robot=identity(EBI_ROBOT,3,1);
    D_8016DAB4_16E6B4=robot->task;anchor_minigame_ebi_collision_begin();
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
    assert(EF(robot->task,0x7c)==-3.f && EF(robot->object,8)==17.f);
    EF(robot->object,8)=999.f;anchor_minigame_ebi_collision_end();
    assert(EF(robot->object,8)==999.f && EF(robot->task,0x7c)==-3.f);
#else
    assert(EF(robot->task,0x7c)==0.f && EF(robot->object,8)==17.f);
    EF(robot->object,8)=999.f;anchor_minigame_ebi_collision_end();
    assert(EF(robot->object,8)==17.f && EF(robot->task,0x7c)==-3.f);
#endif
}
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
static void natural_health_loss(int amount)
{
    anchor_minigame_ebi_health_begin((signed char)-amount);
    D_8015C5D8_15D1D8[3]-=(unsigned int)amount;
    anchor_minigame_ebi_health_end();
}
static void test_shared_health_and_receipts(void)
{
    AnchorMinigameState state,got;
    unsigned char saved[48];
    int request[8]={77,3,5,1,EBI_HEALTH_EVENT,1,1,1};
    fresh();memcpy(saved,backup,sizeof(saved));role(1,1,1);natural_health_loss(1);
    assert(s_health==2 && D_8015C5D8_15D1D8[3]==19 && s_input_count==0);
    assert(input(request,8) && s_health==1 && D_8015C5D8_15D1D8[3]==18);
    assert(!memcmp(saved,backup,sizeof(saved)));
    fresh();state_base(&state);natural_health_loss(1);natural_health_loss(1);
    assert(s_health==3 && s_input_count==2 && D_8015C5D8_15D1D8[3]==18);
    assert(s_inputs[0][0]==1 && s_inputs[1][0]==2);
    state.controller[EC_HEALTH]=1;state.local_ack=2;state.adopted_generation=1;state.full_tick=1;
    fixture_ack=1;assert(apply(&state));
    assert(D_8015C5D8_15D1D8[3]==18 && EI(hud.bytes,0x88)==1 && !s_result);
    memset(&got,0,sizeof(got));assert(capture(&got) && got.ready);
    assert(D_8015C5D8_15D1D8[3]==18 && got.controller[EC_HEALTH]==1 && got.input_count==0);
    assert(s_ack_fence==2); /* Immediate recapture sees the adopted receipt. */
    fresh();natural_health_loss(3);
    assert(D_8015C5D8_15D1D8[3]==17 && EB(s_root.task,0xdc)==1 && !s_result);
    state_base(&state);assert(state.ready && state.controller[EC_HEALTH]==3);
    state.controller[EC_HEALTH]=0;state.local_ack=1;assert(apply(&state));
    assert(s_result==2 && EB(s_root.task,0xdc)==8 && D_8015C5D8_15D1D8[3]==17);
    memset(&got,0,sizeof(got));assert(capture(&got) && got.ready && got.result==2);
    D_8015C5D8_15D1D8[3]=11;health_display(local_ack());
    assert(D_8015C5D8_15D1D8[3]==11); /* Result/restore stats are participant-local. */
    fresh();role(0,1,1);natural_health_loss(1);
    assert(s_health==2 && D_8015C5D8_15D1D8[3]==19 && !s_input_count && !s_result);
    fresh();natural_health_loss(1);s_ack_fence=7;s_health_unqueued=1;
    role(0,1,1);assert(!s_input_count && !s_ack_fence && !s_health_unqueued);
    assert(s_health==2 && D_8015C5D8_15D1D8[3]==19);
    role(1,1,1);memset(&got,0,sizeof(got));assert(capture(&got) && !got.input_count);
    assert(got.controller[EC_HEALTH]==2); /* Reconnect never replays prior-session health claims. */
    fresh();anchor_minigame_ebi_health_begin(-1);++EB(D_801FC604_5B8514,0x74);
    --D_8015C5D8_15D1D8[3];anchor_minigame_ebi_health_end();
    assert(!s_input_count && s_health==3); /* Recycled playable task is fenced. */
    puts("Ebi shared hearts, optimistic pending loss, atomic ACK adoption and native backup boundaries passed");
}
static void test_repeat_hits_and_follower_safety(void)
{
    AnchorMinigameState state,got;
    EbiActor *robot,*bomb,*sweet;
    void *attacker;
    int request[8]={77,3,5,1,EBI_HIT,3,1,1};
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);
    append_actor(&state,EBI_BOMB,4);append_actor(&state,EBI_SWEET,5);
    while(!apply(&state)) tick_initializers();
    robot=identity(EBI_ROBOT,3,1);bomb=identity(EBI_BOMB,4,1);sweet=identity(EBI_SWEET,5,1);
    attacker=make_task(1,0);EB(attacker,0x4c)=0x15;EBI_PTR(robot->task,0x38)=attacker;
    EW(robot->task,0x68)|=0x80;anchor_minigame_ebi_damage(robot->task);
    assert(s_input_count==1 && s_inputs[0][0]==1 && robot->hit_cooldown==60);
    EW(robot->task,0x68)|=0x80;anchor_minigame_ebi_damage(robot->task);assert(s_input_count==1);
    fixture_ack=1;memset(&got,0,sizeof(got));assert(capture(&got) && !got.input_count);
    robot->hit_cooldown=0;EW(robot->task,0x68)&=~1u;EB(robot->task,0x8c)=0;
    EW(robot->task,0x68)|=0x80;anchor_minigame_ebi_damage(robot->task);
    assert(s_input_count==1 && s_inputs[0][0]==2); /* Same living actor, distinct later contact. */
    for(unsigned int i=0;i<2;++i) {
        EbiActor *a=i?bomb:sweet;
        EW(a->task,0x68)|=0x40080u;EBI_PTR(a->task,0x38)=attacker;
        D_8016DAB4_16E6B4=a->task;fixture_collision_slot(a->task);
        assert(live(a) && deaths==0 && awards==0 && !(EW(a->task,0x68)&0x40080u));
    }
    role(1,1,1);allowed=1;EW(robot->task,0x68)&=~1u;EB(robot->task,0x8d)=3;
    EBI_PTR(robot->task,0x38)=0;
    assert(input(request,8) && robot->pending_damage==1);
    ++request[0];++request[3];assert(input(request,8) && robot->pending_damage==1);
    D_8016DAB4_16E6B4=robot->task;ebi_actor_update(robot->task,robot->object);
    assert(EW(robot->task,0x68)&0x40000u);fixture_collision_slot(robot->task);
    assert(EB(robot->task,0x8d)==2 && deaths==0 && (EW(robot->task,0x68)&1u));
    assert(input(request,8) && !robot->pending_damage); /* Native recovery rejects another simultaneous peer hit. */
    puts("Ebi distinct attack sequences, ACK retries, native recovery and follower no-death gates passed");
}
static void test_predictive_movement_and_freshness(void)
{
    AnchorMinigameState state;
    EbiActor *robot,*mechanism;
    float x,frame;
    unsigned int i;
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);
    state.adopted_generation=1;state.full_tick=1;
    state.actors[2][ER_AI]=8;state.actors[2][ER_VX]=float_bits(1.5f);
    state.actors[2][ER_X]=float_bits(10.f);
    while(!apply(&state)) tick_initializers();robot=identity(EBI_ROBOT,3,1);
    frame=EF(robot->object,0x28);
    for(i=0;i<6;++i) {
        D_8016DAB4_16E6B4=robot->task;ebi_actor_update(robot->task,robot->object);
        fixture_collision_slot(robot->task);
        assert(EF(robot->object,8)==10.f+3.f*(float)(i+1));
    }
    assert(EF(robot->object,0x28)==frame+1.5f); /* Native animation advances exactly once per post slot. */
    x=EF(robot->object,8);assert(apply(&state));
    assert(EF(robot->object,8)==x && robot->prediction_age==6); /* Unchanged action graph does not rewind. */
    state.actors[2][ER_X]=float_bits(x+8.f);state.full_tick=2;assert(apply(&state));
    assert(EF(robot->object,8)==x && robot->correction_ticks==8);
    for(i=0;i<8;++i) {
        D_8016DAB4_16E6B4=robot->task;ebi_actor_update(robot->task,robot->object);fixture_collision_slot(robot->task);
    }
    assert(fabsf(EF(robot->object,8)-(x+32.f))<0.001f && !robot->correction_ticks);
    robot->prediction_age=60;x=EF(robot->object,8);frame=EF(robot->object,0x28);
    ebi_actor_update(robot->task,robot->object);fixture_collision_slot(robot->task);
    assert(EF(robot->object,8)==x && EF(robot->object,0x28)==frame);
    ++state.full_tick;assert(apply(&state));assert(robot->prediction_age==0 && EF(robot->object,8)==x);
    state.actors[2][ER_X]=float_bits(500.f);state.adopted_generation=2;assert(apply(&state));
    assert(EF(robot->object,8)==500.f && !robot->correction_ticks);
    state.actors[2][ER_ORBIT_X]=float_bits(200000.f);assert(!validate(&state));
    state.actors[2][ER_ORBIT_X]=0;state.actors[2][ER_AI]=10;
    state.actors[2][ER_RADII]=(int)(10u|(20u<<8)|(4u<<16));
    state.actors[2][ER_ORBIT_Z]=float_bits(20.f);assert(apply(&state));
    D_8016DAB4_16E6B4=robot->task;x=EF(robot->object,8);
    ebi_actor_update(robot->task,robot->object);fixture_collision_slot(robot->task);
    assert(EH(robot->task,0xd0)==4 && fabsf(EF(robot->object,8)-x)>0.01f && deaths==0);
    mechanism=find(EBI_PTR(s_root.task,0xd0));assert(mechanism);
    state.actors[0][ER_EXTRA]=0x101;state.full_tick=10;assert(apply(&state));
    for(i=0;i<120;++i) {
        D_8016DAB4_16E6B4=mechanism->task;ebi_actor_update(mechanism->task,mechanism->object);
        if((i+1)%30==0) { ++state.full_tick;assert(apply(&state)); }
        assert(mechanism->prediction_age<=30);
    }
    /* Fresh full checkpoints preserve cyclic roll but extend freshness. */
    D_8016DAB4_16E6B4=mechanism->task;x=(float)EH(mechanism->object,0x18);
    ebi_actor_update(mechanism->task,mechanism->object);
    assert((float)EH(mechanism->object,0x18)!=x);
    puts("Ebi native2*v integration, frame continuity, bounded correction, full freshness and orbit seed prediction passed");
}
static void test_predicted_blast_lifetime(void)
{
    AnchorMinigameState state,got;
    EbiActor *blast;
    unsigned int i,old_allocations;
    fresh();state_base(&state);append_actor(&state,EBI_BLAST,3);
    while(!apply(&state)) tick_initializers();blast=identity(EBI_BLAST,3,1);assert(blast);
    old_allocations=(unsigned int)allocations;
    for(i=0;i<60;++i) {
        D_8016DAB4_16E6B4=blast->task;ebi_actor_update(blast->task,blast->object);
        fixture_collision_slot(blast->task);
        memset(&got,0,sizeof(got));assert(capture(&got) && got.ready && validate(&got));
        assert(ES(blast->task,0xd2)>=28 && live(blast) && !deaths && !awards);
    }
    assert(ES(blast->task,0xd2)==30 && (unsigned int)allocations==old_allocations);
    puts("Ebi predicted blast retains valid expired visual through60 frames without retirement or new actors passed");
}
static void *fixture_observed_target;
static int fixture_delete_target;
static unsigned int fixture_finalizer_effects;
static void fixture_unsafe_finalizers(void *task)
{
    void *object=EBI_PTR(task,0x18);
    anchor_minigame_ebi_fall_begin(task);
    if((EW(task,0x60)&0x40000000u) && EF(task,0x7c)> -3.f) {
        ++fixture_finalizer_effects;EW(task,0x68)|=2u;
    }
    if((EW(task,0x60)&0x400u) && EF(object,0xc)< -1000.f) EW(task,0x68)=2u;
    anchor_minigame_ebi_fall_end();
    anchor_minigame_ebi_carry_begin(task);
    if((EW(task,0x64)&0x4000u) && EBI_PTR(D_801FC604_5B8514,0xa0)==object)
        EF(D_801FC604_5B8514,0xc0)=321.f;
    anchor_minigame_ebi_carry_end();
    anchor_minigame_ebi_overlap_begin(task);
    if(EW(task,0x60)&0x10000000u) EF(object,8)=99.f;
    anchor_minigame_ebi_overlap_end();
    anchor_minigame_ebi_descriptor_begin(task);
    if((EW(task,0x68)&8u) && EBI_PTR(task,0x70)) {
        EB(EBI_PTR(task,0x70),0x10)=0;EW(task,0x68)=2u;
    }
    anchor_minigame_ebi_descriptor_end();
}
static void fixture_target_callback(void *task,void *object)
{
    fixture_observed_target=EBI_PTR(task,0x84);(void)object;
    EBI_PTR(task,0x84)=EBI_PTR(D_801FC604_5B8514,0x18);anchor_minigame_ebi_target_primary(task);
    assert(EBI_PTR(task,0x84)==fixture_observed_target);
    EBI_PTR(task,0x84)=0;anchor_minigame_ebi_target_secondary(task);
    assert(EBI_PTR(task,0x84)==fixture_observed_target);
    if(fixture_delete_target) func_80034EF8_35AF8(task);
}
static void test_scoped_target_and_finalizer_masks(void)
{
    AnchorMinigameState state;
    Store descriptor;
    EbiActor *robot;
    void *task,*original;
    unsigned int flags;
    fresh();state_base(&state);append_actor(&state,EBI_ROBOT,3);
    while(!apply(&state)) tick_initializers();robot=identity(EBI_ROBOT,3,1);task=robot->task;
    original=EBI_PTR(D_801FC604_5B8514,0x18);EBI_PTR(task,0x84)=original;
    fixture_targets[0]=(AnchorBossTarget){1,100.f,0.f,0.f};
    /* The upstream native-only roster admits miniature/hiding Ebi without
     * a form exclusion. This adapter receives its cid/XYZ, never a form flag. */
    fixture_targets[1]=(AnchorBossTarget){77,10.f,900.f,0.f};fixture_target_count=2;
    robot->native=fixture_target_callback;fixture_delete_target=0;allowed=1;role(1,1,1);
    D_8016DAB4_16E6B4=task;ebi_actor_update(task,robot->object);
    assert(robot->target_cid==77 && fixture_observed_target==robot->target_view);
    assert(EF(fixture_observed_target,8)==10.f && EBI_PTR(task,0x84)==original && !s_target_actor);
    assert(EF(original,8)==0.f); /* Actual playable object never moves for remote selection. */
    fixture_ineligible_cid=77;role(1,1,1);ebi_actor_update(task,robot->object);
    assert(robot->target_cid==1 && EBI_PTR(task,0x84)==original);
    fixture_target_count=0;role(1,1,1);ebi_actor_update(task,robot->object);
    assert(robot->target_cid==0 && EF(fixture_observed_target,8)==EF(robot->object,8));
    allowed=0;role(1,0,1);flags=EW(task,0x60)|0x50000400u;EW(task,0x60)=flags;
    anchor_minigame_ebi_fall_begin(task);assert(!(EW(task,0x60)&0x40000400u));
    EW(task,0x60)^=0x40u;anchor_minigame_ebi_fall_end();assert(EW(task,0x60)==(flags^0x40u));
    anchor_minigame_ebi_overlap_begin(task);assert(!(EW(task,0x60)&0x10000000u));
    anchor_minigame_ebi_overlap_end();assert(EW(task,0x60)==(flags^0x40u));
    EW(task,0x64)|=0x4000u;anchor_minigame_ebi_carry_begin(task);
    assert(!(EW(task,0x64)&0x4000u));anchor_minigame_ebi_carry_end();assert(EW(task,0x64)&0x4000u);
    EF(robot->object,0xc)= -1200.f;EF(task,0x7c)=0.f;EF(robot->object,8)=23.f;
    EBI_PTR(D_801FC604_5B8514,0xa0)=robot->object;EF(D_801FC604_5B8514,0xc0)=123.f;
    fixture_finalizer_effects=0;robot->prediction_age=61;role(1,0,0);
    memset(&descriptor,0,sizeof(descriptor));EB(descriptor.bytes,0x10)=1;
    EBI_PTR(task,0x70)=descriptor.bytes;EW(task,0x68)|=8u;
    fixture_unsafe_finalizers(task);
    assert(!fixture_finalizer_effects && !(EW(task,0x68)&2u) && EF(robot->object,8)==23.f);
    assert(EF(D_801FC604_5B8514,0xc0)==123.f && (EW(task,0x60)&0x40000400u)==(flags&0x40000400u));
    assert(EB(descriptor.bytes,0x10)==1 && (EW(task,0x68)&8u) && EBI_PTR(task,0x70)==descriptor.bytes);
    role(1,0,1);fixture_unsafe_finalizers(task);
    assert(!fixture_finalizer_effects && !(EW(task,0x68)&2u));
    anchor_minigame_ebi_fall_begin(task);++EB(task,0x74);anchor_minigame_ebi_fall_end();
    assert(!(EW(task,0x60)&0x40000400u));--EB(task,0x74);
    allowed=1;role(1,1,1);fixture_delete_target=1;ebi_actor_update(task,robot->object);
    assert(!robot->task && !s_target_actor && !task_for(task)->used);
    puts("Ebi nearest eligible remote targeting, internal target resets, empty roster and guarded mask restoration passed");
}
#endif
static void fixture_remote_drop_setup(void)
{
    fresh();allowed=1;
    EF(D_801FC60C_5B851C,8)=80.f;EF(D_801FC60C_5B851C,0xc)= -60.f;
    EF(D_801FC60C_5B851C,0x10)= -49.f;
    fixture_targets[0]=(AnchorBossTarget){1,0.f,0.f,0.f}; /* Deliberately stale local model position. */
    fixture_targets[1]=(AnchorBossTarget){7,23.f,0.f,17.f};fixture_target_count=2;
    EB(s_root.task,0xdc)=4;ES(s_root.task,0xde)=122;ES(s_root.task,0xe0)=3;
    role(1,1,1);D_8016DAB4_16E6B4=s_root.task;
}
static void test_remote_drop_detection(void)
{
    unsigned char saved[48];unsigned int stats[12];
    void *host;
    unsigned int i;
    fixture_remote_drop_setup();host=D_801FC60C_5B851C;
    memcpy(saved,backup,sizeof(saved));memcpy(stats,D_8015C5D8_15D1D8,sizeof(stats));
    ebi_root_update(s_root.task,s_root.object);
    fprintf(stderr,"Ebi tower detector: phase=%u DE=%d warnings=%u retractions=%u nativecalls=%d\n",
            EB(s_root.task,0xdc),ES(s_root.task,0xde),fixture_drop_warnings,fixture_drop_retractions,root_calls);
    assert(EB(s_root.task,0xdc)==6 && ES(s_root.task,0xde)==1 && s_root.clip==2);
    assert(!fixture_drop_warnings && !fixture_drop_retractions && root_calls==1);
    assert(D_801FC60C_5B851C==host && EF(host,8)==80.f && EF(host,0xc)== -60.f && EF(host,0x10)== -49.f);
    assert(fixture_root_seen_host[0]==80.f && fixture_root_seen_host[1]== -60.f);
    assert(!memcmp(saved,backup,sizeof(saved)) && !memcmp(stats,D_8015C5D8_15D1D8,sizeof(stats)));
    fixture_remote_drop_setup();fixture_targets[1]=(AnchorBossTarget){7,49.f,-60.f,48.f};role(1,1,1);
    ebi_root_update(s_root.task,s_root.object);
    assert(EB(s_root.task,0xdc)==5 && fixture_drop_warnings==1 && fixture_drop_retractions==1);
    fixture_remote_drop_setup();fixture_target_count=1;ES(s_root.task,0xde)=2;role(1,1,1);
    ebi_root_update(s_root.task,s_root.object);
    assert(EB(s_root.task,0xdc)==4 && ES(s_root.task,0xde)==3 && fixture_drop_warnings==1);
    /* Strict native sqrt/height boundaries at the second pad. */
    for(i=0;i<4;++i) {
        fixture_remote_drop_setup();ES(s_root.task,0xde)=2;
        fixture_targets[1]=(AnchorBossTarget){7,i<2?63.f:49.f,i==2?-55.f:-60.f,48.f};
        if(i==1) fixture_targets[1].x=62.999f;
        if(i==3) fixture_targets[1].y=-55.001f;
        role(1,1,1);ebi_root_update(s_root.task,s_root.object);
        assert(EB(s_root.task,0xdc)==((i==0 || i==2)?6:4));
    }
    fixture_remote_drop_setup();ES(s_root.task,0xe0)=5;fixture_sweet_failure=1;pool_limit=task_count;
    ebi_root_update(s_root.task,s_root.object);
    assert(EB(s_root.task,0xdc)==6 && ES(s_root.task,0xde)==0 && ES(s_root.task,0xe0)==5);
    assert(!fixture_drop_warnings && !fixture_drop_retractions && root_calls==1);
    for(i=7;i<=9;++i) {
        fixture_remote_drop_setup();fixture_root_terminal=(int)i;
        ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==i && s_root.clip!=2);
    }
    fixture_remote_drop_setup();fixture_root_ineligible_on_return=7;
    ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==4 && s_root.clip!=2);
    fixture_remote_drop_setup();fixture_ineligible_cid=7;
    ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==5 && fixture_drop_retractions==1);
    fixture_remote_drop_setup();fixture_targets[1].x=bits_float(0x7fc00000);role(1,1,1);
    ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==5 && fixture_drop_retractions==1);
    fixture_remote_drop_setup();role(1,0,1);allowed=0;
    ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==4 && !root_calls);
    fixture_remote_drop_setup();role(0,1,1);
    ebi_root_update(s_root.task,s_root.object);assert(EB(s_root.task,0xdc)==5 && fixture_drop_retractions==1);
    puts("Ebi remote-only exposure, native strict thresholds, epilogue failure, terminal and authority gates passed");
}
static void test_remote_drop_positions(void)
{
    EbiActor *old;
    void *old_task,*bomb;
    unsigned int i,old_allocations;
    fixture_remote_drop_setup();EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;
    old_task=func_80217360_5D2830(s_root.task,0x1ac,6);old=raw_find(old_task);assert(old && !old->initialized);
    EF(old->object,8)=77.f;EF(old->object,0x10)=66.f;
    fixture_targets[1]=(AnchorBossTarget){7,23.f,0.f,17.f};
    fixture_targets[2]=(AnchorBossTarget){9,-200.f,0.f,41.f};fixture_target_count=3;role(1,1,1);
    old_allocations=(unsigned int)allocations;ebi_root_update(s_root.task,s_root.object);
    bomb=fixture_last_bomb;assert(bomb && root_calls==1 && fixture_drop_rng==2 && fixture_bomb_requests==1);
    assert(EF(EBI_PTR(bomb,0x18),8)==26.f && EF(EBI_PTR(bomb,0x18),0x10)==13.f);
    assert(EF(EBI_PTR(bomb,0x18),0xc)==50.f && (unsigned int)allocations==old_allocations+1);
    assert(EF(old->object,8)==77.f && EF(old->object,0x10)==66.f);
    /* A born bomb makes the coordinator temporarily lose ready ownership. */
    role(1,0,0);role(1,1,1);ES(s_root.task,0xde)=21;
    ebi_root_update(s_root.task,s_root.object);bomb=fixture_last_bomb;
    assert(fabsf(EF(EBI_PTR(bomb,0x18),8)-(fixture_scatter_x(-200.f)+3.f))<0.0001f);
    assert(EF(EBI_PTR(bomb,0x18),0x10)==37.f && fixture_drop_rng==4 && fixture_bomb_requests==2);
    ES(s_root.task,0xde)=41;ebi_root_update(s_root.task,s_root.object);bomb=fixture_last_bomb;
    assert(EF(EBI_PTR(bomb,0x18),8)==26.f && fixture_drop_rng==6);
    ES(s_root.task,0xde)=121;ebi_root_update(s_root.task,s_root.object);bomb=fixture_last_bomb;
    assert(EB(s_root.task,0xdc)==5 && fixture_drop_retractions==1);
    assert(EF(EBI_PTR(bomb,0x18),8)== -197.f && fixture_drop_rng==8);
    /* A failed native allocation consumes neither native RNG nor rotation. */
    fixture_remote_drop_setup();EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;pool_limit=task_count;
    ebi_root_update(s_root.task,s_root.object);assert(!fixture_last_bomb && !fixture_drop_rng);
    pool_limit=0;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==26.f && fixture_drop_rng==2);
    fixture_remote_drop_setup();fixture_targets[1]=(AnchorBossTarget){7,80.f,-60.f,-49.f};
    fixture_targets[2]=(AnchorBossTarget){9,63.f,0.f,4.f};fixture_target_count=3;role(1,1,1);
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==66.f);
    fixture_remote_drop_setup();fixture_targets[1]=(AnchorBossTarget){7,199999.f,0.f,0.f};
    fixture_targets[2]=(AnchorBossTarget){9,27.f,0.f,4.f};fixture_target_count=3;role(1,1,1);
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==30.f);
    fixture_remote_drop_setup();fixture_targets[1].x=bits_float(0x7fc00000);
    fixture_targets[2]=(AnchorBossTarget){9,200.f,0.f,4.f};fixture_target_count=3;role(1,1,1);
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=21;ebi_root_update(s_root.task,s_root.object);
    assert(fabsf(EF(EBI_PTR(fixture_last_bomb,0x18),8)-(fixture_scatter_x(200.f)+3.f))<0.0001f);
    fixture_remote_drop_setup();fixture_root_ineligible_on_return=7;
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==83.f);
    /* No exposed target preserves the native drop; local centres use the actual player. */
    fixture_remote_drop_setup();fixture_targets[1]=(AnchorBossTarget){7,80.f,-60.f,-49.f};role(1,1,1);
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==83.f && fixture_drop_rng==2);
    fixture_remote_drop_setup();fixture_target_count=1;EF(D_801FC60C_5B851C,8)=19.f;
    EF(D_801FC60C_5B851C,0xc)=0.f;EF(D_801FC60C_5B851C,0x10)=31.f;role(1,1,1);
    EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;ebi_root_update(s_root.task,s_root.object);
    assert(EF(EBI_PTR(fixture_last_bomb,0x18),8)==22.f && EF(EBI_PTR(fixture_last_bomb,0x18),0x10)==27.f);
    for(i=1;i<=14;++i) {
        fixture_remote_drop_setup();EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;
        fixture_corrupt_bomb=(int)i;ebi_root_update(s_root.task,s_root.object);
        if(i!=4) assert(EF(task_for(fixture_last_bomb)->object.bytes,8)==83.f);
#ifdef ANCHOR_EBI_REMOTE_DETECTOR
        assert(!s_drop_last_cid && !s_drop_scope.root && !s_drop_scope.bomb);
#endif
    }
#ifdef ANCHOR_EBI_REMOTE_DETECTOR
    fixture_remote_drop_setup();EB(s_root.task,0xdc)=6;ES(s_root.task,0xde)=1;
    ebi_root_update(s_root.task,s_root.object);assert(s_drop_last_cid==7);
    role(0,1,1);assert(!s_drop_last_cid && !s_drop_scope.root && !s_drop_scope.bomb);
#endif
    puts("Ebi scoped post-native bomb placement, round-robin, clamps, native counts and lifecycle exclusions passed");
}
static void test_timer_results_and_return(void)
{
    AnchorMinigameState state;
    fresh();role(1,0,0);anchor_minigame_ebi_hud_begin(hud.bytes);EI(hud.bytes,0x80)--;
    anchor_minigame_ebi_hud_end();assert(EI(hud.bytes,0x80)==2970);
    allowed=1;anchor_minigame_ebi_hud_begin(hud.bytes);EI(hud.bytes,0x80)--;
    anchor_minigame_ebi_hud_end();assert(EI(hud.bytes,0x80)==2969);allowed=0;
    D_8016DAB4_16E6B4=s_root.task;ebi_root_update(s_root.task,s_root.object);
    assert(root_calls==0 && camera_calls>0 && camera_commits==1);
    assert(!memcmp(camera_destination.bytes,camera_store.bytes+0x6c,0x60));
    D_801FC628_5B8538=0;ebi_root_update(s_root.task,s_root.object);
    assert(camera_commits==1);
    state_base(&state);state.controller[EC_PHASE]=9;state.controller[EC_PHASE_TIME]=10;
    state.controller[EC_RESULT]=1;assert(apply(&state));
    assert(EB(s_root.task,0xdc)==7 && ES(s_root.task,0xde)==0 && s_result==1);
    EB(s_root.task,0xdc)=9;ES(s_root.task,0xde)=9;assert(!return_pending());
    ES(s_root.task,0xde)=10;assert(return_pending());D_8016DAB4_16E6B4=D_801FC604_5B8514;
    assert(!return_pending());D_8016DAB4_16E6B4=s_root.task;s_result=0;assert(!return_pending());
    fresh();D_8015C5D8_15D1D8[3]=17;
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
    s_health=0;
#endif
    ebi_root_update(s_root.task,s_root.object);
    assert(s_result==2 && EB(s_root.task,0xdc)==8);
    state_base(&state);state.controller[EC_PHASE]=1;
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
    assert(state.ready && apply(&state));assert(EB(s_root.task,0xdc)==8);
#else
    assert(!state.ready && !apply(&state));assert(EB(s_root.task,0xdc)==8);
#endif
    ++visit;assert(!root_ready() && !return_pending());
}
static int fixture_hud_slot(void)
{
    int drawn;
    anchor_minigame_ebi_hud_begin(hud.bytes);
    drawn=EI(hud.bytes,0x80)/30;
    if(--EI(hud.bytes,0x80)<0) EI(hud.bytes,0x80)=0;
    anchor_minigame_ebi_hud_end();return drawn;
}
static void fixture_blast_pre(void *t)
{
    D_8016DAB4_16E6B4=t;
    EB(t,0x30)=(unsigned char)((EB(t,0x30)&~2u)|((EW(t,0x60)&0x40u)?2u:0u));
#ifdef ANCHOR_EBI_TIMER_BLAST
    anchor_minigame_ebi_blast_contact();
#endif
}
static int fixture_blast_player_contact(void *t,int *recovery)
{
    void *o=EBI_PTR(t,0x18),*player=D_801FC60C_5B851C;
    float dx=EF(o,8)-EF(player,8),dy=EF(o,0xc)-EF(player,0xc),dz=EF(o,0x10)-EF(player,0x10);
    float radius=(float)ES(t,0x4e)*EF(o,0x1c);
    if(*recovery || !(EB(t,0x30)&2u) || EB(t,0x4c)!=1 || EW(t,0x48)!=0xffffffffu ||
       dx*dx+dy*dy+dz*dz>radius*radius) return 0;
    natural_health_loss(EB(t,0x6d)?EB(t,0x6d):1);*recovery=60;return 1;
}
static void test_timer_source_ticks(void)
{
    AnchorMinigameState state;
    unsigned int i;
    int before;
    fresh();state_base(&state);state.adopted_generation=1;state.full_tick=1;state.source_tick=1;
    state.controller[EC_TIMER]=1500;assert(apply(&state));
    for(i=1;i<=120;++i) {
        D_8016DAB4_16E6B4=s_root.task;ebi_root_update(s_root.task,s_root.object);
        fixture_hud_slot();
        if(i%10==0) { ++state.source_tick;state.controller[EC_TIMER]=1500-(int)i;assert(apply(&state)); }
        assert(EI(hud.bytes,0x80)==1500-(int)i);
        if(i%7==0) { state.local_ack=i;assert(apply(&state));assert(EI(hud.bytes,0x80)==1500-(int)i); }
    }
    fprintf(stderr,"Ebi timer ACTION source: fullTick=%u raw=%d after120 native ticks\n",state.full_tick,EI(hud.bytes,0x80));
    /* Receipt and readiness retries cannot grant another prediction horizon. */
    for(i=0;i<60;++i) { fixture_hud_slot();assert(apply(&state)); }
    before=EI(hud.bytes,0x80);fixture_hud_slot();assert(EI(hud.bytes,0x80)==before);
    role(1,0,0);assert(apply(&state));role(1,0,1);fixture_hud_slot();assert(EI(hud.bytes,0x80)==before);
    ++state.source_tick;state.controller[EC_TIMER]=1200;assert(apply(&state));
    assert(fixture_hud_slot()==40 && EI(hud.bytes,0x80)==1199);
    state.adopted_generation=2;state.source_tick=0;state.controller[EC_TIMER]=1800;assert(apply(&state));
    assert(fixture_hud_slot()==60 && EI(hud.bytes,0x80)==1799);
    allowed=1;role(0,1,1);assert(fixture_hud_slot()==59 && EI(hud.bytes,0x80)==1798);
    allowed=0;role(1,0,1);assert(apply(&state));assert(EI(hud.bytes,0x80)==1800);
    fresh();role(1,1,1);allowed=1;EI(hud.bytes,0x80)=30;
    assert(fixture_hud_slot()==1 && EI(hud.bytes,0x80)==29);
    D_8016DAB4_16E6B4=s_root.task;ebi_root_update(s_root.task,s_root.object);
    assert(EB(s_root.task,0xdc)==8 && s_result==2); /* Native getter returns seconds, not raw ticks. */
    for(i=0;i<30;++i) fixture_hud_slot();assert(EI(hud.bytes,0x80)==0);
    puts("Ebi timer independent ACTION stamp, duplicate ACK/readiness, native cadence and raw29 expiry passed");
}
static void test_direct_blast_birth_and_contacts(void)
{
    AnchorMinigameState state,got;
    EbiActor *bomb,*visual,*blast;
    void *t;
    unsigned int i,allocation_count;
    int recovery=0;
    fresh();role(1,1,1);allowed=1;
    t=func_80217360_5D2830(s_root.task,0x1ac,6);tick_initializers();bomb=find(t);assert(bomb);
    EF(bomb->object,8)=13.f;EF(bomb->object,0xc)=0.f;EF(bomb->object,0x10)=19.f;
    fixture_bomb_detonate=1;D_8016DAB4_16E6B4=t;ebi_actor_update(t,bomb->object);
    assert(!bomb->task && !task_for(t)->used);
    visual=raw_find(fixture_blast_children[0]);blast=raw_find(fixture_blast_children[1]);
    fprintf(stderr,"Ebi direct explosion: recorded=%d/%d nativeinit=%u\n",visual!=0,blast!=0,fixture_blast_initializations);
    assert(visual && blast && !visual->initialized && !blast->initialized && !fixture_blast_initializations);
    for(i=0;i<2;++i) {
        void *child=fixture_blast_children[i];
        assert(EH(child,0x28)==0x2a && EH(child,0x5c)==0x1ac && EH(child,0x5e)==0x1ac);
        assert((unsigned long)EBI_AI(child)==0x08000b58u && EF(EBI_PTR(child,0x18),8)==13.f);
    }
    memset(&state,0,sizeof(state));assert(capture(&state) && !state.ready);
    resources[0x191]=0;tick_initializers();
    assert(resources[0x191] && fixture_blast_initializations==2 && fixture_blast_effects==1);
    memset(&state,0,sizeof(state));assert(capture(&state) && state.ready && validate(&state) && state.actor_count==4);
    assert(EH(blast->task,0x5e)==0x7e && EB(blast->task,0x4c)==0 && EH(blast->task,0x8a)==30);
    /* Reconstruct both direct children from the exact captured authority graph. */
    fresh();EH(s_root.task,0x5c)=s_root.entity=0x1a9;state.adopted_generation=1;state.source_tick=1;state.full_tick=1;
    for(i=0;i<8 && !apply(&state);++i) tick_initializers();assert(i<8);
    visual=identity(EBI_BLAST,(unsigned int)state.actors[2][ER_ID],1);
    blast=identity(EBI_BLAST,(unsigned int)state.actors[3][ER_ID],1);assert(visual && blast);
    assert(visual->proxy && blast->proxy && fixture_blast_initializations==2 && fixture_blast_effects==1);
    allocation_count=(unsigned int)allocations;
    assert(EB(blast->task,0x4c)==0 && EH(blast->task,0x8a)==30);
    EF(D_801FC60C_5B851C,8)=13.f;EF(D_801FC60C_5B851C,0xc)=0.f;EF(D_801FC60C_5B851C,0x10)=19.f;
    fixture_blast_pre(blast->task);assert(!fixture_blast_player_contact(blast->task,&recovery));
    ebi_actor_update(blast->task,blast->object);
    assert(EB(blast->task,0x4c)==1 && EB(blast->task,0x6d)==1 && ES(blast->task,0x4e)==120 && EW(blast->task,0x48)==0xffffffffu);
    assert(fixture_blast_player_contact(blast->task,&recovery) && D_8015C5D8_15D1D8[3]==19 && s_input_count==1);
    assert(!fixture_blast_player_contact(blast->task,&recovery));
    memset(&got,0,sizeof(got));assert(capture(&got) && got.input_count==1);assert(capture(&got) && s_input_count==1);
    state.controller[EC_HEALTH]=2;state.local_ack=1;assert(apply(&state));
    assert(D_8015C5D8_15D1D8[3]==19 && !s_result && fixture_blast_initializations==2 && fixture_blast_effects==1);
    assert((unsigned int)allocations==allocation_count);
    memset(&got,0,sizeof(got));assert(capture(&got) && !got.input_count);
    /* Cached same-stamp apply must not reset the predicted lifetime. */
    for(i=1;i<30;++i) { fixture_blast_pre(blast->task);ebi_actor_update(blast->task,blast->object); }
    assert(EH(blast->task,0x8a)==0 && (EB(blast->task,0x30)&2u)); /* Last1→0 stays live. */
    recovery=0;assert(fixture_blast_player_contact(blast->task,&recovery));
    fixture_blast_pre(blast->task);ebi_actor_update(blast->task,blast->object);
    assert(!(EB(blast->task,0x30)&2u) && live(blast));
    assert(apply(&state));fixture_blast_pre(blast->task);recovery=0;
    assert(!fixture_blast_player_contact(blast->task,&recovery));
    /* A fresh authoritative live row can reconcile a midblast join. */
    ++state.source_tick;state.actors[3][ER_LIFE]=(state.actors[3][ER_LIFE]&~0xffff)|12;
    state.actors[3][ER_EXTRA+1]=54;assert(apply(&state));fixture_blast_pre(blast->task);
    assert(EB(blast->task,0x30)&2u);
    role(1,0,0);fixture_blast_pre(blast->task);assert(!(EB(blast->task,0x30)&2u));
    assert(apply(&state));role(1,0,1);fixture_blast_pre(blast->task);assert(EB(blast->task,0x30)&2u);
    blast->prediction_age=61;fixture_blast_pre(blast->task);assert(!(EB(blast->task,0x30)&2u));
    blast->prediction_age=0;EB(s_root.task,0xdc)=8;fixture_blast_pre(blast->task);assert(!(EB(blast->task,0x30)&2u));
    EB(s_root.task,0xdc)=1;state.actor_count=2;assert(apply(&state));assert(!blast->task && !visual->task);
    fresh();t=func_8021DDE8_5D92B8(s_root.task,func_08000B58_6F0D68,1,0,0,0,0);
    assert(t && !raw_find(t)); /* Unscoped root/direct constructor never joins the graph. */
    /* Confirmed-but-uninitialized records cannot mutate a recycled task. */
    fresh();state_base(&state);append_actor(&state,EBI_BLAST,3);
    blast=construct(state.actors[2]);assert(blast && !blast->initialized);
    EW(blast->task,0x60)|=0x40u;EB(blast->task,0x30)=2u;
    ++EB(blast->task,0x74);fixture_blast_pre(blast->task);assert(EB(blast->task,0x30)&2u);
    --EB(blast->task,0x74);++EH(blast->task,0x5c);fixture_blast_pre(blast->task);assert(EB(blast->task,0x30)&2u);
    --EH(blast->task,0x5c);fixture_blast_pre(blast->task);assert(!(EB(blast->task,0x30)&2u));
    /* Pending direct constructors reject every incomplete or wrong provenance. */
    for(i=0;i<10;++i) {
        void *child;
        fresh();role(1,1,1);allowed=1;
        t=func_80217360_5D2830(s_root.task,0x1ac,6);tick_initializers();bomb=find(t);assert(bomb);
        D_8016DAB4_16E6B4=t;
        child=func_8021DDE8_5D92B8(t,func_08000B58_6F0D68,1,0,0,0,0);
        assert(child && !raw_find(child));
        EH(child,0x28)=0x2a;EBI_PTR(child,0x2c)=func_800141C4_14DC4(0x2a);EW(child,0xe8)=4;
        if(i==0) EH(child,0x28)=0xfffe;
        if(i==1) EBI_PTR(child,0x2c)=0;
        if(i==2) EW(child,0xe8)=5;
        if(i==3) ++EB(child,0x74);
        if(i==4) ++EH(child,0x5e);
        if(i==5) EBI_PTR(child,0x18)=hud.bytes;
        if(i==6) EBI_PTR(EBI_PTR(child,4),0)=0;
        if(i==7) EBI_AI(child)=func_08001030_6F1240;
        if(i==8) { role(1,1,0);role(1,1,1); }
        if(i==9) { func_80034EF8_35AF8(child); }
        anchor_minigame_ebi_delete();assert(!raw_find(child));
    }
    /* Offline native detonation still registers its two native children. */
    fresh();role(0,1,1);allowed=1;
    t=func_80217360_5D2830(s_root.task,0x1ac,6);tick_initializers();bomb=find(t);assert(bomb);
    fixture_bomb_detonate=1;D_8016DAB4_16E6B4=t;ebi_actor_update(t,bomb->object);
    assert(raw_find(fixture_blast_children[0]) && raw_find(fixture_blast_children[1]));
    puts("Ebi real direct blast birth, once-only native init, proxy first contact, HP receipt and expiry fences passed");
}
static void test_pool_and_pending_birth(void)
{
    AnchorMinigameState state;
    void *a,*b;
    unsigned int i;
    fresh();a=func_80217360_5D2830(s_root.task,0x1ac,6);b=func_80217360_5D2830(s_root.task,0x1ac,6);
    assert(raw_find(a) && raw_find(b) && raw_find(a)!=raw_find(b));
    memset(&state,0,sizeof(state));assert(capture(&state) && !state.ready);
    tick_initializers();memset(&state,0,sizeof(state));assert(capture(&state) && state.ready && state.actor_count==4);
    for(i=4;i<61;++i) { func_80217360_5D2830(s_root.task,0x1ac,6);tick_initializers(); }
    memset(&state,0,sizeof(state));assert(capture(&state) && state.ready && state.actor_count==61);
    assert(validate(&state));
    /* Explicit overflow fails closed rather than advertising a partial graph. */
    for(i=61;i<65;++i) { func_80217360_5D2830(s_root.task,0x1ac,6);tick_initializers(); }
    memset(&state,0,sizeof(state));assert(!capture(&state) && s_overflow);
}
static void test_full_pool_graph_replacement(void)
{
    AnchorMinigameState state,got;
    unsigned int i;
    fresh();state_base(&state);
    for(i=3;i<9;++i) append_actor(&state,EBI_BOMB,(int)i);
    while(!apply(&state)) tick_initializers();
    pool_limit=10;removals=0;
    for(i=2;i<state.actor_count;++i) state.actors[i][ER_ID]+=20;
    ++state.source_tick;state.controller[EC_SERIAL]+=20;state.controller[EC_TIMER]=600;
    for(i=0;i<8 && !apply(&state);++i) {
        assert(EI(hud.bytes,0x80)==2970);tick_initializers();
        memset(&got,0,sizeof(got));assert(capture(&got) && !got.ready);
    }
    assert(i<8 && removals==6 && EI(hud.bytes,0x80)==600);
    assert(identity(EBI_BOMB,23,1) && !identity(EBI_BOMB,3,1));
}
static void test_birth_callback_registration(void)
{
    void *child,*unrelated;
    unsigned int calls;
    fresh();
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    calls=resolver_calls;anchor_minigame_ebi_child(s_root.task,child);
    assert(raw_find(child) && raw_find(child)->kind==EBI_MECHANISM);
    assert(resolver_calls==calls && EBI_AI(child)==(TestCallback)(unsigned long)0x0800139cu);
    child=make_task(0x142,(TestCallback)(unsigned long)0x08801250u);EH(child,0x28)=0x2a;
    anchor_minigame_ebi_child(s_root.task,child);
    assert(raw_find(child) && raw_find(child)->kind==EBI_PLATFORM);
    assert(EBI_AI(child)==(TestCallback)(unsigned long)0x08801250u); /* Disabled alias retained. */
    child=make_task(0x142,func_0800139C_6F15AC);EH(child,0x28)=0x2a;
    calls=resolver_calls;anchor_minigame_ebi_child(s_root.task,child);
    assert(raw_find(child) && resolver_calls==calls); /* Already relocated callback. */
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x20;
    anchor_minigame_ebi_child(s_root.task,child);assert(!raw_find(child));
    child=make_task(0x142,func_0800139C_6F15AC);EH(child,0x28)=0x20;
    calls=resolver_calls;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls);
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x0c;
    calls=resolver_calls;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls); /* Unsupported bank is never resolved. */
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    resources[0x2a]=0;calls=resolver_calls;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls);resources[0x2a]=1;
    unrelated=make_task(0x142,func_08001954_6F1B64);
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    calls=resolver_calls;anchor_minigame_ebi_child(unrelated,child);
    assert(!raw_find(child) && resolver_calls==calls); /* Existing provenance gate. */
}
static void test_callback_mapping_arithmetic(void)
{
    static const unsigned int loaded[]={0x803cf39cu,0x803cf250u,0x803c2720u,
                                       0x803cee2cu,0x803cf030u,0x803ceb58u};
    static const int kinds[]={EBI_MECHANISM,EBI_PLATFORM,EBI_ROBOT,
                              EBI_BOMB,EBI_SWEET,EBI_BLAST};
    unsigned int i,queries,calls;
    void *child;
    reset();memset(tasks,0,sizeof(tasks));memset(pointers,0,sizeof(pointers));
    memset(resources,1,sizeof(resources));task_count=pointer_count=0;
    callback_bank_handles[0]=0xc03ce000u;callback_bank_handles[1]=0xc03c1000u;
    child=make_task(0,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    assert((unsigned long)func_80014840_15440((void *)(unsigned long)0x0800139cu,0x2a)==0x773cf39cu);
    assert(callback_value(func_0800139C_6F15AC)==0x803cf39cu);
    fprintf(stderr,"Ebi callback golden: raw0800139C legacy773CF39C scheduler/import803CF39C\n");
    calls=resolver_calls;queries=resource_queries;
    assert(classify_child(child)==EBI_MECHANISM);
    assert(resolver_calls==calls && resource_queries==queries+1);
    assert(EBI_AI(child)==(TestCallback)(unsigned long)0x0800139cu);
    for(i=0;i<6;++i) {
        const CallbackIdentity *identity=&callback_identities[i];
        EH(child,0x28)=(unsigned short)identity->bank;
        EBI_AI(child)=(TestCallback)(unsigned long)identity->raw;
        assert(callback_value(identity->host)==loaded[i]);
        queries=resource_queries;assert(classify_child(child)==kinds[i]);
        assert(resource_queries==queries+1 && resolver_calls==calls);
        assert((unsigned long)EBI_AI(child)==identity->raw);
        EBI_AI(child)=(TestCallback)(unsigned long)loaded[i];
        assert(classify_child(child)==kinds[i] && resolver_calls==calls);
        assert((unsigned long)EBI_AI(child)==loaded[i]);
    }
    EH(child,0x28)=0x2a;EBI_AI(child)=(TestCallback)(unsigned long)0x0880139cu;
    assert(classify_child(child)==EBI_MECHANISM && (unsigned long)EBI_AI(child)==0x0880139cu);
    EBI_AI(child)=(TestCallback)(unsigned long)0x80bcf39cu;
    assert(classify_child(child)==EBI_MECHANISM && (unsigned long)EBI_AI(child)==0x80bcf39cu);
    EBI_AI(child)=(TestCallback)(unsigned long)0x0800139cu;
    callback_bank_handles[0]=0x803ce000u; /* Untagged resident code base. */
    assert(classify_child(child)==EBI_MECHANISM);
    callback_bank_handles[0]=0;queries=resource_queries;
    assert(!classify_child(child) && resource_queries==queries+1 && resolver_calls==calls);
    callback_bank_handles[0]=0xffffffffu;assert(!classify_child(child) && resolver_calls==calls);
    callback_bank_handles[0]=0x00000001u;assert(!classify_child(child) && resolver_calls==calls);
    callback_bank_handles[0]=0xc03ce001u;assert(!classify_child(child) && resolver_calls==calls);
    callback_bank_handles[0]=0xc03ce000u;
    resources[0x2a]=0;assert(!classify_child(child) && resolver_calls==calls);resources[0x2a]=1;
    EH(child,0x28)=0x20;assert(!classify_child(child) && resolver_calls==calls);
    EBI_AI(child)=(TestCallback)(unsigned long)0x08001720u;
    EH(child,0x28)=0x2a;assert(!classify_child(child) && resolver_calls==calls);
    EH(child,0x28)=0x0c;queries=resource_queries;
    assert(!classify_child(child) && resource_queries==queries && resolver_calls==calls);
    EH(child,0x28)=0x2a;EBI_AI(child)=(TestCallback)(unsigned long)0x0900139cu;
    assert(!classify_child(child) && resolver_calls==calls);
    EBI_AI(child)=(TestCallback)(unsigned long)0x08007777u;
    assert(!classify_child(child) && resolver_calls==calls);
    assert((unsigned long)EBI_AI(child)==0x08007777u);
    puts("Mini Ebisumaru patched scheduler callback mapping golden values and guards passed");
}
#if DEBUG_BUTTON_ENABLED
static void test_birth_diagnostics(void)
{
    void *child,*unrelated;
    unsigned int calls,reports,i,queries;
    fresh();diagnostic_length=diagnostic_calls=0;diagnostic_log[0]=0;
    s_diag_birth_calls=s_diag_birth_registered=0;
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    calls=resolver_calls;queries=resource_queries;anchor_minigame_ebi_child(s_root.task,child);
    assert(raw_find(child) && resolver_calls==calls);
    assert(resource_queries==queries+1); /* Logging does not query residency again. */
    assert(EBI_AI(child)==(TestCallback)(unsigned long)0x0800139cu);
    assert(strstr(diagnostic_log,"gate=ok") && strstr(diagnostic_log,"classify=matched"));
    assert(strstr(diagnostic_log,"[Ebi birth] v=3"));
    assert(strstr(diagnostic_log,"raw=0800139c") && strstr(diagnostic_log,"expectedCallbacks="));
    assert(s_diag_birth_calls==1 && s_diag_birth_registered==1);
    unrelated=make_task(0x142,func_08001954_6F1B64);
    child=make_task(0x142,(TestCallback)(unsigned long)0x0800139cu);EH(child,0x28)=0x2a;
    calls=resolver_calls;queries=resource_queries;anchor_minigame_ebi_child(unrelated,child);
    assert(!raw_find(child) && resolver_calls==calls && strstr(diagnostic_log,"gate=parent"));
    assert(resource_queries==queries);
    EH(s_root.task,0x5c)=0x143;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls && strstr(diagnostic_log,"gate=root"));
    EH(s_root.task,0x5c)=s_root.entity;
    EBI_PTR(child,0x18)=0;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls && strstr(diagnostic_log,"gate=object"));
    assert(resource_queries==queries);
    assert(s_diag_birth_registered==1); /* Object rejection is not a successful birth. */
    EBI_PTR(child,0x18)=task_for(child)->object.bytes;
    resources[0x2a]=0;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls && strstr(diagnostic_log,"classify=not-resident"));
    assert(resource_queries==queries+1);queries=resource_queries;
    resources[0x2a]=1;
    EH(child,0x28)=0x0c;anchor_minigame_ebi_child(s_root.task,child);
    assert(!raw_find(child) && resolver_calls==calls && strstr(diagnostic_log,"classify=bank"));
    assert(resource_queries==queries);
    anchor_minigame_ebi_child(s_root.task,0);
    assert(strstr(diagnostic_log,"gate=child"));
    for(i=s_diag_birth_calls;i<32;++i) anchor_minigame_ebi_child(unrelated,child);
    reports=diagnostic_calls;anchor_minigame_ebi_child(unrelated,child);
    assert(s_diag_birth_calls==33 && diagnostic_calls==reports && resolver_calls==calls);
    ++visit;anchor_minigame_ebi_child(unrelated,child);
    assert(s_diag_birth_calls==1 && s_diag_birth_registered==0 && diagnostic_calls>reports);
    D_800C7AB2=0x166;reports=diagnostic_calls;
    anchor_minigame_ebi_child(unrelated,child);assert(diagnostic_calls==reports);
    D_800C7AB2=0x1e1;
    reset();diagnostic_length=diagnostic_calls=0;diagnostic_log[0]=0;
    for(i=0;i<40;++i) anchor_minigame_ebi_child(unrelated,child);
    assert(s_diag_birth_calls==40 && diagnostic_calls==3); /* One pre-root report. */
    D_8016DAB4_16E6B4=unrelated;anchor_minigame_ebi_root_initialized();
    assert(s_root.task==unrelated && s_diag_birth_calls==0);
    reports=diagnostic_calls;anchor_minigame_ebi_child(unrelated,child);
    assert(s_diag_birth_calls==1 && diagnostic_calls>reports);
    puts("Mini Ebisumaru debug birth gate observations and report budgets passed");
}
static void test_startup_diagnostics(void)
{
    AnchorMinigameState state;
    unsigned int reports,calls,old_allocations,old_root_calls;
    EbiActor *mechanism;
    diagnostic_calls=diagnostic_length=0;diagnostic_log[0]=0;
    fresh();allowed=0;role(1,0,0);
    memset(&state,0,sizeof(state));assert(capture(&state) && validate(&state));
    assert(strstr(diagnostic_log,"[Ebi startup]") && strstr(diagnostic_log,"validate=1"));
    assert(strstr(diagnostic_log,"role=1/0/0 allowed=0") && strstr(diagnostic_log,"banks=7ff"));
    assert(strstr(diagnostic_log,"[Ebi actor] slot=0 kind=1"));
    reports=s_diag_reports;calls=diagnostic_calls;
    old_allocations=(unsigned int)allocations;old_root_calls=(unsigned int)root_calls;
    for(unsigned int i=0;i<100;++i) {
        ES(s_root.task,0xde)=(short)(i+1);ES(s_root.task,0xe0)=(short)(i+2);
        EI(hud.bytes,0x80)=2970-(int)i;EF(s_root.object,0x28)=(float)i;
        memset(&state,0,sizeof(state));assert(capture(&state) && validate(&state));
        role(1,0,0);
    }
    assert(s_diag_reports==reports && diagnostic_calls==calls);
    ES(s_root.task,0xde)=1;ES(s_root.task,0xe0)=2;EI(hud.bytes,0x80)=2970;
    EF(s_root.object,0x28)=0.f;
    assert((unsigned int)allocations==old_allocations && (unsigned int)root_calls==old_root_calls);
    s_diag_attempts=s_diag_last+299;
    memset(&state,0,sizeof(state));assert(capture(&state) && validate(&state));
    assert(s_diag_reports==reports+1); /* Bounded periodic blocked report. */
    mechanism=find(EBI_PTR(s_root.task,0xd0));assert(mechanism);
    EF(mechanism->object,0x28)=bits_float(0x7fc00000);
    memset(&state,0,sizeof(state));assert(capture(&state) && !state.ready && !validate(&state));
    assert(strstr(diagnostic_log,"[Ebi invalid row] index=0 words=32"));
    assert(strstr(diagnostic_log,"7fc00000") && strstr(diagnostic_log,"validate=0"));
    EF(mechanism->object,0x28)=0.f;
    resources[0x1e3]=0;
    memset(&state,0,sizeof(state));assert(capture(&state) && !state.ready && validate(&state));
    assert(strstr(diagnostic_log,"banks=7ef"));resources[0x1e3]=1;
    EBI_PTR(s_root.task,0xd4)=0;
    memset(&state,0,sizeof(state));assert(!capture(&state));
    assert(strstr(diagnostic_log,"rootReady=0 capture=0"));
    /* Reporting itself never advances the game, loads an actor, or adjusts HUD. */
    assert((unsigned int)allocations==old_allocations && (unsigned int)root_calls==old_root_calls);
    assert(EI(hud.bytes,0x80)==2970);
    s_diag_reports=64;calls=diagnostic_calls;
    character_ready=0;role(1,1,1);assert(!capture(&state));
    assert(diagnostic_calls==calls);
    reset();assert(s_diag_reports==0 && !s_diag_have_key && !s_diag_capture);
    puts("Mini Ebisumaru debug startup diagnostics passed");
}
#endif
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--timer-regression")) { test_timer_source_ticks();return 0; }
    if(argc==2 && !strcmp(argv[1],"--blast-regression")) { test_direct_blast_birth_and_contacts();return 0; }
    if(argc==2 && !strcmp(argv[1],"--repeated-return-regression")) {
        test_repeated_initializer_returns();return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--callback-map-regression")) {
        test_callback_mapping_arithmetic();return 0;
    }
    if(argc==2 && (!strcmp(argv[1],"--export-cold-pending") ||
                  !strcmp(argv[1],"--export-cold-ready"))) {
        AnchorMinigameState state;char encoded[ANCHOR_MINIGAME_JSON_SIZE];
        int ready=!strcmp(argv[1],"--export-cold-ready");
        cold_registry=1;fresh_with_children(ready);memset(&state,0,sizeof(state));
        int captured=capture(&state);
        if(!ready) { assert(!captured);puts("null");return 0; }
        assert(captured && state.ready && validate(&state));
        assert(state.actor_count==2 && !resources[0x0c]);
        assert(anchor_minigame_state_encode(&state,encoded,sizeof(encoded)));
        puts(encoded);return 0;
    }
#if DEBUG_BUTTON_ENABLED
    if(argc==2 && !strcmp(argv[1],"--diagnostic-log")) {
        test_startup_diagnostics();fputs(diagnostic_log,stdout);return 0;
    }
#endif
    if(argc==2 && !strcmp(argv[1],"--export")) {
        char encoded[ANCHOR_MINIGAME_JSON_SIZE];AnchorMinigameState native;
        fresh();state_base(&native);
        append_actor(&native,EBI_ROBOT,3);append_actor(&native,EBI_BOMB,4);
        append_actor(&native,EBI_SWEET,5);append_actor(&native,EBI_BLAST,6);
        native.controller[EC_PHASE]=6;native.controller[EC_PHASE_TIME]=71;
        native.controller[EC_TIME]=1217;native.controller[EC_TIMER]=601;
        native.controller[EC_SWEETS]=4;native.actors[2][ER_X]=float_bits(17.f);
        while(!apply(&native)) tick_initializers();
        memset(&native,0,sizeof(native));assert(capture(&native));
        assert(native.ready && native.running && native.actor_count==6);
        assert(anchor_minigame_state_encode(&native,encoded,sizeof(encoded)));
        puts(encoded);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--validate")) {
        char json[ANCHOR_MINIGAME_JSON_SIZE];AnchorMinigameStatus incoming;
        unsigned char saved[48];unsigned int ticks;
        FILE *input_file=fopen(argv[2],"rb");assert(input_file);
        size_t n=fread(json,1,sizeof(json)-1,input_file);assert(!ferror(input_file));
        assert(feof(input_file));fclose(input_file);json[n]=0;
        assert(anchor_minigame_status_decode(json,&incoming));
        assert(incoming.has_state && incoming.state.ready && incoming.state.running);
        fresh();memcpy(saved,backup,sizeof(saved));
        assert(validate(&incoming.state));assert(!apply(&incoming.state));
        for(ticks=0;ticks<8 && !apply(&incoming.state);++ticks) {
            assert(EI(hud.bytes,0x80)==2970 && EB(s_root.task,0xdc)==1);
            tick_initializers();
        }
        assert(ticks<8 && apply(&incoming.state));
        assert(!memcmp(saved,backup,sizeof(saved)) && bomb_native==0);
        assert(EB(s_root.task,0xdc)==6 && ES(s_root.task,0xde)==71);
        assert(EI(hud.bytes,0x80)==601 && D_080032F4_6F3504==4);
        assert(EF(identity(EBI_ROBOT,3,1)->object,8)==17.f);
        assert(EB(identity(EBI_BLAST,6,1)->task,0x4c)==0);
        puts("Ebisumaru Python status -> production codec -> native graph bootstrap/apply passed");
        return 0;
    }
    assert(argc==1);
    test_timer_source_ticks();test_direct_blast_birth_and_contacts();
    test_remote_drop_detection();test_remote_drop_positions();
#ifdef ANCHOR_EBI_SHARED_GAMEPLAY
    test_shared_health_and_receipts();test_repeat_hits_and_follower_safety();
    test_predictive_movement_and_freshness();test_scoped_target_and_finalizer_masks();
    test_predicted_blast_lifetime();
#endif
    test_repeated_initializer_returns();
    test_callback_mapping_arithmetic();
#if DEBUG_BUTTON_ENABLED
    test_birth_diagnostics();
    test_startup_diagnostics();
#endif
    test_birth_callback_registration();test_cold_registry_solo_startup();test_complete_graph_and_late_join();test_claims_hits_and_collision();
    test_timer_results_and_return();test_pool_and_pending_birth();test_full_pool_graph_replacement();
    puts("Mini Ebisumaru native adapter tests passed");return 0;
}
