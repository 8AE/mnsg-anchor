#include "anchor_tsurami_native.h"
#include "anchor_tsurami_damage.h"
#include "anchor_boss_invite_world.h"
#include "anchor_remote_model_pool.h"
#include "boss_sync.h"
#ifndef ANCHOR_TSURAMI_NATIVE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

typedef void (*TsuramiCallback)(void *,void *);
#define U8(p,o) (*(volatile unsigned char *)((unsigned char *)(p)+(o)))
#define U16(p,o) (*(volatile unsigned short *)((unsigned char *)(p)+(o)))
#define U32(p,o) (*(volatile unsigned int *)((unsigned char *)(p)+(o)))
#define F32(p,o) (*(volatile float *)((unsigned char *)(p)+(o)))
#ifndef TSURAMI_PTR
#define TSURAMI_PTR(p,o) (*(void *volatile *)((unsigned char *)(p)+(o)))
#endif
#ifndef TSURAMI_AI
#define TSURAMI_AI(p) (*(TsuramiCallback volatile *)((unsigned char *)(p)+0xC))
#define TSURAMI_POST(p) (*(TsuramiCallback volatile *)((unsigned char *)(p)+0x10))
#endif
#ifndef CALLBACK_DISABLED
#define CALLBACK_DISABLED 0x00800000ul
#endif
#define ROOT_FLAGS_MASK 0x002006e1u
#define ROOT_SPECIAL_MASK 0x09c00400u
#define PROJECTILE_FLAGS_MASK 0x000003ffu
#define SERIAL_MAX 0x7fffffffu
#define PROJECTILE_MAX_AGE 1800u
#define REMOVE_PENDING 2u

extern unsigned short D_800C7AB2;
extern unsigned int D_8015C5E4;
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC60C_5B851C;
extern unsigned char D_8015CC30_15D830[];
extern void *func_800141C4_14DC4(unsigned int file);
extern void *func_8021DDE8_5D92B8(void *,TsuramiCallback,unsigned char,float,float,float,int);
extern void func_8021664C_5D1B1C(void *,unsigned int,float,unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern int func_800240DC_24CDC(int);
extern void func_80024038_24C38(unsigned int);
extern void func_80024088_24C88(unsigned int);
extern void func_80218F30_5D4400(void *,void *);
extern void func_08001D54_6B4FF4(void *,void *);
extern void func_08001DB0_6B5050(void *,void *);
extern void func_08001DF0_6B5090(void *,void *);
extern void func_08001EAC_6B514C(void *,void *);
extern void func_08001F28_6B51C8(void *,void *);
extern void func_08001F68_6B5208(void *,void *);
extern void func_08001FE8_6B5288(void *,void *);
extern void func_08002028_6B52C8(void *,void *);
extern void func_080020D8_6B5378(void *,void *);
extern void func_08002128_6B53C8(void *,void *);
extern void func_08002190_6B5430(void *,void *);
extern void func_080023E0_6B5680(void *,void *);
extern void func_08002460_6B5700(void *,void *);
extern void func_080024A0_6B5740(void *,void *);
extern void func_080024F0_6B5790(void *,void *);
extern void func_08002534_6B57D4(void *,void *);
extern void func_0800257C_6B581C(void *,void *);
extern void func_080027E4_6B5A84(void *,void *);
extern void func_08002864_6B5B04(void *,void *);
extern void func_080028A4_6B5B44(void *,void *);
extern void func_08002918_6B5BB8(void *,void *);
extern void func_080029D8_6B5C78(void *,void *);
extern void func_08002C4C_6B5EEC(void *,void *);
extern void func_08002CD8_6B5F78(void *,void *);
extern void func_08002D2C_6B5FCC(void *,void *);
extern void func_08002D6C_6B600C(void *,void *);
extern void func_08002DA8_6B6048(void *,void *);
extern void func_08002E34_6B60D4(void *,void *);
extern void func_08002EC8_6B6168(void *,void *);
extern void func_08002F0C_6B61AC(void *,void *);
extern void func_08002F50_6B61F0(void *,void *);
extern void func_08002FA4_6B6244(void *,void *);
extern void func_08000388_6B3628(void *,void *);
extern void func_080017F4_6B4A94(void *,void *);
extern void func_080045F8_6B7898(void *,void *);
extern void func_080046B8_6B7958(void *,void *);
extern void func_0800476C_6B7A0C(void *,void *);
extern void func_080049A4_6B7C44(void *,void *);
extern void func_08004ED0_6B8170(void *,void *);
extern void func_080056FC_6B899C(void *,void *);
extern void func_0800587C_6B8B1C(void *,void *);
extern void func_08005B1C_6B8DBC(void *,void *);
extern void func_08005B50_6B8DF0(void *,void *);
extern void func_08002FFC_6B629C(void *,void *);

/* Store overlay callbacks only in volatile BSS after file29 is resident. */
static TsuramiCallback volatile s_phases[33];
static void initialize_callbacks(void)
{
    s_phases[0]=0;
    s_phases[1]=func_08001D54_6B4FF4;
    s_phases[2]=func_08001DB0_6B5050;
    s_phases[3]=func_08001DF0_6B5090;
    s_phases[4]=func_08001EAC_6B514C;
    s_phases[5]=func_08001F28_6B51C8;
    s_phases[6]=func_08001F68_6B5208;
    s_phases[7]=func_08001FE8_6B5288;
    s_phases[8]=func_08002028_6B52C8;
    s_phases[9]=func_080020D8_6B5378;
    s_phases[10]=func_08002128_6B53C8;
    s_phases[11]=func_08002190_6B5430;
    s_phases[12]=func_080023E0_6B5680;
    s_phases[13]=func_08002460_6B5700;
    s_phases[14]=func_080024A0_6B5740;
    s_phases[15]=func_080024F0_6B5790;
    s_phases[16]=func_08002534_6B57D4;
    s_phases[17]=func_0800257C_6B581C;
    s_phases[18]=func_080027E4_6B5A84;
    s_phases[19]=func_08002864_6B5B04;
    s_phases[20]=func_080028A4_6B5B44;
    s_phases[21]=func_08002918_6B5BB8;
    s_phases[22]=func_080029D8_6B5C78;
    s_phases[23]=func_08002C4C_6B5EEC;
    s_phases[24]=func_08002CD8_6B5F78;
    s_phases[25]=func_08002D2C_6B5FCC;
    s_phases[26]=func_08002D6C_6B600C;
    s_phases[27]=func_08002DA8_6B6048;
    s_phases[28]=func_08002E34_6B60D4;
    s_phases[29]=func_08002EC8_6B6168;
    s_phases[30]=func_08002F0C_6B61AC;
    s_phases[31]=func_08002F50_6B61F0;
    s_phases[32]=func_08002FA4_6B6244;
}

typedef struct TsuramiActor {
    void *task,*object,*backlink;
    unsigned short actor;
    unsigned char generation,held;
    TsuramiCallback held_ai,held_post;
} TsuramiActor;
typedef struct TsuramiProjectile {
    TsuramiActor actor;
    unsigned int id,born,kind;
} TsuramiProjectile;
static TsuramiActor s_root,s_visual;
static TsuramiProjectile s_projectiles[ANCHOR_TSURAMI_MAX_PROJECTILES];
static AnchorTsuramiNativeSnapshot s_pending,s_terminal;
static unsigned int s_visit,s_tick,s_serial,s_root_clip,s_visual_clip;
static unsigned int s_injected_id,s_injected_born;
static int s_active,s_owner,s_paused,s_pending_valid,s_adopted;
static int s_terminal_started,s_terminal_valid,s_reconstructing,s_post_seen;
static int s_target_valid,s_target_depth;
static float s_target_object[8];
static void *s_target_saved,*s_target_task,*s_target_pointer;
/* Offline encounters retain native allocations. At most96 overflow identities
 * complement the32 wire slots until they expire or shared mode begins. */
static TsuramiActor s_offline_extras[96];
static void *s_flash;
static int s_flash_capture;
static void *s_health_task;
static unsigned int s_health_bit;

static void zero_bytes(void *p,unsigned int n)
{ volatile unsigned char *out=p;while(n--)*out++=0; }
static void copy_bytes(void *p,const void *q,unsigned int n)
{ volatile unsigned char *out=p;const unsigned char *in=q;while(n--)*out++=*in++; }
static unsigned int float_bits(float f)
{ union {float f;unsigned int u;} v;v.f=f;return v.u; }
static float bits_float(unsigned int u)
{ union {float f;unsigned int u;} v;v.u=u;return v.f; }
static int sane_float(unsigned int u,float lo,float hi)
{ float f=bits_float(u);return (u&0x7f800000u)!=0x7f800000u && f>=lo && f<=hi; }
static int pointer_valid(const void *p)
{
#ifdef ANCHOR_TSURAMI_NATIVE_HOST_TEST
    return p!=0;
#else
    unsigned int a=(unsigned int)(unsigned long)p;
    return !(a&3u) && ((a>=0x80001000u && a<0x80800000u) ||
                       anchor_remote_model_pool_contains(p));
#endif
}
static int callback_is(TsuramiCallback a,TsuramiCallback b)
{ return ((unsigned long)a&~CALLBACK_DISABLED)==((unsigned long)b&~CALLBACK_DISABLED); }
static void bind(TsuramiActor *a,void *task)
{
    a->task=task;a->object=TSURAMI_PTR(task,0x18);a->backlink=TSURAMI_PTR(task,4);
    a->actor=U16(task,0x5c);a->generation=U8(task,0x74);a->held=0;
}
static int live(const TsuramiActor *a)
{
    return D_800C7AB2==0x71 && s_visit==anchor_boss_invite_world_visit() &&
        pointer_valid(a->task) && pointer_valid(a->object) && pointer_valid(a->backlink) &&
        TSURAMI_PTR(a->task,4)==a->backlink && TSURAMI_PTR(a->backlink,0)==a->task &&
        TSURAMI_PTR(a->task,0x18)==a->object && U16(a->task,0x5c)==a->actor &&
        U8(a->task,0x74)==a->generation && !(U32(a->task,0x68)&REMOVE_PENDING);
}
static TsuramiCallback actor_ai(const TsuramiActor *a)
{ return a->held?a->held_ai:TSURAMI_AI(a->task); }
static TsuramiCallback actor_post(const TsuramiActor *a)
{ return a->held?a->held_post:TSURAMI_POST(a->task); }
static unsigned int phase_of(TsuramiCallback ai)
{
    unsigned int i;
    for(i=1;i<33;i++)if(callback_is(ai,s_phases[i]))return i;
    return 0;
}
int anchor_tsurami_native_is_root(const void *task)
{
    return task==s_root.task && live(&s_root) && s_root.actor==0xcb &&
        callback_is(actor_post(&s_root),func_08000388_6B3628);
}
void *anchor_tsurami_native_root_task(void)
{ return anchor_tsurami_native_is_root(s_root.task)?s_root.task:0; }
unsigned int anchor_tsurami_native_visit(void) {return s_visit;}
int anchor_tsurami_native_ready(void)
{
    /* Let the local camera/introduction complete its resource lifecycle. A
     * late entrant starts accepting combat state at the native neutral edge. */
    return anchor_tsurami_native_root_task() && live(&s_visual) &&
        TSURAMI_PTR(s_visual.task,0xd0)==s_root.task &&
        (s_terminal_started || phase_of(actor_ai(&s_root)));
}
int anchor_tsurami_native_snapshot_ready(void)
{
    return anchor_tsurami_native_ready() || (s_terminal_valid && s_terminal_started &&
        D_800C7AB2==0x71 && s_visit==anchor_boss_invite_world_visit());
}
void anchor_tsurami_native_finish_terminal(void) {s_terminal_valid=0;}
static int projectile_live(const TsuramiProjectile *p)
{
    TsuramiCallback ai;
    if(!p->id || !live(&p->actor) || p->actor.actor!=0xcb ||
       !callback_is(actor_post(&p->actor),func_80218F30_5D4400))return 0;
    ai=actor_ai(&p->actor);
    if(p->kind==1)return U16(p->actor.task,0x5e)==0x7e &&
        callback_is(ai,func_0800587C_6B8B1C);
    return U16(p->actor.task,0x5e)==0xcb &&
        (callback_is(ai,func_08004ED0_6B8170) || callback_is(ai,func_08005B50_6B8DF0));
}
int anchor_tsurami_native_is_projectile(const void *task)
{
    unsigned int i;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(s_projectiles[i].actor.task==task && projectile_live(&s_projectiles[i]))return 1;
    return 0;
}
unsigned int anchor_tsurami_native_projectile_id(const void *task)
{
    unsigned int i;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++) {
        TsuramiProjectile *p=&s_projectiles[i];
        if(p->actor.task==task && projectile_live(p) && !p->kind &&
           (U32(task,0xe8)&0x1fu)==1 && callback_is(actor_ai(&p->actor),func_08004ED0_6B8170))
            return p->id;
    }
    return 0;
}
void *anchor_tsurami_native_projectile_task(unsigned int id)
{
    unsigned int i;
    if(!id)return 0;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(s_projectiles[i].id==id && anchor_tsurami_native_projectile_id(s_projectiles[i].actor.task)==id)
            return s_projectiles[i].actor.task;
    return 0;
}
static void hold_noop(void *task,void *object) {(void)task;(void)object;}
static void hold(TsuramiActor *a,int post)
{
    if(!live(a)||a->held)return;
    a->held_ai=TSURAMI_AI(a->task);a->held_post=TSURAMI_POST(a->task);a->held=1;
    TSURAMI_AI(a->task)=hold_noop;if(post)TSURAMI_POST(a->task)=hold_noop;
}
static void unhold(TsuramiActor *a)
{
    if(a->held && live(a)) {
        unsigned long ai=(unsigned long)TSURAMI_AI(a->task),post=(unsigned long)TSURAMI_POST(a->task);
        unsigned long ours=(unsigned long)hold_noop&~CALLBACK_DISABLED;
        if((ai&~CALLBACK_DISABLED)==ours)TSURAMI_AI(a->task)=
            (TsuramiCallback)(((unsigned long)a->held_ai&~CALLBACK_DISABLED)|(ai&CALLBACK_DISABLED));
        if((post&~CALLBACK_DISABLED)==ours)TSURAMI_POST(a->task)=
            (TsuramiCallback)(((unsigned long)a->held_post&~CALLBACK_DISABLED)|(post&CALLBACK_DISABLED));
    }
    a->held=0;
}
static void release_all(void)
{
    unsigned int i;unhold(&s_root);unhold(&s_visual);
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)unhold(&s_projectiles[i].actor);
}
static void release_owned_flash(void);
void anchor_tsurami_native_reset(void)
{
    release_all();
    release_owned_flash();
    if(s_target_depth && D_801FC60C_5B851C==s_target_object)D_801FC60C_5B851C=s_target_saved;
    if(s_target_task && anchor_tsurami_native_is_root(s_target_task) &&
       TSURAMI_PTR(s_target_task,0x84)==s_target_object)TSURAMI_PTR(s_target_task,0x84)=s_target_pointer;
    s_target_depth=0;s_target_saved=s_target_task=s_target_pointer=0;
    s_flash=0;s_flash_capture=0;s_health_task=0;s_health_bit=0;
    zero_bytes(&s_root,sizeof(s_root));zero_bytes(&s_visual,sizeof(s_visual));
    zero_bytes(s_projectiles,sizeof(s_projectiles));
    s_tick=s_serial=s_root_clip=0;s_visual_clip=4;
    s_pending_valid=s_adopted=s_terminal_started=s_terminal_valid=s_post_seen=0;
    s_reconstructing=s_injected_id=s_target_valid=0;
    zero_bytes(s_offline_extras,sizeof(s_offline_extras));
}
void anchor_tsurami_native_set_role(int active,int owner,int paused)
{
    if(!active){release_all();s_pending_valid=0;}
    s_active=!!active;s_owner=!!owner;s_paused=!!paused;
    if(s_active) {
        unsigned int i;
        for(i=0;i<96;i++)if(live(&s_offline_extras[i]))U32(s_offline_extras[i].task,0x68)|=REMOVE_PENDING;
        zero_bytes(s_offline_extras,sizeof(s_offline_extras));
    }
}
void anchor_tsurami_native_set_target(float x,float y,float z)
{ s_target_object[2]=x;s_target_object[3]=y;s_target_object[4]=z;s_target_valid=1; }
void anchor_tsurami_native_clear_target(void) {s_target_valid=0;}

RECOMP_HOOK_RETURN("func_080017F4_6B4A94")
void anchor_tsurami_native_bind_root(void)
{
    void *task=D_8016DAB4_16E6B4;
    if(D_800C7AB2!=0x71 || !pointer_valid(task) || !pointer_valid(TSURAMI_PTR(task,0x18)) ||
       U16(task,0x5c)!=0xcb || !callback_is(TSURAMI_POST(task),func_08000388_6B3628) ||
       (U32(task,0x68)&REMOVE_PENDING))return;
    initialize_callbacks();anchor_tsurami_native_reset();s_visit=anchor_boss_invite_world_visit();
    bind(&s_root,task);anchor_tsurami_damage_bind_root(task);
}
RECOMP_HOOK_RETURN("func_080045F8_6B7898")
void anchor_tsurami_native_bind_visual(void)
{
    void *task=D_8016DAB4_16E6B4;
    if(pointer_valid(task) && anchor_tsurami_native_is_root(TSURAMI_PTR(task,0xd0)) &&
       U16(task,0x5e)==0xcb && callback_is(TSURAMI_AI(task),func_080046B8_6B7958)) {
        bind(&s_visual,task);s_visual_clip=4;
    }
}
RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_tsurami_native_animation(void *task,unsigned int clip)
{
    if(anchor_tsurami_native_is_root(task) && clip<=3)s_root_clip=clip;
    if(task==s_visual.task && live(&s_visual) && clip==4)s_visual_clip=clip;
}
static void register_projectile(void *task,unsigned int kind)
{
    unsigned int i,empty=ANCHOR_TSURAMI_MAX_PROJECTILES;
    if(!pointer_valid(task) || !pointer_valid(TSURAMI_PTR(task,0x18)) || U16(task,0x5c)!=0xcb ||
       !anchor_tsurami_native_root_task() || !callback_is(TSURAMI_POST(task),func_80218F30_5D4400))return;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++) {
        if(projectile_live(&s_projectiles[i]) && s_projectiles[i].actor.task==task)return;
        if(!projectile_live(&s_projectiles[i]) && empty==ANCHOR_TSURAMI_MAX_PROJECTILES)empty=i;
    }
    if(empty==ANCHOR_TSURAMI_MAX_PROJECTILES) {
        /* A complete32-actor checkpoint has a strict packet budget. Retire
         * the excess shared constructor before its first collision pass;
         * never publish a truncated active set or permanently freeze sync.
         * Offline play retains its native allocation/Hyper cadence. */
        if(s_active)U32(task,0x68)|=REMOVE_PENDING;
        else for(i=0;i<96;i++)if(!live(&s_offline_extras[i])) {bind(&s_offline_extras[i],task);break;}
        return;
    }
    if(!s_injected_id && s_serial==SERIAL_MAX) {
        for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)if(projectile_live(&s_projectiles[i]))break;
        if(i<ANCHOR_TSURAMI_MAX_PROJECTILES){if(s_active)U32(task,0x68)|=REMOVE_PENDING;return;}
        s_serial=0;
    }
    bind(&s_projectiles[empty].actor,task);
    s_projectiles[empty].kind=kind;
    s_projectiles[empty].id=s_injected_id?s_injected_id:++s_serial;
    s_projectiles[empty].born=s_injected_id?s_injected_born:s_tick;
}
RECOMP_HOOK_RETURN("func_080049A4_6B7C44")
void anchor_tsurami_native_projectile(void) {register_projectile(D_8016DAB4_16E6B4,0);}
RECOMP_HOOK_RETURN("func_080056FC_6B899C")
void anchor_tsurami_native_ring(void) {register_projectile(D_8016DAB4_16E6B4,1);}

static void target_scope_begin(void *task)
{
    if(s_target_depth){s_target_depth++;return;}
    if(!s_active || !s_owner || !s_target_valid || !anchor_tsurami_native_root_task() ||
       (task!=s_root.task && task!=D_8016DAB4_16E6B4))return;
    s_target_saved=D_801FC60C_5B851C;D_801FC60C_5B851C=s_target_object;s_target_depth=1;
}
static void target_scope_end(void)
{
    if(!s_target_depth || --s_target_depth)return;
    if(D_801FC60C_5B851C==s_target_object)D_801FC60C_5B851C=s_target_saved;
    s_target_saved=0;
}
/* Only these exact native routines read global player XYZ for Tsurami's
 * homing movement. Hyper replays invoke these hooks too. Other world tasks
 * always retain the real player pointer. */
RECOMP_HOOK("func_08001DF0_6B5090")
void anchor_tsurami_native_chase_begin(void *task) {target_scope_begin(task);}
RECOMP_HOOK_RETURN("func_08001DF0_6B5090")
void anchor_tsurami_native_chase_end(void) {target_scope_end();}
RECOMP_HOOK("func_08001EAC_6B514C")
void anchor_tsurami_native_chase_continue_begin(void *task) {target_scope_begin(task);}
RECOMP_HOOK_RETURN("func_08001EAC_6B514C")
void anchor_tsurami_native_chase_continue_end(void) {target_scope_end();}
RECOMP_HOOK("func_080049A4_6B7C44")
void anchor_tsurami_native_projectile_target_begin(void *task)
{
    if(D_800C7AB2==0x71 && pointer_valid(task) && U16(task,0x5c)==0xcb)
        target_scope_begin(task);
}
RECOMP_HOOK_RETURN("func_080049A4_6B7C44")
void anchor_tsurami_native_projectile_target_end(void) {target_scope_end();}
RECOMP_HOOK("func_802197D8_5D4CA8")
void anchor_tsurami_native_target_begin(void *task)
{
    if(s_active && s_owner && s_target_valid && !s_target_task && anchor_tsurami_native_is_root(task)) {
        s_target_task=task;s_target_pointer=TSURAMI_PTR(task,0x84);TSURAMI_PTR(task,0x84)=s_target_object;
    }
}
RECOMP_HOOK_RETURN("func_802197D8_5D4CA8")
void anchor_tsurami_native_target_end(void)
{
    if(s_target_task && anchor_tsurami_native_is_root(s_target_task) &&
       TSURAMI_PTR(s_target_task,0x84)==s_target_object)TSURAMI_PTR(s_target_task,0x84)=s_target_pointer;
    s_target_task=0;
}

static void prepare_flash_owner(void);
RECOMP_HOOK("func_80034734_35334")
void anchor_tsurami_native_scheduler_begin(void)
{
    unsigned int i;
    if(s_terminal_started){release_all();return;}
    if(!s_active || !anchor_tsurami_native_ready())return;
    if(s_owner && !s_paused && !s_pending_valid)prepare_flash_owner();
    if(!s_owner || s_paused || s_pending_valid)hold(&s_root,s_paused||s_pending_valid);
    /* Travelling attack AI chooses targets, reflects, and spawns damage
     * rings. Only the owner runs it. The common pre/post remain installed
     * for each follower's real player collision and regular motion. */
    if(!s_owner || s_paused || s_pending_valid)
        for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
            if(projectile_live(&s_projectiles[i]))hold(&s_projectiles[i].actor,s_paused||s_pending_valid);
    if(s_paused||s_pending_valid)hold(&s_visual,1);
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_tsurami_native_scheduler_end(void) {release_all();}
RECOMP_HOOK("func_08000388_6B3628")
void anchor_tsurami_native_post(void *task)
{
    if(anchor_tsurami_native_is_root(task) && !s_reconstructing && D_8015C5E4)s_post_seen=1;
}
void anchor_tsurami_native_tick(void)
{
    unsigned int i;int advanced=s_post_seen;s_post_seen=0;
    if(!anchor_tsurami_native_root_task())return;
    if(advanced && s_active && (!s_paused||s_terminal_started) &&
       (s_owner||s_adopted) && s_tick<SERIAL_MAX)s_tick++;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(!projectile_live(&s_projectiles[i]))zero_bytes(&s_projectiles[i],sizeof(s_projectiles[i]));
}
int anchor_tsurami_native_world_paused(void)
{
    if(!anchor_tsurami_native_root_task() || !phase_of(actor_ai(&s_root)))return 0;
    return (((unsigned long)actor_ai(&s_root)|(unsigned long)actor_post(&s_root))&CALLBACK_DISABLED)!=0 ||
        (U32(D_8015CC30_15D830,0xd4)&1u)!=0 || !D_8015C5E4;
}

static unsigned int capture_flash(void);
static int snapshot_valid(const AnchorTsuramiNativeSnapshot *s)
{
    unsigned int i,j;const unsigned int *r;
    if(!s)return 0;r=s->root;
    if(!r[TSU_PHASE] || r[TSU_PHASE]>TSU_PHASE_TERMINAL || r[TSU_TIMER]>65535 ||
       !r[TSU_HP] || r[TSU_HP]>12 || r[TSU_HURT]>180 || r[TSU_CLIP]>3 ||
       !sane_float(r[TSU_FRAME],0.f,65536.f) || (r[TSU_ANIM_STATE]&0xff000000u) ||
       (r[TSU_FLAGS]&~ROOT_FLAGS_MASK) || (r[TSU_FLAGS2]&~0x8000u) ||
       r[TSU_STATUS]>1 || (r[TSU_SPECIAL]&~ROOT_SPECIAL_MASK) ||
       r[TSU_COLLIDER_Z]>65535 || r[TSU_SHOT_DIRECTION]>255 ||
       r[TSU_SPIN_SPEED]>255 || r[TSU_BURST]>255 || r[TSU_EVENTS]>7 || r[TSU_FLASH]>511 ||
       (r[TSU_FLASH] && r[TSU_FLASH]<256) ||
       !sane_float(r[TSU_FLIGHT_VY],-100.f,100.f) ||
       s->tick>SERIAL_MAX || s->projectile_serial>SERIAL_MAX ||
       s->projectile_count>ANCHOR_TSURAMI_MAX_PROJECTILES || s->visual[0]!=4 ||
       !sane_float(s->visual[1],0.f,65536.f) ||
       (s->visual[2]&~ROOT_FLAGS_MASK) || s->visual[3]>1)return 0;
    for(i=TSU_X;i<=TSU_Z;i++)if(!sane_float(r[i],-32768.f,32768.f))return 0;
    for(i=TSU_RX;i<=TSU_RZ;i++)if(r[i]>1023)return 0;
    for(i=TSU_SCALE_X;i<=TSU_SCALE_Z;i++)if(!sane_float(r[i],0.f,32.f))return 0;
    for(i=TSU_VX;i<=TSU_VZ;i++)if(!sane_float(r[i],-100.f,100.f))return 0;
    for(i=TSU_DEST_X;i<=TSU_DEST_Z;i++)if(!sane_float(r[i],-32768.f,32768.f))return 0;
    for(i=0;i<s->projectile_count;i++) {
        const unsigned int *p=s->projectile[i];unsigned int mode=p[TSU_PROJECTILE_FLAGS]&0x1fu;
        if(!p[0] || p[0]>s->projectile_serial || p[1]>s->tick ||
           s->tick-p[1]>PROJECTILE_MAX_AGE || p[2]>1 ||
           p[TSU_PROJECTILE_TIMER]>65535 || (p[TSU_PROJECTILE_HEALTH]&0xfffe0000u) ||
           (p[TSU_PROJECTILE_HEALTH]&255)>20 ||
           ((p[TSU_PROJECTILE_HEALTH]>>8)&255)>180 ||
           /* 049A4 calls 8021A310: 0x8000 is a native orientation marker. */
           (p[TSU_PROJECTILE_YAW]>1023 && p[TSU_PROJECTILE_YAW]!=0x8000) ||
           (p[TSU_PROJECTILE_PITCH]>1023 && p[TSU_PROJECTILE_PITCH]!=0x8000) ||
           p[TSU_PROJECTILE_OPACITY]>180 || p[TSU_PROJECTILE_GRAVITY]>255)return 0;
        if(p[2]==0) {
            if((mode!=1 && mode!=2 && mode!=4 && mode!=8 && mode!=16) ||
               (p[3]&~0x25fu) || p[TSU_PROJECTILE_AI]>1 ||
               (p[TSU_PROJECTILE_AI]==1 && mode!=1))return 0;
        } else if((p[3]!=0 && p[3]!=0x20 && p[3]!=0x80 && p[3]!=0x100) ||
                  p[TSU_PROJECTILE_AI]!=2)return 0;
        for(j=TSU_PROJECTILE_X;j<=TSU_PROJECTILE_Z;j++)if(!sane_float(p[j],-32768.f,32768.f))return 0;
        for(j=TSU_PROJECTILE_VX;j<=TSU_PROJECTILE_VZ;j++)if(!sane_float(p[j],-100.f,100.f))return 0;
        for(j=TSU_PROJECTILE_SCALE_X;j<=TSU_PROJECTILE_SCALE_Z;j++)if(!sane_float(p[j],0.f,32.f))return 0;
        for(j=0;j<i;j++)if(s->projectile[j][0]==p[0])return 0;
    }
    return 1;
}
static void capture_root(unsigned int *r)
{
    unsigned int i;void *t=s_root.task,*o=s_root.object;
    r[TSU_PHASE]=s_terminal_started?TSU_PHASE_TERMINAL:phase_of(actor_ai(&s_root));
    r[TSU_TIMER]=U16(t,0x8a);r[TSU_HP]=U8(t,0x8d);r[TSU_HURT]=U8(t,0x8c);
    r[TSU_CLIP]=s_root_clip;r[TSU_FRAME]=float_bits(F32(o,0x28));
    r[TSU_ANIM_STATE]=U8(o,0x7c)|((unsigned int)U16(o,0x7e)<<8);
    r[TSU_FLAGS]=U32(t,0x60)&ROOT_FLAGS_MASK;r[TSU_FLAGS2]=U32(t,0x64)&0x8000u;
    r[TSU_STATUS]=U32(t,0x68)&1u;r[TSU_SPECIAL]=U32(t,0xe8)&ROOT_SPECIAL_MASK;
    for(i=0;i<3;i++) {
        r[TSU_X+i]=float_bits(F32(o,8+4*i));r[TSU_RX+i]=U16(o,0x14+2*i);
        r[TSU_SCALE_X+i]=float_bits(F32(o,0x1c+4*i));r[TSU_VX+i]=float_bits(F32(t,0x78+4*i));
    }
    r[TSU_COLLIDER_XY]=U16(t,0x3c)|((unsigned int)U16(t,0x3e)<<16);
    r[TSU_COLLIDER_Z]=U16(t,0x40);r[TSU_SHOT_DIRECTION]=U8(t,0xd3);
    r[TSU_PALETTE]=U32(t,0xec);r[TSU_SPIN_SPEED]=U8(D_8015CC30_15D830,0x184);
    r[TSU_BURST]=U8(D_8015CC30_15D830,0x186);
    r[TSU_FLIGHT_VY]=U32(D_8015CC30_15D830,0x188);
    for(i=0;i<3;i++)r[TSU_DEST_X+i]=U32(D_8015CC30_15D830,0x18c+4*i);
    r[TSU_FLASH]=capture_flash();
    r[TSU_EVENTS]=(func_800240DC_24CDC(0x178)?1u:0u)|
        (func_800240DC_24CDC(0x179)?2u:0u)|(func_800240DC_24CDC(0x1bc)?4u:0u);
}
static void capture_projectile(const TsuramiProjectile *a,unsigned int *p)
{
    unsigned int i;void *t=a->actor.task,*o=a->actor.object;
    p[0]=a->id;p[1]=a->born;p[2]=a->kind;p[3]=U32(t,0xe8)&PROJECTILE_FLAGS_MASK;
    p[4]=U16(t,0x8a);p[5]=U8(t,0x8d)|((unsigned int)U8(t,0x8c)<<8)|((U32(t,0x68)&1u)<<16);
    for(i=0;i<3;i++) {
        p[TSU_PROJECTILE_X+i]=float_bits(F32(o,8+4*i));
        p[TSU_PROJECTILE_VX+i]=float_bits(F32(t,0x78+4*i));
        p[TSU_PROJECTILE_SCALE_X+i]=float_bits(F32(o,0x1c+4*i));
    }
    p[TSU_PROJECTILE_YAW]=U16(o,0x16);p[TSU_PROJECTILE_PITCH]=U16(o,0x14);
    p[TSU_PROJECTILE_OPACITY]=a->kind?U8(t,0xd1):0;
    p[TSU_PROJECTILE_AI]=a->kind?2u:callback_is(actor_ai(&a->actor),func_08005B50_6B8DF0)?1u:0u;
    p[TSU_PROJECTILE_GRAVITY]=U8(t,0x75);
}
int anchor_tsurami_native_capture(AnchorTsuramiNativeSnapshot *s)
{
    unsigned int i;
    if(!s)return 0;
    if(s_terminal_valid && anchor_tsurami_native_snapshot_ready()) {
        copy_bytes(s,&s_terminal,sizeof(*s));return 1;
    }
    if(s_pending_valid || !anchor_tsurami_native_ready())return 0;
    capture_root(s->root);s->tick=s_tick;s->projectile_serial=s_serial;s->projectile_count=0;
    s->visual[0]=s_visual_clip;s->visual[1]=float_bits(F32(s_visual.object,0x28));
    s->visual[2]=U32(s_visual.task,0x60)&ROOT_FLAGS_MASK;
    s->visual[3]=callback_is(actor_ai(&s_visual),func_0800476C_6B7A0C);
    if(!s_terminal_started)for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(projectile_live(&s_projectiles[i]))capture_projectile(&s_projectiles[i],s->projectile[s->projectile_count++]);
    return snapshot_valid(s);
}
RECOMP_HOOK("func_08002FFC_6B629C")
void anchor_tsurami_native_terminal(void *task)
{
    if(!anchor_tsurami_native_is_root(task) || s_terminal_started)return;
    s_terminal_started=1;s_pending_valid=0;anchor_tsurami_damage_discard_pending();release_all();
    if(anchor_tsurami_native_capture(&s_terminal))s_terminal_valid=1;
}

extern unsigned char *D_8015C5C8_15D1C8;
extern void func_8022026C_5DB73C(void *,unsigned int,unsigned int,unsigned int);
extern void *func_80036158_36D58(void *,void *,unsigned int);
static void *current_flash(void)
{
    return D_8015C5C8_15D1C8?TSURAMI_PTR(D_8015C5C8_15D1C8,0x3b018):0;
}
static int owned_flash_live(void)
{
    void *node;unsigned int count;
    if(!live(&s_root) || !pointer_valid(s_flash) || U8(s_flash,4)!=1 ||
       U8(s_flash,5)!=11)return 0;
    node=TSURAMI_PTR(s_root.task,0x18);
    for(count=0;count<64 && pointer_valid(node);count++) {
        if(node==s_flash)return 1;
        node=TSURAMI_PTR(node,0);
    }
    return 0;
}
static unsigned int capture_flash(void)
{
    /* The native global alias survives free; 8000A228 resets freed alpha
     * to255. Only a live record in this root's list is an active flash. */
    return owned_flash_live()?256u|U8(s_flash,0x10):0;
}
RECOMP_HOOK("func_80036308_36F08")
void anchor_tsurami_native_flash_release(void *object)
{
    if(object==s_flash)s_flash=0;
}
static void release_owned_flash(void)
{
    void *flash=s_flash;int owned=owned_flash_live();s_flash=0;
    /* Pass the owned node explicitly; the global alias can be stale or
     * refer to another scenario's effect. Never clear that global alias. */
    if(owned)(void)func_80036158_36D58(s_root.task,flash,1);
}
static void prepare_flash_owner(void)
{
    unsigned int phase=phase_of(actor_ai(&s_root));
    if((phase==29 || phase==30) &&
       (!owned_flash_live() || current_flash()!=s_flash)) {
        /* A handoff may land after fade release, or while another scenario
         * owns the global effect alias. Native fade helpers act on that
         * alias: skip only their visual steps and keep animation recovery. */
        release_owned_flash();unhold(&s_root);TSURAMI_AI(s_root.task)=s_phases[31];
    }
}
RECOMP_HOOK("func_8022026C_5DB73C")
void anchor_tsurami_native_flash_begin(void *task)
{ s_flash_capture=anchor_tsurami_native_is_root(task); }
RECOMP_HOOK_RETURN("func_8022026C_5DB73C")
void anchor_tsurami_native_flash_end(void)
{
    if(s_flash_capture)s_flash=current_flash();s_flash_capture=0;
}
static int correct_flash(unsigned int state)
{
    if(state) {
        if(!owned_flash_live()) {
            void *f=current_flash();s_flash=0;
            /* A foreign live overlay must finish its own lifecycle. Its
             * presence must not block adoption of the gameplay checkpoint. */
            if(pointer_valid(f) && !(U8(f,4)&0x80u))return 1;
            func_8022026C_5DB73C(s_root.task,128,128,255);
            s_flash=current_flash();
        }
        if(!owned_flash_live())return 0;
        U8(s_flash,0x10)=(unsigned char)state;
    } else release_owned_flash();
    return 1;
}
static void set_frame(TsuramiActor *a,unsigned int bits)
{
    float frame=bits_float(bits),count=func_8001B5AC_1C1AC(a->object);
    if(!(count>0.f)||count>65536.f)frame=0.f;
    else if(frame>=count)frame=count>1.f?count-1.f:0.f;
    F32(a->object,0x28)=frame;
}
static void apply_root(const unsigned int *r)
{
    unsigned int i;void *t=s_root.task,*o=s_root.object;
    if(s_root_clip!=r[TSU_CLIP]) {
        func_8021664C_5D1B1C(t,r[TSU_CLIP],(float)(r[TSU_ANIM_STATE]>>8)/256.f,r[TSU_ANIM_STATE]&255);
        s_root_clip=r[TSU_CLIP];
    }
    set_frame(&s_root,r[TSU_FRAME]);U8(o,0x7c)=(unsigned char)r[TSU_ANIM_STATE];
    U16(o,0x7e)=(unsigned short)(r[TSU_ANIM_STATE]>>8);
    TSURAMI_AI(t)=s_phases[r[TSU_PHASE]];U16(t,0x8a)=(unsigned short)r[TSU_TIMER];
    U8(t,0x8d)=(unsigned char)r[TSU_HP];U8(t,0x8c)=(unsigned char)r[TSU_HURT];
    U32(t,0x60)=(U32(t,0x60)&~ROOT_FLAGS_MASK)|r[TSU_FLAGS];
    U32(t,0x64)=(U32(t,0x64)&~0x8000u)|r[TSU_FLAGS2];
    U32(t,0x68)=(U32(t,0x68)&~1u)|r[TSU_STATUS];
    U32(t,0xe8)=(U32(t,0xe8)&~ROOT_SPECIAL_MASK)|r[TSU_SPECIAL];
    for(i=0;i<3;i++) {
        F32(o,8+4*i)=bits_float(r[TSU_X+i]);U16(o,0x14+2*i)=(unsigned short)r[TSU_RX+i];
        F32(o,0x1c+4*i)=bits_float(r[TSU_SCALE_X+i]);F32(t,0x78+4*i)=bits_float(r[TSU_VX+i]);
    }
    U16(t,0x3c)=(unsigned short)r[TSU_COLLIDER_XY];U16(t,0x3e)=(unsigned short)(r[TSU_COLLIDER_XY]>>16);
    U16(t,0x40)=(unsigned short)r[TSU_COLLIDER_Z];U8(t,0xd3)=(unsigned char)r[TSU_SHOT_DIRECTION];
    U32(t,0xec)=r[TSU_PALETTE];U8(D_8015CC30_15D830,0x184)=(unsigned char)r[TSU_SPIN_SPEED];
    U8(D_8015CC30_15D830,0x186)=(unsigned char)r[TSU_BURST];
    U32(D_8015CC30_15D830,0x188)=r[TSU_FLIGHT_VY];
    for(i=0;i<3;i++)U32(D_8015CC30_15D830,0x18c+4*i)=r[TSU_DEST_X+i];
    for(i=0;i<3;i++) {
        unsigned int flag=i==0?0x178u:i==1?0x179u:0x1bcu;
        if(r[TSU_EVENTS]&(1u<<i))func_80024038_24C38(flag);else func_80024088_24C88(flag);
    }
}
static TsuramiProjectile *find_projectile(unsigned int id)
{
    unsigned int i;
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(s_projectiles[i].id==id && projectile_live(&s_projectiles[i]))return &s_projectiles[i];
    return 0;
}
extern void *func_8021DD4C_5D921C(void *,unsigned int,unsigned int,unsigned int,unsigned int);
extern void *func_8021A26C_5D573C(void *,unsigned int,unsigned int,unsigned int,unsigned int);
static void correct_projectile(TsuramiProjectile *a,const unsigned int *p)
{
    unsigned int i;void *t=a->actor.task,*o=a->actor.object;
    unhold(&a->actor);
    U32(t,0xe8)=(U32(t,0xe8)&~PROJECTILE_FLAGS_MASK)|p[3];U16(t,0x8a)=(unsigned short)p[4];
    U8(t,0x8d)=(unsigned char)p[5];U8(t,0x8c)=(unsigned char)(p[5]>>8);
    U32(t,0x68)=(U32(t,0x68)&~1u)|((p[5]>>16)&1u);
    for(i=0;i<3;i++) {
        F32(o,8+4*i)=bits_float(p[TSU_PROJECTILE_X+i]);
        F32(t,0x78+4*i)=bits_float(p[TSU_PROJECTILE_VX+i]);
        F32(o,0x1c+4*i)=bits_float(p[TSU_PROJECTILE_SCALE_X+i]);
    }
    U16(o,0x16)=(unsigned short)p[TSU_PROJECTILE_YAW];U16(o,0x14)=(unsigned short)p[TSU_PROJECTILE_PITCH];
    U8(t,0x75)=(unsigned char)p[TSU_PROJECTILE_GRAVITY];
    if(a->kind) {
        unsigned int opacity=p[TSU_PROJECTILE_OPACITY];
        /* All four 04ED0 ring spawns inherit this from a travelling shot.
         * Replica allocation uses the root, so restore the native roll. */
        U16(o,0x18)=0x8000;
        U8(t,0xd1)=(unsigned char)opacity;
        if(p[3]&(0x20u|0x80u|0x100u))
            func_8021DD4C_5D921C(t,(p[3]&0x20u)?opacity/2u:opacity,0,0,0);
        else func_8021A26C_5D573C(t,opacity,0,0,0);
    }
    TSURAMI_AI(t)=a->kind?func_0800587C_6B8B1C:
        p[TSU_PROJECTILE_AI]?func_08005B50_6B8DF0:func_08004ED0_6B8170;
    /* Returning projectiles own a local root at+D4. It is reconstructed from
     * this encounter, never copied from a foreign pointer. */
    if(!a->kind && (p[3]&0x11u))TSURAMI_PTR(t,0xd4)=s_root.task;
    a->born=p[1];
}
static TsuramiProjectile *spawn_projectile(const unsigned int *p)
{
    TsuramiCallback constructor=p[2]?func_080056FC_6B899C:func_080049A4_6B7C44;
    void *resource=func_800141C4_14DC4(0x1d),*task,*previous,*o;
    TsuramiProjectile *found;
    unsigned int i;
    if(!resource || (unsigned long)resource==0xfffffffful)return 0;
    /* Native list groups are gameplay semantics: returning shots group5,
     * their boss-impact rings group8, ordinary shots6 and floor rings10. */
    task=func_8021DDE8_5D92B8(s_root.task,constructor,
        p[2]?((p[3]&0x180u)?8:10):((p[3]&0x10u)?5:6),0.f,0.f,0.f,0);
    if(!pointer_valid(task)||!pointer_valid(TSURAMI_PTR(task,0x18)))return 0;
    U16(task,0x28)=0x1d;TSURAMI_PTR(task,0x2c)=resource;TSURAMI_PTR(task,0xd4)=s_root.task;
    U32(task,0xe8)=p[3];o=TSURAMI_PTR(task,0x18);
    for(i=0;i<3;i++)F32(o,8+4*i)=bits_float(p[TSU_PROJECTILE_X+i]);
    s_injected_id=p[0];s_injected_born=p[1];
    previous=D_8016DAB4_16E6B4;D_8016DAB4_16E6B4=task;constructor(task,o);
    /* Host tests do not automatically dispatch recomp hooks. Production
     * constructors invoke registration at return; duplicate registration is
     * identity checked. No historical collision or AI ticks are replayed. */
    register_projectile(task,p[2]);D_8016DAB4_16E6B4=previous;s_injected_id=0;
    found=find_projectile(p[0]);
    if(found)correct_projectile(found,p);else U32(task,0x68)|=REMOVE_PENDING;
    return found;
}
static int checkpoint_contains(const AnchorTsuramiNativeSnapshot *s,unsigned int id)
{
    unsigned int i;for(i=0;i<s->projectile_count;i++)if(s->projectile[i][0]==id)return 1;return 0;
}
static int apply_now(const AnchorTsuramiNativeSnapshot *s)
{
    unsigned int i;void *resource;
    if(!snapshot_valid(s) || !anchor_tsurami_native_ready())return 0;
    if(s_terminal_started){release_all();return 1;}
    resource=func_800141C4_14DC4(0x1d);
    if(!resource || (unsigned long)resource==0xfffffffful)return 0;
    release_all();
    if(s->root[TSU_PHASE]==TSU_PHASE_TERMINAL) {
        if(!correct_flash(0) || !boss_sync_queue_tsurami_shared_terminal())return 0;
        anchor_tsurami_damage_discard_pending();
        s_terminal_started=s_adopted=1;s_tick=s->tick;s_serial=s->projectile_serial;
        copy_bytes(&s_terminal,s,sizeof(s_terminal));s_terminal_valid=1;return 1;
    }
    if(!correct_flash(s->root[TSU_FLASH]))return 0;
    s_reconstructing=1;apply_root(s->root);
    set_frame(&s_visual,s->visual[1]);U32(s_visual.task,0x60)=s->visual[2];
    TSURAMI_AI(s_visual.task)=s->visual[3]?func_0800476C_6B7A0C:func_080046B8_6B7958;
    for(i=0;i<3;i++)F32(s_visual.object,8+4*i)=F32(s_root.object,8+4*i)-(i==1?16.f:0.f);
    for(i=0;i<ANCHOR_TSURAMI_MAX_PROJECTILES;i++)
        if(projectile_live(&s_projectiles[i]) && !checkpoint_contains(s,s_projectiles[i].id))
            U32(s_projectiles[i].actor.task,0x68)|=REMOVE_PENDING;
    for(i=0;i<s->projectile_count;i++) {
        const unsigned int *state=s->projectile[i];
        TsuramiProjectile *p=find_projectile(state[TSU_PROJECTILE_ID]);
        /* IDs may collide with this follower's pre-adoption local births.
         * A new mode/kind owns different native material, health callback
         * and collision-list membership; correcting transforms cannot turn
         * an existing ring or ordinary shot into the authority's actor. */
        if(p && (p->kind!=state[TSU_PROJECTILE_KIND] ||
           (p->kind ? U32(p->actor.task,0xe8)&PROJECTILE_FLAGS_MASK :
                      U32(p->actor.task,0xe8)&0x5fu) !=
           (p->kind ? state[TSU_PROJECTILE_FLAGS] : state[TSU_PROJECTILE_FLAGS]&0x5fu))) {
            U32(p->actor.task,0x68)|=REMOVE_PENDING;p=0;
        }
        if(!p)p=spawn_projectile(state);
        if(!p){s_reconstructing=0;return 0;}
        correct_projectile(p,s->projectile[i]);
    }
    s_tick=s->tick;s_serial=s->projectile_serial;s_adopted=1;s_reconstructing=0;return 1;
}
int anchor_tsurami_native_apply(const AnchorTsuramiNativeSnapshot *s)
{
    if(!snapshot_valid(s) || !anchor_tsurami_native_root_task())return 0;
    if(s_terminal_started)return 1;
    if(!anchor_tsurami_native_ready())return 0;
    copy_bytes(&s_pending,s,sizeof(s_pending));s_pending_valid=1;return 1;
}
RECOMP_HOOK("func_8021925C_5D472C")
void anchor_tsurami_native_adopt_before_pre(void *task)
{
    if(!anchor_tsurami_native_is_root(task) || D_8016DAB4_16E6B4!=task ||
       (U32(D_8015CC30_15D830,0xd4)&1u) || !D_8015C5E4)return;
    /* The scheduler has mapped file29 before this exact root pre. Native
     * constructors, custom damage callbacks and flash allocation are safe
     * here; frame-end bridge delivery only queues pointer-free values. */
    if(s_pending_valid && apply_now(&s_pending))s_pending_valid=0;
    if(!s_pending_valid && s_active && s_owner && !s_paused && !s_terminal_started)
        anchor_tsurami_damage_flush();
    anchor_tsurami_native_scheduler_begin();
}

/* The root private post retains the ordinary collision/damage/animation
 * pipeline on followers. Scope only Tsurami's extra A228 health-controller
 * branch, which would otherwise change root wobble/scale and emit particles
 * from this client's wall clock between authoritative checkpoints. */
RECOMP_HOOK("func_0800A228_6BD4C8")
void anchor_tsurami_native_health_begin(void *task)
{
    if(s_active && !s_owner && !s_terminal_started && !s_health_task &&
       anchor_tsurami_native_is_root(task)) {
        s_health_task=task;s_health_bit=U32(task,0xe8)&0x08000000u;
        U32(task,0xe8)&=~0x08000000u;
    }
}
RECOMP_HOOK_RETURN("func_0800A228_6BD4C8")
void anchor_tsurami_native_health_end(void)
{
    if(s_health_task && anchor_tsurami_native_is_root(s_health_task))U32(s_health_task,0xe8)|=s_health_bit;
    s_health_task=0;s_health_bit=0;
}
