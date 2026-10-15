/* File48 Sudden Impact training. Native controller/target layouts and callback
 * ABIs were audited at ROM707A40..709E5C. Player carrying/throwing stays local;
 * the shared timer and six target state machines have one authority. */
#define ANCHOR_GOEMON_ACTION_V2 1
#ifndef ANCHOR_MINIGAME_GOEMON_HOST_TEST
#include "platform/modding.h"
#include "core/anchor.h"
#endif
#include "world/anchor_minigame_sync.h"
#include "world/anchor_minigame_invites.h"

#define GB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define GS(p,o) (*(short *)((char *)(p)+(o)))
#define GH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define GW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define GI(p,o) (*(int *)((char *)(p)+(o)))
#define GF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef GP
#define GP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef GOEMON_DISABLED
#define GOEMON_DISABLED 0x00800000ul
#endif
typedef void (*GoemonCallback)(void *,void *);
extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void *func_800141C4_14DC4(unsigned int);
extern int func_8003F1D8_3FDD8(void);
extern void func_80038BC8_397C8(unsigned short);
extern float func_80003E10_4A10(unsigned int);
extern void func_080018E8_709328(void *,void *);
extern void func_0800127C_708CBC(void *,void *);
extern void func_08000CC8_708708(void *,void *);
extern void func_080005D0_708010(void *,void *);
extern void func_0800126C_708CAC(void *);
extern void *func_08000000_707A40(void *,GoemonCallback,unsigned char,unsigned short);
extern void func_80216E1C_5D22EC(void *,unsigned int);
extern unsigned int anchor_get_client_id(void);
extern int anchor_get_projectile_session(void);

enum { GT_ROOM=0x1e0, GT_GAME=1, GT_ENTITY=0x1bc, GT_COUNT=6,
       GT_GAME_MUSIC=0x0052,
       GT_TARGET=1, GT_VISUAL=2, GT_TARGET_WORDS=31, GT_VISUAL_WORDS=20,
       GT_CONTROLLER_WORDS=5, GT_VISUAL_PEERS=6, GT_VISUAL_COUNT=18,
       GT_INPUT_HIT=1, GT_INPUT_POSE=2, GT_VISUAL_TTL=90 };
typedef struct {
    void *task,*object;
    unsigned char generation, initialized;
    GoemonCallback native;
} GoemonNode;
typedef struct {
    int row[GT_VISUAL_WORDS];
    unsigned int age;
    GoemonNode clone;
    int net[GT_VISUAL_WORDS];
    unsigned int checkpoint_generation,full_tick,motion_age;
    unsigned char have_net,hidden,correction_ticks;
    float xyz[3],velocity[3],correction[3];
} GoemonVisual;
typedef struct {
    int net[GT_TARGET_WORDS];
    unsigned int generation,full_tick,age;
    unsigned char have_net,correction_ticks;
    float correction[4];
} GoemonMotion;
static GoemonNode s_root,s_targets[GT_COUNT],s_timer;
static GoemonNode s_children[GT_COUNT];
static unsigned char s_child_phase[GT_COUNT];
static void *s_work,*s_init_task,*s_child_in_call;
static GoemonVisual s_visuals[GT_VISUAL_COUNT];
static void *s_cubes[3];
static unsigned char s_cube_task_generation[3];
static unsigned int s_cube_generation[3],s_cube_pose_sequence[3];
static int s_last_target[GT_COUNT][GT_TARGET_WORDS];
static GoemonMotion s_motion[GT_COUNT];
static unsigned int s_action[GT_COUNT];
static unsigned int s_pending_hit[GT_COUNT][2];
static unsigned char s_cube_active[3],s_cube_held[3];
static unsigned int s_cube_emitted[3];
static int s_hits[GT_COUNT][5],s_poses[3][8];
static unsigned int s_sequence,s_visit,s_frames;
static int s_active,s_owner,s_ready,s_host_phase,s_outcome,s_terminal;
static int s_intro_skipped,s_game_music_started;
static int s_timer_value=-1,s_local_cid,s_local_session;
static void root_callback(void *,void *);
static void target_callback(void *,void *);
static void timer_callback(void *,void *);
static void visual_callback(void *,void *);
static void visual_prepare(void);
static void visual_adopt(GoemonVisual *,const int *,unsigned int,unsigned int);
static const AnchorMinigameAdapter s_adapter;

static int callback_is(void *p,GoemonCallback cb)
{ return ((unsigned long)p&~GOEMON_DISABLED)==((unsigned long)cb&~GOEMON_DISABLED); }
static void schedule(GoemonNode *n,GoemonCallback cb)
{
    GP(n->task,0x0c)=(void *)((unsigned long)cb|
        ((unsigned long)GP(n->task,0x0c)&GOEMON_DISABLED));
}
static int pointer_valid(void *p)
{
#ifdef ANCHOR_MINIGAME_GOEMON_HOST_TEST
    return p!=0;
#else
    unsigned int address=(unsigned int)p;
    return address>=0x80000000u && address<0x80800000u && !(address&3u);
#endif
}
static int resource(void)
{
    void *p=func_800141C4_14DC4(48);
    return p && p!=(void *)(unsigned long)0xffffffffu;
}
static int live(const GoemonNode *n)
{
    return D_800C7AB2==GT_ROOM && s_visit==anchor_minigame_sync_visit() &&
        n->task && n->object && GP(n->task,0x18)==n->object &&
        GB(n->task,0x74)==n->generation && !(GW(n->task,0x68)&2u);
}
static int visual_live(const GoemonVisual *v)
{
    return live(&v->clone) && GH(v->clone.task,0x28)==48 &&
        callback_is(GP(v->clone.task,0xc),visual_callback) &&
        (!v->clone.initialized || GH(v->clone.task,0x5c)==GT_ENTITY);
}
static int root_live(void)
{
    return live(&s_root) && resource() && GH(s_root.task,0x5c)==GT_ENTITY &&
           pointer_valid(s_work) && GP(s_root.task,0xd0)==s_work;
}
static int target_live(int i)
{
    GoemonNode *n=&s_targets[i];
    return root_live() && live(n) && n->initialized &&
        GP(s_work,0x18+i*4)==n->task && GH(n->task,0x28)==48 &&
        callback_is(GP(n->task,0x0c),target_callback);
}
static int child_live(int i)
{
    return target_live(i) && live(&s_children[i]) &&
        GP(s_targets[i].task,0xe8)==s_children[i].task &&
        GH(s_children[i].task,0x28)==48;
}
static int timer_live(void)
{
    if(!root_live() || !s_timer.task || GP(s_work,0x14)!=s_timer.task ||
       !callback_is(GP(s_timer.task,0x0c),timer_callback)) return 0;
    return 1;
}
static int position(float f,int *out)
{
    union { float f;unsigned int u; } n;
    n.f=f;
    if((n.u&0x7f800000u)==0x7f800000u || f < -32768.f || f > 32767.f) return 0;
    *out=(int)(f*1024.f);return 1;
}
static int valid_position(int n) { return n>=-33554432 && n<=33553408; }
static int velocity(float f,int *out)
{
    if(!position(f,out)) return 0;
    return *out>=-131072 && *out<=131072;
}
static unsigned int local_ack(void) { return anchor_minigame_sync_local_ack(GT_GAME); }
static int phase(void) { return root_live() ? GB(s_work,0):0; }
static int shared_phase(void) { int p=phase();return p==2 ? 1:p; }
static int graph(void)
{
    int i,p;
    if(!root_live() || !s_root.initialized) return 0;
    p=phase();
    if(p==3 || p==4) return 1;
    if(p<1 || p>7) return 0;
    if(p<=2 && !timer_live()) return 0;
    for(i=0;i<GT_COUNT;++i)
        if((!target_live(i) && (!s_terminal || !s_last_target[i][2])) ||
           (!s_terminal && target_live(i) && GB(s_targets[i].task,0xd1)!=1 &&
            (!child_live(i) || !s_children[i].initialized))) return 0;
    return 1;
}
static void restore(GoemonNode *n,GoemonCallback wrapper)
{ if(live(n) && callback_is(GP(n->task,0x0c),wrapper)) schedule(n,n->native); }
static void reset(void)
{
    int i;
    restore(&s_root,root_callback);
    for(i=0;i<GT_COUNT;++i) restore(&s_targets[i],target_callback);
    if(timer_live()) schedule(&s_timer,s_timer.native);
    for(i=0;i<GT_VISUAL_COUNT;++i)
        if(visual_live(&s_visuals[i])) GW(s_visuals[i].clone.task,0x68)|=2u;
    anchor_minigame_zero(&s_root, sizeof(s_root));
    anchor_minigame_zero(s_targets, sizeof(s_targets));
    anchor_minigame_zero(s_children, sizeof(s_children));
    anchor_minigame_zero(s_child_phase, sizeof(s_child_phase));
    anchor_minigame_zero(&s_timer, sizeof(s_timer));
    anchor_minigame_zero(s_visuals, sizeof(s_visuals));
    anchor_minigame_zero(s_cubes, sizeof(s_cubes));
    anchor_minigame_zero(s_cube_task_generation, sizeof(s_cube_task_generation));
    anchor_minigame_zero(s_cube_generation, sizeof(s_cube_generation));
    anchor_minigame_zero(s_cube_pose_sequence, sizeof(s_cube_pose_sequence));
    anchor_minigame_zero(s_hits, sizeof(s_hits));anchor_minigame_zero(s_poses, sizeof(s_poses));
    anchor_minigame_zero(s_last_target, sizeof(s_last_target));
    anchor_minigame_zero(s_motion,sizeof(s_motion));anchor_minigame_zero(s_action,sizeof(s_action));
    anchor_minigame_zero(s_pending_hit,sizeof(s_pending_hit));
    anchor_minigame_zero(s_cube_active,sizeof(s_cube_active));anchor_minigame_zero(s_cube_held,sizeof(s_cube_held));
    anchor_minigame_zero(s_cube_emitted,sizeof(s_cube_emitted));
    s_work=s_init_task=s_child_in_call=0;
    s_active=s_owner=s_ready=s_host_phase=s_outcome=s_terminal=0;
    s_intro_skipped=s_game_music_started=0;
    s_local_cid=s_local_session=0;s_timer_value=-1;s_sequence=s_visit=s_frames=0;
}
static void role(int active,int owner,int ready)
{
    s_active=active;s_owner=owner;s_ready=ready;
    if(active && !owner) anchor_minigame_zero(s_pending_hit,sizeof(s_pending_hit));
    if(active && (!s_local_cid || !s_local_session)) {
        s_local_cid=(int)anchor_get_client_id();s_local_session=anchor_get_projectile_session();
    }
    if(!active) s_local_cid=s_local_session=0;
}
static unsigned int next_sequence(void)
{ s_sequence=s_sequence>=0x7fffffffu ? 1u:s_sequence+1u;return s_sequence; }
static int visual_slot(int cid,int session,int visit,int slot,int create)
{
    int i,empty=-1,peers=0,last_cid[GT_VISUAL_PEERS],last_session[GT_VISUAL_PEERS],j;
    for(i=0;i<GT_VISUAL_COUNT;++i) {
        int *r=s_visuals[i].row;
        if(r[3]==cid && r[4]==session && r[15]==visit && r[5]==slot) return i;
        if(!r[3]) { if(empty<0) empty=i;continue; }
        for(j=0;j<peers;++j) if(last_cid[j]==r[3] && last_session[j]==r[4]) break;
        if(j==peers && peers<GT_VISUAL_PEERS) {
            last_cid[peers]=r[3];last_session[peers++]=r[4];
        }
    }
    if(!create || empty<0) return -1;
    for(j=0;j<peers;++j) if(last_cid[j]==cid && last_session[j]==session) break;
    if(j==peers && peers==GT_VISUAL_PEERS) return -1;
    return empty;
}
static void visual_retire(GoemonVisual *v)
{
    if(visual_live(v)) GW(v->clone.task,0x68)|=2u;
    anchor_minigame_zero(&v->clone, sizeof(v->clone));
}
static void visuals_stop(void)
{
    int i;
    for(i=0;i<GT_VISUAL_COUNT;++i) if(s_visuals[i].row[3]) {
        GoemonVisual *v=&s_visuals[i];
        v->row[6]=v->row[7]=v->row[19]=0;
        v->row[16]=v->row[17]=v->row[18]=0;visual_retire(v);
    }
}
static void visual_prepare(void)
{
    int i;
    if(s_terminal) { visuals_stop();return; }
    for(i=0;i<GT_VISUAL_COUNT;++i) {
        GoemonVisual *v=&s_visuals[i];int *r=v->row;
        if(!r[3]) continue;
        if(!r[6] || (r[3]==s_local_cid && r[4]==s_local_session)) { visual_retire(v);continue; }
        if(v->clone.task && !visual_live(v)) anchor_minigame_zero(&v->clone, sizeof(v->clone));
        if(!v->clone.task && root_live()) {
            void *task=func_08000000_707A40(s_root.task,visual_callback,0,48);
            if(!task) continue; /* Optional visuals never block core progress. */
            v->clone.task=task;v->clone.object=GP(task,0x18);
            v->clone.generation=GB(task,0x74);v->clone.native=visual_callback;
            /* Remove inherited collision before its first scheduler tick. */
            GW(task,0x48)=0;GB(task,0x4c)=0;
            GS(task,0x4e)=GS(task,0x50)=GS(task,0x52)=0;
            GW(task,0x60)=0x20u;GW(task,0x68)&=~0x8000u;
        }
    }
}
static void visuals_update(void)
{
    int i;
    for(i=0;i<GT_VISUAL_COUNT;++i) if(s_visuals[i].row[3] && ++s_visuals[i].age>GT_VISUAL_TTL) {
        visual_retire(&s_visuals[i]);anchor_minigame_zero(&s_visuals[i],sizeof(s_visuals[i]));
    }
    visual_prepare();
}
static void visual_adopt(GoemonVisual *v,const int *r,unsigned int generation,unsigned int full_tick)
{
    int i,same=v->have_net,hard=!v->have_net || generation!=v->checkpoint_generation;
    for(i=0;i<GT_VISUAL_WORDS && same;++i) if(v->net[i]!=r[i]) same=0;
    if(same && !hard) {
        if(full_tick!=v->full_tick) { v->age=v->motion_age=0;v->full_tick=full_tick; }
        return;
    }
    if(v->have_net && (v->net[2]!=r[2] || v->net[3]!=r[3] || v->net[4]!=r[4] || v->net[15]!=r[15] ||
                      v->net[6]!=r[6] || v->net[7]!=r[7] || v->net[19]!=r[19])) hard=1;
    for(i=0;i<3;++i) {
        float desired=(float)r[8+i]/1024.f,delta=desired-v->xyz[i];
        if(delta>80.f || delta< -80.f) hard=1;
        v->correction[i]=delta;
        v->velocity[i]=(float)r[16+i]/1024.f;
    }
    if(hard) for(i=0;i<3;++i) v->xyz[i]=(float)r[8+i]/1024.f;
    v->correction_ticks=hard?0:8;
    __builtin_memcpy(v->net,r,sizeof(v->net));__builtin_memcpy(v->row,r,sizeof(v->row));
    v->age=v->motion_age=0;v->have_net=1;v->hidden=0;
    v->checkpoint_generation=generation;v->full_tick=full_tick;
}
static void visual_capture(GoemonVisual *v)
{
    int i,*r=v->row;
    if(!r[6]) return;
    if(r[3]==s_local_cid && r[4]==s_local_session) {
        int slot=r[5];void *t=s_cubes[slot],*o;
        if(!t || !pointer_valid(t) || GB(t,0x74)!=s_cube_task_generation[slot] ||
           s_cube_generation[slot]!=(unsigned int)r[2] || !GB(t,0xd1) || !pointer_valid(o=GP(t,0x18))) return;
        for(i=0;i<3;++i) { position(GF(o,8+4*i),&r[8+i]);if(!r[7]) velocity(GF(t,0x78+4*i),&r[16+i]); }
    } else if(v->have_net && r[6]) for(i=0;i<3;++i) {
        position(v->xyz[i],&r[8+i]);velocity(v->velocity[i],&r[16+i]);
    }
}
static int read_target(int i,int out[GT_TARGET_WORDS])
{
    GoemonNode *n=&s_targets[i];void *t=n->task,*o=n->object;
    if(!target_live(i)) return 0;
    out[0]=GT_TARGET;out[1]=i;out[2]=1;out[3]=1;
    out[4]=GB(t,0xd0);out[5]=(signed char)GB(t,0xd1);out[6]=GS(t,0xd6);
    out[7]=GS(t,0xd8);out[8]=GS(t,0xda);
    if(!position(GF(t,0xdc),&out[9]) || !position(GF(t,0xe4),&out[10]) ||
       !position(GF(o,8),&out[11]) || !position(GF(o,0x0c),&out[12]) ||
       !position(GF(o,0x10),&out[13]) || !position(GF(o,0x1c),&out[15]) ||
       !position(GF(o,0x20),&out[16]) || !position(GF(o,0x24),&out[17])) return 0;
    if(!position(GF(t,0xe0),&out[29])) return 0;
    out[30]=(int)s_action[i];
    out[14]=GH(o,0x16);out[18]=s_child_phase[i];out[19]=child_live(i);
    if(out[19]) {
        void *c=s_children[i].object;
        if(!position(GF(c,8),&out[20]) || !position(GF(c,0x0c),&out[21]) ||
           !position(GF(c,0x10),&out[22]) || !position(GF(c,0x1c),&out[24]) ||
           !position(GF(c,0x20),&out[25]) || !position(GF(c,0x24),&out[26])) return 0;
        out[23]=GH(c,0x16);out[27]=GH(c,0x14);out[28]=GH(c,0x18);
    } else anchor_minigame_zero(out+20, sizeof(int)*9);
    return 1;
}
static int validate(const AnchorMinigameState *s)
{
    unsigned int i,j;int p,core,peers=0;
    if(!s || !s->ready || s->controller_count!=GT_CONTROLLER_WORDS || s->controller[0]!=2)
        return 0;
    p=s->controller[1];core=p==3 || p==4 ? 0:GT_COUNT;
    if(p<1 || p==2 || p>7 || s->controller[2]<0 || s->controller[2]>32767 ||
       s->controller[3]<-1 || s->controller[3]>3000 || s->controller[4]<0 ||
       s->controller[4]>2 || s->result!=s->controller[4] || s->running!=(p==1) ||
       (p<5 && s->result) || (p==5 && s->result!=2) || (p==6 && s->result!=1) ||
       (p==7 && !s->result) || (p==1 && s->controller[3]<0) ||
       s->actor_count<(unsigned int)core || s->actor_count>(unsigned int)(core+GT_VISUAL_COUNT)) return 0;
    for(i=0;i<s->actor_count;++i) {
        const int *r=s->actors[i];
        if(i<(unsigned int)core) {
            if(s->actor_words[i]!=GT_TARGET_WORDS || r[0]!=GT_TARGET || r[1]!=(int)i ||
               r[2]!=1 || r[3]<0 || r[3]>1 || (p==1 && !r[3]) || r[4]<0 || r[4]>7 ||
               (r[5]!=0 && r[5]!=1 && (p<5 || (r[5]!=3 && r[5]!=4))) || r[6]<-32768 || r[6]>32767 ||
               r[7]<-32768 || r[7]>32767 || r[8]<-32768 || r[8]>32767 ||
               r[14]<0 || r[14]>65535 || r[18]<0 || r[18]>2 ||
               r[19]<0 || r[19]>1 || r[30]<1 || !valid_position(r[29]) || (p==1 && r[5]!=1 && !r[19])) return 0;
            for(j=9;j<18;++j) if(j!=14 && !valid_position(r[j])) return 0;
            for(j=20;j<27;++j) if(j!=23 && !valid_position(r[j])) return 0;
            if(r[23]<0 || r[23]>65535 || r[27]<0 || r[27]>65535 || r[28]<0 || r[28]>65535)
                return 0;
        } else {
            int known_peer=0;
            if(s->actor_words[i]!=GT_VISUAL_WORDS || r[0]!=GT_VISUAL ||
               r[1]<0 || r[1]>=GT_VISUAL_COUNT || r[2]<1 || r[2]>32767 ||
               r[3]<=0 || r[4]<=0 || r[5]<0 || r[5]>2 || r[6]<0 || r[6]>1 ||
               r[7]<0 || r[7]>1 || r[14]<=0 || r[15]<=0 || r[19]<0 ||
               (r[6] && !r[7] && !r[19]) || (r[7] && (!r[6] || r[19]))) return 0;
            for(j=8;j<11;++j) if(!valid_position(r[j])) return 0;
            for(j=11;j<14;++j) if(r[j]<0 || r[j]>65535) return 0;
            for(j=16;j<19;++j) if(r[j]<-131072 || r[j]>131072) return 0;
            for(j=(unsigned int)core;j<i;++j)
                if(s->actors[j][1]==r[1] || (s->actors[j][3]==r[3] &&
                   s->actors[j][4]==r[4] && s->actors[j][5]==r[5])) return 0;
                else if(s->actors[j][3]==r[3] &&
                        (s->actors[j][4]!=r[4] || s->actors[j][15]!=r[15])) return 0;
                else if(s->actors[j][3]==r[3] && s->actors[j][4]==r[4]) known_peer=1;
            if(!known_peer && ++peers>GT_VISUAL_PEERS) return 0;
        }
    }
    return 1;
}
static int capture(AnchorMinigameState *s)
{
    int i,p;
    if(!graph()) return 0;
    p=shared_phase();if(timer_live()) s_timer_value=GI(s_timer.task,0x84);
    s->ready=1;s->running=p==1;s->result=s_outcome;
    s->controller_count=GT_CONTROLLER_WORDS;s->controller[0]=2;s->controller[1]=p;
    s->controller[2]=p==1 ? 0:GS(s_work,4);s->controller[3]=s_timer_value;
    s->controller[4]=s_outcome;
    if(p!=3 && p!=4) for(i=0;i<GT_COUNT;++i) {
        int *r=s->actors[s->actor_count];
        if(read_target(i,r)) __builtin_memcpy(s_last_target[i],r,sizeof(s_last_target[i]));
        else { __builtin_memcpy(r,s_last_target[i],sizeof(s_last_target[i]));r[3]=0; }
        s->actor_words[s->actor_count++]=GT_TARGET_WORDS;
    }
    if(s_terminal) visuals_stop();
    for(i=0;i<GT_VISUAL_COUNT;++i) if(s_visuals[i].row[3]) {
        visual_capture(&s_visuals[i]);
        __builtin_memcpy(s->actors[s->actor_count],s_visuals[i].row,sizeof(s_visuals[i].row));
        s->actor_words[s->actor_count++]=GT_VISUAL_WORDS;
    }
    for(i=0;i<GT_COUNT;++i) if(s_hits[i][0]) {
        if((unsigned int)s_hits[i][0]<=local_ack()) { s_hits[i][0]=0;continue; }
        __builtin_memcpy(s->inputs[s->input_count],s_hits[i],sizeof(s_hits[i]));
        s->input_words[s->input_count++]=5;
    }
    for(i=0;i<3;++i) if(s_poses[i][0]) {
        __builtin_memcpy(s->inputs[s->input_count],s_poses[i],sizeof(s_poses[i]));
        s->input_words[s->input_count++]=8;
    }
    return validate(s);
}
static int apply(const AnchorMinigameState *s)
{
    unsigned int i;int p,local;
    if(!validate(s) || !root_live()) return 0;
    p=s->controller[1];s_host_phase=p;
    /* Remember only the desired setup checkpoint until local constructors
     * have made the entire graph. No partial target scalars are applied. */
    if(!graph() || ((p==1 || p>=5) && phase()!=1 && phase()!=2 && phase()<5)) return 0;
    if(p==1 || p>=5) {
        if(p==1 && !timer_live()) return 0;
        for(i=0;i<GT_COUNT;++i) {
            if(!target_live((int)i)) { if(!s_terminal) return 0;continue; }
            if(s_terminal && s_child_phase[i]==2 && !child_live((int)i)) continue;
            if(s->actors[i][19] && (!child_live((int)i) || !s_children[i].initialized)) return 0;
            if(child_live((int)i) && s->actors[i][18]==2 &&
               (!s_children[i].initialized || s_child_phase[i]==0)) return 0;
        }
    }
    local=phase();
    if(p==3 || p==4) {
        /* Local dialogue may still be active; native VM remains local. */
        if(local==3 && p==4 && !func_8003F1D8_3FDD8()) {
            s_intro_skipped=1;GB(s_work,0)=4;
        }
    } else {
        for(i=0;i<GT_COUNT;++i) {
            const int *r=s->actors[i];GoemonNode *n=&s_targets[i];
            GoemonMotion *m=&s_motion[i];
            float before[4];
            int same=m->have_net,hard=!m->have_net || m->generation!=s->adopted_generation,j;
            if(!target_live((int)i)) continue;
            before[0]=GF(n->object,0xc);before[1]=GF(n->object,0x1c);
            before[2]=GF(n->object,0x20);before[3]=GF(n->object,0x24);
            s_action[i]=(unsigned int)r[30];
            if(s_hits[i][0] && ((unsigned int)s_hits[i][0]<=s->local_ack || r[30]!=s_hits[i][3] || r[4]!=s_hits[i][4])) s_hits[i][0]=0;
            for(j=0;j<GT_TARGET_WORDS && same;++j) if(m->net[j]!=r[j]) same=0;
            if(same && !hard) {
                if(m->full_tick!=s->full_tick) { m->full_tick=s->full_tick;m->age=0; }
                continue;
            }
            if(m->have_net && (m->net[4]!=r[4] || m->net[30]!=r[30] ||
               m->net[5]!=r[5] || m->net[18]!=r[18] || m->net[19]!=r[19])) hard=1;
            for(j=0;j<3;++j) {
                float delta=GF(n->object,8+4*j)-(float)r[11+j]/1024.f;
                if(delta>80.f || delta< -80.f) hard=1;
            }
            GB(n->task,0xd0)=(unsigned char)r[4];GB(n->task,0xd1)=(unsigned char)r[5];
            GS(n->task,0xd6)=(short)r[6];GS(n->task,0xd8)=(short)r[7];GS(n->task,0xda)=(short)r[8];
            GF(n->task,0xdc)=(float)r[9]/1024.f;GF(n->task,0xe0)=(float)r[29]/1024.f;GF(n->task,0xe4)=(float)r[10]/1024.f;
            GF(n->object,8)=(float)r[11]/1024.f;GF(n->object,0x0c)=(float)r[12]/1024.f;
            GF(n->object,0x10)=(float)r[13]/1024.f;GH(n->object,0x16)=(unsigned short)r[14];
            GF(n->object,0x1c)=(float)r[15]/1024.f;GF(n->object,0x20)=(float)r[16]/1024.f;
            GF(n->object,0x24)=(float)r[17]/1024.f;
            /* Visual child's phase1 is inert; its native phase2 owns effects. */
            if(child_live((int)i)) {
                void *c=s_children[i].object;
                if((r[18]==2 || r[5]==4) && s_child_phase[i]==1) {
                    func_0800126C_708CAC(s_children[i].task);s_child_phase[i]=2;
                }
                if(r[19]) {
                    GF(c,8)=(float)r[20]/1024.f;GF(c,0x0c)=(float)r[21]/1024.f;
                    GF(c,0x10)=(float)r[22]/1024.f;GH(c,0x16)=(unsigned short)r[23];
                    GF(c,0x1c)=(float)r[24]/1024.f;GF(c,0x20)=(float)r[25]/1024.f;
                    GF(c,0x24)=(float)r[26]/1024.f;GH(c,0x14)=(unsigned short)r[27];
                    GH(c,0x18)=(unsigned short)r[28];
                }
            }
            for(j=0;j<4;++j) {
                unsigned int off=j?0x18+4*j:0xc;
                m->correction[j]=before[j]-GF(n->object,off);
                if(!hard) GF(n->object,off)=before[j];
            }
            m->correction_ticks=hard?0:8;m->age=0;m->have_net=1;
            m->generation=s->adopted_generation;m->full_tick=s->full_tick;
            __builtin_memcpy(m->net,r,sizeof(m->net));
            __builtin_memcpy(s_last_target[i],r,sizeof(s_last_target[i]));
        }
        if(timer_live() && !s_terminal) GI(s_timer.task,0x84)=s->controller[3];
        s_timer_value=s->controller[3];
        if(p>=5 && !s_terminal) {
            s_outcome=s->result;s_terminal=1;GB(s_work,0)=(unsigned char)p;GS(s_work,4)=0;
        }
    }
    /* Optional visual state is independently bounded and never a readiness
     * condition for the physical targets, timer or local player. */
    for(i=(p==3 || p==4 ? 0u:GT_COUNT);i<s->actor_count;++i) {
        const int *r=s->actors[i];GoemonVisual *v=&s_visuals[r[1]];
        if(v->row[3]!=r[3] || v->row[4]!=r[4] || v->row[15]!=r[15] ||
           v->row[2]!=r[2]) visual_retire(v);
        visual_adopt(v,r,s->adopted_generation,s->full_tick);
    }
    visual_prepare();return 1;
}
static int input(const int *w,unsigned int count)
{
    int slot,generation,i,*r;
    if(!s_active || !s_owner || !s_ready || !graph()) return 0;
    if(count==8 && w[4]==GT_INPUT_HIT) {
        slot=w[5];generation=w[6];
        if(shared_phase()!=1 || slot<0 || slot>=GT_COUNT || generation<1 || (unsigned int)generation!=s_action[slot] || !target_live(slot)) return 1;
        if((w[7]==1 || w[7]==3 || w[7]==4) && GB(s_targets[slot].task,0xd0)==w[7]) {
            s_pending_hit[slot][0]=(unsigned int)generation;
            s_pending_hit[slot][1]=(unsigned int)w[7];
        }
        return 1;
    }
    if(count!=11 || (w[4]&3)!=GT_INPUT_POSE || w[4]<0 || s_terminal) return 1;
    slot=(w[4]>>2)&3;generation=w[4]>>6;
    if(slot>2 || generation<1 || generation>32767) return 1;
    for(i=5;i<8;++i) if(!valid_position(w[i])) return 1;
    for(i=8;i<11;++i) if((w[4]&32) ? (w[i]<0 || w[i]>65535):(w[i]<-131072 || w[i]>131072)) return 1;
    /* A new visit replaces this participant's old optional clones. */
    for(i=0;i<GT_VISUAL_COUNT;++i) if(s_visuals[i].row[3]==w[0] &&
       (s_visuals[i].row[4]!=w[1] || s_visuals[i].row[15]!=w[2])) {
        visual_retire(&s_visuals[i]);anchor_minigame_zero(s_visuals[i].row, sizeof(s_visuals[i].row));
    }
    i=visual_slot(w[0],w[1],w[2],slot,1);
    if(i<0) return 1; /* More than six visual peers is a graceful omission. */
    {
        int next[GT_VISUAL_WORDS];GoemonVisual *v=&s_visuals[i];
        r=v->row;
        if(r[3] && (generation<r[2] || (generation==r[2] && w[3]<=r[14]))) return 1;
        __builtin_memcpy(next,r,sizeof(next));
        next[0]=GT_VISUAL;next[1]=i;next[2]=generation;next[3]=w[0];next[4]=w[1];next[5]=slot;
        next[6]=(w[4]>>4)&1;next[7]=(w[4]>>5)&1;
        __builtin_memcpy(next+8,w+5,sizeof(int)*3);
        if(next[7]) { __builtin_memcpy(next+11,w+8,sizeof(int)*3);next[19]=0;
            next[16]=next[17]=next[18]=0; }
        else if(next[6]) {
            __builtin_memcpy(next+16,w+8,sizeof(int)*3);
            if(!r[3] || r[2]!=generation || !r[6] || r[7] || !r[19]) next[19]=w[3];
        } else { next[16]=next[17]=next[18]=0;next[19]=0; }
        next[14]=w[3];next[15]=w[2];visual_adopt(v,next,v->checkpoint_generation,v->full_tick);
    }
    return 1;
}
static int return_pending(void)
{
    return root_live() && s_outcome && phase()==7 && GS(s_work,4)==20 &&
           D_8016DAB4_16E6B4==s_root.task;
}
static void result(int outcome)
{
    if(outcome<1 || outcome>2 || s_terminal || !root_live()) return;
    s_outcome=outcome;s_terminal=1;
    GB(s_work,0)=(unsigned char)(outcome==1 ? 6:5);GS(s_work,4)=0;
}
static const AnchorMinigameAdapter s_adapter={
    GT_GAME,reset,capture,validate,apply,role,input,return_pending,result
};
static void cube_poses(void)
{
    int i,j;
    if(!s_local_cid || !s_local_session) return;
    for(i=0;i<3;++i) {
        void *t=GP(s_work,0x30+i*4),*o;
        int *out=s_poses[i],active,held,edge;
        if(t && (!pointer_valid(t) || GH(t,0x28)!=48 ||
           !callback_is(GP(t,0x0c),func_08000CC8_708708) || (GW(t,0x68)&2u))) t=0;
        active=t && GB(t,0xd0)==1 && GB(t,0xd1);held=active && (GW(t,0x68)&0x8000u)!=0;
        edge=t!=s_cubes[i] || (t && GB(t,0x74)!=s_cube_task_generation[i]);
        if(edge) {
            if(t) s_cube_generation[i]=s_cube_generation[i]>=32767u?1:s_cube_generation[i]+1;
            s_cubes[i]=t;s_cube_task_generation[i]=t?GB(t,0x74):0;
        }
        edge=edge || active!=s_cube_active[i] || held!=s_cube_held[i];
        if(!active && !s_cube_active[i] && out[0] && !(out[1]&16)) continue;
        if(!s_cube_generation[i] || (!edge && (!active || s_frames-s_cube_emitted[i]<(held?6u:30u)))) continue;
        o=t?GP(t,0x18):0;
        if(active) {
            int valid=pointer_valid(o);
            for(j=0;j<3 && valid;++j) valid=position(GF(o,8+4*j),&out[2+j]);
            for(j=0;j<3 && valid;++j) {
                if(held) out[5+j]=GH(o,0x14+2*j);
                else valid=velocity(GF(t,0x78+4*j),&out[5+j]);
            }
            if(!valid) continue;
        } else anchor_minigame_zero(out+2,sizeof(int)*6);
        out[0]=(int)next_sequence();s_cube_pose_sequence[i]=(unsigned int)out[0];
        out[1]=(int)(s_cube_generation[i]<<6)|GT_INPUT_POSE|(i<<2)|(active?16:0)|(held?32:0);
        s_cube_active[i]=(unsigned char)active;s_cube_held[i]=(unsigned char)held;s_cube_emitted[i]=s_frames;
    }
}
static void reconcile_game_music(void)
{
    /* Scenario 0x0173 ends with audio command 0x0052. Followers bypass its
     * instruction VM, so supply the final audio command only after their
     * native timer/target setup and the owner's game start. */
    if(s_intro_skipped && !s_game_music_started && !s_terminal &&
       s_host_phase<5 && (s_owner || s_host_phase==1) && root_live() &&
       (phase()==1 || phase()==2) && graph() && timer_live() &&
       !func_8003F1D8_3FDD8()) {
        func_80038BC8_397C8(GT_GAME_MUSIC);
        s_game_music_started=1;
    }
}
static void root_callback(void *task,void *object)
{
    int p;
    if(!root_live() || task!=s_root.task) return;
    reconcile_game_music();
    ++s_frames;p=phase();
    /* In-place joins keep the prior player's resources until native rebinding
     * finishes. Phase0 writes Goemon-specific player state during setup. */
    if(p==0 && !anchor_minigame_invites_character_motion_ready(GT_ROOM)) return;
    if(!s_active || (s_owner&&s_ready) || s_terminal) {
        func_080018E8_709328(task,object);
    } else if(p==0) {
        func_080018E8_709328(task,object);
    } else if(p==3 && s_host_phase && s_host_phase!=3 && !func_8003F1D8_3FDD8()) {
        s_intro_skipped=1;
        GB(s_work,0)=4;GS(s_work,4)=0;
    } else if(p==4 && s_host_phase && s_host_phase!=3 && !func_8003F1D8_3FDD8()) {
        func_080018E8_709328(task,object);
    } else if((p==1 || p==2) && s_ready && s_host_phase==1 && graph()) {
        int count=timer_live() ? GI(s_timer.task,0x84):-1;
        GoemonNode saved[GT_COUNT];unsigned char completed[GT_COUNT];int i;
        for(i=0;i<GT_COUNT;++i) { saved[i]=s_targets[i];completed[i]=GB(saved[i].task,0xd1);GB(saved[i].task,0xd1)=0; }
        /* This is the participant's one normal invocation. It owns carrying,
         * B edges, yaw, sounds and throwing. Keep the native result predicates
         * false during this one call; their cleanup must run only after the
         * authoritative result is adopted. Timer getter uses whole seconds. */
        if(timer_live() && count<30) GI(s_timer.task,0x84)=30;
        func_080018E8_709328(task,object);
        if(timer_live() && count>=0) GI(s_timer.task,0x84)=count;
        for(i=0;i<GT_COUNT;++i) if(saved[i].task==s_targets[i].task &&
            live(&saved[i]) && target_live(i)) GB(saved[i].task,0xd1)=completed[i];
        if(phase()==5 || phase()==6) GB(s_work,0)=(unsigned char)(p==2 ? 2:1);
    }
    p=phase();
    if(p==5) { s_outcome=2;s_terminal=1; }
    if(p==6) { s_outcome=1;s_terminal=1; }
    if(p==1 || p==2 || s_terminal) cube_poses();
    visuals_update();
    reconcile_game_music();
}
static void target_predict(int i)
{
    GoemonNode *n=&s_targets[i];GoemonMotion *m=&s_motion[i];
    void *t=n->task,*o=n->object;
    int p=GB(t,0xd0),frame=GS(t,0xd6),j;
    float current=GF(t,0xdc),speed=GF(t,0xe4),desired=GF(t,0xe0),difference=desired-current;
    if(!m->have_net || m->age>=60u) { m->age=61;return; }
    ++m->age;
    if(GB(t,0xd1)==3 || GB(t,0xd1)==4) { p=7;GB(t,0xd0)=7; }
    if((double)speed>0.001) {
        float absolute=difference<0.f?-difference:difference;
        if(absolute<speed) { current=desired;GF(t,0xe4)=0.f; }
        else current+=difference<0.f?-speed:speed;
    }
    if(p==1) current=(float)((double)current+(GS(t,0xd8)<frame?0.4:0.05));
    else if((p==3 || p==4) && frame<60) {
        unsigned int angle=(unsigned short)((frame*0x3c00)/60);
        float sine=func_80003E10_4A10(angle),q=(float)(60-frame)/60.f;
        float scale=(float)(1.0+(double)sine*0.1*(double)q*(double)q);
        GF(o,0x1c)=GF(o,0x20)=GF(o,0x24)=scale;
    }
    else if((p==3 || p==4) && frame>60) GB(t,0xd0)=1;
    GF(t,0xdc)=current;GF(o,0xc)=current;
    GS(t,0xd6)=(short)(frame+1);
    if(m->correction_ticks) {
        for(j=0;j<4;++j) {
            unsigned int off=j?0x18+4*j:0xc;
            float step=m->correction[j]/(float)m->correction_ticks;
            float base=GF(o,off);
            if(j && !((p==3 || p==4) && frame<60)) base-=m->correction[j];
            m->correction[j]-=step;GF(o,off)=base+m->correction[j];
        }
        --m->correction_ticks;
    }
}
static void target_callback(void *task,void *object)
{
    int i,p;
    for(i=0;i<GT_COUNT;++i) if(s_targets[i].task==task) break;
    if(i==GT_COUNT || !target_live(i) || s_targets[i].object!=object) return;
    p=GB(task,0xd0);
    if(!s_active || (s_owner&&s_ready)) {
        GoemonNode before=s_targets[i];
        unsigned int before_visit=s_visit;
        void *before_callback=GP(task,0xc);
        int injected=0;
        if(s_pending_hit[i][0] && s_pending_hit[i][0]==s_action[i] &&
           s_pending_hit[i][1]==(unsigned int)p && shared_phase()==1 &&
           !s_terminal && GB(task,0xd1)!=3 && GB(task,0xd1)!=4 &&
           !(GW(task,0x68)&0x80u)) {
            GW(task,0x68)|=0x80u;injected=1;
        }
        s_pending_hit[i][0]=s_pending_hit[i][1]=0;
        if((p==1 || p==3 || p==4) && GB(task,0xd1)!=3 &&
           GB(task,0xd1)!=4 && (GW(task,0x68)&0x80u))
            s_action[i]=s_action[i]>=0x7fffffffu?1u:s_action[i]+1u;
        func_0800127C_708CBC(task,object);
        /* Synthetic contacts have no attacker for the native post pass to
         * consume. Retire only our temporary bit after the same live call. */
        if(injected && s_visit==before_visit && s_targets[i].task==before.task &&
           s_targets[i].object==before.object && s_targets[i].generation==before.generation &&
           target_live(i) && GP(task,0xc)==before_callback) GW(task,0x68)&=~0x80u;
    } else if(s_ready && (s_host_phase==1 || s_terminal)) {
        if(!s_terminal && (p==1 || p==3 || p==4) && (GW(task,0x68)&0x80u)) {
            if(s_hits[i][0] && (unsigned int)s_hits[i][0]<=local_ack()) s_hits[i][0]=0;
            if(!s_hits[i][0]) {
                s_hits[i][0]=(int)next_sequence();s_hits[i][1]=GT_INPUT_HIT;
                s_hits[i][2]=i;s_hits[i][3]=(int)s_action[i];s_hits[i][4]=p;
            }
            GW(task,0x68)&=~0x80u;
        }
        target_predict(i);
    }
}
static void timer_callback(void *task,void *object)
{
    int value;
    if(!timer_live() || task!=s_timer.task) return;
    value=GI(task,0x84);func_080005D0_708010(task,object);
    if(s_active && (!s_owner || !s_ready)) GI(task,0x84)=value;
    s_timer_value=GI(task,0x84);
}
static void visual_callback(void *task,void *object)
{
    int i;
    for(i=0;i<GT_VISUAL_COUNT;++i) if(s_visuals[i].clone.task==task) break;
    if(i==GT_VISUAL_COUNT || !visual_live(&s_visuals[i])) return;
    GoemonVisual *v=&s_visuals[i];int *r=v->row;
    if(s_terminal) { visuals_stop();return; }
    object=v->clone.object;
    if(!v->clone.initialized) {
        GH(task,0x5c)=GH(task,0x5e)=GT_ENTITY;GW(task,0x60)=0x20u;
        GW(task,0x48)=0;GB(task,0x4c)=0;
        GS(task,0x4e)=GS(task,0x50)=GS(task,0x52)=0;GW(task,0x68)&=~0x8000u;
        func_80216E1C_5D22EC(task,2);v->clone.initialized=1;
    }
    if(s_active && s_ready && !s_terminal && v->have_net && !v->hidden && v->motion_age<60u) {
        int j;
        ++v->motion_age;
        if(r[6] && !r[7]) {
            v->velocity[1]=(float)((double)v->velocity[1]-10.0/50.0);
            if(v->velocity[1]<-3.f) v->velocity[1]=-3.f;
            for(j=0;j<3;++j) {
                float next=v->xyz[j]+2.f*v->velocity[j];int checked;
                if(!position(next,&checked)) { v->hidden=1;break; }
                v->xyz[j]=next;
            }
            if(v->xyz[1]<0.f) v->hidden=1;
        }
        if(v->correction_ticks) {
            for(j=0;j<3;++j) {
                float step=v->correction[j]/(float)v->correction_ticks;
                v->xyz[j]+=step;v->correction[j]-=step;
            }
            --v->correction_ticks;
        }
    }
    GW(task,0x60)=v->hidden || !r[6]?0u:0x20u;
    GF(object,8)=v->xyz[0];GF(object,0x0c)=v->xyz[1];GF(object,0x10)=v->xyz[2];
    GH(object,0x14)=(unsigned short)r[11];GH(object,0x16)=(unsigned short)r[12];GH(object,0x18)=(unsigned short)r[13];

}

RECOMP_HOOK("func_080023A4_709DE4")
void anchor_minigame_goemon_init(void *task,void *object)
{
    (void)object;
    if(D_800C7AB2!=GT_ROOM || !task || !pointer_valid(GP(task,0x18)) || !resource()) return;
    s_init_task=task;s_root.task=task;s_root.object=GP(task,0x18);
    s_root.generation=GB(task,0x74);s_root.native=func_080018E8_709328;
    s_visit=anchor_minigame_sync_visit();
}
RECOMP_HOOK_RETURN("func_080023A4_709DE4")
void anchor_minigame_goemon_init_return(void)
{
    if(!s_init_task || s_root.task!=s_init_task || !live(&s_root) ||
       !callback_is(GP(s_root.task,0x0c),func_080018E8_709328)) return;
    s_work=GP(s_root.task,0xd0);
    if(!pointer_valid(s_work)) return;
    schedule(&s_root,root_callback);s_root.initialized=1;s_init_task=0;
    (void)anchor_minigame_sync_bind(&s_adapter);
}
RECOMP_HOOK("func_0800127C_708CBC")
void anchor_minigame_goemon_target(void *task,void *object)
{
    int i;
    (void)object;
    if(!root_live() || !task || !pointer_valid(GP(task,0x18)) || GB(task,0xd0)!=0) return;
    for(i=0;i<GT_COUNT;++i) if(GP(s_work,0x18+i*4)==task) break;
    if(i==GT_COUNT) return;
    s_pending_hit[i][0]=s_pending_hit[i][1]=0;
    s_targets[i].task=task;s_targets[i].object=GP(task,0x18);
    s_targets[i].generation=GB(task,0x74);s_targets[i].native=func_0800127C_708CBC;
}
RECOMP_HOOK_RETURN("func_0800127C_708CBC")
void anchor_minigame_goemon_target_return(void)
{
    int i;
    for(i=0;i<GT_COUNT;++i) if(live(&s_targets[i]) && !s_targets[i].initialized &&
       GB(s_targets[i].task,0xd0)==1 && pointer_valid(GP(s_targets[i].task,0xe8))) {
        s_targets[i].initialized=1;s_action[i]=1;schedule(&s_targets[i],target_callback);
        s_children[i].task=GP(s_targets[i].task,0xe8);
        s_children[i].object=GP(s_children[i].task,0x18);
        s_children[i].generation=GB(s_children[i].task,0x74);
        s_child_phase[i]=0;s_children[i].initialized=0;
    }
}
RECOMP_HOOK("func_08000FCC_708A0C")
void anchor_minigame_goemon_child(void *task,void *context)
{
    int i;(void)context;s_child_in_call=0;
    for(i=0;i<GT_COUNT;++i) if(child_live(i) && s_children[i].task==task &&
       !s_children[i].initialized) { s_child_in_call=task;break; }
}
RECOMP_HOOK_RETURN("func_08000FCC_708A0C")
void anchor_minigame_goemon_child_return(void)
{
    int i;
    for(i=0;i<GT_COUNT;++i) if(child_live(i) && !s_children[i].initialized &&
       s_children[i].task==s_child_in_call) {
        s_children[i].initialized=1;s_child_phase[i]=1;
    }
    s_child_in_call=0;
}
RECOMP_HOOK("func_0800126C_708CAC")
void anchor_minigame_goemon_child_terminal(void *task)
{
    int i;
    for(i=0;i<GT_COUNT;++i) if(child_live(i) && s_children[i].task==task) {
        s_child_phase[i]=2;break;
    }
}
RECOMP_HOOK("func_080005D0_708010")
void anchor_minigame_goemon_timer(void *task,void *object)
{
    if(!root_live() || !task || GP(s_work,0x14)!=task || s_timer.task) return;
    s_timer.task=task;s_timer.object=object;s_timer.native=func_080005D0_708010;
    schedule(&s_timer,timer_callback);
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_minigame_goemon_reuse(void *task)
{
    int i;
    if(task==s_root.task) { reset();return; }
    if(task==s_timer.task) anchor_minigame_zero(&s_timer, sizeof(s_timer));
    for(i=0;i<GT_COUNT;++i) {
        if(task==s_targets[i].task) { s_targets[i].task=0;anchor_minigame_zero(&s_motion[i],sizeof(s_motion[i]));s_action[i]=0;s_pending_hit[i][0]=s_pending_hit[i][1]=0; }
        if(task==s_children[i].task) { s_children[i].task=0;s_child_phase[i]=2; }
    }
    for(i=0;i<GT_VISUAL_COUNT;++i) if(task==s_visuals[i].clone.task)
        anchor_minigame_zero(&s_visuals[i].clone, sizeof(s_visuals[i].clone));
}
RECOMP_HOOK("func_80034EF8_35AF8")
void anchor_minigame_goemon_delete(void *task)
{
    int i;
    if(task==s_root.task) { s_root.task=0;anchor_minigame_zero(s_pending_hit,sizeof(s_pending_hit)); }
    if(task==s_timer.task) s_timer.task=0;
    for(i=0;i<GT_COUNT;++i) {
        if(task==s_targets[i].task) { s_targets[i].task=0;anchor_minigame_zero(&s_motion[i],sizeof(s_motion[i]));s_action[i]=0;s_pending_hit[i][0]=s_pending_hit[i][1]=0; }
        if(task==s_children[i].task) { s_children[i].task=0;s_child_phase[i]=2; }
    }
    for(i=0;i<GT_VISUAL_COUNT;++i) if(task==s_visuals[i].clone.task)
        s_visuals[i].clone.task=0;
}
