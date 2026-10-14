/* File47 High Jump: native collision platforms and local camera stay native.
 * Only the elected participant advances the shared controller and timer.
 * Native layout and constructor order verified from ROM 0x705df0..0x707a88. */
#ifndef ANCHOR_MINIGAME_SASUKE_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_minigame_sync.h"

#define SB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define SS(p,o) (*(short *)((char *)(p)+(o)))
#define SH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define SW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define SI(p,o) (*(int *)((char *)(p)+(o)))
#define SF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef SP
#define SP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef SASUKE_DISABLED
#define SASUKE_DISABLED 0x00800000ul
#endif
typedef void (*SasukeCallback)(void *, void *);
extern unsigned short D_800C7AB2;
extern void *D_801FC60C_5B851C;
extern void *D_8016DAB4_16E6B4;
extern void *func_800141C4_14DC4(unsigned int);
extern int func_8003F1D8_3FDD8(void);
extern void func_80038BC8_397C8(unsigned short);
extern void func_80034EF8_35AF8(void *);
extern void func_08000AD4_7068C4(void *, void *);
extern void func_080006F8_7064E8(void *, void *);
extern void func_08000980_706770(void *, void *);
extern void func_080002B4_7060A4(void *, void *);

enum { SJ_ROOM=0x1e2, SJ_GAME=3, SJ_ENTITY=0x1b2, SJ_COUNT=17,
       SJ_ORBIT=1, SJ_SPIN=2, SJ_WORDS=10, SJ_CONTROLLER=8,
       SJ_GOAL_INPUT=1, SJ_PREDICT=6 };
typedef struct { unsigned char kind; short y, yaw, parameter, period; } SasukeConfig;
/* Native constructors use period 360 for the first four orbits, then 240. */
static const SasukeConfig s_config[SJ_COUNT] = {
    {1,70,597,0,360}, {1,120,938,90,360}, {1,145,85,0,360}, {1,195,426,0,360},
    {2,260,426,2,0}, {2,285,256,-2,0}, {2,309,256,2,0}, {2,335,426,-2,0},
    {2,360,85,2,0}, {1,420,938,0,240}, {2,477,256,-2,0}, {1,495,85,0,240},
    {2,520,85,-2,0}, {1,540,597,0,240}, {2,570,426,2,0}, {1,595,938,0,240},
    {1,540,85,120,240}
};
typedef struct {
    void *task, *object;
    unsigned char generation, initialized, predicted;
    SasukeCallback native;
} SasukeNode;
static SasukeNode s_root, s_platforms[SJ_COUNT], s_hud;
static int s_active, s_owner, s_ready, s_host_phase, s_outcome, s_terminal;
static int s_timer=-1, s_goal_sent;
static int s_intro_skipped, s_game_music_started;
static unsigned int s_sequence, s_visit;
static int s_goal[5];
static void *s_hud_text, *s_hud_sprite;
static void root_callback(void *, void *);
static void platform_callback(void *, void *);
static void hud_callback(void *, void *);
static const AnchorMinigameAdapter s_adapter;

static int callback_is(void *pointer, SasukeCallback callback)
{
    return ((unsigned long)pointer & ~SASUKE_DISABLED) ==
           ((unsigned long)callback & ~SASUKE_DISABLED);
}
static void schedule(SasukeNode *n, SasukeCallback callback)
{
    SP(n->task,0x0c)=(void *)((unsigned long)callback |
        ((unsigned long)SP(n->task,0x0c)&SASUKE_DISABLED));
}
static int resource(void)
{
    void *p=func_800141C4_14DC4(47);
    return p && p!=(void *)(unsigned long)0xffffffffu;
}
static int live(const SasukeNode *n)
{
    return D_800C7AB2==SJ_ROOM && s_visit==anchor_minigame_sync_visit() &&
        n->task && n->object && SP(n->task,0x18)==n->object &&
        SB(n->task,0x74)==n->generation && !(SW(n->task,0x68)&2u);
}
static int root_live(void)
{
    return live(&s_root) && SH(s_root.task,0x5c)==SJ_ENTITY && resource();
}
static int platform_live(int i)
{
    SasukeNode *n=&s_platforms[i];
    const SasukeConfig *c=&s_config[i];
    if(!n->task) return 0;
    int config=c->kind==SJ_ORBIT ?
        SF(n->task,0xdc)==(float)c->y && SS(n->task,0xd2)==c->yaw &&
        SS(n->task,0xd4)==c->period && SF(n->task,0xd8)==30.f && SF(n->task,0xe0)==20.f :
        SF(n->task,0xd4)==(float)c->y && SS(n->task,0xd2)==c->parameter;
    return root_live() && live(n) && n->initialized && config &&
        SH(n->task,0x5c)==SJ_ENTITY && SH(n->task,0x28)==47 &&
        SB(n->task,0xd0)==1 && callback_is(SP(n->task,0x0c),platform_callback);
}
static int hud_live(void)
{
    /* HUD task is a sprite task, not an entity1b2 platform. */
    return root_live() && s_hud.task && SP(s_root.task,0xe4)==s_hud.task &&
        s_hud_text && s_hud_sprite && SP(s_hud.task,0x78)==s_hud_text &&
        SP(s_hud.task,0x7c)==s_hud_sprite &&
        callback_is(SP(s_hud.task,0x0c),hud_callback);
}
static int graph(void)
{
    int i, phase;
    if (!root_live() || !s_root.initialized ||
        !SP(s_root.task,0xdc) || !SP(s_root.task,0xe0)) return 0;
    phase=SB(s_root.task,0xd0);
    if (phase<1 || phase==4 || phase>7) return 0;
    for(i=0;i<SJ_COUNT;++i) if(!platform_live(i)) return 0;
    return phase!=3 || hud_live();
}
static void restore(SasukeNode *n, SasukeCallback wrapper)
{
    if(live(n) && callback_is(SP(n->task,0x0c),wrapper)) schedule(n,n->native);
}
static void reset(void)
{
    int i;
    restore(&s_root,root_callback);
    for(i=0;i<SJ_COUNT;++i) restore(&s_platforms[i],platform_callback);
    if(s_hud.task && SP(s_hud.task,0x78)==s_hud_text &&
       callback_is(SP(s_hud.task,0x0c),hud_callback)) schedule(&s_hud,s_hud.native);
    anchor_minigame_zero(&s_root,sizeof(s_root));
    anchor_minigame_zero(s_platforms,sizeof(s_platforms));
    anchor_minigame_zero(&s_hud,sizeof(s_hud));
    s_active=s_owner=s_ready=s_host_phase=s_outcome=s_terminal=0;
    s_intro_skipped=s_game_music_started=0;
    s_timer=-1; s_goal_sent=0; s_sequence=0; s_visit=0;s_hud_text=s_hud_sprite=0;
}
static void role(int active,int owner,int ready)
{
    s_active=active; s_owner=owner; s_ready=ready;
}
static int position_word(float f,int *out)
{
    union { float f; unsigned int u; } value;
    value.f=f;
    if((value.u&0x7f800000u)==0x7f800000u || f < -32768.f || f > 32767.f) return 0;
    *out=(int)(f*1024.f);return 1;
}
static int valid_position(int value)
{
    return value>=-33554432 && value<=33553408;
}
static int validate(const AnchorMinigameState *s)
{
    unsigned int i;
    int phase,result;
    if(!s || s->controller_count!=SJ_CONTROLLER || s->actor_count!=SJ_COUNT ||
       s->ready!=1 || s->controller[0]!=1) return 0;
    phase=s->controller[1]; result=s->controller[7];
    if(phase<1 || phase==4 || phase>7 || s->controller[2]<0 || s->controller[2]>2 ||
       s->controller[3]<0 || s->controller[3]>2 || s->controller[4]<0 ||
       s->controller[4]>32767 || s->controller[5]<0 || s->controller[5]>65535 ||
       s->controller[6]<-1 || s->controller[6]>3960 || result<0 || result>2 ||
       s->result!=result || s->running!=(phase==3) ||
       (phase<5 && result) ||
       (phase==3 && s->controller[6]<0) || (phase==5 && result!=2) ||
       (phase==6 && result!=1) || (phase==7 && !result)) return 0;
    for(i=0;i<SJ_COUNT;++i) {
        const int *r=s->actors[i];
        if(s->actor_words[i]!=SJ_WORDS || r[0]!=s_config[i].kind || r[1]!=(int)i ||
           r[2]!=1 || r[3]!=1 || r[4]!=s_config[i].y || r[5]!=s_config[i].yaw ||
           r[6]<-32768 || r[6]>32767 ||
           (r[0]==SJ_SPIN && r[6]!=s_config[i].parameter) || !valid_position(r[7]) ||
           !valid_position(r[8]) || r[9]<0 || r[9]>65535) return 0;
    }
    return 1;
}
static int capture(AnchorMinigameState *s)
{
    int phase,i;
    if(!graph()) return 0;
    phase=SB(s_root.task,0xd0);
    if(hud_live()) s_timer=SI(s_hud.task,0x80);
    if(phase==5) s_outcome=2;
    if(phase==6) s_outcome=1;
    s->ready=1; s->running=phase==3; s->result=s_outcome;
    s->controller_count=SJ_CONTROLLER;
    s->controller[0]=1; s->controller[1]=phase;
    s->controller[2]=(signed char)SB(s_root.task,0xd1);
    s->controller[3]=(signed char)SB(s_root.task,0xd2);
    s->controller[4]=SS(s_root.task,0xd4);
    s->controller[5]=SH(s_root.task,0xd6);
    s->controller[6]=s_timer; s->controller[7]=s_outcome;
    s->actor_count=SJ_COUNT;
    for(i=0;i<SJ_COUNT;++i) {
        SasukeNode *n=&s_platforms[i];int *r=s->actors[i];
        s->actor_words[i]=SJ_WORDS;
        r[0]=s_config[i].kind;r[1]=i;r[2]=1;r[3]=1;
        r[4]=s_config[i].y;r[5]=s_config[i].yaw;
        r[6]=r[0]==SJ_ORBIT ? SS(n->task,0xd6) : s_config[i].parameter;
        if(!position_word(SF(n->object,8),&r[7]) ||
           !position_word(SF(n->object,0x10),&r[8])) return 0;
        r[9]=SH(n->object,0x16);
    }
    if(s_goal_sent && !s_owner) {
        s->input_count=1;s->input_words[0]=5;
        for(i=0;i<5;++i) s->inputs[0][i]=s_goal[i];
    }
    return validate(s);
}
static int apply(const AnchorMinigameState *s)
{
    int i;
    if(!validate(s) || !root_live()) return 0;
    s_host_phase=s->controller[1];
    /* This hint changes no graph scalar. The next native controller slot
     * constructs its own HUD after the verified dialogue-only phase. */
    if(!graph() || (s_host_phase>=3 && !hud_live() &&
        SB(s_root.task,0xd0)<3)) return 0;
    if(!s_terminal) {
        if(SB(s_root.task,0xd0)==1 && s_host_phase>=2) s_intro_skipped=1;
        SB(s_root.task,0xd0)=(unsigned char)s_host_phase;
        SB(s_root.task,0xd2)=(unsigned char)s->controller[3];
        SS(s_root.task,0xd4)=(short)s->controller[4];
        /* D1 is local camera selection, D6 button repeat, D8 previous input.
         * Their validation on the wire does not make them shared controls. */
        s_timer=s->controller[6];
        if(hud_live() && s_timer>=0) SI(s_hud.task,0x80)=s_timer;
        if(s->result) {
            s_outcome=s->result; s_terminal=1;
            SB(s_root.task,0xd0)=(unsigned char)(s_outcome==1 ? 6 : 5);
            SS(s_root.task,0xd4)=0;
        }
    }
    for(i=0;i<SJ_COUNT;++i) {
        SasukeNode *n=&s_platforms[i]; const int *r=s->actors[i];
        if(r[0]==SJ_ORBIT) SS(n->task,0xd6)=(short)r[6];
        SF(n->object,8)=(float)r[7]/1024.f;
        SF(n->object,0x0c)=(float)s_config[i].y;
        SF(n->object,0x10)=(float)r[8]/1024.f;
        SH(n->object,0x16)=(unsigned short)r[9];n->predicted=0;
    }
    s_hud.predicted=0;
    return 1;
}
static void retire_hud(void)
{
    if(hud_live()) {
        s_timer=SI(s_hud.task,0x80);
        func_80034EF8_35AF8(s_hud.task);
        SP(s_root.task,0xe4)=0;
        anchor_minigame_zero(&s_hud,sizeof(s_hud));
        s_hud_text=s_hud_sprite=0;
    }
}
static int input(const int *words,unsigned int count)
{
    float x,y,z;
    if(!root_live() || !s_owner || !s_ready) return 0;
    if(count!=8 || words[4]!=SJ_GOAL_INPUT || !valid_position(words[5]) ||
       !valid_position(words[6]) || !valid_position(words[7])) return 1;
    y=(float)words[5]/1024.f;x=(float)words[6]/1024.f;z=(float)words[7]/1024.f;
    if(SB(s_root.task,0xd0)==3 && y>620.f && x*x+z*z>2000.f) {
        /* Applied in the controller scheduler slot, where native HUD teardown
         * and outcome transitions normally happen. */
        s_outcome=1; s_terminal=1;
    }
    return 1;
}
static int return_pending(void)
{
    return root_live() && s_outcome && SB(s_root.task,0xd0)==7 &&
           SS(s_root.task,0xd4)==10 && s_root.task==D_8016DAB4_16E6B4;
}
static void result(int outcome)
{
    if(outcome<1 || outcome>2 || s_terminal || !root_live()) return;
    s_outcome=outcome;s_terminal=1;
    if(SB(s_root.task,0xd0)<5) {
        SB(s_root.task,0xd0)=(unsigned char)(outcome==1 ? 6 : 5);
        SS(s_root.task,0xd4)=0;
    }
}
static const AnchorMinigameAdapter s_adapter={
    SJ_GAME,reset,capture,validate,apply,role,input,return_pending,result
};

static void reconcile_game_music(void)
{
    /* Scenario 179 ends with native audio command 0x52. Followers bypass that
     * VM, so supply its audio once after their own native HUD initialization.
     * The full native intro remains responsible for an owner's music. */
    if(s_intro_skipped && !s_game_music_started && !s_terminal &&
       s_host_phase<5 && (s_owner || s_host_phase==3) && root_live() &&
       SB(s_root.task,0xd0)==3 && hud_live() && !func_8003F1D8_3FDD8()) {
        func_80038BC8_397C8(0x0052);
        s_game_music_started=1;
    }
}
static void root_callback(void *task,void *object)
{
    int phase;
    if(!root_live() || task!=s_root.task || object!=s_root.object) return;
    reconcile_game_music();
    phase=SB(task,0xd0);
    if(s_terminal && phase==3) {
        if(s_outcome==1) retire_hud();
        SB(task,0xd0)=(unsigned char)(s_outcome==1 ? 6 : 5);
        SS(task,0xd4)=0;phase=SB(task,0xd0);
    } else if(s_terminal && s_outcome==1 && phase>=5 && phase<=7) retire_hud();
    if(!s_active || (s_owner && s_ready) || s_terminal) {
        func_08000AD4_7068C4(task,object);
        if(SB(task,0xd0)==5) s_outcome=2;
        if(SB(task,0xd0)==6) s_outcome=1;
        reconcile_game_music();
        return;
    }
    if(phase==1 && s_host_phase>=2 && !func_8003F1D8_3FDD8()) {
        s_intro_skipped=1;
        SB(task,0xd0)=2;SS(task,0xd4)=0;return;
    }
    if(phase==2 && s_host_phase>=3 && !func_8003F1D8_3FDD8()) {
        func_08000AD4_7068C4(task,object);reconcile_game_music();return;
    }
    if(phase==3 && s_ready) {
        void *hud=SP(task,0xe4);short frames=SS(task,0xd4);
        /* Native phase3 computes this participant's camera. Its tail only
         * reads/deletes HUD and sets result phase; hiding the owned HUD for
         * this call keeps timeout/warnings authoritative and prevents deletion
         * when this participant reaches the native goal. */
        SP(task,0xe4)=0;
        func_08000AD4_7068C4(task,object);
        SP(task,0xe4)=hud;
        if(SB(task,0xd0)==6 && !s_goal_sent && D_801FC60C_5B851C) {
            int y,x,z;
            if(position_word(SF(D_801FC60C_5B851C,0x0c),&y) &&
               position_word(SF(D_801FC60C_5B851C,8),&x) &&
               position_word(SF(D_801FC60C_5B851C,0x10),&z)) {
                s_sequence=s_sequence>=0x7fffffffu ? 1u:s_sequence+1u;
                s_goal[0]=(int)s_sequence;s_goal[1]=SJ_GOAL_INPUT;
                s_goal[2]=y;s_goal[3]=x;s_goal[4]=z;s_goal_sent=1;
            }
        }
        SB(task,0xd0)=3;SS(task,0xd4)=frames;
    }
}
static void platform_callback(void *task,void *object)
{
    int i;
    for(i=0;i<SJ_COUNT;++i) if(s_platforms[i].task==task) break;
    if(i==SJ_COUNT || !platform_live(i) || object!=s_platforms[i].object) return;
    /* Platform phase1 is verified transform/visibility-only. Native collision
     * remains on the real initialized object, handled by the engine afterward.
     * Predict at most one transport interval, then hold during packet loss. */
    if(!s_active || (s_owner&&s_ready) || (!s_owner&&s_ready &&
       s_platforms[i].predicted<SJ_PREDICT)) {
        s_platforms[i].native(task,object);
        if(s_active && !s_owner) ++s_platforms[i].predicted;
    } else {
        /* Native visibility follows this participant's height even when the
         * synchronized pose is held. Phase1 has no other gameplay side effect. */
        float x=SF(object,8),y=SF(object,0x0c),z=SF(object,0x10);
        unsigned short yaw=SH(object,0x16);short tick=SS(task,0xd6);
        s_platforms[i].native(task,object);
        SF(object,8)=x;SF(object,0x0c)=y;SF(object,0x10)=z;SH(object,0x16)=yaw;
        if(s_config[i].kind==SJ_ORBIT) SS(task,0xd6)=tick;
    }
}
static void hud_callback(void *task,void *object)
{
    int timer;
    if(!hud_live() || task!=s_hud.task) return;
    timer=SI(task,0x80);
    /* The callback draws local HUD before its final saturating decrement. */
    func_080002B4_7060A4(task,object);
    if(s_active && (!s_ready || (!s_owner && s_hud.predicted>=SJ_PREDICT)))
        SI(task,0x80)=timer;
    else if(s_active && !s_owner) ++s_hud.predicted;
    s_timer=SI(task,0x80);
}

RECOMP_HOOK("func_08000AD4_7068C4")
void anchor_minigame_sasuke_root(void *task,void *object)
{
    if(D_800C7AB2!=SJ_ROOM || !task || !object || SB(task,0xd0)!=0 || !resource()) return;
    if(s_root.task && s_root.task!=task) return;
    s_visit=anchor_minigame_sync_visit();s_root.task=task;s_root.object=object;
    s_root.generation=SB(task,0x74);s_root.native=func_08000AD4_7068C4;
}
RECOMP_HOOK_RETURN("func_08000AD4_7068C4")
void anchor_minigame_sasuke_root_return(void)
{
    if(!root_live() || s_root.initialized || SB(s_root.task,0xd0)!=1) return;
    s_root.initialized=1;schedule(&s_root,root_callback);
    (void)anchor_minigame_sync_bind(&s_adapter);
}
static void platform_hook(void *task,void *object,int kind)
{
    int i;
    if(!root_live() || !task || !object || SH(task,0x28)!=47 || SB(task,0xd0)!=0) return;
    for(i=0;i<SJ_COUNT;++i) {
        const SasukeConfig *c=&s_config[i];
        if(c->kind!=kind) continue;
        if(kind==SJ_ORBIT) {
            if(SF(task,0xdc)!=(float)c->y || SS(task,0xd2)!=c->yaw ||
               SS(task,0xd6)!=c->parameter || SS(task,0xd4)!=c->period ||
               SF(task,0xd8)!=30.f || SF(task,0xe0)!=20.f) continue;
        } else if(SF(task,0xd4)!=(float)c->y || SS(task,0xd2)!=c->parameter ||
                  SH(object,0x16)!=(unsigned short)c->yaw) continue;
        if(s_platforms[i].task && s_platforms[i].task!=task) return;
        s_platforms[i].task=task;s_platforms[i].object=object;
        s_platforms[i].generation=SB(task,0x74);
        s_platforms[i].native=kind==SJ_ORBIT ? func_080006F8_7064E8:func_08000980_706770;
        return;
    }
}
static void platform_return(void)
{
    int i;
    for(i=0;i<SJ_COUNT;++i) if(live(&s_platforms[i]) &&
       !s_platforms[i].initialized && SB(s_platforms[i].task,0xd0)==1 &&
       SH(s_platforms[i].task,0x5c)==SJ_ENTITY) {
        s_platforms[i].initialized=1;schedule(&s_platforms[i],platform_callback);
    }
}
RECOMP_HOOK("func_080006F8_7064E8")
void anchor_minigame_sasuke_orbit(void *task,void *object) { platform_hook(task,object,SJ_ORBIT); }
RECOMP_HOOK_RETURN("func_080006F8_7064E8")
void anchor_minigame_sasuke_orbit_return(void) { platform_return(); }
RECOMP_HOOK("func_08000980_706770")
void anchor_minigame_sasuke_spin(void *task,void *object) { platform_hook(task,object,SJ_SPIN); }
RECOMP_HOOK_RETURN("func_08000980_706770")
void anchor_minigame_sasuke_spin_return(void) { platform_return(); }
RECOMP_HOOK("func_080002B4_7060A4")
void anchor_minigame_sasuke_hud(void *task,void *object)
{
    if(!root_live() || !task || SP(s_root.task,0xe4)!=task || s_hud.task) return;
    s_hud.task=task;s_hud.object=object;s_hud.generation=SB(task,0x74);
    s_hud_text=SP(task,0x78);s_hud_sprite=SP(task,0x7c);
    s_hud.native=func_080002B4_7060A4;
    schedule(&s_hud,hud_callback);
}

/* Entity generations live in entity-private +74. Sprite HUD scratch has a
 * different layout, so allocator/deleter boundaries invalidate its handle. */
RECOMP_HOOK("func_80034A10_35610")
void anchor_minigame_sasuke_reuse(void *task)
{
    int i;
    if(task==s_hud.task) {
        anchor_minigame_zero(&s_hud,sizeof(s_hud));s_hud_text=s_hud_sprite=0;
    }
    if(task==s_root.task) { reset();return; }
    for(i=0;i<SJ_COUNT;++i) if(task==s_platforms[i].task)
        anchor_minigame_zero(&s_platforms[i],sizeof(s_platforms[i]));
}
RECOMP_HOOK("func_80034EF8_35AF8")
void anchor_minigame_sasuke_delete(void *task)
{
    /* Do not restore a callback during native teardown. */
    int i;
    if(task==s_hud.task) {
        anchor_minigame_zero(&s_hud,sizeof(s_hud));s_hud_text=s_hud_sprite=0;
    }
    if(task==s_root.task) s_root.task=0;
    for(i=0;i<SJ_COUNT;++i) if(task==s_platforms[i].task) s_platforms[i].task=0;
}
