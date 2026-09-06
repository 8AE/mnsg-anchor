#include "anchor_congo_native.h"
#include "anchor_congo_damage.h"
#include "anchor_boss_invite_world.h"
#include "anchor_remote_model_pool.h"
#ifndef ANCHOR_CONGO_NATIVE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

typedef void (*CongoCallback)(void *, void *);
#define U8(p,o) (*(volatile unsigned char *)((unsigned char *)(p)+(o)))
#define U16(p,o) (*(volatile unsigned short *)((unsigned char *)(p)+(o)))
#define U32(p,o) (*(volatile unsigned int *)((unsigned char *)(p)+(o)))
#define F32(p,o) (*(volatile float *)((unsigned char *)(p)+(o)))
#ifndef CONGO_PTR
#define CONGO_PTR(p,o) (*(void *volatile *)((unsigned char *)(p)+(o)))
#endif
#ifndef CONGO_AI
#define CONGO_AI(p) (*(CongoCallback volatile *)((unsigned char *)(p)+0xC))
#define CONGO_POST(p) (*(CongoCallback volatile *)((unsigned char *)(p)+0x10))
#endif
extern unsigned short D_800C7AB2;
extern unsigned char *D_8015C5C8_15D1C8;
extern void *D_8016DAB4_16E6B4;
extern void *func_800141C4_14DC4(unsigned int file);
extern void *func_8021DDE8_5D92B8(void *, CongoCallback, unsigned char,
                                float, float, float, int);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern unsigned char D_8015C562_15D162;
extern unsigned char D_8015CC30_15D830[];
extern void func_80023DF0_249F0(unsigned int);
extern void func_80024038_24C38(unsigned int);
extern void func_80023E40_24A40(unsigned int);
extern void func_08000DCC_6B406C(void *, void *);
extern void func_08000F44_6B41E4(void *, void *);
extern void func_0800110C_6B43AC(void *, void *);
extern void func_080066F4_6B9994(void *, void *);
extern void func_080068FC_6B9B9C(void *, void *);
extern void func_08006B04_6B9DA4(void *, void *);
extern void func_08006D0C_6B9FAC(void *, void *);
extern void func_08006F14_6BA1B4(void *, void *);
extern void func_0800711C_6BA3BC(void *, void *);
extern void func_080073D0_6BA670(void *, void *);
extern void func_080073FC_6BA69C(void *, void *);
extern void func_08007488_6BA728(void *, void *);
extern void func_080074CC_6BA76C(void *, void *);
extern void func_0800751C_6BA7BC(void *, void *);
extern void func_080075E8_6BA888(void *, void *);
extern void func_08007694_6BA934(void *, void *);
extern void func_08007724_6BA9C4(void *, void *);
extern void func_080077A4_6BAA44(void *, void *);
extern void func_0800781C_6BAABC(void *, void *);
extern void func_080079B0_6BAC50(void *, void *);
extern void func_080079E4_6BAC84(void *, void *);
extern void func_08007A64_6BAD04(void *, void *);
extern void func_08007ACC_6BAD6C(void *, void *);
extern void func_08007B58_6BADF8(void *, void *);
extern void func_08007BB0_6BAE50(void *, void *);
extern void func_08007C18_6BAEB8(void *, void *);
extern void func_08007C70_6BAF10(void *, void *);
extern void func_08007CBC_6BAF5C(void *, void *);
extern void func_08007D24_6BAFC4(void *, void *);
extern void func_08007DEC_6BB08C(void *, void *);
extern void func_08008194_6BB434(void *, void *);
extern void func_0800820C_6BB4AC(void *, void *);
extern void func_08008280_6BB520(void *, void *);
extern void func_080082F8_6BB598(void *, void *);
extern void func_0800833C_6BB5DC(void *, void *);
extern void func_080083A0_6BB640(void *, void *);
extern void func_080088C4_6BBB64(void *, void *);
extern void func_08009C04_6BCEA4(void *, void *);
extern void func_0800A228_6BD4C8(void *, void *);
/* RecompModTool forbids static pointer initializers into relocatable game
 * overlays. Refresh these volatile BSS tables after each root initialization,
 * when file29 is loaded. Volatile stores keep Clang from inventing a rodata
 * copy containing the same forbidden relocations. */
static CongoCallback volatile s_phases[27];
static CongoCallback volatile s_part_updates[6];
static void initialize_callbacks(void)
{
    s_phases[0]=0;
    s_phases[1]=func_080073FC_6BA69C;
    s_phases[2]=func_08007488_6BA728;
    s_phases[3]=func_080074CC_6BA76C;
    s_phases[4]=func_0800751C_6BA7BC;
    s_phases[5]=func_080075E8_6BA888;
    s_phases[6]=func_08007694_6BA934;
    s_phases[7]=func_08007724_6BA9C4;
    s_phases[8]=func_080077A4_6BAA44;
    s_phases[9]=func_0800781C_6BAABC;
    s_phases[10]=func_080079B0_6BAC50;
    s_phases[11]=func_080079E4_6BAC84;
    s_phases[12]=func_08007A64_6BAD04;
    s_phases[13]=func_08007ACC_6BAD6C;
    s_phases[14]=func_08007B58_6BADF8;
    s_phases[15]=func_08007BB0_6BAE50;
    s_phases[16]=func_08007C18_6BAEB8;
    s_phases[17]=func_08007C70_6BAF10;
    s_phases[18]=func_08007CBC_6BAF5C;
    s_phases[19]=func_08007D24_6BAFC4;
    s_phases[20]=func_08007DEC_6BB08C;
    s_phases[21]=func_08008194_6BB434;
    s_phases[22]=func_0800820C_6BB4AC;
    s_phases[23]=func_08008280_6BB520;
    s_phases[24]=func_080082F8_6BB598;
    s_phases[25]=func_0800833C_6BB5DC;
    s_phases[26]=func_080083A0_6BB640;
    s_part_updates[0]=func_080066F4_6B9994;
    s_part_updates[1]=func_080068FC_6B9B9C;
    s_part_updates[2]=func_08006B04_6B9DA4;
    s_part_updates[3]=func_08006D0C_6B9FAC;
    s_part_updates[4]=func_08006F14_6BA1B4;
    s_part_updates[5]=func_0800711C_6BA3BC;
}

#define PHASE_COUNT (sizeof(s_phases)/sizeof(s_phases[0]))
#define PHASE_VICTORY 19u
#define FLAME_MAX_AGE 111u
#define CALLBACK_DISABLED 0x00800000ul
#define ROOT_FLAGS_MASK 0x002006E1u
#define ROOT_FLAGS2_MASK 0x00008000u
#define ROOT_SPECIAL_MASK 0x04003080u
#define PART_FLAGS_MASK 0x00000021u
/* A04C emits once per five native ticks. DCC starts opacity120 and every
 * flame update decreases it; 110C deletes at10. At most23 births overlap
 * including initialization.32 allows scheduler ordering slack, not players. */
typedef struct CongoActor {
    void *task, *object;
    unsigned short actor;
    unsigned char generation;
    CongoCallback held_ai, held_post;
    unsigned char held;
} CongoActor;
typedef struct CongoFlame {
    CongoActor actor;
    unsigned int seed[7];
} CongoFlame;
static CongoActor s_root, s_parts[6], s_camera, s_rays[12];
static CongoFlame s_flames[32];
static unsigned int s_part_clip[6], s_visit, s_tick, s_spin_serial, s_flame_serial;
static unsigned int s_ray_age;
static unsigned int s_terminal_phase;
static unsigned int s_seen_flames[64], s_seen_next;
static int s_active, s_owner, s_paused, s_adopted, s_reconstructing;
static int s_root_post_seen;
static AnchorCongoNativeSnapshot s_pending_snapshot;
static int s_pending_valid;
static int s_terminal_started;
static int s_target_valid;
static float s_target_object[8];
static void *s_target_saved;
static void *s_target_root;
static unsigned int s_health_saved;
static void *s_health_root;
static unsigned short s_emitter_clock;
static int s_emitter_scoped;

/* The freestanding mod has no libc memset/memcpy imports. Volatile byte
 * stores retain these bounded operations when Clang optimizes large structs. */
static void zero_bytes(void *p,unsigned int count)
{
    volatile unsigned char *out=p;
    while(count--)*out++=0;
}
static void copy_bytes(void *p,const void *q,unsigned int count)
{
    volatile unsigned char *out=p;const unsigned char *in=q;
    while(count--)*out++=*in++;
}
static int pointer_valid(const void *p)
{
#ifdef ANCHOR_CONGO_NATIVE_HOST_TEST
    return p != 0;
#else
    unsigned int a=(unsigned int)(unsigned long)p;
    return (a&3u)==0 && ((a>=0x80001000u && a<0x80800000u) ||
                        anchor_remote_model_pool_contains(p));
#endif
}
static void bind(CongoActor *a, void *task)
{
    a->task=task; a->object=CONGO_PTR(task,0x18);
    a->actor=U16(task,0x5C); a->generation=U8(task,0x74); a->held=0;
}
static int live(const CongoActor *a)
{
    void *link;
    if (!pointer_valid(a->task) || !pointer_valid(a->object) ||
        D_800C7AB2!=0x16 || s_visit!=anchor_boss_invite_world_visit() ||
        CONGO_PTR(a->task,0x18)!=a->object ||
        U16(a->task,0x5C)!=a->actor || U8(a->task,0x74)!=a->generation ||
        (U32(a->task,0x68)&2u)) return 0;
    link=CONGO_PTR(a->task,4);
    return pointer_valid(link) && CONGO_PTR(link,0)==a->task;
}
int anchor_congo_native_is_root(const void *task)
{
    return task==s_root.task && live(&s_root) && U16(task,0x5E)==0x323;
}
void *anchor_congo_native_root_task(void)
{
    return anchor_congo_native_is_root(s_root.task)?s_root.task:0;
}
unsigned int anchor_congo_native_visit(void) { return s_visit; }
static unsigned int phase_of(CongoCallback callback)
{
    unsigned int i;
    for(i=1;i<PHASE_COUNT;i++)
        if(((unsigned long)callback&~CALLBACK_DISABLED)==
           ((unsigned long)s_phases[i]&~CALLBACK_DISABLED)) return i;
    return 0;
}
static int callback_is(CongoCallback a,CongoCallback b)
{
    return ((unsigned long)a&~CALLBACK_DISABLED)==((unsigned long)b&~CALLBACK_DISABLED);
}
static CongoCallback root_ai(void)
{
    return s_root.held?s_root.held_ai:CONGO_AI(s_root.task);
}
int anchor_congo_native_ready(void)
{
    unsigned int i;
    if(!anchor_congo_native_root_task()) return 0;
    for(i=0;i<6;i++) if(!live(&s_parts[i]) || CONGO_PTR(s_parts[i].task,0xDC)!=s_root.task) return 0;
    return 1;
}
static void prepare_follower(void);
static void hold_noop(void *task,void *object) { (void)task;(void)object; }
static void hold(CongoActor *a,int post)
{
    if(!live(a) || a->held) return;
    a->held_ai=CONGO_AI(a->task); a->held_post=CONGO_POST(a->task); a->held=1;
    CONGO_AI(a->task)=hold_noop;
    if(post) CONGO_POST(a->task)=hold_noop;
}
static void unhold(CongoActor *a)
{
    if(a->held && live(a)) {
        unsigned long ai=(unsigned long)CONGO_AI(a->task);
        unsigned long post=(unsigned long)CONGO_POST(a->task);
        unsigned long ours=(unsigned long)hold_noop&~CALLBACK_DISABLED;
        /*1925C->18FE8 pre stages local contacts and may toggle the native
         * scheduler-disable tag on+C/+10 for flute/cutscene. Keep that tag
         * while removing our temporary callback, even when it changed. */
        if((ai&~CALLBACK_DISABLED)==ours)
            CONGO_AI(a->task)=(CongoCallback)(((unsigned long)a->held_ai&~CALLBACK_DISABLED)|(ai&CALLBACK_DISABLED));
        if((post&~CALLBACK_DISABLED)==ours)
            CONGO_POST(a->task)=(CongoCallback)(((unsigned long)a->held_post&~CALLBACK_DISABLED)|(post&CALLBACK_DISABLED));
    }
    a->held=0;
}
static void release_all(void)
{
    unsigned int i;
    unhold(&s_root);
    for(i=0;i<6;i++) unhold(&s_parts[i]);
    for(i=0;i<12;i++) unhold(&s_rays[i]);
    for(i=0;i<32;i++) unhold(&s_flames[i].actor);
}
void anchor_congo_native_reset(void)
{
    unsigned int i;
    release_all();
    zero_bytes(&s_root,sizeof(s_root));zero_bytes(&s_camera,sizeof(s_camera));
    for(i=0;i<6;i++){zero_bytes(&s_parts[i],sizeof(s_parts[i]));s_part_clip[i]=i;}
    for(i=0;i<12;i++) zero_bytes(&s_rays[i],sizeof(s_rays[i]));
    for(i=0;i<32;i++) zero_bytes(&s_flames[i],sizeof(s_flames[i]));
    for(i=0;i<64;i++) s_seen_flames[i]=0;
    s_tick=s_spin_serial=s_flame_serial=s_seen_next=0;
    s_terminal_phase=0;
    s_adopted=s_terminal_started=s_target_valid=0;
    s_root_post_seen=0;s_pending_valid=0;
}
void anchor_congo_native_set_role(int active,int owner,int paused)
{
    if(!active) {release_all();s_pending_valid=0;}
    s_active=active!=0;s_owner=owner!=0;s_paused=paused!=0;
}
void anchor_congo_native_set_target(float x,float y,float z)
{
    s_target_object[2]=x;s_target_object[3]=y;s_target_object[4]=z;
    s_target_valid=1;
}
void anchor_congo_native_clear_target(void) { s_target_valid=0; }
RECOMP_HOOK("func_802197D8_5D4CA8")
void anchor_congo_native_target_begin(void *task)
{
    if(s_active && s_owner && s_target_valid && !s_target_root &&
       anchor_congo_native_is_root(task)) {
        s_target_root=task;s_target_saved=CONGO_PTR(task,0x84);
        CONGO_PTR(task,0x84)=s_target_object;
    }
}
RECOMP_HOOK_RETURN("func_802197D8_5D4CA8")
void anchor_congo_native_target_end(void)
{
    if(s_target_root && anchor_congo_native_is_root(s_target_root) &&
       CONGO_PTR(s_target_root,0x84)==s_target_object)
        CONGO_PTR(s_target_root,0x84)=s_target_saved;
    s_target_root=0;
}
/* Follower contacts still traverse000C->18E7C for the damage bridge, but
 * A228 must not select a local threshold/death state from those contacts. */
RECOMP_HOOK("func_0800A228_6BD4C8")
void anchor_congo_native_health_begin(void *task)
{
    if(s_active && !s_owner && !s_reconstructing && !s_health_root &&
       anchor_congo_native_is_root(task)) {
        s_health_root=task;s_health_saved=U32(task,0xE8)&0x04000000u;
        U32(task,0xE8)&=~0x04000000u;
    }
}
RECOMP_HOOK_RETURN("func_0800A228_6BD4C8")
void anchor_congo_native_health_end(void)
{
    if(s_health_root && anchor_congo_native_is_root(s_health_root))
        U32(s_health_root,0xE8)|=s_health_saved;
    s_health_root=0;
}
RECOMP_HOOK("func_0800A04C_6BD2EC")
void anchor_congo_native_emitter_begin(void *task)
{
    if(s_active && s_owner && !s_emitter_scoped && D_8015C5C8_15D1C8 &&
       anchor_congo_native_is_root(task)) {
        s_emitter_clock=U16(D_8015C5C8_15D1C8,0x3ADCE);
        U16(D_8015C5C8_15D1C8,0x3ADCE)=(unsigned short)(s_tick%15u);
        s_emitter_scoped=1;
    }
}
RECOMP_HOOK_RETURN("func_0800A04C_6BD2EC")
void anchor_congo_native_emitter_end(void)
{
    if(s_emitter_scoped && D_8015C5C8_15D1C8)
        U16(D_8015C5C8_15D1C8,0x3ADCE)=s_emitter_clock;
    s_emitter_scoped=0;
}
RECOMP_HOOK_RETURN("func_08005EDC_6B917C")
void anchor_congo_native_bind_root(void)
{
    void *task=D_8016DAB4_16E6B4;
    CongoActor camera;
    int keep_camera=live(&s_camera);
    copy_bytes(&camera,&s_camera,sizeof(camera));
    if(!pointer_valid(task) || !pointer_valid(CONGO_PTR(task,0x18)) ||
       U16(task,0x5E)!=0x323 || (U32(task,0x68)&2u) || D_800C7AB2!=0x16) return;
    /* Part constructors execute later; the root initializer only enqueues them. */
    initialize_callbacks();
    anchor_congo_native_reset();s_visit=anchor_boss_invite_world_visit();
    if(keep_camera)copy_bytes(&s_camera,&camera,sizeof(camera));
    bind(&s_root,task);anchor_congo_damage_bind_root(task);
}
RECOMP_HOOK_RETURN("func_080083BC_6BB65C")
void anchor_congo_native_bind_camera(void)
{
    void *task=D_8016DAB4_16E6B4;
    if(D_800C7AB2==0x16 && pointer_valid(task) && pointer_valid(CONGO_PTR(task,0xD0))) {
        if(!s_root.task)s_visit=anchor_boss_invite_world_visit();
        bind(&s_camera,task);
    }
}
static void bind_part(void *task,unsigned int part)
{
    if(pointer_valid(task) && anchor_congo_native_is_root(CONGO_PTR(task,0xDC))) {
        bind(&s_parts[part],task);s_part_clip[part]=part;
    }
}
RECOMP_HOOK_RETURN("func_080066B0_6B9950")
void anchor_congo_native_part_0(void) { bind_part(D_8016DAB4_16E6B4,0); }
RECOMP_HOOK_RETURN("func_080068B8_6B9B58")
void anchor_congo_native_part_1(void) { bind_part(D_8016DAB4_16E6B4,1); }
RECOMP_HOOK_RETURN("func_08006AC0_6B9D60")
void anchor_congo_native_part_2(void) { bind_part(D_8016DAB4_16E6B4,2); }
RECOMP_HOOK_RETURN("func_08006CC8_6B9F68")
void anchor_congo_native_part_3(void) { bind_part(D_8016DAB4_16E6B4,3); }
RECOMP_HOOK_RETURN("func_08006ED0_6BA170")
void anchor_congo_native_part_4(void) { bind_part(D_8016DAB4_16E6B4,4); }
RECOMP_HOOK_RETURN("func_080070D8_6BA378")
void anchor_congo_native_part_5(void) { bind_part(D_8016DAB4_16E6B4,5); }

RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_congo_native_animation(void *task,unsigned int clip)
{
    unsigned int i;
    for(i=0;i<6;i++) if(task==s_parts[i].task && live(&s_parts[i]) && clip<18u)
        s_part_clip[i]=clip;
}
RECOMP_HOOK("func_08009C04_6BCEA4")
void anchor_congo_native_spin(void *task)
{
    if(anchor_congo_native_is_root(task) && !s_reconstructing) s_spin_serial++;
}
RECOMP_HOOK("func_08007D24_6BAFC4")
void anchor_congo_native_victory(void *task)
{
    if(anchor_congo_native_is_root(task))s_terminal_started=1;
}
RECOMP_HOOK_RETURN("func_080005F4_6B3894")
void anchor_congo_native_ray(void)
{
    void *task=D_8016DAB4_16E6B4;
    unsigned int i;
    if(!pointer_valid(task) || !anchor_congo_native_is_root(CONGO_PTR(task,0xD0))) return;
    for(i=0;i<12;i++) if(!live(&s_rays[i])) {
        bind(&s_rays[i],task);
        if(s_active && !s_owner && s_adopted)
            F32(s_rays[i].object,0x24)=(float)(s_ray_age>=29u?30u:1u+s_ray_age);
        return;
    }
}
static unsigned int float_bits(float f) { union {float f;unsigned int u;} v;v.f=f;return v.u; }
static float bits_float(unsigned int u) { union {float f;unsigned int u;} v;v.u=u;return v.f; }
static int sane_float(unsigned int bits,float low,float high)
{
    float f=bits_float(bits);
    return (bits&0x7F800000u)!=0x7F800000u && f>=low && f<=high;
}
static int seen(unsigned int id)
{
    unsigned int i;for(i=0;i<64;i++)if(s_seen_flames[i]==id)return 1;return 0;
}
static void remember(unsigned int id) { s_seen_flames[s_seen_next++%64u]=id; }
RECOMP_HOOK_RETURN("func_08000DCC_6B406C")
void anchor_congo_native_flame(void)
{
    void *task=D_8016DAB4_16E6B4;
    unsigned int i;void *o;
    if(!pointer_valid(task) || !anchor_congo_native_is_root(CONGO_PTR(task,0xD0))) return;
    for(i=0;i<32;i++)if(!live(&s_flames[i].actor)) {
        CongoFlame *f=&s_flames[i];bind(&f->actor,task);o=f->actor.object;
        if(s_reconstructing) return;
        f->seed[0]=++s_flame_serial;f->seed[1]=s_tick;
        f->seed[2]=U32(task,0xE8)&0x70u;
        f->seed[3]=float_bits(F32(o,8));f->seed[4]=float_bits(F32(o,12));
        f->seed[5]=float_bits(F32(o,16));f->seed[6]=U16(s_root.object,0x16);
        remember(f->seed[0]);return;
    }
}
RECOMP_HOOK("func_80034734_35334")
void anchor_congo_native_scheduler_begin(void)
{
    unsigned int i;
    if(!s_active || !anchor_congo_native_ready() || !phase_of(root_ai()))return;
    prepare_follower();
    if((!s_owner && phase_of(root_ai())<PHASE_VICTORY) || s_paused || s_pending_valid)
        hold(&s_root,s_paused || s_pending_valid);
    if(s_paused || s_pending_valid) {
        for(i=0;i<6;i++)hold(&s_parts[i],1);
        for(i=0;i<12;i++)hold(&s_rays[i],1);
        for(i=0;i<32;i++)hold(&s_flames[i].actor,1);
    }
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_congo_native_scheduler_end(void) { release_all(); }
RECOMP_HOOK("func_0800000C_6B32AC")
void anchor_congo_native_observe_post(void *task)
{
    if(anchor_congo_native_is_root(task) && !s_reconstructing)s_root_post_seen=1;
}
void anchor_congo_native_tick(void)
{
    unsigned int i;
    int advanced=s_root_post_seen;
    s_root_post_seen=0;
    if(!anchor_congo_native_root_task())return;
    /* The native pre can skip AI/post on the same frame Start/flute begins,
     * before the bridge learns that pause. Count actual post passes instead
     * of yesterday's role/pause value or the wall clock. */
    if(advanced && s_active && !s_paused && anchor_congo_native_ready() &&
       phase_of(root_ai()) && (s_owner||s_adopted))s_tick++;
    for(i=0;i<32;i++) if(!live(&s_flames[i].actor))zero_bytes(&s_flames[i].actor,sizeof(s_flames[i].actor));
}

static int snapshot_valid(const AnchorCongoNativeSnapshot *s)
{
    unsigned int i,j;
    if(!s || s->root[CONGO_PHASE]==0 || s->root[CONGO_PHASE]>=PHASE_COUNT ||
       s->root[CONGO_TIMER]>65535u || s->root[CONGO_HP]>30u ||
       s->root[CONGO_HURT]>60u || s->flame_count>32u) return 0;
    if((s->root[CONGO_FLAGS]&~ROOT_FLAGS_MASK) ||
       (s->root[CONGO_FLAGS2]&~ROOT_FLAGS2_MASK) ||
       (s->root[CONGO_STATUS]&~1u) ||
       (s->root[CONGO_SPECIAL]&~ROOT_SPECIAL_MASK))return 0;
    for(i=CONGO_X;i<=CONGO_Z;i++)if(!sane_float(s->root[i],-32768.f,32768.f))return 0;
    for(i=CONGO_RX;i<=CONGO_RZ;i++)if(s->root[i]>1023u)return 0;
    for(i=CONGO_SCALE_X;i<=CONGO_SCALE_Z;i++)if(!sane_float(s->root[i],0.f,32.f))return 0;
    for(i=CONGO_VX;i<=CONGO_VZ;i++)if(!sane_float(s->root[i],-100.f,100.f))return 0;
    if(!sane_float(s->root[CONGO_SOUND_COOLDOWN],0.f,60.f))return 0;
    for(i=CONGO_RADIUS;i<=CONGO_COLLIDER_Y;i++)if(s->root[i]>65535u)return 0;
    for(i=0;i<6;i++) {
        if(s->part[i][0]>=18u || s->part[i][0]%6u!=i ||
           !sane_float(s->part[i][1],0.f,65536.f) || s->part[i][3]>1u ||
           (s->part[i][2]&~PART_FLAGS_MASK))return 0;
    }
    for(i=0;i<s->flame_count;i++) {
        const unsigned int *f=s->flame[i];
        if(!f[0] || (f[2]!=0x10u && f[2]!=0x20u && f[2]!=0x40u) ||
           f[6]>1023u || s->tick-f[1]>FLAME_MAX_AGE) return 0;
        for(j=3;j<=5;j++)if(!sane_float(f[j],-32768.f,32768.f))return 0;
        for(j=0;j<i;j++)if(s->flame[j][0]==f[0])return 0;
    }
    return 1;
}
static void capture_root(unsigned int *r)
{
    void *t=s_root.task,*o=s_root.object;unsigned int i;
    r[CONGO_PHASE]=phase_of(root_ai());r[CONGO_TIMER]=U16(t,0x8A);
    r[CONGO_HP]=U8(t,0x8D);r[CONGO_HURT]=U8(t,0x8C);
    r[CONGO_FLAGS]=U32(t,0x60)&ROOT_FLAGS_MASK;r[CONGO_FLAGS2]=U32(t,0x64)&ROOT_FLAGS2_MASK;
    r[CONGO_STATUS]=U32(t,0x68)&1u;r[CONGO_SPECIAL]=U32(t,0xE8)&ROOT_SPECIAL_MASK;
    for(i=0;i<3;i++) {
        r[CONGO_X+i]=float_bits(F32(o,8+i*4));
        r[CONGO_RX+i]=U16(o,0x14+i*2);
        r[CONGO_SCALE_X+i]=float_bits(F32(o,0x1C+i*4));
        r[CONGO_VX+i]=float_bits(F32(t,0x78+i*4));
        r[CONGO_RADIUS+i]=U16(t,0x3C+i*2);
    }
    r[CONGO_SOUND_COOLDOWN]=float_bits(F32(t,0xDC));
}
int anchor_congo_native_capture(AnchorCongoNativeSnapshot *s)
{
    unsigned int i,j;
    if(!s || s_pending_valid || !anchor_congo_native_ready())return 0;
    capture_root(s->root);s->tick=s_tick;s->spin_serial=s_spin_serial;s->flame_count=0;
    for(i=0;i<6;i++) {
        s->part[i][0]=s_part_clip[i];s->part[i][1]=float_bits(F32(s_parts[i].object,0x28));
        s->part[i][2]=U32(s_parts[i].task,0x60)&PART_FLAGS_MASK;
        s->part[i][3]=callback_is(CONGO_AI(s_parts[i].task),func_080073D0_6BA670);
    }
    for(i=0;i<32;i++)if(live(&s_flames[i].actor) &&
                          s_tick-s_flames[i].seed[1]<=FLAME_MAX_AGE) {
        for(j=0;j<7;j++)s->flame[s->flame_count][j]=s_flames[i].seed[j];
        s->flame_count++;
    }
    return snapshot_valid(s);
}
static void apply_root(const unsigned int *r)
{
    void *t=s_root.task,*o=s_root.object;unsigned int i;
    CONGO_AI(t)=s_phases[r[CONGO_PHASE]];U16(t,0x8A)=(unsigned short)r[CONGO_TIMER];
    U8(t,0x8D)=(unsigned char)r[CONGO_HP];U8(t,0x8C)=(unsigned char)r[CONGO_HURT];
    U32(t,0x60)=(U32(t,0x60)&~ROOT_FLAGS_MASK)|r[CONGO_FLAGS];
    U32(t,0x64)=(U32(t,0x64)&~ROOT_FLAGS2_MASK)|r[CONGO_FLAGS2];
    /* Contact pointers and this client's contact flags stay local. */
    U32(t,0x68)=(U32(t,0x68)&~1u)|(r[CONGO_STATUS]&1u);
    U32(t,0xE8)=(U32(t,0xE8)&~ROOT_SPECIAL_MASK)|r[CONGO_SPECIAL];
    for(i=0;i<3;i++) {
        F32(o,8+i*4)=bits_float(r[CONGO_X+i]);
        U16(o,0x14+i*2)=(unsigned short)r[CONGO_RX+i];
        F32(o,0x1C+i*4)=bits_float(r[CONGO_SCALE_X+i]);
        F32(t,0x78+i*4)=bits_float(r[CONGO_VX+i]);
        U16(t,0x3C+i*2)=(unsigned short)r[CONGO_RADIUS+i];
    }
    F32(t,0xDC)=bits_float(r[CONGO_SOUND_COOLDOWN]);
}
static void call_as(CongoActor *a,CongoCallback callback)
{
    void *previous=D_8016DAB4_16E6B4;
    if(!live(a))return;
    /* Adoption runs only inside root1925C pre, after the scheduler installed
     * file29. Every manually advanced actor here belongs to that same file;
     * only current-task ownership changes, and is restored even on removal. */
    D_8016DAB4_16E6B4=a->task;callback(a->task,a->object);
    D_8016DAB4_16E6B4=previous;
}
/* The camera initializer owns its camera/light resources. Finish only its
 * verified intro, via88C4's native viewport and player-control restoration.
 * Never touch an unrelated scenario or the ordinary/death camera callbacks. */
extern void func_08008560_6BB800(void *,void *);
extern void func_08008624_6BB8C4(void *,void *);
extern void func_0800868C_6BB92C(void *,void *);
extern void func_0800876C_6BBA0C(void *,void *);
extern void func_08008804_6BBAA4(void *,void *);
extern void func_08008888_6BBB28(void *,void *);
extern void func_080088C4_6BBB64(void *,void *);
static int congo_camera_is_intro(CongoCallback ai)
{
    return ai==func_08008560_6BB800 ||
           ai==func_08008624_6BB8C4 ||
           ai==func_0800868C_6BB92C ||
           ai==func_0800876C_6BBA0C ||
           ai==func_08008804_6BBAA4 ||
           ai==func_08008888_6BBB28 ||
           ai==func_080088C4_6BBB64;
}
static void prepare_follower(void);

static int finish_intro(void)
{
    CongoCallback ai;
    if(phase_of(root_ai()))return 1;
    if(!live(&s_camera))return 0;
    ai=CONGO_AI(s_camera.task);
    if(!ai)return 0;
    /* The exact callback list is declared below rather than accepting an
     * arbitrary pointer range from this overlay. */
    if(!congo_camera_is_intro(ai))return 0;
    func_80024038_24C38(0x128);func_80024038_24C38(0x12F);
    func_80024038_24C38(0x133);
    U8(s_camera.task,0xDD)=2;
    call_as(&s_camera,func_080088C4_6BBB64);
    return 1;
}
static void spawn_flame(const unsigned int *seed,unsigned int now)
{
    void *resource,*task,*o;unsigned int i,j,age;unsigned short yaw;
    CongoActor actor;
    if(seen(seed[0]))return;
    age=now-seed[1];
    if(age>=FLAME_MAX_AGE){remember(seed[0]);return;}
    resource=func_800141C4_14DC4(0x1D);
    if(!resource || (unsigned long)resource==0xFFFFFFFFul)return;
    task=func_8021DDE8_5D92B8(s_root.task,func_08000DCC_6B406C,10,0.f,35.f,60.f,0);
    if(!pointer_valid(task) || !pointer_valid(CONGO_PTR(task,0x18)))return;
    U16(task,0x28)=0x1D;CONGO_PTR(task,0x2C)=resource;CONGO_PTR(task,0xD0)=s_root.task;
    U32(task,0xE8)=seed[2];o=CONGO_PTR(task,0x18);
    for(i=0;i<3;i++)F32(o,8+4*i)=bits_float(seed[3+i]);
    bind(&actor,task);yaw=U16(s_root.object,0x16);
    U16(s_root.object,0x16)=(unsigned short)seed[6];
    call_as(&actor,func_08000DCC_6B406C);
    U16(s_root.object,0x16)=yaw;
    for(i=0;i<3;i++)F32(o,8+4*i)=bits_float(seed[3+i]);
    /* Replay visual AI only, then its constant X/Z motion. Historical hits
     * must not be scanned during catch-up. DCC velocity is sin/cos(yaw)*2.2;
     * F44 adds native Y descent for its first16 ticks;110C has no Y motion. */
    for(i=0;i<age && live(&actor);i++) {
        CongoCallback ai=CONGO_AI(task);
        if(ai!=func_08000F44_6B41E4 && ai!=func_0800110C_6B43AC)break;
        /* Birth's first scheduler pass runs DCC then post, not F44. */
        if(i)call_as(&actor,ai);
        if(!live(&actor))break;
        F32(o,8)+=F32(task,0x78);F32(o,16)+=F32(task,0x80);
    }
    for(i=0;i<32;i++)if(s_flames[i].actor.task==task) {
        for(j=0;j<7;j++)s_flames[i].seed[j]=seed[j];break;
    }
    if(seed[0]>s_flame_serial)s_flame_serial=seed[0];remember(seed[0]);
}
static unsigned int s_follower_flags,s_follower_yaw,s_follower_phase,s_snapshot_tick;
static void prepare_follower(void)
{
    if(!s_owner && s_adopted && s_follower_phase<PHASE_VICTORY && anchor_congo_native_root_task()) {
        U32(s_root.task,0xE8)=(U32(s_root.task,0xE8)&~ROOT_SPECIAL_MASK)|s_follower_flags;
        if(s_follower_phase==15u)U16(s_root.object,0x16)=
            (unsigned short)((s_follower_yaw+(s_tick-s_snapshot_tick)*4u)&1023u);
    }
}
static int apply_now(const AnchorCongoNativeSnapshot *s)
{
    unsigned int i,j,current_phase,root_fields[24];void *resource;
    if(!snapshot_valid(s) || !anchor_congo_native_root_task())return 0;
    for(i=0;i<6;i++)if(!live(&s_parts[i]))return 0;
    resource=func_800141C4_14DC4(0x1D);
    if(!resource || (unsigned long)resource==0xFFFFFFFFul)return 0;
    release_all();
    if(!finish_intro())return 0;
    current_phase=phase_of(root_ai());
    if(s_terminal_started && s->root[CONGO_PHASE]>=PHASE_VICTORY &&
       current_phase>s->root[CONGO_PHASE]) {
        /* A remote correction cannot replay a local victory entry already
         * consumed. This also makes repeated phase19 checkpoints harmless. */
        s_tick=s->tick;
        return 1;
    }
    for(i=0;i<24;i++)root_fields[i]=s->root[i];
    if(s_terminal_started && current_phase>=PHASE_VICTORY &&
       current_phase==root_fields[CONGO_PHASE] &&
       U16(s_root.task,0x8A)<root_fields[CONGO_TIMER])
        root_fields[CONGO_TIMER]=U16(s_root.task,0x8A);
    s_reconstructing=1;
    if(s->root[CONGO_PHASE]>=PHASE_VICTORY && !s_terminal_started) {
        U8(s_root.task,0x8D)=0;U32(s_root.task,0xE8)|=0x04000000u;
        call_as(&s_root,func_0800A228_6BD4C8);
        call_as(&s_root,func_08007D24_6BAFC4);
        s_terminal_started=1;
    }
    if(root_fields[CONGO_PHASE]==PHASE_VICTORY) {
        /*7D24's one-time entry just ran above (or ran natively). Its pending
         * continuation is7DEC; never put7D24 back into the task. */
        root_fields[CONGO_PHASE]=20u;root_fields[CONGO_TIMER]=120u;
    }
    apply_root(root_fields);
    /* Settled part state replaces global one-frame animation pulses; this
     * also restores missing late-join parts without restarting every frame. */
    for(i=0;i<9;i++)func_80023E40_24A40(i);
    for(i=0;i<6;i++) {
        void *task=s_parts[i].task,*o=s_parts[i].object;
        if(s_part_clip[i]!=s->part[i][0]) {
            func_8021664C_5D1B1C(task,s->part[i][0],0.05f,0);
            s_part_clip[i]=s->part[i][0];
        }
        {
            float frame=bits_float(s->part[i][1]);
            float count=func_8001B5AC_1C1AC(o);
            /* Never hand a renderer a network frame outside its actual
             * resource, even if a different mod changed that animation. */
            if(!(count>0.f) || count>65536.f)frame=0.f;
            else if(frame>=count)frame=count>1.f?count-1.f:0.f;
            F32(o,0x28)=frame;
        }
        U32(task,0x60)=s->part[i][2];
        CONGO_AI(task)=s->part[i][3]?func_080073D0_6BA670:s_part_updates[i];
        U16(o,0x16)=(unsigned short)s->root[CONGO_YAW];
    }
    if(s->spin_serial!=s_spin_serial && s->root[CONGO_PHASE]>=14u &&
       s->root[CONGO_PHASE]<=16u) {
        for(i=0;i<12;i++)if(live(&s_rays[i]))U32(s_rays[i].task,0x68)|=2u;
        call_as(&s_root,func_08009C04_6BCEA4);
    }
    s_ray_age=s->root[CONGO_PHASE]==14u && s->root[CONGO_TIMER]<=90u
                  ?90u-s->root[CONGO_TIMER]:90u;
    s_spin_serial=s->spin_serial;s_tick=s->tick;
    /* Remove authority-expired flames; a complete active set accompanies
     * checkpoints, so entering late or losing a birth packet is recoverable. */
    for(i=0;i<32;i++)if(live(&s_flames[i].actor)) {
        for(j=0;j<s->flame_count;j++)if(s_flames[i].seed[0]==s->flame[j][0])break;
        if(j==s->flame_count)U32(s_flames[i].actor.task,0x68)|=2u;
    }
    for(i=0;i<s->flame_count;i++)spawn_flame(s->flame[i],s->tick);
    /* A late checkpoint may skip a one-tick victory callback. Apply only
     * its monotonic milestones, while the owned local native death/camera
     * chain continues to render its own cosmetic explosions between them. */
    if(s->root[CONGO_PHASE]>=PHASE_VICTORY) {
        if(s_terminal_phase<20u)for(i=1;i<=5;i++)func_80023DF0_249F0(i);
        if(s->root[CONGO_PHASE]>=22u && s_terminal_phase<22u)func_80023DF0_249F0(0);
        if(s->root[CONGO_PHASE]>=23u && s_terminal_phase<23u)D_8015C562_15D162=1;
        if(s->root[CONGO_PHASE]>=26u && s_terminal_phase<26u) {
            func_80024038_24C38(0x12B);func_80024038_24C38(0x12E);
            D_8015C562_15D162=0;
        }
        if(s->root[CONGO_PHASE]>s_terminal_phase)s_terminal_phase=s->root[CONGO_PHASE];
    }
    s_reconstructing=0;s_adopted=1;
    s_follower_flags=s->root[CONGO_SPECIAL];s_follower_yaw=s->root[CONGO_YAW];
    s_follower_phase=s->root[CONGO_PHASE];s_snapshot_tick=s->tick;
    return 1;
}

int anchor_congo_native_apply(const AnchorCongoNativeSnapshot *s)
{
    if(!snapshot_valid(s) || !anchor_congo_native_ready())return 0;
    copy_bytes(&s_pending_snapshot,s,sizeof(s_pending_snapshot));s_pending_valid=1;
    /* Acceptance is distinct from publication: capture is unavailable until
     * native pre applies this queue. This avoids chasing a10Hz stream forever
     * when a recipient renders fewer than10 frames/second. */
    return 1;
}
int anchor_congo_native_world_paused(void)
{
    unsigned long ai,post;
    if(!anchor_congo_native_root_task() || !phase_of(root_ai()))return 0;
    ai=(unsigned long)(s_root.held?s_root.held_ai:CONGO_AI(s_root.task));
    post=(unsigned long)(s_root.held?s_root.held_post:CONGO_POST(s_root.task));
    return (ai&CALLBACK_DISABLED)!=0 || (post&CALLBACK_DISABLED)!=0 ||
           (U32(D_8015CC30_15D830,0xD4)&1u)!=0;
}
RECOMP_HOOK("func_8021925C_5D472C")
void anchor_congo_native_adopt_before_pre(void *task)
{
    if(!s_pending_valid || !anchor_congo_native_is_root(task) ||
       D_8016DAB4_16E6B4!=task || (U32(D_8015CC30_15D830,0xD4)&1u))return;
    /* The normal scheduler just ran1481C on this root. Its pre remains in
     * place so native contact staging still runs. All owned manual calls
     * below use this already installed file29 context; no unrelated or
     * missing previous task ever needs its TLB mapping guessed/restored. */
    if(apply_now(&s_pending_snapshot)) {
        s_pending_valid=0;
    }
    /* apply_now restores previously held callbacks to write the checkpoint.
     * Reapply this frame's waiting/pause/follower gate before native pre
     * continues; handoff roles change only after the bridge gets its ack. */
    anchor_congo_native_scheduler_begin();
}
