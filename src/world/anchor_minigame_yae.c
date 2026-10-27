/* File69 Mermaid trial. Native actor/VM layouts and register arguments are
 * audited in mnsg-yae-native-20261025.md. The swimmer and camera are local;
 * the authority advances the timer and real, reconstructed obstacle actors. */
#ifndef ANCHOR_MINIGAME_YAE_HOST_TEST
#include "platform/modding.h"
#include "platform/recomputils.h"
#endif
#include "world/anchor_minigame_sync.h"
#include "world/anchor_minigame_invites.h"
#include "player/anchor_player_models.h"
#include "player/anchor_remote_model_pool.h"

#define YB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define YS(p,o) (*(short *)((char *)(p)+(o)))
#define YH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define YW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define YI(p,o) (*(int *)((char *)(p)+(o)))
#define YF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef YP
#define YP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef YAE_DISABLED
#define YAE_DISABLED 0x00800000ul
#endif
typedef void (*YaeCallback)(void *,void *);
extern unsigned short D_800C7AB2;
extern unsigned int D_8015C5DC_15D1DC;
extern unsigned char *D_8015C5C8_15D1C8;
extern void osViBlack(unsigned char active);
extern int item_sync_save_is_loaded(void);
extern void *D_801FC604_5B8514, *D_801FC60C_5B851C, *D_8016DAB4_16E6B4;
extern void *D_801FC628_5B8538;
extern void *func_800141C4_14DC4(unsigned int);
extern int func_8003F1D8_3FDD8(void);
extern void func_80221F70_5DD440(void);
extern void func_80024088_24C88(int);
extern void func_80038BC8_397C8(unsigned short);
extern void func_80038B98_39798(unsigned short);
extern void func_80034EF8_35AF8(void *);
extern void *func_08000000_72B850(void *,YaeCallback,unsigned char);
extern void func_080014E8_72CD38(void *,void *);
extern void func_08000E1C_72C66C(void *,void *);
extern void func_0800107C_72C8CC(void *,void *);
extern void func_080002B4_72BB04(void *);
extern void func_08000AB4_72C304(void *,float);
extern void func_08000AC8_72C318(void *,float);
extern int anchor_boss_invite_world_can_prompt(void);
extern int anchor_dialog_busy(void);
extern void *func_8000DBF0_E7F0(void *,unsigned int,unsigned int,
    float,float,float,short,short,short,float,float,float,short,short);
extern void func_800115E0_121E0(void *,int,int);
extern void func_8001136C_11F6C(void *);
extern void *func_80036158_36D58(void *,void *,unsigned char);

enum { Y_ROOM=0x154,Y_GAME=4,Y_COUNT=64,Y_CONTROLLER=9,Y_WORDS=15,
       Y_GOAL=1,Y_HIT=2,Y_PREDICT=6,Y_INPUTS=32,Y_MOTION=0x00800200 };
typedef struct {
    void *task,*object;
    unsigned char generation,initialized,predicted,clone,staged,pending_hit,contact_reported,contact_pending,motion_held;
    unsigned int id,motion_bits;
    YaeCallback native;
} YaeNode;
static YaeNode s_root,s_obstacles[Y_COUNT],s_hud,s_swimmer;
static void *s_local,*s_hud_text,*s_hud_sprite,*s_hud_marker;
static void *s_hud_remotes[ANCHOR_MINIGAME_MAX_PARTICIPANTS];
typedef struct {
    void *task,*object,*local;
    unsigned int visit;
    unsigned char generation,phase;
} YaeCandidate;
static YaeCandidate s_entry,s_candidate;
static unsigned int s_visit,s_next_id,s_sequence,s_input_count;
static int s_inputs[Y_INPUTS][5];
static unsigned int s_input_words[Y_INPUTS];
static int s_active,s_owner,s_ready,s_host_phase,s_outcome,s_terminal;
static int s_personal_failed,s_music_started,s_intro_skipped,s_debug_intro,s_warning,s_timer=-1;
static unsigned int s_goal_sequence;
static int s_black_owned,s_black_release_pending,s_world_fade_had_record,s_direct_bootstrap;
static unsigned int s_direct_frames,s_direct_host_tick,s_direct_host_generation;
static int s_direct_wait_visible,s_direct_result_pending;
static void *s_swimmer_init_task,*s_hud_draw_task;
static int s_hud_draw_timer;
static void root_callback(void *,void *);
static void obstacle_callback(void *,void *);
static void hud_callback(void *,void *);
static void swimmer_callback(void *,void *);
static int root_live(void);
static int root_identity(void);
static int direct_graph_ready(void);
static int direct_intro_scene_ready(void);
static const AnchorMinigameAdapter s_adapter;

static int callback_is(void *pointer,YaeCallback callback)
{
    return ((unsigned long)pointer&~YAE_DISABLED)==
           ((unsigned long)callback&~YAE_DISABLED);
}
static int native_pointer(const void *pointer)
{
#ifdef ANCHOR_MINIGAME_YAE_HOST_TEST
    return pointer && pointer!=(void *)(unsigned long)0xffffffffu;
#else
    unsigned int address=(unsigned int)(unsigned long)pointer;
    return address>=0x80001000u && address<0x80800000u;
#endif
}
static int resource(void)
{
    void *handle=func_800141C4_14DC4(69);
    return handle && handle!=(void *)(unsigned long)0xffffffffu;
}
static void schedule(YaeNode *node,YaeCallback callback)
{
    YP(node->task,0x0c)=(void *)((unsigned long)callback |
        ((unsigned long)YP(node->task,0x0c)&YAE_DISABLED));
}
/* VI black hides explicit arrival without changing the native world fade's
 * alpha/return, so its room-start/control release can finish normally. */
static int ordinary_transition_scope(void)
{
    unsigned char *system=D_8015C5C8_15D1C8;
    if(!native_pointer(system)) return 0;
    int step=YB(system,0x3add4);
    return (step==12 || (step==13 && D_800C7AB2==Y_ROOM)) &&
        !YH(system,0x3ae26) && item_sync_save_is_loaded();
}
static int same_world_reveal_scope(void)
{
    unsigned char *system=D_8015C5C8_15D1C8;
    return ordinary_transition_scope() && YB(system,0x3add4)==13 &&
        YB(system,0x3adde)==1 && !YS(system,0x3ae16) &&
        !YB(system,0x3ae29) &&
        (!YB(system,0xcf8a2) || (s_direct_bootstrap && root_identity()));
}
static int ordinary_handoff_ready(void)
{
    /* An unexpected ordinary destination can finish without another fade
     * record. Release our deferred mask only after its own world/player
     * guards prove that native transition has fully relinquished control. */
    unsigned char *system=D_8015C5C8_15D1C8;
    return native_pointer(system) && YB(system,0x3add4)==13 && YB(system,0x3adde)==1 &&
        !YH(system,0x3ae26) && !YS(system,0x3ae16) && !YB(system,0x3ae29) &&
        !YB(system,0xcf8a2) && item_sync_save_is_loaded() &&
        anchor_boss_invite_world_can_prompt() &&
        native_pointer(D_801FC604_5B8514) && YB(D_801FC604_5B8514,0xcd)==0 &&
        !anchor_dialog_busy() && !func_8003F1D8_3FDD8();
}
void anchor_minigame_yae_direct_load_begin(void)
{
    s_black_owned=1;s_black_release_pending=0;osViBlack(1);
}
void anchor_minigame_yae_direct_load_cancel(void)
{
    if(!s_black_owned) return;
    /* During another native teardown/reset, retain a pending handoff until a
     * proven native unblank writer runs; never forget an asserted VI mask. */
    if(same_world_reveal_scope() || ordinary_handoff_ready()) {
        s_black_owned=s_black_release_pending=0;osViBlack(0);
    } else s_black_release_pending=1;
}
static void reassert_direct_black(int native_unblanked)
{
    if(!s_black_owned) return;
    int current=anchor_minigame_invites_yae_black_scope() && ordinary_transition_scope();
    if(s_black_release_pending || !current) {
        if(native_unblanked) s_black_owned=s_black_release_pending=0;
        else if(same_world_reveal_scope() || ordinary_handoff_ready()) {
            s_black_owned=s_black_release_pending=0;osViBlack(0);
        } else s_black_release_pending=1;
        return;
    }
    osViBlack(1);
}
RECOMP_HOOK_RETURN("func_80003880_4480")
void anchor_minigame_yae_world_reset_return(void) { reassert_direct_black(1); }
RECOMP_HOOK("func_80036860_37460")
void anchor_minigame_yae_world_fade(void)
{
    /* The no-record native early return does NOT call osViBlack(0). Cache
     * exactly its entry predicate; the record may be freed in the body. */
    s_world_fade_had_record=native_pointer(D_8015C5C8_15D1C8) &&
        YP(D_8015C5C8_15D1C8,0x3b014)!=0;
}
RECOMP_HOOK_RETURN("func_80036860_37460")
void anchor_minigame_yae_world_fade_return(void)
{
    int unblanked=s_world_fade_had_record;s_world_fade_had_record=0;
    reassert_direct_black(unblanked);
}

static int live(const YaeNode *node)
{
    return D_800C7AB2==Y_ROOM && s_visit==anchor_minigame_sync_visit() &&
        native_pointer(node->task) && native_pointer(node->object) &&
        YP(node->task,0x18)==node->object && YB(node->task,0x74)==node->generation &&
        !(YW(node->task,0x68)&2u);
}
static int root_identity(void)
{
    return D_800C7AB2==Y_ROOM && s_visit==anchor_minigame_sync_visit() &&
        native_pointer(s_root.task) && native_pointer(s_root.object) && native_pointer(s_local) &&
        YP(s_root.task,0x18)==s_root.object && YB(s_root.task,0x74)==s_root.generation &&
        YP(s_root.task,0xd0)==s_local && YH(s_root.task,0x5e)==0x327 && resource();
}
static int root_live(void)
{
    return root_identity() && !(YW(s_root.task,0x68)&2u);
}
static int hud_live(void)
{
    return root_live() && native_pointer(s_hud.task) &&
        YP(s_local,0x10)==s_hud.task && YP(s_hud.task,0x78)==s_hud_text &&
        YP(s_hud.task,0x7c)==s_hud_sprite && YP(s_hud.task,0x80)==s_hud_marker &&
        s_hud_text && s_hud_sprite && s_hud_marker &&
        callback_is(YP(s_hud.task,0x0c),hud_callback);
}
static int hud_marker_owned(void *marker)
{
    void *cursor;
    unsigned int i;
    if(!hud_live() || !marker) return 0;
    cursor=YP(s_hud.task,0x18);
    for(i=0;i<ANCHOR_MINIGAME_MAX_PARTICIPANTS+16u &&
        (native_pointer(cursor) || anchor_remote_model_pool_contains(cursor));++i) {
        if(cursor==marker) return YB(cursor,4)==2 && YB(cursor,5)==10;
        cursor=YP(cursor,0);
    }
    return 0;
}
static void release_hud_markers(void)
{
    unsigned int i;
    for(i=0;i<ANCHOR_MINIGAME_MAX_PARTICIPANTS;++i) {
        void *marker=s_hud_remotes[i];s_hud_remotes[i]=0;
        if(hud_marker_owned(marker)) (void)func_80036158_36D58(s_hud.task,marker,1);
    }
}
static void update_hud_markers(void)
{
    int members[ANCHOR_MINIGAME_MAX_PARTICIPANTS];
    int heights[ANCHOR_MINIGAME_MAX_PARTICIPANTS];
    unsigned int i,j,members_count,count=0;
    float x,y,z,spacing=18.f,scale=3.f;
    if(!hud_live()) return;
    for(i=0;i<ANCHOR_MINIGAME_MAX_PARTICIPANTS;++i) {
        if(hud_marker_owned(s_hud_remotes[i])) YB(s_hud_remotes[i],0x64)|=1u;
        else s_hud_remotes[i]=0;
    }
    if(!s_active || !s_ready || s_direct_bootstrap || s_terminal || s_personal_failed ||
       YB(s_local,0)!=7 || !hud_marker_owned(s_hud_marker)) return;
    members_count=anchor_minigame_sync_participants(Y_GAME,members,
        ANCHOR_MINIGAME_MAX_PARTICIPANTS);
    /* The committed roster is sorted by CID. Filter before assigning lanes so
     * the local member or a missing render snapshot cannot leave a gap. */
    for(i=0;i<members_count;++i) {
        unsigned int height_bits;
        if(!anchor_player_models_get_display_position(members[i],&x,&y,&z)) continue;
        __builtin_memcpy(&height_bits,&y,sizeof(height_bits));
        if((height_bits&0x7f800000u)==0x7f800000u) continue;
        /* Published player Y is proxy Y+15; the local meter uses proxy Y+440.
         * Clamp the float before conversion to avoid overflow on bad samples. */
        y+=425.f;
        heights[count++]=y<=0.f ? 0:y>=820.f ? 820:(int)y;
    }
    if(count>4u) { spacing=72.f/(float)count;scale=12.f/(float)count; }
    for(i=0;i<count;++i) {
        void *marker=s_hud_remotes[i];
        if(!marker) {
            marker=func_8000DBF0_E7F0(s_hud.task,0x480002e0u,0x8006d920u,
                0.f,0.f,0.f,(short)0x8000,(short)0x8000,(short)0x8000,
                scale,scale,scale,0x4d1,0);
            if(!marker) continue;
            YB(marker,5)=10;s_hud_remotes[i]=marker;
        }
        /* Only the first16-byte icon wrapper is shared. The native countdown
         * and green/black fill remain exclusively on the original HUD. */
        YP(marker,0x30)=YP(s_hud_marker,0x30);
        for(j=0;j<3u;++j) YF(marker,0x1c+4u*j)=scale;
        func_800115E0_121E0(marker,(int)(48.f+spacing*(float)i),
            ((820-heights[i])*150)/820+56);
        YB(marker,0x64)&=(unsigned char)~1u;
    }
    if(count) func_8001136C_11F6C(s_hud.task);
}
static void *swimmer_object(void)
{
    void *task;
    if(!root_live() || !native_pointer(task=YP(s_local,0x18)) ||
       !native_pointer(YP(task,0x18)) || YH(task,0x28)!=69 ||
       (!callback_is(YP(task,0x0c),func_0800107C_72C8CC) &&
        !callback_is(YP(task,0x0c),swimmer_callback)) ||
       (YW(task,0x68)&2u)) return 0;
    return YP(task,0x18);
}
static int obstacle_live(const YaeNode *node)
{
    return root_live() && live(node) && node->initialized && node->id &&
        YH(node->task,0x28)==69 && YH(node->task,0x5e)==0x33a &&
        callback_is(YP(node->task,0x0c),obstacle_callback);
}
/* Native scheduler post80218F30 runs gravity and2*velocity integration after
 * the actor callback. Holding velocity alone still falls by.4 every frame.
 * Preserve only its two verified motion-enable bits; collisions stay native. */
static void hold_motion(YaeNode *node)
{
    if(!live(node)) return;
    if(!node->motion_held) {
        node->motion_bits=YW(node->task,0x60)&Y_MOTION;node->motion_held=1;
    }
    YW(node->task,0x60)&=~(unsigned int)Y_MOTION;
}
static void restore_motion(YaeNode *node)
{
    if(!node->motion_held) return;
    if(live(node))
        YW(node->task,0x60)=(YW(node->task,0x60)&~(unsigned int)Y_MOTION)|node->motion_bits;
    node->motion_held=0;
}
static void reset(void)
{
    int i;
    release_hud_markers();
    if(live(&s_root) && callback_is(YP(s_root.task,0x0c),root_callback))
        schedule(&s_root,func_080014E8_72CD38);
    for(i=0;i<Y_COUNT;++i) {
        YaeNode *n=&s_obstacles[i];
        restore_motion(n);
        if(live(n) && callback_is(YP(n->task,0x0c),obstacle_callback))
            schedule(n,func_08000E1C_72C66C);
    }
    if(live(&s_swimmer) && callback_is(YP(s_swimmer.task,0x0c),swimmer_callback))
        schedule(&s_swimmer,func_0800107C_72C8CC);
    if(s_direct_bootstrap && !anchor_minigame_invites_yae_direct_pending())
        anchor_minigame_yae_direct_load_cancel();
    s_direct_bootstrap=0;s_direct_frames=s_direct_host_tick=s_direct_host_generation=0;
    s_direct_wait_visible=s_direct_result_pending=0;
    s_swimmer_init_task=s_hud_draw_task=0;
    anchor_minigame_zero(&s_swimmer,sizeof(s_swimmer));
    if(hud_live()) YP(s_hud.task,0x0c)=(void *)((unsigned long)func_080002B4_72BB04 |
        ((unsigned long)YP(s_hud.task,0x0c)&YAE_DISABLED));
    anchor_minigame_zero(&s_root,sizeof(s_root));
    anchor_minigame_zero(s_obstacles,sizeof(s_obstacles));
    anchor_minigame_zero(&s_hud,sizeof(s_hud));
    s_local=s_hud_text=s_hud_sprite=s_hud_marker=0;
    s_candidate.task=s_entry.task=0;
    s_visit=s_next_id=s_sequence=s_input_count=s_goal_sequence=0;
    s_active=s_owner=s_ready=s_host_phase=s_outcome=s_terminal=0;
    s_personal_failed=s_music_started=s_intro_skipped=s_debug_intro=s_warning=0;s_timer=-1;
}
static void role(int active,int owner,int ready)
{
    s_active=active;s_owner=owner;s_ready=ready;
}
static int position(float value,int *out)
{
    union { float f;unsigned int u; } bits;
    bits.f=value;
    if((bits.u&0x7f800000u)==0x7f800000u || value<-32768.f || value>32767.f)
        return 0;
    *out=(int)(value*1024.f);return 1;
}
static int position_valid(int value)
{
    return value>=-33554432 && value<=33553408;
}
static int phase_valid(int phase)
{
    return phase>=4 && phase<=12 && phase!=8;
}
static int validate(const AnchorMinigameState *s)
{
    unsigned int i,j;
    int phase,outcome;
    if(!s || s->ready!=1 || s->controller_count!=Y_CONTROLLER ||
       s->controller[0]!=1 || s->actor_count>Y_COUNT) return 0;
    phase=s->controller[1];outcome=s->controller[5];
    if(!phase_valid(phase) || s->controller[2]<0 || s->controller[2]>32767 ||
       s->controller[3]<-1 || s->controller[3]>1500 ||
       s->controller[4]<0 || s->controller[4]>2 || outcome<0 || outcome>2 ||
       s->result!=outcome || s->running!=(phase==7) ||
       s->controller[6]<0 || s->controller[6]>0x7fffffff ||
       s->controller[7]<0 || s->controller[7]>1 ||
       s->controller[8]!=0 || (phase<7 && s->actor_count) ||
       (phase==7 && (s->controller[3]<0 || outcome)) ||
       (phase==9 && outcome!=1) || (phase==10 && outcome!=2) ||
       (phase>=11 && !outcome) || (phase<9 && outcome)) return 0;
    for(i=0;i<s->actor_count;++i) {
        const int *r=s->actors[i];
        if(s->actor_words[i]!=Y_WORDS || r[0]!=1 || r[1]<=0 ||
           r[1]>s->controller[6] || r[2]!=1 || r[3]<1 || r[3]>2 ||
           r[4]<0 || r[4]>1 || r[5]<0 || r[5]>32767) return 0;
        for(j=6;j<9;++j) if(!position_valid(r[j])) return 0;
        for(j=9;j<12;++j) if(r[j]<0 || r[j]>65535) return 0;
        for(j=12;j<15;++j) if(r[j]<-65536 || r[j]>65536) return 0;
        if(i && s->actors[i-1][1]>=r[1]) return 0;
    }
    return 1;
}
static int node_for_id(unsigned int id)
{
    int i;
    for(i=0;i<Y_COUNT;++i)
        if(s_obstacles[i].task && s_obstacles[i].id==id) return i;
    return -1;
}
static int free_node(void)
{
    int i;
    for(i=0;i<Y_COUNT;++i)
        if(!s_obstacles[i].task || !live(&s_obstacles[i])) return i;
    return -1;
}
static int capture(AnchorMinigameState *s)
{
    int phase,i,j;
    unsigned int count=0;
    if(!root_live() || s_personal_failed) return 0;
    phase=YB(s_local,0);
    if(!phase_valid(phase)) return 0;
    if(phase==7 && (!hud_live() || !swimmer_object())) return 0;
    if(hud_live()) s_timer=YI(s_hud.task,0x84);
    if((int)YB(s_local,2)>s_warning) s_warning=YB(s_local,2);
    if(phase==9) s_outcome=1;
    if(phase==10 && !s_personal_failed) s_outcome=2;
    s->ready=1;s->running=phase==7;s->result=s_outcome;
    s->controller_count=Y_CONTROLLER;
    s->controller[0]=1;s->controller[1]=phase;s->controller[2]=YS(s_local,4);
    s->controller[3]=s_timer;s->controller[4]=YB(s_local,2);
    s->controller[5]=s_outcome;s->controller[6]=(int)s_next_id;
    s->controller[7]=s_music_started;s->controller[8]=0;
    for(i=0;i<Y_COUNT;++i)
        if(live(&s_obstacles[i]) && s_obstacles[i].staged) return 0;
    for(i=0;i<Y_COUNT;++i) if(obstacle_live(&s_obstacles[i])) {
        YaeNode *n=&s_obstacles[i];int *r=s->actors[count];
        if(n->staged) return 0;
        if(YB(n->task,0xd0)!=1 && YB(n->task,0xd0)!=2) return 0;
        s->actor_words[count]=Y_WORDS;
        r[0]=1;r[1]=(int)n->id;r[2]=1;r[3]=YB(n->task,0xd0);
        r[4]=YB(n->task,0xd1);r[5]=YS(n->task,0xd4);
        for(j=0;j<3;++j) {
            if(!position(YF(n->object,8+4*j),&r[6+j]) ||
               !position(YF(n->task,0x78+4*j),&r[12+j])) return 0;
            r[9+j]=YH(n->object,0x14+2*j);
        }
        ++count;
    }
    /* Allocation slots are reused locally; stable spawn order is the wire order. */
    for(i=1;i<(int)count;++i) {
        int row[Y_WORDS];
        for(j=0;j<Y_WORDS;++j) row[j]=s->actors[i][j];
        int k=i;
        while(k>0 && s->actors[k-1][1]>row[1]) {
            for(j=0;j<Y_WORDS;++j) s->actors[k][j]=s->actors[k-1][j];
            --k;
        }
        for(j=0;j<Y_WORDS;++j) s->actors[k][j]=row[j];
    }
    s->actor_count=count;
    unsigned int ack=anchor_minigame_sync_local_ack(Y_GAME);
    for(i=0,j=0;i<(int)s_input_count;++i) {
        if((unsigned int)s_inputs[i][0]<=ack) continue;
        if(i!=j) {
            s_input_words[j]=s_input_words[i];
            for(int k=0;k<5;++k) s_inputs[j][k]=s_inputs[i][k];
        }
        ++j;
    }
    s_input_count=(unsigned int)j;
    s->input_count=s_input_count;
    for(i=0;i<(int)s_input_count;++i) {
        s->input_words[i]=s_input_words[i];
        for(j=0;j<(int)s_input_words[i];++j) s->inputs[i][j]=s_inputs[i][j];
    }
    return validate(s);
}
static int has_id(const AnchorMinigameState *s,unsigned int id)
{
    unsigned int i;
    for(i=0;i<s->actor_count;++i) if((unsigned int)s->actors[i][1]==id) return 1;
    return 0;
}
static void reconcile_music(void)
{
    if(!s_direct_bootstrap && !s_music_started && !s_terminal && root_live() && YB(s_local,0)>=6 &&
       !func_8003F1D8_3FDD8()) {
        /* Scenario177's only gameplay music command is 0292, not 0052. */
        if(s_intro_skipped) func_80038BC8_397C8(0x0292);
        s_music_started=1;
    }
}
static int apply(const AnchorMinigameState *s)
{
    unsigned int i;
    int j,missing=0;
    if(!validate(s) || !root_live() || s_personal_failed) return 0;
    if(s_direct_result_pending && s->result && s->result!=s_direct_result_pending) return 0;
    s_host_phase=s->controller[1];
    /* Prelude reconstruction is scheduled natively. Never skip the camera,
     * fade, HUD or local swimmer constructors by copying an advanced phase. */
    if(s_host_phase>=7 && YB(s_local,0)<7) return 0;
    if(s_direct_bootstrap && s_host_phase>=7 && !direct_graph_ready()) return 0;
    if(s_host_phase==7 && (!hud_live() || !swimmer_object())) return 0;
    if(s_terminal) return 1;
    /* Retire obsolete identities before reserving the replacement graph. This
     * also permits a full64-row checkpoint to replace a full local graph. New
     * allocations remain staged until every required identity is initialized. */
    for(i=0;i<Y_COUNT;++i) if(live(&s_obstacles[i]) &&
       !has_id(s,s_obstacles[i].id)) {
        void *task=s_obstacles[i].task;
        s_obstacles[i].task=0;func_80034EF8_35AF8(task);
    }
    for(i=0;i<s->actor_count;++i) {
        int index=node_for_id((unsigned int)s->actors[i][1]);
        if(index>=0 && obstacle_live(&s_obstacles[index])) continue;
        if(index>=0 && live(&s_obstacles[index])) { missing=1;continue; }
        index=free_node();
        if(index<0) return 0;
        void *task=func_08000000_72B850(s_root.task,func_08000E1C_72C66C,5);
        if(!native_pointer(task) || !native_pointer(YP(task,0x18))) return 0;
        YaeNode *n=&s_obstacles[index];
        anchor_minigame_zero(n,sizeof(*n));
        n->task=task;n->object=YP(task,0x18);n->generation=YB(task,0x74);
        n->native=func_08000E1C_72C66C;n->id=(unsigned int)s->actors[i][1];n->clone=1;n->staged=1;
        /* Native phase0 preserves XYZ. Keep incomplete graph actors far above
         * the playable[-440,400]course until atomic adoption; no origin hits. */
        YF(n->object,8)=0.f;YF(n->object,0x0c)=30000.f;YF(n->object,0x10)=0.f;
        /* Native phase0 is allowed exactly once at its normal scheduler slot. */
        missing=1;
    }
    if(missing) return 0;
    /* All live identities/resources were checked before committing graph scalars. */
    for(i=0;i<Y_COUNT;++i) if(obstacle_live(&s_obstacles[i]) &&
       !has_id(s,s_obstacles[i].id)) {
        void *task=s_obstacles[i].task;
        s_obstacles[i].task=0;func_80034EF8_35AF8(task);
    }
    for(i=0;i<s->actor_count;++i) {
        YaeNode *n=&s_obstacles[node_for_id((unsigned int)s->actors[i][1])];
        const int *r=s->actors[i];
        /* Until the reliable semantic hit is reflected, retain this local
         * native collapse across older phase1 checkpoints, avoiding a replay. */
        if((!n->contact_reported && !n->contact_pending) || r[3]!=1) {
            YB(n->task,0xd0)=(unsigned char)r[3];YB(n->task,0xd1)=(unsigned char)r[4];
            YS(n->task,0xd4)=(short)r[5];
        }
        for(j=0;j<3;++j) {
            YF(n->object,8+4*j)=(float)r[6+j]/1024.f;
            YH(n->object,0x14+2*j)=(unsigned short)r[9+j];
            YF(n->task,0x78+4*j)=(float)r[12+j]/1024.f;
        }
        n->predicted=0;n->staged=0;restore_motion(n);
    }
    s_next_id=(unsigned int)s->controller[6];
    s_timer=s->controller[3];
    if(hud_live() && s_timer>=0) YI(s_hud.task,0x84)=s_timer;
    if(s_host_phase==7 && YB(s_local,0)==7) {
        YS(s_local,4)=(short)s->controller[2];
        if(s->controller[4]>s_warning) {
            func_80038B98_39798(0x0b);s_warning=s->controller[4];
        }
        YB(s_local,2)=(unsigned char)s->controller[4];
    }
    s_hud.predicted=0;
    if(s->result) {
        s_outcome=s->result;s_terminal=1;
        YB(s_local,0)=(unsigned char)(s_outcome==1 ? 9:10);YS(s_local,4)=0;
    }
    if(s_direct_bootstrap && !s_owner && s_host_phase>=4 && s_host_phase<=6 &&
       s->source_tick &&
       (s->adopted_generation>s_direct_host_generation ||
        (s->adopted_generation==s_direct_host_generation && s->source_tick>s_direct_host_tick))) {
        /* A live natural host may read its instructions for more than20s.
         * Renew only for a successfully adopted advancing authority sample,
         * never for local frames or an incomplete local native graph. */
        s_direct_host_generation=s->adopted_generation;s_direct_host_tick=s->source_tick;
        if(direct_intro_scene_ready() || direct_graph_ready()) s_direct_frames=0;
    }
    reconcile_music();return 1;
}
static int queue_input(int kind,int a,int b,int c,unsigned int count)
{
    unsigned int i;
    if(s_input_count>=Y_INPUTS || s_sequence>=0x7fffffffu) return 0;
    for(i=0;i<s_input_count;++i)
        if(s_inputs[i][1]==kind && (kind==Y_GOAL || s_inputs[i][2]==a)) return 1;
    int *row=s_inputs[s_input_count];
    row[0]=(int)++s_sequence;row[1]=kind;row[2]=a;row[3]=b;row[4]=c;
    s_input_words[s_input_count++]=count;
    if(kind==Y_GOAL) s_goal_sequence=s_sequence;
    return 1;
}
static int input(const int *words,unsigned int count)
{
    if(!root_live() || !s_owner || !s_ready || s_personal_failed) return 0;
    if(count<5 || !anchor_minigame_sync_participant_current(Y_GAME,words[0])) return 1;
    if(words[4]==Y_GOAL) {
        if(count==8 && YB(s_local,0)==7 && position_valid(words[5]) &&
           position_valid(words[6]) && position_valid(words[7]) &&
           words[5]>380*1024 && words[5]<=450*1024 &&
           words[6]>=-80*1024 && words[6]<=80*1024) {
            s_outcome=1;s_terminal=1;
        }
    } else if(words[4]==Y_HIT && count==7 && words[5]>0 && words[6]==1 &&
              YB(s_local,0)==7) {
        int index=node_for_id((unsigned int)words[5]);
        if(index>=0 && obstacle_live(&s_obstacles[index]) &&
           YB(s_obstacles[index].task,0xd0)==1) {
            /* Scalar native break transition; never replay collision/damage. */
            s_obstacles[index].pending_hit=1;
        }
    }
    return 1;
}
static int return_pending(void)
{
    return root_live() && s_terminal &&
        ((YB(s_local,0)==11 && YS(s_local,4)==20) || YB(s_local,0)==12) &&
        D_8016DAB4_16E6B4==s_root.task;
}
static void result(int outcome)
{
    if(outcome<1 || outcome>2 || s_terminal || s_direct_result_pending ||
       s_personal_failed || !root_live()) return;
    /* Native9 reads the proxy on entry and10/f1 writes its task. A result
     * received during the visible prelude must finish constructing it first. */
    if(s_direct_bootstrap && !direct_graph_ready()) { s_direct_result_pending=outcome;return; }
    s_outcome=outcome;s_terminal=1;
    YB(s_local,0)=(unsigned char)(outcome==1 ? 9:10);YS(s_local,4)=0;
}
static const AnchorMinigameAdapter s_adapter={
    Y_GAME,reset,capture,validate,apply,role,input,return_pending,result
};
static void finish_if_cleaned(int old_phase,int old_frame)
{
    if(!root_live()) return;
    if((old_phase==12 && YB(s_local,0)==1) ||
       ((s_personal_failed || s_outcome==2) && old_phase==11 && old_frame==20 &&
        !YP(s_local,0x0c) && !YP(s_local,0x10) && !YP(s_local,0x18)))
        anchor_minigame_invites_yae_finished();
}
static int direct_graph_ready(void)
{
    void *camera,*proxy;
    return root_live() && hud_live() && live(&s_swimmer) && s_swimmer.initialized &&
        native_pointer(proxy=YP(s_local,0x18)) && proxy==s_swimmer.task &&
        YB(proxy,0xd0)==1 && native_pointer(swimmer_object()) &&
        native_pointer(camera=YP(s_local,0x0c)) &&
        native_pointer(YP(camera,0x6c)) && YB(s_local,1)==2;
}
static int direct_intro_scene_ready(void)
{
    static const float intro[6]={0.f,-354.f,-110.f,0.f,-404.f,190.f};
    void *camera,*descriptor,*world;
    unsigned int i;
    if(!root_live() || YB(s_local,0)!=4 || YS(s_local,4)!=21 ||
       YP(s_local,0x10) || YP(s_local,0x18) ||
       !native_pointer(camera=YP(s_local,0x0c)) ||
       !native_pointer(descriptor=YP(camera,0x6c)) ||
       !native_pointer(D_801FC628_5B8538)) return 0;
#ifdef ANCHOR_MINIGAME_YAE_HOST_TEST
    world=YP(D_801FC628_5B8538,0x2c);
#else
    /* Native controller08001504..28 decodes this tagged CPU descriptor. */
    world=(void *)(unsigned long)(YW(D_801FC628_5B8538,0x2c)&0x8ffffffeu);
#endif
    if(!native_pointer(world) || YF(camera,0x74)!=2560.f ||
       YH(descriptor,0x1a)!=0xa00u || YH(world,0x1a)!=0xa00u) return 0;
    /* The initializer copies an OLD world descriptor before changing camera
     * scalars. Wait for its scheduled callback and the root's common-tail
     * copy to publish the actual intro camera, rather than trusting pointers. */
    for(i=0;i<6u;++i)
        if(YF(camera,0x80+4u*i)!=intro[i] || YF(descriptor,4u*i)!=intro[i] ||
           YW(descriptor,4u*i)!=YW(world,4u*i)) return 0;
    return 1;
}
static void reveal_direct_intro(void)
{
    void *fade;
    if(s_direct_wait_visible || !s_active || s_owner || !s_ready || s_terminal ||
       s_host_phase<4 || s_host_phase>5 || !s_direct_host_tick ||
       !direct_intro_scene_ready() || !same_world_reveal_scope() ||
       func_8003F1D8_3FDD8() || !native_pointer(fade=YP(s_local,0x1c))) return;
    func_08000AB4_72C304(fade,0.f);func_08000AC8_72C318(fade,0.f);
    s_direct_wait_visible=1;
    if(s_black_owned) { s_black_owned=s_black_release_pending=0;osViBlack(0); }
}
static int direct_commit_ready(void)
{
    if(!direct_graph_ready() || !same_world_reveal_scope() ||
       func_8003F1D8_3FDD8()) return 0;
    if(s_active && !s_direct_result_pending &&
       (!s_ready || (!s_owner && s_host_phase<7 && !s_terminal))) return 0;
    return YB(s_local,0)==7 || s_terminal;
}
static void finish_direct_bootstrap(void)
{
    if(!direct_commit_ready()) return;
    void *fade=YP(s_local,0x1c);
    if(!native_pointer(fade)) return;
    /* A new owner has held its1500 frames. A joining follower keeps the timer
     * from its last successful authoritative apply, never resets the host. */
    if(!s_direct_wait_visible) func_08000AC8_72C318(fade,0.f);
    s_direct_bootstrap=0;s_direct_frames=0;
    if(s_black_owned) { s_black_owned=s_black_release_pending=0;osViBlack(0); }
    if(s_direct_result_pending) {
        int outcome=s_direct_result_pending;s_direct_result_pending=0;result(outcome);
    }
    reconcile_music();
}
static void direct_bootstrap_callback(void *task,void *object)
{
    int phase=YB(s_local,0),frame=YS(s_local,4);
    if(++s_direct_frames>600u) {
        /* No new constructor-OOM recovery is inferred. For a live initialized
         * root whose readiness never arrives, use its existing nullable
         * cleanup phase rather than keeping our output mask indefinitely. */
        s_personal_failed=1;
        anchor_minigame_yae_direct_load_cancel();s_direct_bootstrap=0;
        YB(s_local,0)=11;YS(s_local,4)=0;return;
    }
    if(phase==4 && frame==1) {
        func_080014E8_72CD38(task,object);
        if(!root_live()) return;
        void *fade=YP(s_local,0x1c);
        if(native_pointer(fade)) {
            func_08000AB4_72C304(fade,255.f);func_08000AC8_72C318(fade,255.f);
        }
        YS(s_local,4)=20;return;
    }
    if(phase==4 && frame==20) {
        func_080014E8_72CD38(task,object);
        if(!root_live()) return;
        void *fade=YP(s_local,0x1c);
        if(native_pointer(fade) && (!s_active || s_owner || s_host_phase>=6)) {
            func_08000AB4_72C304(fade,255.f);func_08000AC8_72C318(fade,255.f);
        }
        /* Native4/f20 places the player and camera at the tutorial bottom.
         * Hold the following prelude slot while its host readsVM177. */
        if(s_active && !s_owner && s_host_phase<6 && !s_terminal && !s_direct_result_pending) {
            YS(s_local,4)=21;reveal_direct_intro();return;
        }
        YB(s_local,0)=6;YS(s_local,4)=1;return;
    }
    if(phase==4 && frame==21) {
        if(s_active && !s_owner && s_host_phase<6 && !s_terminal && !s_direct_result_pending) {
            /* This slot has no constructors or VM. Its native common tail
             * publishes the scheduled camera descriptor once per frame. */
            func_080014E8_72CD38(task,object);
            if(root_live()) { YS(s_local,4)=21;reveal_direct_intro(); }
            return;
        }
        /* Keep an already-visible intro visible through the nativephase6
         * camera/swimmer transition. Input and timer still await readiness. */
        YB(s_local,0)=6;YS(s_local,4)=1;return;
    }
    if(phase==6) {
        func_080014E8_72CD38(task,object);
        /* Native6/f1 constructs exactly once; its20-tick camera interpolation
         * and6/f20 descriptor copy remain naturally scheduled. */
        if(root_live() && YB(s_local,0)==7) finish_direct_bootstrap();
        return;
    }
    /* Once local construction reaches7, keep input/time held until a real
     * adopted running host graph is ready; a natural host may still be in177. */
    finish_direct_bootstrap();
}
static void root_callback(void *task,void *object)
{
    int phase,frame;
    void *swimmer;
    if(!root_live() || task!=s_root.task || object!=s_root.object) return;
    phase=YB(s_local,0);frame=YS(s_local,4);
    if(s_direct_bootstrap) { direct_bootstrap_callback(task,object);return; }
    if(s_debug_intro && phase>=4 && phase<=6) {
        /* Debug auto-entry retains the complete native tutorial, including
         * an already-running adopted host. Its own VM177 owns command0292. */
        func_080014E8_72CD38(task,object);
        if(!root_live()) return;
        if(YB(s_local,0)>=7) s_debug_intro=0;
        reconcile_music();return;
    }
    if(s_terminal && phase==7) {
        YB(s_local,0)=(unsigned char)(s_outcome==1 ? 9:10);YS(s_local,4)=0;
        phase=YB(s_local,0);frame=0;
    }
    if(s_personal_failed || s_terminal || !s_active || (s_owner&&s_ready)) {
        func_080014E8_72CD38(task,object);
        if(!root_live()) return;
        if(phase==7 && YB(s_local,0)==10) {
            swimmer=swimmer_object();
            if(hud_live() && YI(s_hud.task,0x84)>=30 && swimmer && YF(swimmer,0x0c)<-440.f)
                s_personal_failed=1;
            else { s_outcome=2;s_terminal=1; }
        } else if(YB(s_local,0)==9) { s_outcome=1;s_terminal=1; }
        reconcile_music();finish_if_cleaned(phase,frame);return;
    }
    /* Every client constructs native camera/fade/HUD/proxy before adopting.
     * Followers omit instruction VM177 once the host is already beyond it. */
    if(phase>=4 && phase<=6) {
        if(phase==4 && frame==40) {
            if(s_host_phase>=6) {
                s_intro_skipped=1;YB(s_local,0)=6;YS(s_local,4)=0;
            }
        } else func_080014E8_72CD38(task,object);
        reconcile_music();return;
    }
    if(phase!=7 || !s_ready || !(swimmer=swimmer_object())) return;
    /* Input and camera stay native. Hide only the HUD from the controller's
     * shared timeout/outcome tail; its draw callback remains scheduled. */
    void *hud=YP(s_local,0x10);
    YP(s_local,0x10)=0;
    if(frame%40==0) YS(s_local,4)=(short)(frame+1); /* no follower RNG/spawns */
    func_080014E8_72CD38(task,object);
    YP(s_local,0x10)=hud;YS(s_local,4)=(short)frame;YB(s_local,0)=7;
    if(hud_live()) YI(hud,0x90)=(int)(YF(swimmer,0x0c)+440.f);
    if(YF(swimmer,0x0c)>380.f) {
        int y,x,z;
        if(position(YF(swimmer,0x0c),&y) && position(YF(swimmer,8),&x) &&
           position(YF(swimmer,0x10),&z)) queue_input(Y_GOAL,y,x,z,5);
    } else if(YF(swimmer,0x0c)<-440.f) {
        s_personal_failed=1;YB(s_local,0)=10;YS(s_local,4)=0;
    }
}
static void obstacle_callback(void *task,void *object)
{
    int i;
    for(i=0;i<Y_COUNT;++i) if(s_obstacles[i].task==task) break;
    if(i==Y_COUNT || !obstacle_live(&s_obstacles[i]) || object!=s_obstacles[i].object) return;
    YaeNode *n=&s_obstacles[i];
    if(n->staged) { hold_motion(n);return; }
    int advance=!s_active || (s_owner&&s_ready) || s_terminal || s_personal_failed ||
        (s_ready && n->predicted<Y_PREDICT);
    if(advance) restore_motion(n);else hold_motion(n);
    if(!s_active || (s_owner&&s_ready) || s_terminal || s_personal_failed) {
        if(n->pending_hit && YB(task,0xd0)==1) {
            n->pending_hit=0;YB(task,0xd0)=2;YB(task,0xd1)=0;YS(task,0xd4)=0;
        }
        func_08000E1C_72C66C(task,object);return;
    }
    if(!n->contact_reported && YB(task,0xd0)==1 && (YW(task,0x68)&0x2200u))
        n->contact_pending=1;
    if(n->contact_pending && !n->contact_reported) {
        if(queue_input(Y_HIT,(int)n->id,1,0,4)) n->contact_reported=1;
        /* The engine applies each player's native impact independently. The
         * authority commits obstacle lifetime for everyone on the next edge. */
    }
    if(s_ready && n->predicted<Y_PREDICT) {
        /* Keep a speculative native collapse in phase2 until the authority
         * checkpoint arrives. Rewinding it would replay impact13B each frame. */
        func_08000E1C_72C66C(task,object);
        if(!live(n)) return;
        ++n->predicted;
    }
}
static void hud_callback(void *task,void *object)
{
    int timer;
    (void)object;
    if(!hud_live() || task!=s_hud.task) return;
    timer=YI(task,0x84);func_080002B4_72BB04(task);
    if(s_direct_bootstrap || (s_active && (!s_ready || (!s_owner && s_hud.predicted>=Y_PREDICT))))
        YI(task,0x84)=timer;
    else if(s_active && !s_owner) ++s_hud.predicted;
    s_timer=YI(task,0x84);
}
static void bind_active(void *task,void *object,void *local)
{
    anchor_minigame_invites_yae_started();
    s_visit=anchor_minigame_sync_visit();
    s_root.task=task;s_root.object=object;s_root.generation=YB(task,0x74);
    s_root.native=func_080014E8_72CD38;s_root.initialized=1;s_local=local;
    s_host_phase=YB(local,0);s_debug_intro=0;
    schedule(&s_root,root_callback);(void)anchor_minigame_sync_bind(&s_adapter);
}
/* Request diagnostics are bounded and exclusive to explicit debug attempts.
 * They describe observed gates; no individual gate is assumed to have caused
 * the reported runtime failure without values from that installed run. */
static void launch_trace(int reason,int origin)
{
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && !defined(ANCHOR_MINIGAME_YAE_HOST_TEST)
    static int last_reason;
    static unsigned int last_visit,frames;
    static const char *reasons[]={"idle","waiting-controller","controller-identity-or-visit",
        "nonidle-phase","character-not-settled","interaction-not-idle","world-not-ready",
        "mod-dialog-busy","native-scenario-busy","join-no-longer-current","cancelled-manual-sign","starting"};
    if(origin!=ANCHOR_YAE_LAUNCH_DEBUG) { last_reason=0;frames=0;return; }
    unsigned int visit=anchor_minigame_sync_visit();
    if(reason==last_reason && visit==last_visit && ++frames<120u) return;
    last_reason=reason;last_visit=visit;frames=0;
    int phase=-1,cd=-1,task_character=-1;
    if(native_pointer(s_candidate.task) && native_pointer(s_candidate.local) &&
       YP(s_candidate.task,0xd0)==s_candidate.local) phase=YB(s_candidate.local,0);
    if(native_pointer(D_801FC604_5B8514)) {
        cd=YB(D_801FC604_5B8514,0xcd);task_character=YB(D_801FC604_5B8514,0x60);
    }
    recomp_printf("[YaeDebugLaunch] gate=%s visit=%u room=%X phase=%d char=%u taskChar=%d cd=%d prompt=%d dialog=%d vm=%d\n",
        reasons[reason>=0 && reason<12 ? reason:0],visit,(unsigned int)D_800C7AB2,phase,
        D_8015C5DC_15D1DC,task_character,cd,
        anchor_boss_invite_world_can_prompt(),anchor_dialog_busy(),func_8003F1D8_3FDD8());
#else
    (void)reason;(void)origin;
#endif
}
static int candidate_live(void)
{
    return D_800C7AB2==Y_ROOM && s_candidate.visit==anchor_minigame_sync_visit() &&
        native_pointer(s_candidate.task) && native_pointer(s_candidate.object) &&
        native_pointer(s_candidate.local) &&
        YP(s_candidate.task,0x18)==s_candidate.object &&
        YP(s_candidate.task,0xd0)==s_candidate.local &&
        YB(s_candidate.task,0x74)==s_candidate.generation &&
        YH(s_candidate.task,0x5e)==0x327 && !(YW(s_candidate.task,0x68)&2u) &&
        callback_is(YP(s_candidate.task,0x0c),func_080014E8_72CD38) && resource();
}
RECOMP_HOOK("func_080014E8_72CD38")
void anchor_minigame_yae_root(void *task,void *object)
{
    s_entry.task=0;
    if(D_800C7AB2!=Y_ROOM || !native_pointer(task) || !native_pointer(object) ||
       YP(task,0x18)!=object || !native_pointer(YP(task,0xd0))) return;
    s_entry.task=task;s_entry.object=object;s_entry.local=YP(task,0xd0);
    s_entry.generation=YB(task,0x74);s_entry.visit=anchor_minigame_sync_visit();
    s_entry.phase=YB(s_entry.local,0);
    if(s_entry.phase==2 || s_entry.phase==3 || s_entry.phase==12) {
        /* A manual sign interaction now owns native control. Revoke any
         * deferred permission before its refusal can return to idle. */
        int origin=anchor_minigame_invites_yae_request_origin();
        if(origin) launch_trace(10,origin);
        anchor_minigame_invites_yae_cancel_request();s_candidate.task=0;
    }
}
RECOMP_HOOK_RETURN("func_080014E8_72CD38")
void anchor_minigame_yae_root_return(void)
{
    YaeCandidate observed=s_entry;
    void *task=observed.task,*object=observed.object,*local=observed.local;
    s_entry.task=0;
    if(!task || observed.visit!=anchor_minigame_sync_visit() ||
       D_800C7AB2!=Y_ROOM || YP(task,0x18)!=object || YP(task,0xd0)!=local ||
       YB(task,0x74)!=observed.generation || !resource() ||
       YH(task,0x5e)!=0x327 || (YW(task,0x68)&2u)) return;
    if(root_live() && task==s_root.task) return;
    if(!callback_is(YP(task,0x0c),func_080014E8_72CD38)) return;
    int phase=YB(local,0);
    if(phase==1) {
        s_candidate=observed;s_candidate.phase=1;
    } else {
        s_candidate.task=0;
        if(phase==2 || phase==3 || phase==12) {
            int origin=anchor_minigame_invites_yae_request_origin();
            if(origin) launch_trace(10,origin);
            anchor_minigame_invites_yae_cancel_request();
        }
    }
    /* Observe only the native accepted branch. Ordinary phases1..3 and their
     * story/choice checks are never rewritten by this observer. */
    if(observed.phase==3 && phase==4 && D_8015C5DC_15D1DC==3)
        bind_active(task,object,local);
}
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_minigame_yae_frame(void)
{
    if(s_black_owned && s_black_release_pending &&
       (same_world_reveal_scope() || ordinary_handoff_ready()))
        anchor_minigame_yae_direct_load_cancel();
    int origin=anchor_minigame_invites_yae_request_origin();
    if(!origin) {
        launch_trace(0,0);
        /* Cancelled requests retain their source/character until a native
         * sign dialogue has completely released control at idle. */
        if(anchor_minigame_invites_yae_cancel_pending() &&
           native_pointer(D_801FC604_5B8514) && YB(D_801FC604_5B8514,0xcd)==0 &&
           anchor_boss_invite_world_can_prompt() && !anchor_dialog_busy() &&
           !func_8003F1D8_3FDD8()) anchor_minigame_invites_yae_finished();
        return;
    }
    if(!s_candidate.task) { launch_trace(1,origin);return; }
    if(s_candidate.visit!=anchor_minigame_sync_visit()) {
        /* An initializer can run before the resource-load RETURN hooks count
         * the new visit. Discard its old-scope candidate and wait for its next
         * scheduled callback; the request itself names the current visit. */
        launch_trace(2,origin);s_candidate.task=0;return;
    }
    if(!candidate_live()) {
        launch_trace(2,origin);s_candidate.task=0;
        anchor_minigame_invites_yae_cancel_request();return;
    }
    if(YB(s_candidate.local,0)!=1) {
        launch_trace(3,origin);anchor_minigame_invites_yae_cancel_request();
        s_candidate.task=0;return;
    }
#if !defined(DEBUG_BUTTON_ENABLED) || !DEBUG_BUTTON_ENABLED
    if(origin==ANCHOR_YAE_LAUNCH_DEBUG) {
        anchor_minigame_invites_yae_cancel_request();return;
    }
#endif
    if(D_8015C5DC_15D1DC!=3 || !native_pointer(D_801FC604_5B8514) ||
       YB(D_801FC604_5B8514,0x60)!=3) { launch_trace(4,origin);return; }
    if(!native_pointer(D_801FC604_5B8514) || YB(D_801FC604_5B8514,0xcd)!=0) {
        launch_trace(5,origin);return;
    }
    if(!anchor_boss_invite_world_can_prompt()) { launch_trace(6,origin);return; }
    if(anchor_dialog_busy()) { launch_trace(7,origin);return; }
    if(func_8003F1D8_3FDD8()) { launch_trace(8,origin);return; }
    void *task=s_candidate.task,*object=s_candidate.object,*local=s_candidate.local;
    if(origin==ANCHOR_YAE_LAUNCH_JOIN && !anchor_minigame_invites_yae_validate_join()) {
        launch_trace(9,origin);anchor_minigame_invites_yae_cancel_request();return;
    }
    /* Both explicit origins start a fresh attempt; only DEBUG can do so
     * without a pinned valid host. Story15 and obtained1F/F0 stay untouched. */
    func_80024088_24C88(0x1d);func_80024088_24C88(0x1e);YB(local,2)=0;
    launch_trace(11,origin);
    YB(D_801FC604_5B8514,0xcd)=4;func_80221F70_5DD440();
    YW(task,0x68)=(YW(task,0x68)&~0x100u)|0x4400u;
    YS(local,4)=(short)(origin==ANCHOR_YAE_LAUNCH_JOIN ? 1:0);
    YB(local,0)=4;s_candidate.task=0;
    bind_active(task,object,local);
    s_direct_bootstrap=origin==ANCHOR_YAE_LAUNCH_JOIN ? origin:0;
    s_direct_frames=s_direct_host_tick=s_direct_host_generation=0;
    s_direct_wait_visible=s_direct_result_pending=0;
    s_debug_intro=origin==ANCHOR_YAE_LAUNCH_DEBUG;
    s_intro_skipped=origin==ANCHOR_YAE_LAUNCH_JOIN;
}
RECOMP_HOOK("func_08000E1C_72C66C")
void anchor_minigame_yae_obstacle(void *task,void *object)
{
    int index;
    if(!root_live() || !native_pointer(task) || !native_pointer(object) ||
       YH(task,0x28)!=69 || YB(task,0xd0)!=0 || (YW(task,0x68)&2u)) return;
    for(index=0;index<Y_COUNT;++index) if(s_obstacles[index].task==task) return;
    if((index=free_node())<0 || s_next_id>=0x7fffffffu) return;
    YaeNode *node=&s_obstacles[index];anchor_minigame_zero(node,sizeof(*node));
    node->task=task;node->object=object;node->generation=YB(task,0x74);
    node->native=func_08000E1C_72C66C;node->id=++s_next_id;
}
RECOMP_HOOK_RETURN("func_08000E1C_72C66C")
void anchor_minigame_yae_obstacle_return(void)
{
    int i;
    for(i=0;i<Y_COUNT;++i) {
        YaeNode *n=&s_obstacles[i];
        if(live(n) && !n->initialized && YB(n->task,0xd0)==1 &&
           YH(n->task,0x5e)==0x33a && YH(n->task,0x28)==69) {
            n->initialized=1;schedule(n,obstacle_callback);
            if(n->staged) hold_motion(n);
        }
    }
}
static void swimmer_callback(void *task,void *object)
{
    if(!root_live() || !live(&s_swimmer) || task!=s_swimmer.task || object!=s_swimmer.object)
        return;
    if(s_direct_bootstrap) return;
    func_0800107C_72C8CC(task,object);
}
RECOMP_HOOK("func_0800107C_72C8CC")
void anchor_minigame_yae_swimmer(void *task,void *object)
{
    s_swimmer_init_task=0;
    if(!s_direct_bootstrap || !root_live() || task!=YP(s_local,0x18) ||
       !native_pointer(task) || !native_pointer(object) || YP(task,0x18)!=object ||
       YH(task,0x28)!=69 || YB(task,0xd0)!=0 || (YW(task,0x68)&2u)) return;
    s_swimmer.task=task;s_swimmer.object=object;s_swimmer.generation=YB(task,0x74);
    s_swimmer.native=func_0800107C_72C8CC;s_swimmer_init_task=task;
}
RECOMP_HOOK_RETURN("func_0800107C_72C8CC")
void anchor_minigame_yae_swimmer_return(void)
{
    if(!s_swimmer_init_task || s_swimmer_init_task!=s_swimmer.task ||
       !root_live() || !live(&s_swimmer) || YP(s_local,0x18)!=s_swimmer.task ||
       YB(s_swimmer.task,0xd0)!=1 || YH(s_swimmer.task,0x5e)!=0x33a) {
        s_swimmer_init_task=0;return;
    }
    s_swimmer_init_task=0;s_swimmer.initialized=1;
    schedule(&s_swimmer,swimmer_callback);
}
RECOMP_HOOK("func_080002B4_72BB04")
void anchor_minigame_yae_hud(void *task)
{
    s_hud_draw_task=0;
    if(root_live() && task==YP(s_local,0x10) && native_pointer(task)) {
        s_hud_draw_task=task;s_hud_draw_timer=YI(task,0x84);
    }
    if(!root_live() || !task || task!=YP(s_local,0x10) || s_hud.task) return;
    s_hud.task=task;s_hud_text=YP(task,0x78);s_hud_sprite=YP(task,0x7c);
    s_hud_marker=YP(task,0x80);
    YP(task,0x0c)=(void *)((unsigned long)hud_callback |
        ((unsigned long)YP(task,0x0c)&YAE_DISABLED));
}
RECOMP_HOOK_RETURN("func_080002B4_72BB04")
void anchor_minigame_yae_hud_return(void)
{
    if(s_hud_draw_task && s_direct_bootstrap && hud_live() && s_hud_draw_task==s_hud.task)
        YI(s_hud.task,0x84)=s_hud_draw_timer;
    if(s_hud_draw_task && s_hud_draw_task==s_hud.task) update_hud_markers();
    s_hud_draw_task=0;
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_minigame_yae_reuse(void *task)
{
    int i;
    if(task==s_candidate.task) {
        s_candidate.task=0;anchor_minigame_invites_yae_cancel_request();
    }
    if(task==s_swimmer.task) anchor_minigame_zero(&s_swimmer,sizeof(s_swimmer));
    if(task==s_root.task) { reset();return; }
    if(task==s_hud.task) {
        anchor_minigame_zero(s_hud_remotes,sizeof(s_hud_remotes));
        anchor_minigame_zero(&s_hud,sizeof(s_hud));s_hud_text=s_hud_sprite=s_hud_marker=0;
    }
    for(i=0;i<Y_COUNT;++i) if(task==s_obstacles[i].task) {
        restore_motion(&s_obstacles[i]);
        anchor_minigame_zero(&s_obstacles[i],sizeof(s_obstacles[i]));
    }
}
RECOMP_HOOK("func_80034EF8_35AF8")
void anchor_minigame_yae_delete(void *task)
{
    int i;
    if(task==s_candidate.task) {
        s_candidate.task=0;anchor_minigame_invites_yae_cancel_request();
    }
    if(task==s_swimmer.task) anchor_minigame_zero(&s_swimmer,sizeof(s_swimmer));
    if(task==s_root.task) {
        if(s_direct_bootstrap) anchor_minigame_yae_direct_load_cancel();
        s_direct_bootstrap=s_debug_intro=0;s_root.task=0;
        s_direct_wait_visible=s_direct_result_pending=0;
        anchor_minigame_zero(s_hud_remotes,sizeof(s_hud_remotes));
    }
    if(task==s_hud.task) {
        anchor_minigame_zero(s_hud_remotes,sizeof(s_hud_remotes));
        anchor_minigame_zero(&s_hud,sizeof(s_hud));s_hud_text=s_hud_sprite=s_hud_marker=0;
    }
    for(i=0;i<Y_COUNT;++i) if(task==s_obstacles[i].task) s_obstacles[i].task=0;
}
