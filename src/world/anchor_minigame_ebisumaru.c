/* File42 Mini Ebisumaru: shared controller and complete native gameplay graph.
 * Local camera, native player statistics, scheduler pointers and the 48-byte
 * backup stay local; only the three-heart pool is shared. Initializers run once;
 * emitter, collection and damage callbacks advance only on the authority. */
#define ANCHOR_EBI_SHARED_GAMEPLAY 1
#define ANCHOR_EBI_REMOTE_DETECTOR 1
#define ANCHOR_EBI_TIMER_BLAST 1
#ifndef ANCHOR_MINIGAME_EBISUMARU_HOST_TEST
#include "platform/modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif
#if DEBUG_BUTTON_ENABLED
#ifndef ANCHOR_MINIGAME_EBISUMARU_HOST_TEST
#include "platform/recomputils.h"
#else
extern int recomp_printf(const char *, ...);
#endif
#endif
#include "world/anchor_minigame_sync.h"
#include "world/anchor_minigame_invites.h"
#include "player/anchor_player_models.h"

typedef void (*EbiCallback)(void *, void *);
#define EB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define EH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define ES(p,o) (*(short *)((char *)(p)+(o)))
#define EW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define EI(p,o) (*(int *)((char *)(p)+(o)))
#define EF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef EBI_PTR
#define EBI_PTR(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef EBI_AI
#define EBI_AI(p) (*(EbiCallback *)((char *)(p)+0x0c))
#endif
#ifndef EBI_CALLBACK_VALUE
#define EBI_CALLBACK_VALUE(callback) ((unsigned long)(callback))
#endif
#ifndef EBI_CAMERA_DEST
#define EBI_CAMERA_DEST(p) ((void *)(unsigned long)(EW(p,0x2c)&0x8ffffffeu))
#endif

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4, *D_801FC604_5B8514, *D_801FC628_5B8538;
extern void *D_801FC60C_5B851C;
extern unsigned int anchor_get_client_id(void);
extern unsigned char D_080032F4_6F3504;
extern unsigned int D_8015C5D8_15D1D8[];
extern void *func_800141C4_14DC4(unsigned int);
extern void *func_80217360_5D2830(void *, unsigned short, unsigned char);
extern void *func_8021DDE8_5D92B8(void *, EbiCallback, unsigned char,
                                float, float, float, int);
extern void func_80034EF8_35AF8(void *);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern void func_80216E1C_5D22EC(void *, int);
extern void func_80219E70_5D5340(void *, unsigned char, unsigned char);
extern void func_8021A310_5D57E0(void *);
extern void func_80218DA8_5D4278(void *, int, unsigned short, short);
extern void func_80219448_5D4918(void *);
extern int func_802196FC_5D4BCC(void *,float,float,float,int);
extern int func_8021BB04_5D6FD4(void *,float,float,float);
extern unsigned int func_8021ADD0_5D62A0(float,float);
/* Verified shared native bomb material ABI: task, alpha, r, g, b. These
 * rewrite the local object+80 display list without emitting gameplay tasks. */
extern void *func_8021A26C_5D573C(void *,unsigned int,int,unsigned int,unsigned int);
extern void *func_8021DD4C_5D921C(void *,unsigned int,int,unsigned int,unsigned int);
extern void func_80217144_5D2614(void *);
extern void func_800124A0_130A0(void *, float);
extern void func_800124AC_130AC(void *, float);
extern void func_80012628_13228(void *, float, float, float);
extern void func_80012518_13118(void *, float, float, float);
extern void func_80012764_13364(void *, float);
extern void func_80012900_13500(void *, void *);
extern double D_080032B0_6F34C0, D_080032B8_6F34C8;
extern double D_080032C0_6F34D0, D_080032C8_6F34D8;
extern void func_08001954_6F1B64(void *, void *);
extern void func_0800139C_6F15AC(void *, void *);
extern void func_08001250_6F1460(void *, void *);
extern void func_08001030_6F1240(void *, void *);
extern void func_08000E2C_6F103C(void *, void *);
extern void func_08000B58_6F0D68(void *, void *);
#define EBI_ROBOT_PHASES(X) \
 X(func_08001884_6D0964) X(func_080018D0_6D09B0) \
 X(func_0800191C_6D09FC) X(func_08001958_6D0A38) \
 X(func_080019D0_6D0AB0) X(func_08001A40_6D0B20) \
 X(func_08001AA4_6D0B84) X(func_08001AFC_6D0BDC) \
 X(func_08001B40_6D0C20) X(func_08001BC8_6D0CA8) \
 X(func_08001C4C_6D0D2C) X(func_08001CD0_6D0DB0) \
 X(func_08001D7C_6D0E5C)
#define EBI_DECLARE(f) extern void f(void *, void *);
EBI_ROBOT_PHASES(EBI_DECLARE)
#undef EBI_DECLARE
extern void func_08001720_6D0800(void *, void *);

enum { EBI_MECHANISM=1, EBI_PLATFORM, EBI_ROBOT, EBI_BOMB,
       EBI_SWEET, EBI_BLAST };
enum { EC_VERSION, EC_PHASE, EC_PHASE_TIME, EC_TIME, EC_THRESHOLD,
       EC_TIMER, EC_SWEETS, EC_SERIAL, EC_RESULT, EC_FLAGS,
       EC_CLIP, EC_FRAME, EC_HEALTH, EC_WORDS };
enum { ER_KIND, ER_ID, ER_GENERATION, ER_X, ER_Y, ER_Z,
       ER_ANGLES, ER_ROLL, ER_SX, ER_SY, ER_SZ, ER_FRAME,
       ER_FLAGS, ER_FLAGS2, ER_VX, ER_VY, ER_VZ, ER_AI, ER_CLIP,
       ER_LIFE, ER_ACTION, ER_STATUS, ER_MODE, ER_EXTRA, ER_WORDS=32,
       ER_ORBIT=32, ER_RADII, ER_ORBIT_X, ER_ORBIT_Z, ER_TARGET,
       ER_TARGET_X, ER_TARGET_Y, ER_TARGET_Z, ER_ROBOT_WORDS };
enum { EBI_COLLECT=1, EBI_HIT=2, EBI_HEALTH_EVENT=3 };
typedef struct EbiActor {
    void *task, *object;
    EbiCallback native;
    unsigned int id, generation, clip;
    unsigned short entity;
    unsigned char task_generation, kind, initialized, proxy;
    unsigned int pending_damage;
    int net[ER_ROBOT_WORDS];
    unsigned char have_net;
    unsigned char hit_cooldown, correction_ticks, contact_expired;
    unsigned int prediction_age, checkpoint_generation, full_tick;
    float correction[3], target_view[5];
    float held_xyz[3],held_frame;
    int target_cid;
} EbiActor;
static EbiActor s_root, s_actors[ANCHOR_MINIGAME_MAX_ACTORS];
static EbiCallback volatile s_robot_phases[13];
static unsigned int s_visit, s_serial=2, s_input_serial;
static int s_active, s_owner, s_ready, s_result;
static int s_local_failure;
static int s_graph_staging;
static int s_root_resources_requested;
static unsigned int s_overflow;
static int s_inputs[ANCHOR_MINIGAME_MAX_INPUTS][5];
static unsigned int s_input_count;
static const int *s_birth_row;
static int s_health=3,s_health_started,s_health_observing,s_health_before;
static unsigned int s_health_visit,s_prediction_age;
static unsigned int s_full_tick,s_checkpoint_generation;
static unsigned int s_ack_fence,s_health_unqueued;
static void *s_health_player,*s_health_object;
static unsigned char s_health_player_generation;
static unsigned short s_health_player_entity;
static AnchorBossTarget s_targets[64];
static int s_target_count;
static EbiActor *s_target_actor;
static int s_local_cid,s_drop_last_cid;
typedef struct EbiDropScope {
    void *root,*object,*host_object,*child,*child_object;
    EbiActor *bomb;
    unsigned int visit,serial,bomb_id;
    unsigned short entity;
    unsigned char generation;
    int branch,target_cid;
    float host[3],target[3];
} EbiDropScope;
static EbiDropScope s_drop_scope;
/* Direct DDE8 children have no code bank until their File42 caller returns. */
typedef struct EbiPendingBlast {
    void *parent,*parent_object,*child,*object,*link;
    const int *row;
    unsigned int visit;
    unsigned short entity,model;
    unsigned char generation;
} EbiPendingBlast;
static EbiPendingBlast s_pending_blasts[2];
static unsigned int s_timer_generation,s_timer_tick,s_timer_age;
static int s_timer_stamped;
static void clear_pending(void *);
static void confirm_blasts(void *,void *);

static void health_display(unsigned int);
static void predict_actor(EbiActor *);
static void fence_blast(EbiActor *);
static unsigned int actor_words(int kind) { return kind==EBI_ROBOT?ER_ROBOT_WORDS:ER_WORDS; }
static unsigned int local_ack(void)
{
    unsigned int ack=anchor_minigame_sync_local_ack(2);
    return ack>s_ack_fence?ack:s_ack_fence;
}
static void ebi_root_update(void *, void *);
static void ebi_actor_update(void *, void *);
static const AnchorMinigameAdapter s_adapter;

static int pointer_valid(void *p)
{
#ifdef ANCHOR_MINIGAME_EBISUMARU_HOST_TEST
    return p != 0;
#else
    unsigned int a=(unsigned int)(unsigned long)p;
    return !(a&3u) && a>=0x80001000u && a<0x80800000u;
#endif
}
static int resident(unsigned int file)
{
    void *p=func_800141C4_14DC4(file);
    return p && p!=(void *)(unsigned long)0xffffffffu;
}
static int callback_is(EbiCallback a, EbiCallback b)
{
    return (EBI_CALLBACK_VALUE(a)&~0x00800000ul)==
           (EBI_CALLBACK_VALUE(b)&~0x00800000ul);
}
static void callbacks(void)
{
    unsigned int i=0;
#define EBI_ADDRESS(f) s_robot_phases[i++]=f;
    EBI_ROBOT_PHASES(EBI_ADDRESS)
#undef EBI_ADDRESS
}
static int live(const EbiActor *a)
{
    void *link;
    if (D_800C7AB2!=0x1e1 || s_visit!=anchor_minigame_sync_visit() ||
        !pointer_valid(a->task) || !pointer_valid(a->object) ||
        EBI_PTR(a->task,0x18)!=a->object ||
        EH(a->task,0x5c)!=a->entity || EB(a->task,0x74)!=a->task_generation ||
        (EW(a->task,0x68)&2u)) return 0;
    link=EBI_PTR(a->task,4);
    return pointer_valid(link) && EBI_PTR(link,0)==a->task;
}
static int owned_handle(const EbiActor *a)
{
    void *link;
    if(D_800C7AB2!=0x1e1 || s_visit!=anchor_minigame_sync_visit() ||
       !pointer_valid(a->task) || !pointer_valid(a->object) ||
       EBI_PTR(a->task,0x18)!=a->object) return 0;
    if(a->initialized && (EH(a->task,0x5c)!=a->entity ||
                          EB(a->task,0x74)!=a->task_generation)) return 0;
    link=EBI_PTR(a->task,4);
    return pointer_valid(link) && EBI_PTR(link,0)==a->task;
}
static EbiActor *find(void *task)
{
    unsigned int i;
    for (i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i)
        if (s_actors[i].task==task && live(&s_actors[i])) return &s_actors[i];
    return 0;
}
static EbiActor *raw_find(void *task)
{
    unsigned int i;
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i)
        if(s_actors[i].task==task) return &s_actors[i];
    return 0;
}
static EbiActor *identity(int kind, unsigned int id, unsigned int generation)
{
    unsigned int i;
    for (i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i)
        if (s_actors[i].kind==kind && s_actors[i].id==id &&
            s_actors[i].generation==generation && live(&s_actors[i]))
            return &s_actors[i];
    return 0;
}
static EbiActor *record(void *task, int kind)
{
    unsigned int i;
    EbiActor *a=raw_find(task);
    if (a) return a;
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i)
        if(!s_actors[i].task) break;
    if(i==ANCHOR_MINIGAME_MAX_ACTORS) { s_overflow=1;return 0; }
    a=&s_actors[i];
    anchor_minigame_zero(a,sizeof(*a));
    a->task=task; a->object=EBI_PTR(task,0x18); a->kind=(unsigned char)kind;
    a->entity=EH(task,0x5c); a->task_generation=EB(task,0x74);
    a->native=EBI_AI(task); a->generation=1;
    a->id=kind==EBI_MECHANISM?1u:kind==EBI_PLATFORM?2u:++s_serial;
    return a;
}
static int phase(EbiActor *a)
{
    unsigned int i;
    EbiCallback callback=callback_is(EBI_AI(a->task),ebi_actor_update)?
                         a->native:EBI_AI(a->task);
    if(a->kind!=EBI_ROBOT) return 0;
    for(i=0;i<13;++i) if(callback_is(callback,s_robot_phases[i])) return (int)i;
    return -1;
}
static int float_valid(int bits, float limit)
{
    union { unsigned int bits; float value; } v;
    v.bits=(unsigned int)bits;
    return (v.bits&0x7f800000u)!=0x7f800000u &&
           v.value>=-limit && v.value<=limit;
}
static int float_bits(float value)
{
    union { unsigned int bits; float value; } v;
    v.value=value; return (int)v.bits;
}
static float bits_float(int bits)
{
    union { unsigned int bits; float value; } v;
    v.bits=(unsigned int)bits; return v.value;
}
static int root_ready(void)
{
    void *mechanism, *platform;
    EbiActor *m,*p;
    if(!live(&s_root) || EH(s_root.task,0x5e)!=0x142 || EB(s_root.task,0xdc)<1 ||
       !pointer_valid(EBI_PTR(s_root.task,0xd4)) ||
       !pointer_valid(EBI_PTR(s_root.task,0xd8))) return 0;
    mechanism=EBI_PTR(s_root.task,0xd0); m=find(mechanism);
    if(!m || m->kind!=EBI_MECHANISM || !m->initialized) return 0;
    platform=EBI_PTR(mechanism,0xd0); p=find(platform);
    return p && p->kind==EBI_PLATFORM && p->initialized;
}
static int root_resources_ready(unsigned int clip)
{
    void *o=s_root.object;
    float old_frame;
    unsigned short old_speed;
    unsigned char old_loop;
    if(resident(0x1cf) && resident(0x173)) { s_root_resources_requested=0;return 1; }
    if(!s_root_resources_requested && clip<=2) {
        /* The stock phase2 animation binder also loads both root resources.
         * Request that native graph during online bootstrap, retaining the
         * local checkpoint while the resource loader completes. */
        old_frame=EF(o,0x28);old_speed=EH(o,0x7e);old_loop=EB(o,0x7c);
        s_root_resources_requested=1;
        func_8021664C_5D1B1C(s_root.task,clip,0.05f,0);
        EF(o,0x28)=old_frame;EH(o,0x7e)=old_speed;EB(o,0x7c)=old_loop;
    }
    return resident(0x1cf) && resident(0x173);
}
static int resources_ready(int kind)
{
    /* File12 is loaded directly at 0x8020D2A0, outside the dynamic registry
     * queried by resident(). Only this graph's registered banks are gates. */
    if(!resident(0x2a)) return 0;
    if(kind==EBI_ROBOT) return resident(0x20) && resident(0x197) && resident(0x159);
    if(kind==EBI_BOMB) return resident(0x1e1);
    if(kind==EBI_SWEET) return resident(0x1e2);
    if(kind==EBI_MECHANISM) return resident(0x1e3);
    if(kind==EBI_PLATFORM) return resident(0x1e6);
    return kind==EBI_BLAST && resident(0x191);
}
static int row_valid(const int *r)
{
    unsigned int i;
    if(r[ER_KIND]<EBI_MECHANISM || r[ER_KIND]>EBI_BLAST ||
       r[ER_ID]<1 || r[ER_GENERATION]!=1 || r[ER_ROLL]<0 ||
       r[ER_ROLL]>65535 || r[ER_CLIP]<0 ||
       ((unsigned int)r[ER_STATUS]&2u)) return 0;
    /* FB has six native animation pointers, followed by a null terminator.
     * Other synchronized families use their single stock variant; blast
     * variant8 is initialized by 16DF8 and is never passed to this binder. */
    if(r[ER_CLIP]>(r[ER_KIND]==EBI_ROBOT?5:0)) return 0;
    if((r[ER_KIND]==EBI_MECHANISM && r[ER_ID]!=1) ||
       (r[ER_KIND]==EBI_PLATFORM && r[ER_ID]!=2) ||
       (r[ER_KIND]>=EBI_ROBOT && r[ER_ID]<3)) return 0;
    for(i=ER_X;i<=ER_Z;++i) if(!float_valid(r[i],200000.f)) return 0;
    for(i=ER_SX;i<=ER_SZ;++i) if(!float_valid(r[i],64.f)) return 0;
    if(!float_valid(r[ER_FRAME],100000.f)) return 0;
    for(i=ER_VX;i<=ER_VZ;++i) if(!float_valid(r[i],1000.f)) return 0;
    if(r[ER_KIND]==EBI_ROBOT) {
        if(r[ER_AI]<0 || r[ER_AI]>=13 ||
           !(r[ER_FLAGS]&0x40000) || !(r[ER_FLAGS2]&0x8000) ||
               ((unsigned int)r[ER_EXTRA+1]&0xffu)>1u ||
               ((unsigned int)r[ER_EXTRA+1]>>8)>2u ||
               r[ER_EXTRA+2]<-32768 || r[ER_EXTRA+2]>32767 ||
               !float_valid(r[ER_EXTRA+3],1000.f) ||
               ((unsigned int)r[ER_ORBIT]&0xffffu)>1023u ||
               ((unsigned int)r[ER_RADII]&0xff000000u) ||
               !float_valid(r[ER_ORBIT_X],510.f) ||
               !float_valid(r[ER_ORBIT_Z],510.f) || r[ER_TARGET]<0 ||
               !float_valid(r[ER_TARGET_X],200000.f) ||
               !float_valid(r[ER_TARGET_Y],200000.f) ||
               !float_valid(r[ER_TARGET_Z],200000.f)) return 0;
    } else if(r[ER_AI]!=0) return 0;
    if(r[ER_KIND]==EBI_MECHANISM)
        return (r[ER_EXTRA]&0xff)==1 &&
               (unsigned int)r[ER_EXTRA]<=0x0101u &&
               float_valid(r[ER_EXTRA+1],200000.f) &&
               float_valid(r[ER_EXTRA+2],1000.f);
    if(r[ER_KIND]==EBI_PLATFORM) return r[ER_EXTRA]==1;
    if(r[ER_KIND]==EBI_BOMB) return r[ER_EXTRA]==1;
    if(r[ER_KIND]==EBI_SWEET)
        return (r[ER_EXTRA]&0xff)>=1 && (r[ER_EXTRA]&0xff)<=2 &&
               (unsigned int)r[ER_EXTRA]<=0x0102u &&
               ((unsigned int)r[ER_LIFE]&0xffffu)<=270u;
    if(r[ER_KIND]==EBI_BLAST)
        return r[ER_EXTRA]==1 && (r[ER_MODE]==1 || r[ER_MODE]==4) &&
               r[ER_EXTRA+1]>=28 && r[ER_EXTRA+1]<=90 &&
               ((unsigned int)r[ER_LIFE]&0xffffu)<=30u;
    return 1;
}
static int validate_state(const AnchorMinigameState *s)
{
    unsigned int i,mechanism=0,platform=0,robots=0;
    const int *c=s->controller;
    if(!anchor_minigame_state_valid(s) || s->controller_count!=EC_WORDS ||
       c[EC_VERSION]!=2 || c[EC_PHASE]<1 || c[EC_PHASE]>9 ||
       c[EC_PHASE_TIME]<0 || c[EC_PHASE_TIME]>32767 ||
       c[EC_TIME]<-32768 || c[EC_TIME]>32767 ||
       c[EC_THRESHOLD]<0 || c[EC_THRESHOLD]>249 ||
       c[EC_TIMER]<0 || c[EC_TIMER]>2970 || c[EC_SWEETS]<0 ||
       c[EC_SWEETS]>255 || c[EC_SERIAL]<2 || c[EC_RESULT]<0 ||
       c[EC_RESULT]>2 || c[EC_CLIP]<0 || c[EC_CLIP]>2 ||
       !float_valid(c[EC_FRAME],100000.f) || c[EC_HEALTH]<0 || c[EC_HEALTH]>3) return 0;
    for(i=0;i<s->actor_count;++i) {
        if(s->actor_words[i]!=actor_words(s->actors[i][ER_KIND]) || !row_valid(s->actors[i]) ||
           s->actors[i][ER_ID]>c[EC_SERIAL]) return 0;
        mechanism+=s->actors[i][ER_KIND]==EBI_MECHANISM;
        platform+=s->actors[i][ER_KIND]==EBI_PLATFORM;
        robots+=s->actors[i][ER_KIND]==EBI_ROBOT;
    }
    return mechanism==1 && platform==1 && robots<=2;
}
static void capture_actor(EbiActor *a,int *r)
{
    void *t=a->task,*o=a->object;
    unsigned int i;
    anchor_minigame_zero(r,actor_words(a->kind)*sizeof(*r));
    r[ER_KIND]=a->kind;r[ER_ID]=(int)a->id;r[ER_GENERATION]=(int)a->generation;
    for(i=0;i<3;++i) {
        r[ER_X+i]=float_bits(EF(o,8+4*i));
        r[ER_SX+i]=float_bits(EF(o,0x1c+4*i));
        r[ER_VX+i]=float_bits(EF(t,0x78+4*i));
    }
    r[ER_ANGLES]=(int)((unsigned int)EH(o,0x14)|((unsigned int)EH(o,0x16)<<16));
    r[ER_ROLL]=EH(o,0x18);r[ER_FRAME]=float_bits(EF(o,0x28));
    r[ER_FLAGS]=(int)EW(t,0x60);r[ER_FLAGS2]=(int)EW(t,0x64);
    r[ER_AI]=phase(a);r[ER_CLIP]=(int)a->clip;
    r[ER_LIFE]=(int)((unsigned int)EH(t,0x8a)|((unsigned int)EB(t,0x8c)<<16)|
                     ((unsigned int)EB(t,0x8d)<<24));
    r[ER_ACTION]=(int)((unsigned int)EH(t,0x94)|((unsigned int)EH(t,0x96)<<16));
    r[ER_STATUS]=(int)(EW(t,0x68)&~0x280u);r[ER_MODE]=(int)EW(t,0xe8);
    r[ER_EXTRA+7]=(int)((unsigned int)EB(o,0x7c)|((unsigned int)EH(o,0x7e)<<16));
    r[ER_EXTRA+4]=(int)((unsigned int)EH(t,0x3c)|((unsigned int)EH(t,0x3e)<<16));
    r[ER_EXTRA+5]=(int)((unsigned int)EH(t,0x40)|((unsigned int)EH(t,0x4e)<<16));
    r[ER_EXTRA+6]=(int)((unsigned int)EH(t,0x50)|((unsigned int)EH(t,0x52)<<16));
    r[ER_EXTRA+8]=(int)EW(t,0x98);
    if(a->kind==EBI_ROBOT) {
        r[ER_EXTRA]=(int)((unsigned int)EH(t,0xa0)|((unsigned int)EH(t,0xa2)<<16));
        r[ER_EXTRA+1]=(int)((unsigned int)EB(t,0xa8)|((unsigned int)EB(t,0xa9)<<8));
        r[ER_EXTRA+2]=ES(t,0xb4);r[ER_EXTRA+3]=float_bits(EF(t,0xe0));
        r[ER_ORBIT]=(int)((unsigned int)EH(t,0xd0)|((unsigned int)EH(t,0xd2)<<16));
        r[ER_RADII]=(int)((unsigned int)EB(t,0xd4)|((unsigned int)EB(t,0xd5)<<8)|
                         ((unsigned int)EB(t,0xd6)<<16));
        r[ER_ORBIT_X]=float_bits(EF(t,0xd8));r[ER_ORBIT_Z]=float_bits(EF(t,0xdc));
        r[ER_TARGET]=a->target_cid;
        for(i=0;i<3;++i) r[ER_TARGET_X+i]=float_bits(a->target_view[i+2]);
    } else if(a->kind==EBI_MECHANISM) {
        r[ER_EXTRA]=(int)((unsigned int)EB(t,0xd4)|((unsigned int)EB(t,0xd5)<<8));
        r[ER_EXTRA+1]=float_bits(EF(t,0xd8));r[ER_EXTRA+2]=float_bits(EF(t,0xdc));
    } else {
        r[ER_EXTRA]=EB(t,0xd0);
        if(a->kind==EBI_SWEET) r[ER_EXTRA]|=(int)EB(t,0xd1)<<8;
        if(a->kind==EBI_BLAST) r[ER_EXTRA+1]=ES(t,0xd2);
    }
}
#if DEBUG_BUTTON_ENABLED
/* Observe existing capture/validation outcomes only. Clock/frame values are
 * printed, but excluded from the signature so a held startup cannot spam. */
static const AnchorMinigameState *s_diag_capture;
static int s_diag_captured, s_diag_valid=-1, s_diag_key[20], s_diag_have_key;
static unsigned int s_diag_attempts, s_diag_last, s_diag_reports;
static unsigned int s_diag_birth_visit, s_diag_birth_calls, s_diag_birth_registered;
static EbiCallback s_diag_birth_resolved;
static int s_diag_birth_resident;
static const char *s_diag_birth_classification;
static void diagnostic_task(const char *label,void *t)
{
    void *link,*object;
    if(!pointer_valid(t)) return;
    link=EBI_PTR(t,4);object=EBI_PTR(t,0x18);
    recomp_printf("[Ebi native child] %s task=%08x object=%08x bank=%04x callback=%08x entity/model=%04x/%04x gen=%u status=%08x link=%08x back=%08x D0=%08x D4=%02x\n",
        label,(unsigned int)(unsigned long)t,(unsigned int)(unsigned long)object,
        EH(t,0x28),(unsigned int)(unsigned long)EBI_AI(t),EH(t,0x5c),EH(t,0x5e),
        EB(t,0x74),EW(t,0x68),(unsigned int)(unsigned long)link,
        pointer_valid(link)?(unsigned int)(unsigned long)EBI_PTR(link,0):0,
        (unsigned int)(unsigned long)EBI_PTR(t,0xd0),EB(t,0xd4));
}
static void diagnostic_birth(void *parent,void *child,const char *gate,int classified,int kind)
{
    void *root=s_root.task,*link=0;
    int root_readable=pointer_valid(root),child_readable=pointer_valid(child);
    unsigned int visit;
    if(D_800C7AB2!=0x1e1) return;
    visit=anchor_minigame_sync_visit();
    if(visit!=s_diag_birth_visit) {
        s_diag_birth_visit=visit;s_diag_birth_calls=s_diag_birth_registered=0;
    }
    ++s_diag_birth_calls;
    if(gate[0]=='o' && gate[1]=='k') ++s_diag_birth_registered;
    /* Pre-root room children cannot consume the controller's report budget. */
    if(s_diag_birth_calls>(s_root.task?32u:1u)) return;
    if(root_readable) link=EBI_PTR(root,4);
    recomp_printf("[Ebi birth] v=3 visit=%u/%u call=%u gate=%s parent=%08x root=%08x current=%08x child=%08x object=%08x bank=%04x resident=%d raw=%08x resolved=%08x kind=%d classify=%s\n",
        s_visit,visit,s_diag_birth_calls,gate,(unsigned int)(unsigned long)parent,
        (unsigned int)(unsigned long)root,(unsigned int)(unsigned long)D_8016DAB4_16E6B4,
        (unsigned int)(unsigned long)child,
        child_readable?(unsigned int)(unsigned long)EBI_PTR(child,0x18):0,
        child_readable?EH(child,0x28):0,classified?s_diag_birth_resident:-1,
        child_readable?(unsigned int)(unsigned long)EBI_AI(child):0,
        classified?(unsigned int)(unsigned long)s_diag_birth_resolved:0,kind,
        classified?s_diag_birth_classification:"not-attempted");
    recomp_printf("[Ebi birth root] object=%08x expectedObject=%08x entity=%04x/%04x gen=%u/%u status=%08x link=%08x back=%08x expectedCallbacks=%08x/%08x/%08x\n",
        root_readable?(unsigned int)(unsigned long)EBI_PTR(root,0x18):0,
        (unsigned int)(unsigned long)s_root.object,
        root_readable?EH(root,0x5c):0,s_root.entity,
        root_readable?EB(root,0x74):0,s_root.task_generation,
        root_readable?EW(root,0x68):0,(unsigned int)(unsigned long)link,
        pointer_valid(link)?(unsigned int)(unsigned long)EBI_PTR(link,0):0,
        (unsigned int)(unsigned long)func_0800139C_6F15AC,
        (unsigned int)(unsigned long)func_08001250_6F1460,
        (unsigned int)(unsigned long)func_08001720_6D0800);
    diagnostic_task("birth",child);
}
#else
#define diagnostic_birth(parent,child,gate,classified,kind) ((void)0)
#endif
#if DEBUG_BUTTON_ENABLED
static void diagnostic(const AnchorMinigameState *state)
{
    static const unsigned short banks[]={0x2a,0x20,0x1cf,0x173,0x1e3,0x1e6,
                                         0x197,0x159,0x1e1,0x1e2,0x191};
    int key[20], registered=0, initialized_count=0, live_count=0, bad=-1, same=1;
    unsigned int i,mask=0;
    void *t=s_root.task,*cam=0,*hud=0;
    int valid_task=pointer_valid(t),root_live=live(&s_root),motion;
    if(valid_task) { cam=EBI_PTR(t,0xd4);hud=EBI_PTR(t,0xd8); }
    for(i=0;i<sizeof(banks)/sizeof(banks[0]);++i)
        if(resident(banks[i])) mask|=1u<<i;
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i) {
        EbiActor *a=&s_actors[i];
        registered+=a->task!=0;initialized_count+=a->task && a->initialized;
        live_count+=live(a)!=0;
    }
    if(state && s_diag_captured)
        for(i=0;i<state->actor_count;++i)
            if(state->actor_words[i]!=actor_words(state->actors[i][ER_KIND]) || !row_valid(state->actors[i]) ||
               state->actors[i][ER_ID]>state->controller[EC_SERIAL]) { bad=(int)i;break; }
    motion=anchor_minigame_invites_character_motion_ready(0x1e1);
    key[0]=(int)s_visit;key[1]=(int)anchor_minigame_sync_visit();key[2]=D_800C7AB2;
    key[3]=root_live;key[4]=valid_task?EB(t,0xdc):-1;key[5]=root_ready();
    key[6]=(int)mask;key[7]=registered;key[8]=initialized_count;key[9]=live_count;
    key[10]=bad;key[11]=s_diag_captured;key[12]=state && s_diag_captured?state->ready:-1;
    key[13]=s_diag_valid;key[14]=s_active|(s_owner<<1)|(s_ready<<2);
    key[15]=anchor_minigame_sync_native_allowed(2);key[16]=motion;
    key[17]=s_overflow|(s_local_failure<<1)|(s_graph_staging<<2)|(s_root_resources_requested<<3);
    key[18]=(int)D_8015C5D8_15D1D8[1];
    key[19]=valid_task?(int)EH(t,0x5e):-1;
    for(i=0;i<20;++i) if(key[i]!=s_diag_key[i]) { same=0;break; }
    if(s_diag_reports>=64 || (s_diag_have_key && same &&
       (key[15] || s_diag_attempts-s_diag_last<300u))) return;
    __builtin_memcpy(s_diag_key,key,sizeof(key));s_diag_have_key=1;
    s_diag_last=s_diag_attempts;++s_diag_reports;
    recomp_printf("[Ebi startup] room=%04x visit=%u/%u root=%08x obj=%08x live=%d entity/model=%04x/%04x gen=%u phase=%d DE=%d E0=%d E2=%d camera=%08x/%d HUD=%08x/%d timer=%d rootReady=%d capture=%d ready=%d validate=%d actors=%d/%d/%d banks=%03x flags=%x role=%d/%d/%d allowed=%d char=%u motion=%d HP=%u\n",
        D_800C7AB2,s_visit,anchor_minigame_sync_visit(),(unsigned int)(unsigned long)t,
        (unsigned int)(unsigned long)s_root.object,root_live,
        valid_task?EH(t,0x5c):0,valid_task?EH(t,0x5e):0,valid_task?EB(t,0x74):0,
        key[4],valid_task?ES(t,0xde):0,valid_task?ES(t,0xe0):0,valid_task?ES(t,0xe2):0,
        (unsigned int)(unsigned long)cam,pointer_valid(cam),
        (unsigned int)(unsigned long)hud,pointer_valid(hud),pointer_valid(hud)?EI(hud,0x80):-1,
        key[5],s_diag_captured,key[12],s_diag_valid,registered,initialized_count,live_count,
        mask,key[17],s_active,s_owner,s_ready,key[15],D_8015C5D8_15D1D8[1],motion,D_8015C5D8_15D1D8[3]);
    {
        void *link=valid_task?EBI_PTR(t,4):0;
        recomp_printf("[Ebi root link] taskObject=%08x link=%08x back=%08x expectedEntity/gen=%04x/%u status=%08x rootChild=%08x\n",
            valid_task?(unsigned int)(unsigned long)EBI_PTR(t,0x18):0,
            (unsigned int)(unsigned long)link,
            pointer_valid(link)?(unsigned int)(unsigned long)EBI_PTR(link,0):0,
            s_root.entity,s_root.task_generation,valid_task?EW(t,0x68):0,
            valid_task?(unsigned int)(unsigned long)EBI_PTR(t,0xd0):0);
    }
    recomp_printf("[Ebi births] v=3 visit=%u calls=%u registered=%u expectedCallbacks=%08x/%08x/%08x\n",
        s_diag_birth_visit,s_diag_birth_calls,s_diag_birth_registered,
        (unsigned int)(unsigned long)func_0800139C_6F15AC,
        (unsigned int)(unsigned long)func_08001250_6F1460,
        (unsigned int)(unsigned long)func_08001720_6D0800);
    if(valid_task) {
        void *mechanism=EBI_PTR(t,0xd0);
        diagnostic_task("mechanism",mechanism);
        if(pointer_valid(mechanism)) diagnostic_task("platform",EBI_PTR(mechanism,0xd0));
    }
    recomp_printf("[Ebi startup] banks bits: 2A 20 1CF 173 1E3 1E6 197 159 1E1 1E2 191\n");
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i) {
        EbiActor *a=&s_actors[i];int readable=pointer_valid(a->task);
        if(!a->task) continue;
        recomp_printf("[Ebi actor] slot=%u kind=%u id=%u task=%08x obj=%08x init=%u live=%d entity/model=%04x/%04x gen=%u phase=%u/%u native=%08x\n",
            i,a->kind,a->id,(unsigned int)(unsigned long)a->task,(unsigned int)(unsigned long)a->object,
            a->initialized,live(a),readable?EH(a->task,0x5c):0,readable?EH(a->task,0x5e):0,
            readable?EB(a->task,0x74):0,readable?EB(a->task,0xd4):0,readable?EB(a->task,0xd0):0,
            (unsigned int)(unsigned long)a->native);
        {
            void *link=readable?EBI_PTR(a->task,4):0;
            recomp_printf("[Ebi actor link] slot=%u taskObject=%08x link=%08x back=%08x expectedEntity/gen=%04x/%u status=%08x child=%08x\n",
                i,readable?(unsigned int)(unsigned long)EBI_PTR(a->task,0x18):0,
                (unsigned int)(unsigned long)link,
                pointer_valid(link)?(unsigned int)(unsigned long)EBI_PTR(link,0):0,
                a->entity,a->task_generation,readable?EW(a->task,0x68):0,
                readable?(unsigned int)(unsigned long)EBI_PTR(a->task,0xd0):0);
        }
    }
    if(state && s_diag_captured && (bad>=0 || !s_diag_valid)) {
        recomp_printf("[Ebi controller] words=%u",state->controller_count);
        for(i=0;i<EC_WORDS;++i) recomp_printf(" %08x",(unsigned int)state->controller[i]);
        recomp_printf("\n");
        if(bad>=0) {
            recomp_printf("[Ebi invalid row] index=%d words=%u",bad,state->actor_words[bad]);
            for(i=0;i<ER_WORDS;++i) recomp_printf(" %08x",(unsigned int)state->actors[bad][i]);
            recomp_printf("\n");
        }
    }
}
static void diagnostic_capture(const AnchorMinigameState *s,int captured)
{
    if(s!=s_diag_capture || !captured || !s_diag_captured) s_diag_valid=-1;
    s_diag_capture=s;s_diag_captured=captured;
    ++s_diag_attempts;diagnostic(s);
}
#else
#define diagnostic_capture(s,captured) ((void)0)
#endif
static int validate(const AnchorMinigameState *s)
{
    int valid=validate_state(s);
#if DEBUG_BUTTON_ENABLED
    if(s==s_diag_capture) { s_diag_valid=valid;diagnostic(s); }
#endif
    return valid;
}
static int health_window(void)
{
    return s_health_started && live(&s_root) &&
           EB(s_root.task,0xdc)>=1 && EB(s_root.task,0xdc)<=6;
}
static void health_display(unsigned int ack)
{
    unsigned int i,pending=s_health_unqueued;
    int display;
    if(!s_active || !health_window()) return;
    if(!s_owner) for(i=0;i<s_input_count;++i)
        if(s_inputs[i][1]==EBI_HEALTH_EVENT && (unsigned int)s_inputs[i][0]>ack)
            pending+=(unsigned int)s_inputs[i][4];
    display=pending>=(unsigned int)s_health?0:s_health-(int)pending;
    D_8015C5D8_15D1D8[3]=(unsigned int)(17+display);
    if(pointer_valid(EBI_PTR(s_root.task,0xd8)))
        EI(EBI_PTR(s_root.task,0xd8),0x88)=display;
    if(s_health==0) {
        s_result=2;EB(s_root.task,0xdc)=8;ES(s_root.task,0xde)=0;
    }
}
RECOMP_HOOK("func_801DCD48_598C58")
void anchor_minigame_ebi_health_begin(signed char delta)
{
    void *link;
    s_health_observing=0;
    if(delta>=0 || !health_window() || s_graph_staging ||
       !pointer_valid(D_801FC604_5B8514) || D_8015C5D8_15D1D8[1]!=1) return;
    s_health_player=D_801FC604_5B8514;
    s_health_object=EBI_PTR(s_health_player,0x18);
    link=EBI_PTR(s_health_player,4);
    if(!pointer_valid(s_health_object) || !pointer_valid(link) || EBI_PTR(link,0)!=s_health_player) return;
    s_health_player_generation=EB(s_health_player,0x74);s_health_visit=s_visit;
    s_health_player_entity=EH(s_health_player,0x5c);
    s_health_before=(int)D_8015C5D8_15D1D8[3];s_health_observing=1;
}
static int queue(EbiActor *,int,unsigned int);
RECOMP_HOOK_RETURN("func_801DCD48_598C58")
void anchor_minigame_ebi_health_end(void)
{
    int lost;
    void *link;
    if(!s_health_observing) return;
    s_health_observing=0;
    if(!health_window() || s_health_visit!=s_visit ||
       s_health_player!=D_801FC604_5B8514 ||
       EBI_PTR(s_health_player,0x18)!=s_health_object ||
       EB(s_health_player,0x74)!=s_health_player_generation ||
       EH(s_health_player,0x5c)!=s_health_player_entity) return;
    link=EBI_PTR(s_health_player,4);
    if(!pointer_valid(link) || EBI_PTR(link,0)!=s_health_player) return;
    lost=s_health_before-(int)D_8015C5D8_15D1D8[3];
    if(lost<=0) return;if(lost>3) lost=3;
    if(s_owner || !s_active) s_health=lost>=s_health?0:s_health-lost;
    else if(!queue(0,EBI_HEALTH_EVENT,(unsigned int)lost)) {
        s_health_unqueued+=(unsigned int)lost;if(s_health_unqueued>3u) s_health_unqueued=3u;
    }
    health_display(local_ack());
}
static int capture(AnchorMinigameState *s)
{
    unsigned int i,j;
    void *t=s_root.task,*hud;
    int *c=s->controller;
    if(!root_ready() || !resident(0x2a) || s_overflow) {
        diagnostic_capture(s,0);return 0;
    }
    hud=EBI_PTR(t,0xd8);
    health_display(local_ack());
    s->controller_count=EC_WORDS;c[EC_VERSION]=2;c[EC_PHASE]=EB(t,0xdc);
    c[EC_PHASE_TIME]=ES(t,0xde);c[EC_TIME]=ES(t,0xe0);c[EC_THRESHOLD]=ES(t,0xe2);
    c[EC_TIMER]=EI(hud,0x80);c[EC_SWEETS]=D_080032F4_6F3504;
    c[EC_SERIAL]=(int)s_serial;c[EC_RESULT]=s_result;c[EC_FLAGS]=(int)EW(t,0x60);
    c[EC_CLIP]=(int)s_root.clip;c[EC_FRAME]=float_bits(EF(s_root.object,0x28));
    c[EC_HEALTH]=s_health;
    s->ready=root_resources_ready(s_root.clip) && !s_local_failure && !s_graph_staging;
    s->running=c[EC_PHASE]<7;s->result=s_result;
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i) {
        EbiActor *a=&s_actors[i];
        if(a->task && !a->initialized) { s->ready=0;continue; }
        if(!live(a)) continue;
        if(!a->initialized) { s->ready=0;continue; }
        if(!resources_ready(a->kind)) s->ready=0;
        j=s->actor_count++;
        s->actor_words[j]=actor_words(a->kind);capture_actor(a,s->actors[j]);
        if(!row_valid(s->actors[j])) s->ready=0;
    }
    /* Keep a claim until the committed graph has removed its actor. Repeated
     * capture is harmless: transport receipts deduplicate the same sequence. */
    for(i=0,j=0;i<s_input_count;++i) {
        int *q=s_inputs[i];
        if((unsigned int)q[0]<=local_ack()) continue;
        if(q[1]!=EBI_HEALTH_EVENT && !identity(q[1]==EBI_COLLECT?EBI_SWEET:EBI_ROBOT,
                     (unsigned int)q[2],(unsigned int)q[3])) continue;
        if(i!=j) __builtin_memcpy(s_inputs[j],q,sizeof(s_inputs[0]));
        s->input_words[j]=5;
        __builtin_memcpy(s->inputs[j],s_inputs[j],sizeof(s_inputs[0]));
        ++j;
    }
    s_input_count=j;s->input_count=j;
    if(s_health_unqueued && queue(0,EBI_HEALTH_EVENT,s_health_unqueued)) {
        s_health_unqueued=0;s->input_words[j]=5;
        __builtin_memcpy(s->inputs[j],s_inputs[j],sizeof(s_inputs[0]));++s->input_count;
    }
    diagnostic_capture(s,1);return 1;
}
static int queue(EbiActor *a,int kind,unsigned int amount)
{
    unsigned int i;
    int *r;
    for(i=0;i<s_input_count;++i)
        if((unsigned int)s_inputs[i][0]>local_ack() &&
           kind!=EBI_HEALTH_EVENT && s_inputs[i][1]==kind &&
           (unsigned int)s_inputs[i][2]==a->id &&
           (unsigned int)s_inputs[i][3]==a->generation) return 1;
    if(s_input_count==ANCHOR_MINIGAME_MAX_INPUTS || s_input_serial>=0x7fffffffu) return 0;
    r=s_inputs[s_input_count++];r[0]=(int)++s_input_serial;r[1]=kind;
    r[2]=a?(int)a->id:1;r[3]=a?(int)a->generation:1;r[4]=(int)amount;
    return 1;
}
static int classify(EbiCallback ai)
{
    if(callback_is(ai,func_0800139C_6F15AC)) return EBI_MECHANISM;
    if(callback_is(ai,func_08001250_6F1460)) return EBI_PLATFORM;
    if(callback_is(ai,func_08001720_6D0800)) return EBI_ROBOT;
    if(callback_is(ai,func_08000E2C_6F103C)) return EBI_BOMB;
    if(callback_is(ai,func_08001030_6F1240)) return EBI_SWEET;
    if(callback_is(ai,func_08000B58_6F0D68)) return EBI_BLAST;
    return 0;
}
static int classify_child(void *child)
{
    unsigned int bank=EH(child,0x28);
    EbiCallback callback=EBI_AI(child);
    unsigned long raw=(unsigned long)callback&~0x00800000ul;
    void *handle;
    int kind;
#if DEBUG_BUTTON_ENABLED
    s_diag_birth_resolved=callback;s_diag_birth_resident=-1;
    s_diag_birth_classification="bank";
#endif
    if(bank!=0x2a && bank!=0x20) return 0;
    handle=func_800141C4_14DC4(bank);
    if(!handle || handle==(void *)(unsigned long)0xffffffffu) {
#if DEBUG_BUTTON_ENABLED
        s_diag_birth_resident=0;s_diag_birth_classification="not-resident";
#endif
        return 0;
    }
#if DEBUG_BUTTON_ENABLED
    s_diag_birth_resident=1;
#endif
    /* Mirror patched1481C's callback address, not legacy14840's return.
     * These banks use native segment11, while recomp callbacks use segment08.
     * Only compare a read-only copy; the child's first slot owns relocation. */
    if(raw>=0x08000000ul && raw<0x09000000ul)
        callback=(EbiCallback)(unsigned long)((unsigned int)raw-0x08000000u+
                    ((unsigned int)(unsigned long)handle&~0x40000000u));
    kind=classify(callback);
#if DEBUG_BUTTON_ENABLED
    s_diag_birth_resolved=callback;
    s_diag_birth_classification=!kind?"callback":
        bank!=(kind==EBI_ROBOT?0x20u:0x2au)?"callback-bank":"matched";
#endif
    return kind && bank==(kind==EBI_ROBOT?0x20u:0x2au)?kind:0;
}
static void clear_pending(void *task)
{
    unsigned int i;
    for(i=0;i<2;++i) if(!task || s_pending_blasts[i].parent==task || s_pending_blasts[i].child==task)
        anchor_minigame_zero(&s_pending_blasts[i],sizeof(s_pending_blasts[i]));
}
static int pending_blast(void *parent,void *child)
{
    EbiActor *a=find(parent);
    unsigned int i;
    unsigned long raw=EBI_CALLBACK_VALUE(EBI_AI(child))&~0x00800000ul;
    int proxy=s_birth_row && s_birth_row[ER_KIND]==EBI_BLAST && parent==s_root.task;
    void *base,*link;
    if(raw_find(child) || EH(child,0x28)!=0 || EBI_PTR(child,0x2c) ||
       (!proxy && (!a || !a->initialized || a->kind!=EBI_BOMB || a->entity!=0x1ac ||
                   parent!=D_8016DAB4_16E6B4)) ||
       (raw!=0x08000b58ul && !callback_is(EBI_AI(child),func_08000B58_6F0D68))) return 0;
    base=func_800141C4_14DC4(0x2a);
    link=EBI_PTR(child,4);
    if(!base || base==(void *)(unsigned long)0xffffffffu || !pointer_valid(link) || EBI_PTR(link,0)!=child) return 0;
    for(i=0;i<2;++i) if(!s_pending_blasts[i].child) break;
    if(i==2) { s_overflow=1;return 0; }
    s_pending_blasts[i]=(EbiPendingBlast){parent,EBI_PTR(parent,0x18),child,EBI_PTR(child,0x18),link,
                                     proxy?s_birth_row:0,s_visit,EH(parent,0x5c),EH(parent,0x5e),EB(parent,0x74)};
    return 1;
}
static void confirm_blasts(void *parent,void *only_child)
{
    unsigned int i;
    for(i=0;i<2;++i) {
        EbiPendingBlast *p=&s_pending_blasts[i];
        EbiActor *a,*owner;
        void *base;
        if(!p->child || p->parent!=parent || (only_child && p->child!=only_child)) continue;
        owner=find(parent);base=func_800141C4_14DC4(0x2a);
        if(p->visit==s_visit && live(&s_root) && pointer_valid(parent) &&
           EBI_PTR(parent,0x18)==p->parent_object && EH(parent,0x5c)==p->entity && EH(parent,0x5e)==p->model && EB(parent,0x74)==p->generation &&
           ((p->row && p->row==s_birth_row && parent==s_root.task) ||
            (!p->row && owner && owner->kind==EBI_BOMB && owner->initialized && owner->entity==0x1ac &&
             parent==D_8016DAB4_16E6B4)) &&
           pointer_valid(p->child) && EBI_PTR(p->child,0x18)==p->object &&
           EBI_PTR(p->child,4)==p->link && EBI_PTR(p->link,0)==p->child &&
           EH(p->child,0x5c)==p->entity && EH(p->child,0x5e)==p->model && EB(p->child,0x74)==p->generation &&
           !(EW(p->child,0x68)&2u) && EH(p->child,0x28)==0x2a &&
           base && base!=(void *)(unsigned long)0xffffffffu && EBI_PTR(p->child,0x2c)==base &&
           (EW(p->child,0xe8)==1u || EW(p->child,0xe8)==4u) && classify_child(p->child)==EBI_BLAST) {
            a=record(p->child,EBI_BLAST);
            if(a && p->row) { a->id=(unsigned int)p->row[ER_ID];a->generation=(unsigned int)p->row[ER_GENERATION];a->proxy=1; }
        }
        anchor_minigame_zero(p,sizeof(*p));
    }
}
static void bomb_initialize(void *t,void *o)
{
    EbiActor *a=find(t);
    if(!a || a->kind!=EBI_BOMB || !resident(0x2a)) return;
    EW(t,0x60)=0x02a087e1u;EW(t,0x64)|=0x8000u;
    func_80216E1C_5D22EC(t,0);func_80219E70_5D5340(t,10,0);
    func_8021A310_5D57E0(t);EB(t,0xd0)=1;
    a->native=func_08000E2C_6F103C;a->initialized=1;EBI_AI(t)=ebi_actor_update;
    (void)o;
}
/* Register births before their first scheduler slot. The constructor assigns
 * identity after this hook; the verified initializer hook refreshes it. */
RECOMP_HOOK("func_80218C28_5D40F8")
void anchor_minigame_ebi_child(void *parent,void *child)
{
    int kind;
    EbiActor *a,*p=raw_find(parent);
    int initializing=p && !p->initialized && parent==D_8016DAB4_16E6B4 &&
                     EBI_PTR(parent,0x18)==p->object;
    if(!live(&s_root)) { diagnostic_birth(parent,child,"root",0,0);return; }
    if(!find(parent) && !initializing && parent!=s_root.task) {
        diagnostic_birth(parent,child,"parent",0,0);return;
    }
    if(!pointer_valid(child)) { diagnostic_birth(parent,child,"child",0,0);return; }
    if(!pointer_valid(EBI_PTR(child,0x18))) {
        diagnostic_birth(parent,child,"object",0,0);return;
    }
    if(pending_blast(parent,child)) { diagnostic_birth(parent,child,"pending-blast",1,EBI_BLAST);return; }
    kind=classify_child(child);
    if(!kind) { diagnostic_birth(parent,child,"classify",1,0);return; }
    a=record(child,kind);
    if(!a) { diagnostic_birth(parent,child,"capacity",1,kind);return; }
    if(s_birth_row) {
        a->id=(unsigned int)s_birth_row[ER_ID];a->generation=(unsigned int)s_birth_row[ER_GENERATION];
        a->proxy=1;
    }
    if(kind==EBI_BOMB && !s_birth_row && !a->proxy && !a->initialized &&
       parent==s_drop_scope.root && parent==D_8016DAB4_16E6B4 &&
       s_drop_scope.branch && s_drop_scope.visit==s_visit && a->id>s_drop_scope.serial &&
       !s_drop_scope.bomb) {
        s_drop_scope.bomb=a;s_drop_scope.child=child;s_drop_scope.child_object=a->object;
        s_drop_scope.bomb_id=a->id;
    }
    diagnostic_birth(parent,child,"ok",1,kind);
}
static EbiActor *construct(const int *r)
{
    EbiActor *a;
    void *t;
    unsigned int entity;
    if(r[ER_KIND]==EBI_MECHANISM || r[ER_KIND]==EBI_PLATFORM) return 0;
    s_birth_row=r;
    if(r[ER_KIND]==EBI_BLAST) {
        t=func_8021DDE8_5D92B8(s_root.task,func_08000B58_6F0D68,
                             r[ER_MODE]==1?0:1,0.f,0.f,0.f,0);
        if(t) { EW(t,0xe8)=(unsigned int)r[ER_MODE];EH(t,0x28)=0x2a;
                 EBI_PTR(t,0x2c)=func_800141C4_14DC4(0x2a);confirm_blasts(s_root.task,t); }
    } else {
        entity=r[ER_KIND]==EBI_ROBOT?0xfbu:r[ER_KIND]==EBI_BOMB?0x1acu:0x1adu;
        t=func_80217360_5D2830(s_root.task,(unsigned short)entity,
                              r[ER_KIND]==EBI_SWEET?9:6);
        if(t && r[ER_KIND]!=EBI_SWEET) {
            func_80217144_5D2614(t);
            if(r[ER_KIND]==EBI_ROBOT) EW(t,0x64)|=0x8000u;
        }
    }
    clear_pending(s_root.task);s_birth_row=0;
    if(!t) return 0;
    /* Allocation may have called the birth hook before assigning identity. */
    a=0;
    { unsigned int i;for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i)
        if(s_actors[i].task==t) { a=&s_actors[i];break; } }
    if(!a) { func_80034EF8_35AF8(t);return 0; }
    a->entity=EH(t,0x5c);a->task_generation=EB(t,0x74);
    a->id=(unsigned int)r[ER_ID];a->generation=(unsigned int)r[ER_GENERATION];a->proxy=1;
    EF(a->object,8)=bits_float(r[ER_X]);EF(a->object,0xc)=bits_float(r[ER_Y]);
    EF(a->object,0x10)=bits_float(r[ER_Z]);
    if(a->kind==EBI_BOMB) EBI_AI(t)=bomb_initialize;
    return a;
}
static void apply_actor(EbiActor *a,const int *r,unsigned int generation,unsigned int full_tick)
{
    unsigned int i,n=actor_words(a->kind);
    int same=1,hard=!a->have_net || a->checkpoint_generation!=generation;
    float old_frame=EF(a->object,0x28),old_xyz[3];
    void *t=a->task,*o=a->object;
    for(i=0;i<n;++i) if(a->net[i]!=r[i]) { same=0;break; }
    if(a->have_net && same && !hard) {
        if(full_tick!=a->full_tick) { a->full_tick=full_tick;a->prediction_age=0; }
        return;
    }
    if(a->have_net && (a->net[ER_AI]!=r[ER_AI] || a->net[ER_CLIP]!=r[ER_CLIP])) hard=1;
    for(i=0;i<3;++i) {
        old_xyz[i]=EF(o,8+4*i);
        if(old_xyz[i]-bits_float(r[ER_X+i])>80.f ||
           old_xyz[i]-bits_float(r[ER_X+i])< -80.f) hard=1;
    }
    if(a->clip!=(unsigned int)r[ER_CLIP] && a->kind!=EBI_BLAST) {
        func_8021664C_5D1B1C(t,(unsigned int)r[ER_CLIP],0.05f,0);
        a->clip=(unsigned int)r[ER_CLIP];
    }
    for(i=0;i<3;++i) {
        EF(o,8+4*i)=bits_float(r[ER_X+i]);EF(o,0x1c+4*i)=bits_float(r[ER_SX+i]);
        EF(t,0x78+4*i)=bits_float(r[ER_VX+i]);
    }
    EH(o,0x14)=(unsigned short)r[ER_ANGLES];EH(o,0x16)=(unsigned short)((unsigned int)r[ER_ANGLES]>>16);
    EH(o,0x18)=(unsigned short)r[ER_ROLL];EF(o,0x28)=bits_float(r[ER_FRAME]);
    EB(o,0x7c)=(unsigned char)r[ER_EXTRA+7];EH(o,0x7e)=(unsigned short)((unsigned int)r[ER_EXTRA+7]>>16);
    EW(t,0x60)=(unsigned int)r[ER_FLAGS];EW(t,0x64)=(unsigned int)r[ER_FLAGS2];
    EH(t,0x8a)=(unsigned short)r[ER_LIFE];EB(t,0x8c)=(unsigned char)((unsigned int)r[ER_LIFE]>>16);
    EB(t,0x8d)=(unsigned char)((unsigned int)r[ER_LIFE]>>24);
    EH(t,0x94)=(unsigned short)r[ER_ACTION];EH(t,0x96)=(unsigned short)((unsigned int)r[ER_ACTION]>>16);
    EW(t,0x68)=(unsigned int)r[ER_STATUS]&~0x280u;EW(t,0xe8)=(unsigned int)r[ER_MODE];
    EH(t,0x3c)=(unsigned short)r[ER_EXTRA+4];EH(t,0x3e)=(unsigned short)((unsigned int)r[ER_EXTRA+4]>>16);
    EH(t,0x40)=(unsigned short)r[ER_EXTRA+5];EH(t,0x4e)=(unsigned short)((unsigned int)r[ER_EXTRA+5]>>16);
    EH(t,0x50)=(unsigned short)r[ER_EXTRA+6];EH(t,0x52)=(unsigned short)((unsigned int)r[ER_EXTRA+6]>>16);
    EW(t,0x98)=(unsigned int)r[ER_EXTRA+8];
    EI(o,0x68)=(int)EB(t,0x99)/2;EI(o,0x6c)=(int)EB(t,0x98);
    if(a->kind==EBI_ROBOT) {
        a->native=s_robot_phases[r[ER_AI]];
        EH(t,0xa0)=(unsigned short)r[ER_EXTRA];EH(t,0xa2)=(unsigned short)((unsigned int)r[ER_EXTRA]>>16);
        EB(t,0xa8)=(unsigned char)r[ER_EXTRA+1];EB(t,0xa9)=(unsigned char)((unsigned int)r[ER_EXTRA+1]>>8);
        ES(t,0xb4)=(short)r[ER_EXTRA+2];EF(t,0xe0)=bits_float(r[ER_EXTRA+3]);
        EH(t,0xd0)=(unsigned short)r[ER_ORBIT];EH(t,0xd2)=(unsigned short)((unsigned int)r[ER_ORBIT]>>16);
        EB(t,0xd4)=(unsigned char)r[ER_RADII];EB(t,0xd5)=(unsigned char)((unsigned int)r[ER_RADII]>>8);
        EB(t,0xd6)=(unsigned char)((unsigned int)r[ER_RADII]>>16);
        EF(t,0xd8)=bits_float(r[ER_ORBIT_X]);EF(t,0xdc)=bits_float(r[ER_ORBIT_Z]);
        a->target_cid=r[ER_TARGET];
        for(i=0;i<3;++i) a->target_view[i+2]=bits_float(r[ER_TARGET_X+i]);
    } else if(a->kind==EBI_MECHANISM) {
        EB(t,0xd4)=(unsigned char)r[ER_EXTRA];EB(t,0xd5)=(unsigned char)((unsigned int)r[ER_EXTRA]>>8);
        EF(t,0xd8)=bits_float(r[ER_EXTRA+1]);EF(t,0xdc)=bits_float(r[ER_EXTRA+2]);
    } else {
        EB(t,0xd0)=(unsigned char)r[ER_EXTRA];
        if(a->kind==EBI_SWEET) EB(t,0xd1)=(unsigned char)((unsigned int)r[ER_EXTRA]>>8);
        if(a->kind==EBI_BLAST) {
            void *material;
            ES(t,0xd2)=(short)r[ER_EXTRA+1];
            material=r[ER_MODE]==1?
                func_8021A26C_5D573C(t,(unsigned int)r[ER_EXTRA+1],0,0,0):
                func_8021DD4C_5D921C(t,(unsigned int)r[ER_EXTRA+1],0,0,0);
            /* File42: lui 0x2000, OR helper return, store object+30. */
            EBI_PTR(o,0x30)=(void *)((unsigned long)material|0x20000000ul);
            a->contact_expired=0;
            EB(t,0x4c)=0;
            if(r[ER_MODE]==4) {
                EB(t,0x6d)=1;
                if((r[ER_LIFE]&0xffff)<30) { func_80218DA8_5D4278(t,1200,0,0);EB(t,0x4c)=1; }
            }
        }
    }
    EBI_AI(t)=ebi_actor_update;
    if(!hard) {
        for(i=0;i<3;++i) {
            a->correction[i]=bits_float(r[ER_X+i])-old_xyz[i];EF(o,8+4*i)=old_xyz[i];
        }
        EF(o,0x28)=old_frame;a->correction_ticks=8;
    } else a->correction_ticks=0;
    __builtin_memcpy(a->net,r,n*sizeof(a->net[0]));a->have_net=1;
    a->prediction_age=0;a->checkpoint_generation=generation;a->full_tick=full_tick;
}
static int apply(const AnchorMinigameState *s)
{
    unsigned int i,j;
    EbiActor *map[ANCHOR_MINIGAME_MAX_ACTORS];
    const int *c=s->controller;
    void *t,*hud;
    if(!validate(s) || !root_ready() || !resident(0x2a)) return 0;
    /* This player's native result has already cleared its enemies. Preserve
     * that cleanup instead of rebuilding an ongoing host graph around it. */
    if(s_result && EB(s_root.task,0xdc)>=7) return !s_local_failure;
    s_graph_staging=1;
    for(i=0;i<s->actor_count;++i)
        if(s->actors[i][ER_KIND]==EBI_ROBOT && !resident(0x20)) return 0;
    /* An entire validated snapshot is an explicit dynamic lifetime census.
     * Retire obsolete owned tasks during staging, without rewards, to free
     * native pool slots before constructing their replacements. Keeping both
     * generations alive would deadlock when the stock 60/61-task pool is full.
     * No shared controller or retained actor scalar commits during staging. */
    for(i=0;i<ANCHOR_MINIGAME_MAX_ACTORS;++i) {
        EbiActor *a=&s_actors[i];
        if(!a->task || a->kind==EBI_MECHANISM || a->kind==EBI_PLATFORM) continue;
        for(j=0;j<s->actor_count;++j) {
            const int *r=s->actors[j];
            if(a->kind==r[ER_KIND] && a->id==(unsigned int)r[ER_ID] &&
               a->generation==(unsigned int)r[ER_GENERATION]) break;
        }
        if(j==s->actor_count && owned_handle(a)) {
            void *dead=a->task;a->task=0;func_80034EF8_35AF8(dead);
        }
    }
    /* Construct and wait for all native model/collision initializers. No live
     * controller, timer, existing actor or local backup changes during staging. */
    for(i=0;i<s->actor_count;++i) {
        const int *r=s->actors[i];
        map[i]=identity(r[ER_KIND],(unsigned int)r[ER_ID],(unsigned int)r[ER_GENERATION]);
        if(!map[i]) map[i]=construct(r);
        if(!map[i] || !map[i]->initialized) return 0;
    }
    for(i=0;i<s->actor_count;++i)
        if(!resources_ready(s->actors[i][ER_KIND])) return 0;
    if(!root_resources_ready((unsigned int)c[EC_CLIP])) return 0;
    for(i=0;i<s->actor_count;++i) apply_actor(map[i],s->actors[i],s->adopted_generation,s->full_tick);
    t=s_root.task;hud=EBI_PTR(t,0xd8);
    /* A local failure already owns native result/cleanup. An arriving host
     * snapshot must not rewind that player into an active controller phase. */
    if(EB(t,0xdc)<7 && c[EC_PHASE]<7) {
        EB(t,0xdc)=(unsigned char)c[EC_PHASE];ES(t,0xde)=(short)c[EC_PHASE_TIME];
        ES(t,0xe0)=(short)c[EC_TIME];ES(t,0xe2)=(short)c[EC_THRESHOLD];
    }
    /* ACTION snapshots have fresh controller clocks independently of the full
     * actor census. Receipt/readiness reapply must not rewind this local clock. */
    if(!s_timer_stamped || s->adopted_generation!=s_timer_generation || s->source_tick>s_timer_tick) {
        EI(hud,0x80)=c[EC_TIMER];s_timer_age=0;s_timer_stamped=1;
        s_timer_generation=s->adopted_generation;s_timer_tick=s->source_tick;
    }
    EI(hud,0x84)=c[EC_SWEETS];
    D_080032F4_6F3504=(unsigned char)c[EC_SWEETS];s_serial=(unsigned int)c[EC_SERIAL];
    EW(t,0x60)=(unsigned int)c[EC_FLAGS];
    if(s_root.clip!=(unsigned int)c[EC_CLIP]) {
        func_8021664C_5D1B1C(t,(unsigned int)c[EC_CLIP],0.05f,0);
        s_root.clip=(unsigned int)c[EC_CLIP];
    }
    EF(s_root.object,0x28)=bits_float(c[EC_FRAME]);
    s_health=c[EC_HEALTH];s_health_started=1;
    if(s->full_tick!=s_full_tick || s->adopted_generation!=s_checkpoint_generation) s_prediction_age=0;
    s_full_tick=s->full_tick;s_checkpoint_generation=s->adopted_generation;
    if(s->local_ack>s_ack_fence) s_ack_fence=s->local_ack;
    health_display(s->local_ack);
    if(c[EC_RESULT] && !s_result && EB(t,0xdc)<7) {
        s_result=c[EC_RESULT];EB(t,0xdc)=(unsigned char)(s_result==1?7:8);ES(t,0xde)=0;
    }
    s_graph_staging=0;
    return 1;
}
static void camera(void)
{
    void *cam,*o,*destination;
    float x,y,z,duration,divisor;
    unsigned int p;
    if(!live(&s_root) || !pointer_valid(D_801FC604_5B8514) ||
       !pointer_valid(o=EBI_PTR(D_801FC604_5B8514,0x18)) ||
       !pointer_valid(cam=EBI_PTR(s_root.task,0xd4))) return;
    x=EF(o,8);if(x < -112.f) x=-112.f;if(x > 112.f) x=112.f;
    y=EF(o,0xc)+15.f;z=EF(o,0x10);p=EB(s_root.task,0xdc);
    if(p==1 || p==2) {
        duration=10.f-z/-186.f*9.f;if(duration<0.f) duration=-duration;
        if(duration<0.01f) duration=0.01f;
        func_800124A0_130A0(cam,duration);
        if(p==1) func_800124AC_130AC(cam,2048.f);
        func_80012628_13228(cam,x,y,z);
        func_80012518_13118(cam,x,-20.f-z/-186.f*50.f,-186.f);
    } else if(p>=3 && p<=6) {
        divisor=(float)(p==3?D_080032B0_6F34C0:p==4?D_080032B8_6F34C8:
                        p==5?D_080032C8_6F34D8:D_080032C0_6F34D0);
        if(divisor==0.f) return;
        x/=divisor;
        if(p==3 || p==6) func_800124A0_130A0(cam,20.f);
        if(p==3) func_800124AC_130AC(cam,4096.f);
        if(p==5) func_800124AC_130AC(cam,2048.f);
        func_80012628_13228(cam,x,-40.f,0.f);
        func_80012518_13118(cam,x,-20.f-z/-186.f*20.f,-186.f);
        if(p==6) {
            func_800124A0_130A0(cam,2.f);
            func_80012764_13364(cam,(float)(((unsigned int)ES(s_root.task,0xde)&4u)*4u)-40.f);
        }
    }
    /* Match the root's local scene-camera commit after its native setters.
     * The tagged destination is local scene storage, never packet data. */
    if(p>=1 && p<=6 && pointer_valid(D_801FC628_5B8538) &&
       pointer_valid(destination=EBI_CAMERA_DEST(D_801FC628_5B8538)))
        func_80012900_13500(cam,destination);
}
static int target_position(int cid,float *xyz)
{
    int i;
    if(!anchor_minigame_sync_participant_current(2,cid)) return 0;
    for(i=0;i<s_target_count;++i) if(s_targets[i].cid==cid) {
        xyz[0]=s_targets[i].x;xyz[1]=s_targets[i].y;xyz[2]=s_targets[i].z;return 1;
    }
    return 0;
}
static void select_target(EbiActor *a)
{
    int i,best=-1;
    float distance=0.f;
    for(i=0;i<s_target_count;++i) {
        float dx,dz,d;
        if(!anchor_minigame_sync_participant_current(2,s_targets[i].cid)) continue;
        dx=s_targets[i].x-EF(a->object,8);dz=s_targets[i].z-EF(a->object,0x10);d=dx*dx+dz*dz;
        if(best<0 || d<distance) { best=i;distance=d; }
    }
    a->target_cid=best<0?0:s_targets[best].cid;
    if(best>=0) {
        a->target_view[2]=s_targets[best].x;a->target_view[3]=s_targets[best].y;
        a->target_view[4]=s_targets[best].z;
    } else { a->target_view[2]=EF(a->object,8);a->target_view[3]=EF(a->object,0xc);a->target_view[4]=EF(a->object,0x10); }
}
static void target_reset(void *task)
{
    if(s_target_actor && s_active && s_owner && s_ready &&
       task==s_target_actor->task && task==D_8016DAB4_16E6B4 && live(s_target_actor)) {
        if(s_target_actor->target_cid && !anchor_minigame_sync_participant_current(2,s_target_actor->target_cid)) {
            unsigned int i;s_target_actor->target_cid=0;
            for(i=0;i<3;++i) s_target_actor->target_view[i+2]=EF(s_target_actor->object,8+4*i);
        }
        EBI_PTR(task,0x84)=s_target_actor->target_view;
    }
}
RECOMP_HOOK_RETURN("func_8021927C_5D474C")
void anchor_minigame_ebi_target_primary(void *task) { target_reset(task); }
RECOMP_HOOK_RETURN("func_8021928C_5D475C")
void anchor_minigame_ebi_target_secondary(void *task) { target_reset(task); }
static void predict_actor(EbiActor *a)
{
    unsigned int i;
    void *t=a->task,*o=a->object;
    if(!a->have_net || a->prediction_age>=60u) { a->prediction_age=61;return; }
    ++a->prediction_age;
    if(a->kind==EBI_MECHANISM) {
        float difference=EF(t,0xd8)-EF(o,8),speed=EF(t,0xdc);
        if(EB(t,0xd5)==1) EH(o,0x18)^=2u;
        if(speed>0.001f) {
            if(difference<speed && difference> -speed) { EF(o,8)=EF(t,0xd8);EF(t,0xdc)=0.f; }
            else EF(o,8)+=difference<0.f?-speed:speed;
        }
    } else if(a->kind==EBI_ROBOT) {
        int p=a->net[ER_AI];
        float xyz[3],speed=__builtin_sqrtf(bits_float(a->net[ER_VX])*bits_float(a->net[ER_VX])+
                                        bits_float(a->net[ER_VZ])*bits_float(a->net[ER_VZ]));
        for(i=0;i<3;++i) xyz[i]=a->target_view[i+2];
        if(a->target_cid && !target_position(a->target_cid,xyz)) {
            a->target_cid=0;EF(t,0x78)=EF(t,0x80)=0.f;
        }
        for(i=0;i<3;++i) a->target_view[i+2]=xyz[i];
        if(p==3 || p==4) {
            EH(o,0x16)=(unsigned short)((EH(o,0x16)+(p==3?32:ES(t,0x8a)))&1023);
            if(ES(t,0x8a)>0) --ES(t,0x8a);EF(t,0x78)=EF(t,0x80)=0.f;
        } else if(p==5) { if(ES(t,0x8a)>0) --ES(t,0x8a); }
        else if(p==7 && a->target_cid)
            func_802196FC_5D4BCC(t,xyz[0],xyz[1],xyz[2],8);
        else if(p==8) {
            if(a->target_cid) {
                float dx=xyz[0]-EF(o,8),dz=xyz[2]-EF(o,0x10);
                float length=__builtin_sqrtf(dx*dx+dz*dz);
                if(length>0.f && !(dx<5.f && dx> -5.f && dz<5.f && dz> -5.f)) {
                    EF(t,0x78)=dx/length*speed;EF(t,0x80)=dz/length*speed;
                }
            }
            if(EF(t,0x78)!=0.f || EF(t,0x80)!=0.f)
                EH(o,0x16)=(unsigned short)(func_8021ADD0_5D62A0(EF(t,0x78),EF(t,0x80))&1023u);
        } else if(p==10) {
            func_80219448_5D4918(t);
            if(EF(t,0x78)!=0.f || EF(t,0x80)!=0.f)
                EH(o,0x16)=(unsigned short)(func_8021ADD0_5D62A0(EF(t,0x78),EF(t,0x80))&1023u);
        } else if(p==11) func_802196FC_5D4BCC(t,(float)ES(t,0xa0),0.f,(float)ES(t,0xa2),8);
        else if(p==12) func_8021BB04_5D6FD4(t,(float)ES(t,0xa0),(float)ES(t,0xa2),2.f);
    } else if(a->kind==EBI_SWEET) {
        unsigned int life=EH(t,0x8a);
        if(life) EH(t,0x8a)=(unsigned short)(life-1);
        if(life<120) EW(t,0x60)=(EW(t,0x60)&~0x20u)|((life&1u)?0x20u:0u);
    } else if(a->kind==EBI_BLAST) {
        int alpha=ES(t,0xd2)-2;
        void *material;
        if(!EH(t,0x8a)) { a->contact_expired=1;return; }
        if(EW(t,0xe8)==4) { func_80218DA8_5D4278(t,1200,0,0);EB(t,0x4c)=1;EB(t,0x6d)=1; }
        if(alpha<28) alpha=28;ES(t,0xd2)=(short)alpha;
        for(i=0;i<3;++i) EF(o,0x1c+4*i)+=EW(t,0xe8)==1?0.03f:0.02f;
        EH(o,0x16)=(unsigned short)((EH(o,0x16)+64)&1023);
        material=EW(t,0xe8)==1?func_8021A26C_5D573C(t,(unsigned int)alpha,0,0,0):
                                     func_8021DD4C_5D921C(t,(unsigned int)alpha,0,0,0);
        EBI_PTR(o,0x30)=(void *)((unsigned long)material|0x20000000ul);
        if(EH(t,0x8a)) --EH(t,0x8a);
    }
}
static int drop_hidden(const float *xyz)
{
    static const short pads[4][2]={{80,-49},{49,48},{-96,99},{-62,-23}};
    unsigned int i;
    if(!(xyz[1]+15.f< -40.f)) return 0;
    for(i=0;i<4;++i) {
        float dx=(float)pads[i][0]-xyz[0],dz=(float)pads[i][1]-xyz[2];
        if(__builtin_sqrtf(dx*dx+dz*dz)<14.f) return 1;
    }
    return 0;
}
static int drop_candidate(int index,const float *host,float *xyz,int remote_only)
{
    int cid=s_targets[index].cid;
    unsigned int i;
    if(cid<=0 || !s_local_cid || (remote_only && cid==s_local_cid) ||
       !anchor_minigame_sync_participant_current(2,cid)) return 0;
    xyz[0]=s_targets[index].x;xyz[1]=s_targets[index].y;xyz[2]=s_targets[index].z;
    if(cid==s_local_cid) for(i=0;i<3;++i) xyz[i]=host[i];
    for(i=0;i<3;++i) if(!float_valid(float_bits(xyz[i]),200000.f)) return 0;
    if(xyz[0]< -199900.f || xyz[0]>199900.f || xyz[2]< -199900.f || xyz[2]>199900.f) return 0;
    return !drop_hidden(xyz);
}
static int choose_drop_target(const float *host,float *out,int remote_only)
{
    int i,next=0,first=0;
    float xyz[3],next_xyz[3],first_xyz[3];
    unsigned int j;
    for(i=0;i<s_target_count;++i) {
        int cid=s_targets[i].cid;
        if(!drop_candidate(i,host,xyz,remote_only)) continue;
        if(!first || cid<first) { first=cid;for(j=0;j<3;++j) first_xyz[j]=xyz[j]; }
        if(cid>s_drop_last_cid && (!next || cid<next)) {
            next=cid;for(j=0;j<3;++j) next_xyz[j]=xyz[j];
        }
    }
    if(next) { for(j=0;j<3;++j) out[j]=next_xyz[j];return next; }
    if(first) for(j=0;j<3;++j) out[j]=first_xyz[j];
    return first;
}
static float scatter_center(float x)
{
    if(x< -112.f) x=-112.f;if(x>112.f) x=112.f;
    return (float)((double)x/1.2);
}
static int drop_root_current(const EbiDropScope *scope)
{
    return scope->root==s_root.task && scope->root==D_8016DAB4_16E6B4 &&
           scope->object==s_root.object && scope->visit==s_visit && live(&s_root) &&
           scope->entity==EH(scope->root,0x5c) &&
           scope->generation==EB(scope->root,0x74) &&
           scope->host_object==D_801FC60C_5B851C && !s_result && EB(scope->root,0xdc)<7;
}
static void retarget_drop(void)
{
    EbiDropScope *scope=&s_drop_scope;
    EbiActor *a=scope->bomb;
    float old_center,new_center,x,z;
    if(!scope->branch || !a || !s_active || !s_owner || !s_ready || !drop_root_current(scope) ||
       !anchor_minigame_sync_participant_current(2,scope->target_cid) ||
       a->kind!=EBI_BOMB || a->proxy || a->initialized || a->id!=scope->bomb_id ||
       a->id<=scope->serial || a->task!=scope->child || a->object!=scope->child_object ||
       !owned_handle(a) || EH(a->task,0x5c)!=0x1ac || EH(a->task,0x5e)!=0x1ac ||
       EB(a->task,0x74)!=scope->generation || EH(a->task,0x28)!=0x2a ||
       (EW(a->task,0x68)&2u) || classify_child(a->task)!=EBI_BOMB) return;
    old_center=scope->branch==1?scope->host[0]:scatter_center(scope->host[0]);
    new_center=scope->branch==1?scope->target[0]:scatter_center(scope->target[0]);
    x=EF(a->object,8)+(new_center-old_center);
    z=EF(a->object,0x10)+(scope->target[2]-scope->host[2]);
    if(!float_valid(float_bits(x),200000.f) || !float_valid(float_bits(z),200000.f)) return;
    EF(a->object,8)=x;EF(a->object,0x10)=z;
    s_drop_last_cid=scope->target_cid;
}
static void ebi_root_update(void *task,void *object)
{
    unsigned int p;
    if(task!=s_root.task || !live(&s_root)) return;
    p=EB(task,0xdc);
    health_display(local_ack());p=EB(task,0xdc);
    if(s_active && s_ready && !s_owner && p>=1 && p<=6 && s_prediction_age<61u)
        ++s_prediction_age;
    /* Phase0 snapshots and rewrites the local player stats. Wait for the
     * invite's native Ebisumaru rebind before taking that local backup. */
    if(p==0 && !anchor_minigame_invites_character_motion_ready(0x1e1)) return;
    /* Native phase0 creates this player's backup, camera, HUD and mechanism.
     * Native result phases own this player's animation, stats restoration and
     * guarded return helper. They are never replayed from a shared snapshot. */
    if(p==0 || p>=7 || anchor_minigame_sync_native_allowed(2)) {
        int exposed_remote=0;
        float host[3],target[3];
        EbiDropScope root_scope;
        unsigned int i;
        anchor_minigame_zero(&s_drop_scope,sizeof(s_drop_scope));
        anchor_minigame_zero(&root_scope,sizeof(root_scope));
        if(s_active && s_owner && s_ready && (p==4 || p==6) && object==s_root.object &&
           task==D_8016DAB4_16E6B4 && EH(task,0x5c)==0x142 &&
           pointer_valid(D_801FC60C_5B851C) && s_local_cid) {
            int valid_host=1;
            for(i=0;i<3;++i) {
                host[i]=EF(D_801FC60C_5B851C,8+4*i);
                if(!float_valid(float_bits(host[i]),200000.f)) valid_host=0;
            }
            if(valid_host) {
                root_scope.root=task;root_scope.object=object;root_scope.host_object=D_801FC60C_5B851C;
                root_scope.visit=s_visit;root_scope.entity=EH(task,0x5c);root_scope.generation=EB(task,0x74);
                if(p==4 && drop_hidden(host)) {
                    root_scope.target_cid=choose_drop_target(host,target,1);
                    if(root_scope.target_cid) { exposed_remote=1;ES(task,0xde)=0; }
                } else if(p==6 && (ES(task,0xde)%40==1 || ES(task,0xde)%40==21)) {
                    int cid=choose_drop_target(host,target,0);
                    if(cid) {
                        s_drop_scope=root_scope;s_drop_scope.target_cid=cid;
                        s_drop_scope.branch=ES(task,0xde)%40;s_drop_scope.serial=s_serial;
                        for(i=0;i<3;++i) { s_drop_scope.host[i]=host[i];s_drop_scope.target[i]=target[i]; }
                    }
                }
            }
        }
        func_08001954_6F1B64(task,object);
        if(exposed_remote && s_active && s_owner && s_ready && drop_root_current(&root_scope) &&
           anchor_minigame_sync_participant_current(2,root_scope.target_cid) && EB(task,0xdc)==4) {
            func_8021664C_5D1B1C(task,2,0.25f,1);EB(task,0xdc)=6;
        }
        retarget_drop();anchor_minigame_zero(&s_drop_scope,sizeof(s_drop_scope));
        if(live(&s_root)) {
            p=EB(task,0xdc);
            if(p==7) s_result=1;if(p==8) s_result=2;
            EBI_AI(task)=ebi_root_update;
        }
        return;
    }
    camera();
    if(pointer_valid(EBI_PTR(task,0xd8)))
        EI(EBI_PTR(task,0xd8),0x88)=(int)D_8015C5D8_15D1D8[3]-17;
}
static void ebi_actor_update(void *task,void *object)
{
    EbiActor *a=find(task);
    void *saved_target=0;
    if(!a || !a->initialized) return;
    if(a->hit_cooldown) --a->hit_cooldown;
    if(!anchor_minigame_sync_native_allowed(2)) {
        if(s_ready && !s_owner && a->kind==EBI_SWEET &&
           (EW(task,0x68)&0x200u)) {
            if(queue(a,EBI_COLLECT,1)) EW(task,0x68)&=~0x200u;
        }
        if(s_active && s_ready && !s_owner) predict_actor(a);
        fence_blast(a);return;
    }
    if(a->pending_damage && a->kind==EBI_ROBOT) {
        unsigned int hp=EB(task,0x8d),damage=a->pending_damage;
        a->pending_damage=0;
        /* The one ordinary post-update slot subtracts its fast-hit point,
         * updates the native kill counter and owns no-loot removal. */
        EB(task,0x8d)=(unsigned char)(damage>=hp?1u:hp-damage+1u);
        EW(task,0x68)|=0x40000u;
    }
    if(s_active && s_owner && s_ready && a->kind==EBI_ROBOT) {
        select_target(a);
        saved_target=EBI_PTR(task,0x84);s_target_actor=a;EBI_PTR(task,0x84)=a->target_view;
    }
    EBI_AI(task)=a->native;
    a->native(task,object);
    clear_pending(task);
    if(s_target_actor==a) {
        if(live(a)) EBI_PTR(task,0x84)=saved_target;
        s_target_actor=0;
    }
    if(live(a)) { a->native=EBI_AI(task);EBI_AI(task)=ebi_actor_update; }
}
static void initialized(int kind)
{
    void *t=D_8016DAB4_16E6B4;
    EbiActor *a=raw_find(t);
    /* These native functions also return on ordinary update slots. Only the
     * first completed initializer may capture identity and install wrapping. */
    if(!a || a->initialized || a->kind!=kind || D_800C7AB2!=0x1e1) return;
    a->object=EBI_PTR(t,0x18);a->entity=EH(t,0x5c);a->task_generation=EB(t,0x74);
    if(!live(a)) return;
    if(kind==EBI_MECHANISM && EB(t,0xd4)!=1) return;
    if(kind!=EBI_MECHANISM && kind!=EBI_ROBOT && EB(t,0xd0)<1) return;
    if(kind==EBI_ROBOT) callbacks();
    a->initialized=1;a->native=EBI_AI(t);EBI_AI(t)=ebi_actor_update;
}
RECOMP_HOOK_RETURN("func_0800139C_6F15AC")
void anchor_minigame_ebi_mechanism_initialized(void) { initialized(EBI_MECHANISM); }
RECOMP_HOOK_RETURN("func_08001250_6F1460")
void anchor_minigame_ebi_platform_initialized(void) { initialized(EBI_PLATFORM); }
RECOMP_HOOK_RETURN("func_08001720_6D0800")
void anchor_minigame_ebi_robot_initialized(void) { initialized(EBI_ROBOT); }
RECOMP_HOOK_RETURN("func_08000E2C_6F103C")
void anchor_minigame_ebi_bomb_initialized(void) { initialized(EBI_BOMB); }
RECOMP_HOOK_RETURN("func_08001030_6F1240")
void anchor_minigame_ebi_sweet_initialized(void) { initialized(EBI_SWEET); }
RECOMP_HOOK_RETURN("func_08000B58_6F0D68")
void anchor_minigame_ebi_blast_initialized(void) { initialized(EBI_BLAST); }
RECOMP_HOOK_RETURN("func_08001954_6F1B64")
void anchor_minigame_ebi_root_initialized(void)
{
    void *t=D_8016DAB4_16E6B4;
    if(D_800C7AB2!=0x1e1 || !pointer_valid(t) || !pointer_valid(EBI_PTR(t,0x18))) return;
    if(!s_root.task) {
        anchor_minigame_zero(&s_root,sizeof(s_root));s_visit=anchor_minigame_sync_visit();
        s_root.task=t;s_root.object=EBI_PTR(t,0x18);s_root.entity=EH(t,0x5c);
        s_root.task_generation=EB(t,0x74);s_root.native=func_08001954_6F1B64;
#if DEBUG_BUTTON_ENABLED
        s_diag_birth_visit=s_visit;s_diag_birth_calls=s_diag_birth_registered=0;
#endif
        callbacks();anchor_minigame_sync_bind(&s_adapter);
    }
    if(t==s_root.task && live(&s_root)) {
        if(!s_health_started && EB(t,0xdc)>=1 && EB(t,0xdc)<=6) {
            s_health=3;s_health_started=1;
        }
        if(EB(t,0xdc)==7) s_result=1;if(EB(t,0xdc)==8) s_result=2;
        EBI_AI(t)=ebi_root_update;
    }
}
RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_minigame_ebi_clip(void *t,unsigned int clip,float rate,unsigned int flags)
{
    EbiActor *a;
    if(D_800C7AB2!=0x1e1) return;
    a=raw_find(t);if(a) a->clip=clip;
    else if(t==s_root.task) s_root.clip=clip;
    (void)rate;(void)flags;
}
static void forget(void *t)
{
    confirm_blasts(t,0);clear_pending(t);
    EbiActor *a=raw_find(t);if(a) a->task=0;
    if(s_root.task==t) s_root.task=0;
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_minigame_ebi_delete(void) { forget(D_8016DAB4_16E6B4); }
RECOMP_HOOK("func_80034EF8_35AF8")
void anchor_minigame_ebi_delete_named(void *t) { forget(t); }
/* Retain native collision evaluation, but prevent a replica from entering
 * native death/reward callbacks. Only a bounded, validated owner hit follows. */
RECOMP_HOOK("func_80218350_5D3820")
void anchor_minigame_ebi_damage(void *t)
{
    EbiActor *a=find(t);
    void *attacker;
    unsigned int kind,damage;
    if(!a || !s_active || s_owner) return;
    attacker=EBI_PTR(t,0x38);
    if(a->kind==EBI_ROBOT && (EW(t,0x68)&0x80u) && s_ready && pointer_valid(attacker) && (EW(t,0x60)&0x200000u) &&
       !(EW(t,0x60)&0x400000u) && !(EW(t,0x68)&1u) && !a->hit_cooldown) {
        kind=EB(attacker,0x4c);damage=kind==0x1b?3u:
            (kind==0x16 || kind==0x23)?2u:(kind==0x17 || kind==0x24)?4u:kind==0x22?8u:1u;
        if(queue(a,EBI_HIT,damage)) {
            a->hit_cooldown=60;EW(t,0x68)|=1u;EB(t,0x8c)=60;
        }
    }
    EW(t,0x68)&=~0x40080u;
}
/* PRE rebuilds outgoing byte30 from flags60. Fence after that native work,
 * before the scene contact scan; retained stale visuals cannot hurt players. */
static void fence_blast(EbiActor *a)
{
    if(!s_active || s_owner || !a || a->kind!=EBI_BLAST || !owned_handle(a) ||
       EH(a->task,0x5c)!=a->entity || EB(a->task,0x74)!=a->task_generation) return;
    if(!live(a) || !a->initialized || !s_ready || s_result || !health_window() ||
       a->prediction_age>60u || a->contact_expired || EW(a->task,0xe8)!=4u)
        EB(a->task,0x30)&=~2u;
}
RECOMP_HOOK_RETURN("func_8021925C_5D472C")
void anchor_minigame_ebi_blast_contact(void) { fence_blast(raw_find(D_8016DAB4_16E6B4)); }
/* The ordinary finalizer alone owns animation, gravity and 2*v integration. */
RECOMP_HOOK("func_80218E7C_5D434C")
void anchor_minigame_ebi_collision_begin(void)
{
    EbiActor *a=find(D_8016DAB4_16E6B4);
    if(s_active && !s_owner && a && (!s_ready || a->prediction_age>60u)) {
        unsigned int i;
        for(i=0;i<3;++i) { a->held_xyz[i]=EF(a->object,8+4*i);EF(a->task,0x78+4*i)=0.f; }
        a->held_frame=EF(a->object,0x28);
    }
}
RECOMP_HOOK_RETURN("func_80218E7C_5D434C")
void anchor_minigame_ebi_collision_end(void)
{
    EbiActor *a=find(D_8016DAB4_16E6B4);
    if(s_active && !s_owner && a) {
        unsigned int i;
        if(!s_ready || a->prediction_age>60u) {
            for(i=0;i<3;++i) EF(a->object,8+4*i)=a->held_xyz[i];
            EF(a->object,0x28)=a->held_frame;
        } else if(a->correction_ticks) {
            for(i=0;i<3;++i) {
                float step=a->correction[i]/(float)a->correction_ticks;
                EF(a->object,8+4*i)+=step;a->correction[i]-=step;
            }
            --a->correction_ticks;
        }
    }
}
typedef struct EbiMask {
    EbiActor *actor;void *task,*object;
    unsigned short entity;unsigned char generation;
    unsigned int offset,mask,bits;
} EbiMask;
static EbiMask s_masks[4];
static void mask_begin(unsigned int slot,void *task,unsigned int offset,unsigned int mask)
{
    EbiMask *m=&s_masks[slot];EbiActor *a=find(task);m->actor=0;
    if(!s_active || s_owner || !a || !a->initialized) return;
    m->actor=a;m->task=task;m->object=a->object;m->entity=a->entity;m->generation=a->task_generation;
    m->offset=offset;m->mask=mask;m->bits=EW(task,offset)&mask;EW(task,offset)&=~mask;
}
static void mask_end(unsigned int slot)
{
    EbiMask *m=&s_masks[slot];EbiActor *a=m->actor;
    if(a && a->task==m->task && a->object==m->object && a->entity==m->entity &&
       a->task_generation==m->generation && live(a))
        EW(m->task,m->offset)=(EW(m->task,m->offset)&~m->mask)|m->bits;
    m->actor=0;
}
RECOMP_HOOK("func_8021B610_5D6AE0")
void anchor_minigame_ebi_fall_begin(void *t) { mask_begin(0,t,0x60,0x40000400u); }
RECOMP_HOOK_RETURN("func_8021B610_5D6AE0")
void anchor_minigame_ebi_fall_end(void) { mask_end(0); }
RECOMP_HOOK("func_8021A978_5D5E48")
void anchor_minigame_ebi_carry_begin(void *t) { mask_begin(1,t,0x64,0x4000u); }
RECOMP_HOOK_RETURN("func_8021A978_5D5E48")
void anchor_minigame_ebi_carry_end(void) { mask_end(1); }
RECOMP_HOOK("func_80218990_5D3E60")
void anchor_minigame_ebi_overlap_begin(void *t) { mask_begin(2,t,0x60,0x10000000u); }
RECOMP_HOOK_RETURN("func_80218990_5D3E60")
void anchor_minigame_ebi_overlap_end(void) { mask_end(2); }
RECOMP_HOOK("func_8021A328_5D57F8")
void anchor_minigame_ebi_descriptor_begin(void *t) { mask_begin(3,t,0x68,8u); }
RECOMP_HOOK_RETURN("func_8021A328_5D57F8")
void anchor_minigame_ebi_descriptor_end(void) { mask_end(3); }
static void *s_hud_render;
static int s_hud_timer;
RECOMP_HOOK("func_080005DC_6F07EC")
void anchor_minigame_ebi_hud_begin(void *hud)
{
    s_hud_render=0;
    if(live(&s_root) && hud==EBI_PTR(s_root.task,0xd8) &&
       !anchor_minigame_sync_native_allowed(2) &&
       !(s_active && s_ready && !s_owner && EB(s_root.task,0xdc)>=1 &&
         EB(s_root.task,0xdc)<=6 && s_timer_stamped && s_timer_age<60u)) {
        s_hud_render=hud;s_hud_timer=EI(hud,0x80);
    } else if(s_active && !s_owner && s_ready && live(&s_root) &&
              hud==EBI_PTR(s_root.task,0xd8) && EB(s_root.task,0xdc)>=1 && EB(s_root.task,0xdc)<=6)
        ++s_timer_age;
}
RECOMP_HOOK_RETURN("func_080005DC_6F07EC")
void anchor_minigame_ebi_hud_end(void)
{
    if(s_hud_render && live(&s_root) && s_hud_render==EBI_PTR(s_root.task,0xd8))
        EI(s_hud_render,0x80)=s_hud_timer;
    else if(live(&s_root) && pointer_valid(EBI_PTR(s_root.task,0xd8)) &&
            EI(EBI_PTR(s_root.task,0xd8),0x80)<0) EI(EBI_PTR(s_root.task,0xd8),0x80)=0;
    s_hud_render=0;
}
static void reset(void)
{
    anchor_minigame_zero(&s_root,sizeof(s_root));
    anchor_minigame_zero(s_actors,sizeof(s_actors));
    s_serial=2;s_input_serial=s_input_count=s_overflow=0;
    s_active=s_owner=s_ready=s_result=s_local_failure=s_graph_staging=s_root_resources_requested=0;
    s_birth_row=0;s_hud_render=0;
    s_health=3;s_health_started=s_health_observing=0;s_prediction_age=0;
    s_full_tick=s_checkpoint_generation=0;
    s_timer_stamped=0;s_timer_age=s_timer_tick=s_timer_generation=0;clear_pending(0);
    s_ack_fence=s_health_unqueued=0;
    s_target_count=0;s_target_actor=0;
    s_local_cid=s_drop_last_cid=0;anchor_minigame_zero(&s_drop_scope,sizeof(s_drop_scope));
    anchor_minigame_zero(s_masks,sizeof(s_masks));
#if DEBUG_BUTTON_ENABLED
    s_diag_capture=0;s_diag_captured=0;s_diag_valid=-1;s_diag_have_key=0;
    s_diag_attempts=s_diag_last=s_diag_reports=0;
    s_diag_birth_visit=s_diag_birth_calls=s_diag_birth_registered=0;
#endif
}
static void role(int active,int owner,int ready)
{
    if(s_active && !active) {
        if(health_window()) {
            int hp=(int)D_8015C5D8_15D1D8[3]-17;
            s_health=hp<0?0:hp>3?3:hp;
        }
        s_input_count=0;s_ack_fence=s_health_unqueued=0;
        s_drop_last_cid=0;s_timer_stamped=0;s_timer_age=0;
    }
    if(!active || !ready) clear_pending(0);
    s_active=active;s_owner=owner;s_ready=ready;
    s_local_cid=active && ready?(int)anchor_get_client_id():0;
    s_target_count=active && ready?anchor_player_models_get_boss_targets(s_targets,64):0;
#if DEBUG_BUTTON_ENABLED
    if(s_diag_capture) diagnostic(s_diag_capture);
#endif
}
static int input(const int *r,unsigned int count)
{
    EbiActor *a;
    if(count!=8 || r[0]<1 || r[1]<1 || r[2]<1 || r[3]<1 ||
       r[6]!=1 || r[7]<1 || r[7]>8) return 1;
    if(!root_ready() || !s_owner || !s_ready) return 0;
    if(r[4]==EBI_HEALTH_EVENT) {
        if(r[5]==1 && r[7]<=3 && EB(s_root.task,0xdc)<7) {
            s_health=r[7]>=s_health?0:s_health-r[7];
            health_display(local_ack());
        }
        return 1;
    }
    if(r[5]<3 || (r[4]!=EBI_COLLECT && r[4]!=EBI_HIT)) return 1;
    a=identity(r[4]==EBI_COLLECT?EBI_SWEET:EBI_ROBOT,
               (unsigned int)r[5],(unsigned int)r[6]);
    if(!a || EB(s_root.task,0xdc)>=7) return 1;
    if(r[4]==EBI_COLLECT) {
        if(r[7]==1 && EB(a->task,0xd0)==1) EB(a->task,0xd0)=2;
    } else if(r[4]==EBI_HIT && a->kind==EBI_ROBOT &&
              (EW(a->task,0x60)&0x200000u) && !(EW(a->task,0x60)&0x400000u) &&
              (EW(a->task,0x64)&0x8000u) && !(EW(a->task,0x68)&1u) && !a->pending_damage) {
        /* Damage is performed in this actor's scheduler slot, ensuring native
         * current-task teardown and the FB no-drop branch remain correct. */
        if(a->pending_damage<255u) a->pending_damage+=(unsigned int)r[7];
    }
    return 1;
}
static int return_pending(void)
{
    return live(&s_root) && s_result && s_root.task==D_8016DAB4_16E6B4 &&
           EB(s_root.task,0xdc)==9 && ES(s_root.task,0xde)==10;
}
static void result(int value)
{
    if(value<1 || value>2 || !root_ready() || EB(s_root.task,0xdc)>=7) return;
    s_result=value;EB(s_root.task,0xdc)=(unsigned char)(value==1?7:8);ES(s_root.task,0xde)=0;
}
static const AnchorMinigameAdapter s_adapter={
    2,reset,capture,validate,apply,role,input,return_pending,result
};
