#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef void (*CongoCallback)(void *,void *);
typedef struct Fixture { unsigned char task[256],object[256],link[16]; void *ptr[64];CongoCallback ai,post; } Fixture;
static Fixture pool[64];static unsigned int pool_count;
static void **test_ptr(void *p,unsigned int offset)
{
    unsigned int i;
    for(i=0;i<pool_count;i++) {
        if(p==pool[i].task)return &pool[i].ptr[offset/4];
        if(p==pool[i].link && offset==0)return &pool[i].ptr[63];
    }
    assert(!"unknown pointer slot");return 0;
}
static CongoCallback *test_cb(void *p,int post)
{
    unsigned int i;for(i=0;i<pool_count;i++)if(p==pool[i].task)return post?&pool[i].post:&pool[i].ai;
    assert(!"unknown callback");return 0;
}
#define CONGO_PTR(p,o) (*test_ptr((void *)(p),(o)))
#define CONGO_AI(p) (*test_cb((void *)(p),0))
#define CONGO_POST(p) (*test_cb((void *)(p),1))
#define ANCHOR_CONGO_NATIVE_HOST_TEST
#include "../src/anchor_congo_native.c"
unsigned short D_800C7AB2=0x16;
unsigned char D_8015C562_15D162;
unsigned char D_8015CC30_15D830[0x100];
float func_8001B5AC_1C1AC(void *o) { (void)o;return 120.f; }
void func_8001481C_1541C(void *t) { assert(t); }
static unsigned char system_data[0x3AE40];
unsigned char *D_8015C5C8_15D1C8=system_data;
void *D_8016DAB4_16E6B4;
static unsigned int visit=1,init_calls,spin_calls,intro_calls,death_calls;
static unsigned char flags[512],events[16];
unsigned int anchor_boss_invite_world_visit(void) { return visit; }
void anchor_congo_damage_bind_root(void *p) { (void)p; }
void func_80023DF0_249F0(unsigned int i) { assert(i<16);events[i]=1; }
void func_80023E40_24A40(unsigned int i) { assert(i<16);events[i]=0; }
void func_80024038_24C38(unsigned int i) { assert(i<512);flags[i]=1; }
void *func_800141C4_14DC4(unsigned int file) { assert(file==29);return &system_data[0]; }
static Fixture *new_actor(unsigned int entity)
{
    Fixture *f;assert(pool_count<64);f=&pool[pool_count++];memset(f,0,sizeof(*f));
    CONGO_PTR(f->task,4)=f->link;CONGO_PTR(f->link,0)=f->task;CONGO_PTR(f->task,0x18)=f->object;
    U16(f->task,0x5C)=(unsigned short)pool_count;U16(f->task,0x5E)=(unsigned short)entity;U8(f->task,0x74)=1;
    F32(f->object,0x1C)=F32(f->object,0x20)=F32(f->object,0x24)=1.f;
    f->ai=func_080073FC_6BA69C;f->post=hold_noop;return f;
}
void *func_8021DDE8_5D92B8(void *p,CongoCallback init,unsigned char kind,float x,float y,float z,int a)
{
    Fixture *f=new_actor(0x7E);(void)x;(void)y;(void)z;(void)a;assert(kind==10);
    CONGO_PTR(f->task,0xD0)=p;f->ai=init;return f->task;
}
void func_8021664C_5D1B1C(void *t,unsigned int clip,float rate,unsigned int flags_value)
{
    assert(rate==0.05f && flags_value==0);anchor_congo_native_animation(t,clip);
    F32(CONGO_PTR(t,0x18),0x28)=0;
}
void func_08000DCC_6B406C(void *t,void *o)
{
    init_calls++;U16(t,0x5E)=0x7E;U8(t,0xED)=120;U16(t,0x8A)=15;
    F32(t,0x78)=2.2f;F32(t,0x80)=0.f;U16(o,0x7E)=256;
    CONGO_AI(t)=func_08000F44_6B41E4;anchor_congo_native_flame();
}
void func_08000F44_6B41E4(void *t,void *o)
{
    U8(t,0xED)--;F32(o,12)-=2; if(U16(t,0x8A)--==0)CONGO_AI(t)=func_0800110C_6B43AC;
}
void func_0800110C_6B43AC(void *t,void *o)
{
    (void)o;if(--U8(t,0xED)<=10)U32(t,0x68)|=2;
}
void func_080088C4_6BBB64(void *t,void *o)
{
    (void)o;assert(U8(t,0xDD)==2);intro_calls++;flags[0x129]=flags[0x130]=1;CONGO_AI(t)=hold_noop;
}
void func_0800A228_6BD4C8(void *t,void *o)
{
    (void)o;if(U8(t,0x8D)==0){U8(t,0x8D)=1;flags[0x1A1]=1;CONGO_AI(t)=func_08007D24_6BAFC4;}
}
void func_08007D24_6BAFC4(void *t,void *o)
{
    (void)o;anchor_congo_native_victory(t);death_calls++;flags[0x12D]=1;CONGO_AI(t)=func_08007DEC_6BB08C;
}
void func_08009C04_6BCEA4(void *t,void *o) { (void)t;(void)o;spin_calls++; }
void func_080066F4_6B9994(void *t,void *o) { (void)t;(void)o; }
void func_080068FC_6B9B9C(void *t,void *o) { (void)t;(void)o; }
void func_08006B04_6B9DA4(void *t,void *o) { (void)t;(void)o; }
void func_08006D0C_6B9FAC(void *t,void *o) { (void)t;(void)o; }
void func_08006F14_6BA1B4(void *t,void *o) { (void)t;(void)o; }
void func_0800711C_6BA3BC(void *t,void *o) { (void)t;(void)o; }
void func_080073D0_6BA670(void *t,void *o) { (void)t;(void)o; }
void func_080073FC_6BA69C(void *t,void *o) { (void)t;(void)o; }
void func_08007488_6BA728(void *t,void *o) { (void)t;(void)o; }
void func_080074CC_6BA76C(void *t,void *o) { (void)t;(void)o; }
void func_0800751C_6BA7BC(void *t,void *o) { (void)t;(void)o; }
void func_080075E8_6BA888(void *t,void *o) { (void)t;(void)o; }
void func_08007694_6BA934(void *t,void *o) { (void)t;(void)o; }
void func_08007724_6BA9C4(void *t,void *o) { (void)t;(void)o; }
void func_080077A4_6BAA44(void *t,void *o) { (void)t;(void)o; }
void func_0800781C_6BAABC(void *t,void *o) { (void)t;(void)o; }
void func_080079B0_6BAC50(void *t,void *o) { (void)t;(void)o; }
void func_080079E4_6BAC84(void *t,void *o) { (void)t;(void)o; }
void func_08007A64_6BAD04(void *t,void *o) { (void)t;(void)o; }
void func_08007ACC_6BAD6C(void *t,void *o) { (void)t;(void)o; }
void func_08007B58_6BADF8(void *t,void *o) { (void)t;(void)o; }
void func_08007BB0_6BAE50(void *t,void *o) { (void)t;(void)o; }
void func_08007C18_6BAEB8(void *t,void *o) { (void)t;(void)o; }
void func_08007C70_6BAF10(void *t,void *o) { (void)t;(void)o; }
void func_08007CBC_6BAF5C(void *t,void *o) { (void)t;(void)o; }
void func_08007DEC_6BB08C(void *t,void *o) { (void)t;(void)o; }
void func_08008194_6BB434(void *t,void *o) { (void)t;(void)o; }
void func_0800820C_6BB4AC(void *t,void *o) { (void)t;(void)o; }
void func_08008280_6BB520(void *t,void *o) { (void)t;(void)o; }
void func_080082F8_6BB598(void *t,void *o) { (void)t;(void)o; }
void func_0800833C_6BB5DC(void *t,void *o) { (void)t;(void)o; }
void func_080083A0_6BB640(void *t,void *o) { (void)t;(void)o; }
void func_08008560_6BB800(void *t,void *o) { (void)t;(void)o; }
void func_08008624_6BB8C4(void *t,void *o) { (void)t;(void)o; }
void func_0800868C_6BB92C(void *t,void *o) { (void)t;(void)o; }
void func_0800876C_6BBA0C(void *t,void *o) { (void)t;(void)o; }
void func_08008804_6BBAA4(void *t,void *o) { (void)t;(void)o; }
void func_08008888_6BBB28(void *t,void *o) { (void)t;(void)o; }

static Fixture *setup(void)
{
    Fixture *root;unsigned int i;
    anchor_congo_native_reset();pool_count=0;memset(pool,0,sizeof(pool));
    memset(flags,0,sizeof(flags));memset(events,0,sizeof(events));
    s_visit=visit;s_active=s_owner=s_paused=0;root=new_actor(0x323);
    U8(root->task,0x8D)=30;U32(root->task,0xE8)=0x04000000;
    U16(root->task,0x3C)=10;U16(root->task,0x3E)=80;
    D_8016DAB4_16E6B4=root->task;anchor_congo_native_bind_root();
    for(i=0;i<6;i++){Fixture *p=new_actor(0x323);CONGO_PTR(p->task,0xDC)=root->task;bind_part(p->task,i);p->ai=s_part_updates[i];}
    assert(anchor_congo_native_ready());return root;
}
static void deferred_context_test(void)
{
    Fixture *r=setup();AnchorCongoNativeSnapshot s;void *foreign=new_actor(1)->task;
    assert(anchor_congo_native_capture(&s));anchor_congo_native_set_role(1,0,1);
    D_8016DAB4_16E6B4=0;assert(anchor_congo_native_apply(&s));
    {AnchorCongoNativeSnapshot tmp;assert(!anchor_congo_native_capture(&tmp));}
    anchor_congo_native_adopt_before_pre(r->task);assert(s_pending_valid);
    D_8016DAB4_16E6B4=foreign;anchor_congo_native_adopt_before_pre(r->task);assert(s_pending_valid);
    D_8016DAB4_16E6B4=r->task;U32(D_8015CC30_15D830,0xD4)=1;
    anchor_congo_native_adopt_before_pre(r->task);assert(s_pending_valid);
    U32(D_8015CC30_15D830,0xD4)=0;anchor_congo_native_scheduler_begin();
    anchor_congo_native_adopt_before_pre(r->task);assert(!s_pending_valid);
    assert(D_8016DAB4_16E6B4==r->task && r->ai==hold_noop);
    anchor_congo_native_scheduler_end();assert(anchor_congo_native_capture(&s));
    s.tick++;assert(anchor_congo_native_apply(&s));anchor_congo_native_set_role(0,0,0);
    assert(!s_pending_valid);
}
static void lifecycle_test(void)
{
    Fixture *r=setup();CongoCallback ai=r->ai,post=r->post;void *original=new_actor(1)->object;
    CONGO_PTR(r->task,0x84)=original;
    anchor_congo_native_set_role(1,0,0);
    anchor_congo_native_scheduler_begin();assert(r->ai==hold_noop && r->post==post);
    anchor_congo_native_scheduler_end();assert(r->ai==ai && r->post==post);
    anchor_congo_native_scheduler_begin();
    r->ai=(CongoCallback)((unsigned long)r->ai|CALLBACK_DISABLED);
    r->post=(CongoCallback)((unsigned long)r->post|CALLBACK_DISABLED);
    anchor_congo_native_scheduler_end();
    assert(callback_is(r->ai,ai) && ((unsigned long)r->ai&CALLBACK_DISABLED));
    assert(callback_is(r->post,post));r->ai=ai;r->post=post;
    anchor_congo_native_set_role(1,1,1);anchor_congo_native_scheduler_begin();
    assert(r->ai==hold_noop && r->post==hold_noop);
    assert(s_parts[0].held);anchor_congo_native_set_role(0,0,0);
    assert(r->ai==ai && r->post==post && !s_parts[0].held);
    anchor_congo_native_set_role(1,1,0);anchor_congo_native_set_target(1,2,3);
    anchor_congo_native_target_begin(r->task);assert(CONGO_PTR(r->task,0x84)==s_target_object);
    anchor_congo_native_target_end();assert(CONGO_PTR(r->task,0x84)==original);
    anchor_congo_native_clear_target();anchor_congo_native_target_begin(r->task);
    assert(CONGO_PTR(r->task,0x84)==original);
    anchor_congo_native_set_role(1,0,0);U32(r->task,0xE8)=0x04003080;
    anchor_congo_native_health_begin(r->task);assert(U32(r->task,0xE8)==0x3080);
    anchor_congo_native_health_end();assert(U32(r->task,0xE8)==0x04003080);
    /* Recycled tasks may never receive callback restoration. */
    anchor_congo_native_scheduler_begin();r->ai=func_08007488_6BA728;U8(r->task,0x74)++;
    anchor_congo_native_scheduler_end();assert(r->ai==func_08007488_6BA728);
    assert(!anchor_congo_native_root_task());
}
static void snapshot_test(void)
{
    Fixture *r=setup();AnchorCongoNativeSnapshot s,invalid;unsigned int before;
    r->ai=func_08007BB0_6BAE50;U16(r->object,0x16)=400;U16(r->task,0x8A)=60;
    s_tick=200;s_spin_serial=3;assert(anchor_congo_native_capture(&s));
    invalid=s;invalid.root[CONGO_X]=0x7FC00000;assert(!apply_now(&invalid));
    invalid=s;invalid.root[CONGO_STATUS]=0x80;assert(!apply_now(&invalid));
    invalid=s;invalid.root[CONGO_FLAGS]=0x80000000;assert(!apply_now(&invalid));
    invalid=s;invalid.root[CONGO_PHASE]=99;assert(!apply_now(&invalid));
    invalid=s;invalid.part[2][0]=17;assert(!apply_now(&invalid));
    r=setup();anchor_congo_native_set_role(1,0,0);before=spin_calls;
    assert(apply_now(&s));assert(spin_calls==before+1);assert(s_tick==200);
    assert(r->ai==func_08007BB0_6BAE50 && U16(r->object,0x16)==400);
    assert(apply_now(&s));assert(spin_calls==before+1);
    anchor_congo_native_observe_post(r->task);anchor_congo_native_tick();anchor_congo_native_scheduler_begin();
    assert(U16(r->object,0x16)==404);anchor_congo_native_scheduler_end();
    anchor_congo_native_set_role(1,1,0);anchor_congo_native_scheduler_begin();
    assert(r->ai==func_08007BB0_6BAE50);anchor_congo_native_scheduler_end();
    /* First checkpoint includes historical, still-live flames. */
    s.flame_count=1;s.flame[0][0]=17;s.flame[0][1]=190;s.flame[0][2]=16;
    s.flame[0][3]=float_bits(10);s.flame[0][4]=float_bits(35);s.flame[0][5]=0;s.flame[0][6]=400;
    before=init_calls;assert(apply_now(&s));assert(init_calls==before+1);
    assert(s_flames[0].seed[0]==17);assert(F32(s_flames[0].actor.object,12)==17);
    assert(F32(s_flames[0].actor.object,8)>31.99f && F32(s_flames[0].actor.object,8)<32.01f);
    assert(apply_now(&s));assert(init_calls==before+1);
    s.flame_count=0;assert(apply_now(&s));assert(!live(&s_flames[0].actor));
}
static void intro_and_death_test(void)
{
    Fixture *r=setup(),*camera;AnchorCongoNativeSnapshot s;unsigned int before;
    assert(anchor_congo_native_capture(&s));
    r->ai=hold_noop;assert(anchor_congo_native_ready());anchor_congo_native_set_role(1,0,1);
    anchor_congo_native_scheduler_begin();assert(!s_root.held);anchor_congo_native_scheduler_end();
    assert(!apply_now(&s));camera=new_actor(0);
    camera->ai=func_08008560_6BB800;CONGO_PTR(camera->task,0xD0)=camera->object;
    D_8016DAB4_16E6B4=camera->task;anchor_congo_native_bind_camera();before=intro_calls;
    assert(apply_now(&s));assert(intro_calls==before+1);
    assert(flags[0x128] && flags[0x12F] && flags[0x133] && flags[0x129] && flags[0x130]);
    s.root[CONGO_PHASE]=19;s.root[CONGO_HP]=1;before=death_calls;
    assert(apply_now(&s));assert(death_calls==before+1 && flags[0x1A1] && flags[0x12D]);
    assert(apply_now(&s));assert(death_calls==before+1);
    assert(r->ai==func_08007DEC_6BB08C);
    U16(r->task,0x8A)=90;s.root[CONGO_PHASE]=20;s.root[CONGO_TIMER]=120;
    assert(apply_now(&s));assert(U16(r->task,0x8A)==90);
    s.root[CONGO_PHASE]=19;assert(apply_now(&s));
    assert(r->ai==func_08007DEC_6BB08C && death_calls==before+1);
    /* A follower must run its owned native victory/camera progression. */
    anchor_congo_native_set_role(1,0,0);anchor_congo_native_scheduler_begin();
    assert(r->ai==func_08007DEC_6BB08C);anchor_congo_native_scheduler_end();
    s.root[CONGO_PHASE]=26;assert(apply_now(&s));
    assert(flags[0x12B] && flags[0x12E] && !D_8015C562_15D162);
    before=s_tick;anchor_congo_native_tick();assert(s_tick==before);
    anchor_congo_native_observe_post(r->task);anchor_congo_native_tick();assert(s_tick==before+1);
    anchor_congo_native_tick();assert(s_tick==before+1);
}
int main(void)
{
    deferred_context_test();lifecycle_test();snapshot_test();intro_and_death_test();
    puts("Congo native snapshot, authority, pause, target, intro and hazard contracts passed");return 0;
}
