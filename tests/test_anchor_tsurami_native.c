#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*TestCallback)(void *, void *);

typedef struct Fixture
{
    union { void *alignment; unsigned char bytes[0x200]; } task_store;
    union { void *alignment; unsigned char bytes[0x200]; } object_store;
    union { void *alignment; unsigned char bytes[0x20]; } backlink_store;
    TestCallback ai;
    TestCallback post;
    TestCallback reaction;
    void *ptr04;
    void *ptr18;
    void *ptr1c;
    void *object_next;
    void *ptr2c;
    void *ptr38;
    void *ptr5c;
    void *ptr84;
    void *ptrd0;
    void *ptrd4;
    void *backlink_ptr0;
} Fixture;

#define TEST_CAPACITY 48
static Fixture pool[TEST_CAPACITY];
static unsigned char system_data[0x40000],flash_data[0x40],foreign_flash[0x40];
static void *flash_pointer;
static void *flash_next;
static unsigned int pool_count;

static Fixture *fixture_for(const void *task)
{
    unsigned int index;
    for (index = 0; index < pool_count; ++index)
        if (task == pool[index].task_store.bytes)
            return &pool[index];
    return 0;
}

static void **fixture_pointer_slot(void *record, unsigned int offset)
{
    unsigned int index;
    Fixture *fixture;
    if(record==system_data && offset==0x3b018)return &flash_pointer;
    if(record==flash_data && offset==0)return &flash_next;
    for (index = 0; index < pool_count; ++index)
    {
        fixture = &pool[index];
        if(record==fixture->object_store.bytes && offset==0)return &fixture->object_next;
        if (record == fixture->backlink_store.bytes && offset == 0)
            return &fixture->backlink_ptr0;
        if (record != fixture->task_store.bytes)
            continue;
        if (offset == 0x04) return &fixture->ptr04;
        if (offset == 0x18) return &fixture->ptr18;
        if (offset == 0x1c) return &fixture->ptr1c;
        if (offset == 0x2c) return &fixture->ptr2c;
        if (offset == 0x38) return &fixture->ptr38;
        if (offset == 0x5c) return &fixture->ptr5c;
        if (offset == 0x84) return &fixture->ptr84;
        if (offset == 0xd0) return &fixture->ptrd0;
        if (offset == 0xd4) return &fixture->ptrd4;
    }
    assert(!"unexpected pointer slot");
    return &pool[0].ptr04;
}

#define TSURAMI_PTR(p, o) \
    (*fixture_pointer_slot((void *)(p), (o)))
#define TSURAMI_AI(p) (fixture_for(p)->ai)
#define TSURAMI_POST(p) (fixture_for(p)->post)
/* The native tag bit can occur naturally in a host function address. */
#define CALLBACK_DISABLED (1ul << (sizeof(unsigned long) * 8 - 1))
#define ANCHOR_TSURAMI_NATIVE_HOST_TEST
#include "../src/anchor_tsurami_native.c"

unsigned short D_800C7AB2;
unsigned int D_8015C5E4;
void *D_8016DAB4_16E6B4,*D_801FC60C_5B851C;
unsigned char D_8015CC30_15D830[0x300];
unsigned char *D_8015C5C8_15D1C8=system_data;
static unsigned int current_visit;
static int terminal_calls,damage_flushes,resource_ready=1,flash_allocations,flash_releases;
static unsigned char resource;
static unsigned char flags[0x300];
static unsigned int observed_group,observed_opacity;
void *func_8021DD4C_5D921C(void *t,unsigned int alpha,unsigned int r,unsigned int g,unsigned int b){(void)t;(void)r;(void)g;(void)b;observed_opacity=alpha;return 0;}
void *func_8021A26C_5D573C(void *t,unsigned int alpha,unsigned int r,unsigned int g,unsigned int b){return func_8021DD4C_5D921C(t,alpha,r,g,b);}
int anchor_remote_model_pool_contains(const void *p){return p!=0;}
unsigned int anchor_boss_invite_world_visit(void){return current_visit;}
#ifndef ANCHOR_TSURAMI_REFLECTION_INTEGRATION
void anchor_tsurami_damage_bind_root(void *p){assert(p);}
void anchor_tsurami_damage_flush(void){++damage_flushes;}
void anchor_tsurami_damage_discard_pending(void){}
#endif
int boss_sync_queue_tsurami_shared_terminal(void){++terminal_calls;return 1;}
int func_800240DC_24CDC(int flag){return flags[flag];}
void func_80024038_24C38(unsigned int flag){flags[flag]=1;}
void func_80024088_24C88(unsigned int flag){flags[flag]=0;}
void *func_800141C4_14DC4(unsigned int file){return resource_ready && file==0x1d?&resource:(void *)0xfffffffful;}
float func_8001B5AC_1C1AC(void *p){assert(p);return 100.f;}
void func_8021664C_5D1B1C(void *task,unsigned int clip,float rate,unsigned int flags){
    anchor_tsurami_native_animation(task,clip);U8(TSURAMI_PTR(task,0x18),0x7c)=flags;
    U16(TSURAMI_PTR(task,0x18),0x7e)=(unsigned short)(rate*256.f);
}
void func_8022026C_5DB73C(void *task,unsigned int r,unsigned int g,unsigned int b){
    (void)r;(void)g;(void)b;
    assert(U8(flash_data,4)!=1);flash_next=0;
    TSURAMI_PTR(TSURAMI_PTR(task,0x1c),0)=flash_data;TSURAMI_PTR(task,0x1c)=flash_data;
    U8(flash_data,4)=1;U8(flash_data,5)=11;U8(flash_data,0x10)=0;
    flash_pointer=flash_data;++flash_allocations;
}
void *func_80036158_36D58(void *task,void *object,unsigned int count){
    void *node=TSURAMI_PTR(task,0x18),*previous=0;
    assert(count==1);
    while(node && node!=object){previous=node;node=TSURAMI_PTR(node,0);}
    if(!node)return 0;
    if(previous)TSURAMI_PTR(previous,0)=TSURAMI_PTR(node,0);
    else TSURAMI_PTR(task,0x18)=TSURAMI_PTR(node,0);
    if(TSURAMI_PTR(task,0x1c)==node)TSURAMI_PTR(task,0x1c)=previous;
    anchor_tsurami_native_flash_release(node);
    /* Native 8000A228 frees kind1, resets alpha255, and leaves the alias. */
    U8(node,4)=0x81;U8(node,5)=0;U8(node,0x10)=255;++flash_releases;
    return 0;
}
int func_80220168_5DB638(void *task,unsigned int speed){
    int alpha;
    if(!flash_pointer)return 0;
    alpha=U8(flash_pointer,0x10)-(speed&255);
    if(alpha>0){U8(flash_pointer,0x10)=alpha;return 1;}
    U8(flash_pointer,0x10)=0;func_80036158_36D58(task,flash_pointer,1);return 0;
}
static Fixture *new_fixture(void){
    Fixture *f=&pool[pool_count++];assert(pool_count<=TEST_CAPACITY);memset(f,0,sizeof(*f));
    f->ptr04=f->backlink_store.bytes;f->backlink_ptr0=f->task_store.bytes;f->ptr18=f->object_store.bytes;
    f->ptr1c=f->ptr18;U8(f->ptr18,4)=2;
    U16(f->task_store.bytes,0x5c)=0xcb;U8(f->task_store.bytes,0x74)=pool_count;
    U8(f->task_store.bytes,0x8d)=1;
    F32(f->ptr18,0x1c)=F32(f->ptr18,0x20)=F32(f->ptr18,0x24)=1.f;
    return f;
}
void *func_8021DDE8_5D92B8(void *parent,TsuramiCallback init,unsigned char priority,float x,float y,float z,int yaw){
    Fixture *f=new_fixture();observed_group=priority;f->ai=init;f->post=func_80218F30_5D4400;f->ptrd0=parent;
    unsigned int i;
    /* 802171A8 inherits object rotations; DDE8's angle is task +0x88. */
    for(i=0;i<3;i++)U16(f->ptr18,0x14+2*i)=U16(TSURAMI_PTR(parent,0x18),0x14+2*i);
    F32(f->ptr18,8)=x;F32(f->ptr18,12)=y;F32(f->ptr18,16)=z;U16(f->task_store.bytes,0x88)=yaw;return f->task_store.bytes;
}
#define EMPTY_CALLBACK(name) void name(void *task,void *object){(void)task;(void)object;}
EMPTY_CALLBACK(func_08001D54_6B4FF4)
EMPTY_CALLBACK(func_08001DB0_6B5050)
EMPTY_CALLBACK(func_08001DF0_6B5090)
EMPTY_CALLBACK(func_08001EAC_6B514C)
EMPTY_CALLBACK(func_08001F28_6B51C8)
EMPTY_CALLBACK(func_08001F68_6B5208)
EMPTY_CALLBACK(func_08001FE8_6B5288)
EMPTY_CALLBACK(func_08002028_6B52C8)
EMPTY_CALLBACK(func_080020D8_6B5378)
EMPTY_CALLBACK(func_08002128_6B53C8)
EMPTY_CALLBACK(func_08002190_6B5430)
EMPTY_CALLBACK(func_080023E0_6B5680)
EMPTY_CALLBACK(func_08002460_6B5700)
EMPTY_CALLBACK(func_080024A0_6B5740)
EMPTY_CALLBACK(func_080024F0_6B5790)
EMPTY_CALLBACK(func_08002534_6B57D4)
EMPTY_CALLBACK(func_0800257C_6B581C)
EMPTY_CALLBACK(func_080027E4_6B5A84)
EMPTY_CALLBACK(func_08002864_6B5B04)
EMPTY_CALLBACK(func_080028A4_6B5B44)
EMPTY_CALLBACK(func_08002918_6B5BB8)
EMPTY_CALLBACK(func_080029D8_6B5C78)
EMPTY_CALLBACK(func_08002C4C_6B5EEC)
EMPTY_CALLBACK(func_08002CD8_6B5F78)
EMPTY_CALLBACK(func_08002D2C_6B5FCC)
EMPTY_CALLBACK(func_08002D6C_6B600C)
EMPTY_CALLBACK(func_08002DA8_6B6048)
EMPTY_CALLBACK(func_08002E34_6B60D4)
EMPTY_CALLBACK(func_08002EC8_6B6168)
EMPTY_CALLBACK(func_08002F0C_6B61AC)
EMPTY_CALLBACK(func_08002F50_6B61F0)
EMPTY_CALLBACK(func_08002FA4_6B6244)
EMPTY_CALLBACK(func_08000388_6B3628)
EMPTY_CALLBACK(func_080017F4_6B4A94)
EMPTY_CALLBACK(func_080045F8_6B7898)
EMPTY_CALLBACK(func_080046B8_6B7958)
EMPTY_CALLBACK(func_0800476C_6B7A0C)
EMPTY_CALLBACK(func_08004ED0_6B8170)
EMPTY_CALLBACK(func_0800587C_6B8B1C)
#ifndef ANCHOR_TSURAMI_REFLECTION_INTEGRATION
EMPTY_CALLBACK(func_08005B1C_6B8DBC)
EMPTY_CALLBACK(func_08005B50_6B8DF0)
EMPTY_CALLBACK(func_80218F30_5D4400)
#endif
EMPTY_CALLBACK(func_08002FFC_6B629C)
void func_080049A4_6B7C44(void *task,void *object){
    /* Native 049A4 calls 8021A310 after selecting the projectile model. */
    U16(object,0x14)=U16(object,0x16)=U16(object,0x18)=0x8000;
    U16(task,0x5e)=0xcb;TSURAMI_AI(task)=func_08004ED0_6B8170;
    TSURAMI_POST(task)=func_80218F30_5D4400;U8(task,0x8d)=20;
    if(U32(task,0xe8)&1u) {
        U32(task,0x60)=0x200ee1;U32(task,0x64)|=0x8000;
        fixture_for(task)->reaction=func_08005B1C_6B8DBC;
    }
}
void func_080056FC_6B899C(void *task,void *object){
    if(U32(task,0xe8)&0x100)U16(object,0x14)=512;
    U16(task,0x5e)=0x7e;TSURAMI_AI(task)=func_0800587C_6B8B1C;
    TSURAMI_POST(task)=func_80218F30_5D4400;U8(task,0xd1)=180;
}
static Fixture *setup(void){
    Fixture *r,*v;anchor_tsurami_native_reset();memset(pool,0,sizeof(pool));pool_count=0;
    memset(flags,0,sizeof(flags));memset(D_8015CC30_15D830,0,sizeof(D_8015CC30_15D830));
    D_800C7AB2=0x71;D_8015C5E4=1;current_visit++;r=new_fixture();
    r->ai=func_08001D54_6B4FF4;r->post=func_08000388_6B3628;
    U8(r->task_store.bytes,0x8d)=12;U8(r->task_store.bytes,0x8c)=180;
    U32(r->task_store.bytes,0x60)=0x2006e1;U32(r->task_store.bytes,0x64)=0x8000;
    U32(r->task_store.bytes,0xe8)=0x08000000;
    D_8016DAB4_16E6B4=r->task_store.bytes;anchor_tsurami_native_bind_root();
    v=new_fixture();v->ptrd0=r->task_store.bytes;v->ai=func_080046B8_6B7958;v->post=func_80218F30_5D4400;
    U16(v->task_store.bytes,0x5e)=0xcb;D_8016DAB4_16E6B4=v->task_store.bytes;anchor_tsurami_native_bind_visual();
    D_8016DAB4_16E6B4=r->task_store.bytes;anchor_tsurami_native_set_role(1,1,0);return r;
}
#ifndef ANCHOR_TSURAMI_REFLECTION_INTEGRATION
static void test_adoption_and_pause(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,out;void *task=r->task_store.bytes;
    assert(anchor_tsurami_native_ready());assert(anchor_tsurami_native_capture(&s));
    s.root[TSU_PHASE]=22;s.root[TSU_TIMER]=100;s.root[TSU_HP]=4;s.root[TSU_X]=float_bits(200.f);
    s.root[TSU_ANIM_STATE]=0x10000;s.root[TSU_BURST]=19;s.root[TSU_SPIN_SPEED]=64;
    s.root[TSU_EVENTS]=5;s.tick=20;
    anchor_tsurami_native_set_role(1,0,0);assert(anchor_tsurami_native_apply(&s));
    assert(!anchor_tsurami_native_capture(&out));anchor_tsurami_native_scheduler_begin();
    assert(r->ai==hold_noop && r->post==hold_noop);
    anchor_tsurami_native_adopt_before_pre(task);
    assert(r->ai==hold_noop && r->post==func_08000388_6B3628);
    anchor_tsurami_native_health_begin(task);assert(!(U32(task,0xe8)&0x08000000u));anchor_tsurami_native_health_end();assert(U32(task,0xe8)&0x08000000u);
    assert(F32(r->ptr18,8)==200.f && U8(task,0x8d)==4 && flags[0x1bc]);
    anchor_tsurami_native_scheduler_end();assert(r->ai==func_080029D8_6B5C78);
    assert(anchor_tsurami_native_capture(&out));assert(out.root[TSU_TIMER]==100);
    anchor_tsurami_native_set_role(1,0,1);anchor_tsurami_native_scheduler_begin();
    assert(r->ai==hold_noop && r->post==hold_noop);
    r->ai=(TsuramiCallback)((unsigned long)r->ai|CALLBACK_DISABLED);
    anchor_tsurami_native_scheduler_end();assert(((unsigned long)r->ai&CALLBACK_DISABLED)!=0);
    assert(anchor_tsurami_native_world_paused());
    r->ai=func_080029D8_6B5C78;anchor_tsurami_native_set_role(1,1,0);
    anchor_tsurami_native_adopt_before_pre(task);assert(damage_flushes);
    current_visit++;assert(!anchor_tsurami_native_ready());
}
static void test_hazards_and_validation(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,bad;unsigned int *p;void *task=r->task_store.bytes,*projectile;
    assert(anchor_tsurami_native_capture(&s));s.projectile_count=2;s.projectile_serial=2;s.tick=100;
    memset(s.projectile,0,sizeof(s.projectile));p=s.projectile[0];p[0]=1;p[1]=90;p[3]=0x41;p[4]=200;p[5]=20;
    p[TSU_PROJECTILE_X]=float_bits(15.f);p[TSU_PROJECTILE_VX]=float_bits(2.f);
    p[TSU_PROJECTILE_SCALE_X]=p[TSU_PROJECTILE_SCALE_Y]=p[TSU_PROJECTILE_SCALE_Z]=float_bits(1.f);
    p=s.projectile[1];p[0]=2;p[1]=95;p[2]=1;p[3]=0x20;p[4]=60;p[TSU_PROJECTILE_AI]=2;p[TSU_PROJECTILE_OPACITY]=165;
    p[TSU_PROJECTILE_SCALE_X]=p[TSU_PROJECTILE_SCALE_Y]=p[TSU_PROJECTILE_SCALE_Z]=float_bits(3.f);
    assert(snapshot_valid(&s));bad=s;bad.projectile[1][0]=1;assert(!anchor_tsurami_native_apply(&bad));
    bad=s;bad.root[TSU_X]=0x7fc00000;assert(!snapshot_valid(&bad));
    bad=s;bad.projectile[0][3]=3;assert(!snapshot_valid(&bad));
    bad=s;bad.projectile[0][1]=101;assert(!snapshot_valid(&bad));
    anchor_tsurami_native_set_role(1,0,0);assert(anchor_tsurami_native_apply(&s));
    resource_ready=0;anchor_tsurami_native_adopt_before_pre(task);assert(s_pending_valid);
    resource_ready=1;anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(!s_pending_valid);
    projectile=anchor_tsurami_native_projectile_task(1);assert(projectile);assert(anchor_tsurami_native_projectile_id(projectile)==1);
    assert(anchor_tsurami_native_is_projectile(s_projectiles[1].actor.task));
    assert(F32(TSURAMI_PTR(projectile,0x18),8)==15.f && U32(s_projectiles[1].actor.task,0xe8)==0x20);
    assert(anchor_tsurami_native_capture(&bad) && bad.projectile_count==2);
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(pool_count==4);assert(observed_group==10 && observed_opacity==82);
    s.projectile_count=0;assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();
    assert(!anchor_tsurami_native_projectile_task(1));assert(anchor_tsurami_native_capture(&bad)&&bad.projectile_count==0);
}
static void test_constructor_groups_and_cap(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s;unsigned int *p;unsigned int i;void *task=r->task_store.bytes;
    assert(anchor_tsurami_native_capture(&s));memset(s.projectile,0,sizeof(s.projectile));
    s.projectile_count=1;s.projectile_serial=1;s.tick=1;p=s.projectile[0];
    p[0]=1;p[1]=1;p[3]=16;p[TSU_PROJECTILE_SCALE_X]=p[TSU_PROJECTILE_SCALE_Y]=p[TSU_PROJECTILE_SCALE_Z]=float_bits(1.f);
    anchor_tsurami_native_set_role(1,0,0);assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(observed_group==5);
    p[0]=2;s.projectile_serial=2;p[2]=1;p[3]=0x80;p[TSU_PROJECTILE_AI]=2;p[TSU_PROJECTILE_OPACITY]=111;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(observed_group==8&&observed_opacity==111);
    p[0]=3;s.projectile_serial=3;p[3]=0;assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(observed_group==10&&observed_opacity==111);
    setup();
    for(i=0;i<33;i++) {
        Fixture *f=new_fixture();f->ai=func_08004ED0_6B8170;f->post=func_80218F30_5D4400;
        U16(f->task_store.bytes,0x5e)=0xcb;U32(f->task_store.bytes,0xe8)=2;
        register_projectile(f->task_store.bytes,0);
        assert(!!(U32(f->task_store.bytes,0x68)&2)==(i==32));
    }
    assert(anchor_tsurami_native_capture(&s)&&s.projectile_count==32);
    anchor_tsurami_native_set_role(0,0,0);
    {Fixture *f=new_fixture();f->ai=func_08004ED0_6B8170;f->post=func_80218F30_5D4400;U16(f->task_store.bytes,0x5e)=0xcb;U32(f->task_store.bytes,0xe8)=2;
     register_projectile(f->task_store.bytes,0);assert(!(U32(f->task_store.bytes,0x68)&2));
     anchor_tsurami_native_set_role(1,1,0);assert(U32(f->task_store.bytes,0x68)&2);}
}
static void test_colliding_ids_recreate_native_kind(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s;unsigned int *p;void *task=r->task_store.bytes,*old;
    assert(anchor_tsurami_native_capture(&s));memset(s.projectile,0,sizeof(s.projectile));
    s.projectile_count=1;s.projectile_serial=1;s.tick=1;p=s.projectile[0];
    p[0]=1;p[1]=1;p[3]=1;p[5]=20;p[TSU_PROJECTILE_SCALE_X]=p[TSU_PROJECTILE_SCALE_Y]=p[TSU_PROJECTILE_SCALE_Z]=float_bits(1.f);
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();
    old=find_projectile(1)->actor.task;assert(observed_group==6);
    p[3]=16;assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();
    assert(U32(old,0x68)&2);assert(find_projectile(1)->actor.task!=old&&observed_group==5);
    old=find_projectile(1)->actor.task;p[2]=1;p[3]=0x80;p[TSU_PROJECTILE_AI]=2;p[TSU_PROJECTILE_OPACITY]=120;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();
    assert(U32(old,0x68)&2);assert(find_projectile(1)->actor.task!=old&&find_projectile(1)->kind==1&&observed_group==8);
    old=find_projectile(1)->actor.task;p[3]=0x20;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();
    assert(U32(old,0x68)&2);assert(find_projectile(1)->actor.task!=old&&observed_group==10&&observed_opacity==60);
}
static void test_native_projectile_orientation_capture_and_adoption(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,out,bad;
    const unsigned int modes[]={1,2,4,8,16};
    const unsigned int invalid[]={1024,0x7fff,0x8001,0xffff};
    unsigned int i;void *task;
    for(i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
        task=func_8021DDE8_5D92B8(r->task_store.bytes,func_080049A4_6B7C44,
            modes[i]==16?5:6,15.f+i,25.f,35.f,0);
        U32(task,0xe8)=modes[i];TSURAMI_PTR(task,0xd4)=r->task_store.bytes;
        D_8016DAB4_16E6B4=task;
        func_080049A4_6B7C44(task,TSURAMI_PTR(task,0x18));
        anchor_tsurami_native_projectile();
    }
    /* These native constructor values used to reject the entire owner snapshot. */
    assert(anchor_tsurami_native_capture(&s));assert(s.projectile_count==5);
    for(i=0;i<s.projectile_count;i++) {
        assert(s.projectile[i][TSU_PROJECTILE_YAW]==0x8000);
        assert(s.projectile[i][TSU_PROJECTILE_PITCH]==0x8000);
    }
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        bad=s;bad.projectile[0][TSU_PROJECTILE_YAW]=invalid[i];assert(!snapshot_valid(&bad));
        bad=s;bad.projectile[0][TSU_PROJECTILE_PITCH]=invalid[i];assert(!snapshot_valid(&bad));
    }
    bad=s;bad.root[TSU_YAW]=0x8000;assert(!snapshot_valid(&bad));

    r=setup();anchor_tsurami_native_set_role(1,0,0);
    assert(anchor_tsurami_native_apply(&s));
    anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);anchor_tsurami_native_scheduler_end();
    assert(!s_pending_valid);assert(anchor_tsurami_native_capture(&out));
    assert(out.projectile_count==s.projectile_count);
    assert(memcmp(s.projectile,out.projectile,s.projectile_count*sizeof(s.projectile[0]))==0);
    for(i=0;i<s.projectile_count;i++) {
        TsuramiProjectile *projectile=find_projectile(s.projectile[i][TSU_PROJECTILE_ID]);
        assert(projectile);task=projectile->actor.task;
        assert(task && U16(TSURAMI_PTR(task,0x18),0x18)==0x8000);
    }
}
static void test_flash_and_terminal(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,out;void *task=r->task_store.bytes;
    assert(anchor_tsurami_native_capture(&s));s.root[TSU_PHASE]=29;s.root[TSU_FLASH]=256|160;
    anchor_tsurami_native_set_role(1,0,0);assert(anchor_tsurami_native_apply(&s));
    anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(flash_allocations==1&&U8(flash_pointer,0x10)==160);
    assert(anchor_tsurami_native_capture(&out)&&out.root[TSU_FLASH]==416);
    s.root[TSU_PHASE]=2;s.root[TSU_FLASH]=0;assert(anchor_tsurami_native_apply(&s));
    anchor_tsurami_native_adopt_before_pre(task);anchor_tsurami_native_scheduler_end();assert(flash_pointer==flash_data&&U8(flash_data,4)==0x81&&flash_releases==1);
    s.root[TSU_PHASE]=TSU_PHASE_TERMINAL;s.root[TSU_HP]=1;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);assert(terminal_calls==1);
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(task);assert(terminal_calls==1);
    U32(task,0x68)|=2;assert(anchor_tsurami_native_snapshot_ready());assert(anchor_tsurami_native_capture(&out));
    assert(out.root[TSU_PHASE]==TSU_PHASE_TERMINAL);anchor_tsurami_native_finish_terminal();assert(!anchor_tsurami_native_snapshot_ready());
}
static void test_native_ring_inherited_orientation(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,out;
    const unsigned int ring_flags[]={0,0x20,0x80,0x100};
    unsigned int i;void *shot,*ring;
    U16(r->ptr18,0x18)=77;
    shot=func_8021DDE8_5D92B8(r->task_store.bytes,func_080049A4_6B7C44,6,0,0,0,0);
    U32(shot,0xe8)=8;D_8016DAB4_16E6B4=shot;
    func_080049A4_6B7C44(shot,TSURAMI_PTR(shot,0x18));anchor_tsurami_native_projectile();
    for(i=0;i<sizeof(ring_flags)/sizeof(ring_flags[0]);i++) {
        ring=func_8021DDE8_5D92B8(shot,func_080056FC_6B899C,i<2?10:8,0,0,0,0);
        U32(ring,0xe8)=ring_flags[i];D_8016DAB4_16E6B4=ring;
        func_080056FC_6B899C(ring,TSURAMI_PTR(ring,0x18));anchor_tsurami_native_ring();
        assert(U16(TSURAMI_PTR(ring,0x18),0x18)==0x8000);
    }
    assert(anchor_tsurami_native_capture(&s));assert(s.projectile_count==5);
    r=setup();U16(r->ptr18,0x18)=91;anchor_tsurami_native_set_role(1,0,0);
    assert(anchor_tsurami_native_apply(&s));
    anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);anchor_tsurami_native_scheduler_end();
    assert(!s_pending_valid);assert(anchor_tsurami_native_capture(&out));
    assert(out.projectile_count==s.projectile_count);
    assert(memcmp(s.projectile,out.projectile,s.projectile_count*sizeof(s.projectile[0]))==0);
    for(i=1;i<s.projectile_count;i++) {
        TsuramiProjectile *projectile=find_projectile(s.projectile[i][TSU_PROJECTILE_ID]);
        assert(projectile && projectile->kind==1);
        assert(U16(projectile->actor.object,0x18)==0x8000);
    }
}
static void test_native_flash_free_does_not_publish_opaque_blue(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s;void *task=r->task_store.bytes;
    anchor_tsurami_native_flash_begin(task);
    func_8022026C_5DB73C(task,128,128,255);anchor_tsurami_native_flash_end();
    U8(flash_data,0x10)=255;assert(anchor_tsurami_native_capture(&s)&&s.root[TSU_FLASH]==511);
    assert(func_80220168_5DB638(task,80));assert(func_80220168_5DB638(task,80));
    assert(func_80220168_5DB638(task,80));assert(!func_80220168_5DB638(task,80));
    assert(flash_pointer==flash_data && U8(flash_data,4)==0x81 && U8(flash_data,0x10)==255);
    assert(anchor_tsurami_native_capture(&s)&&s.root[TSU_FLASH]==0);
}
static void test_flash_reuse_and_foreign_alias(void){
    Fixture *r=setup();AnchorTsuramiNativeSnapshot s,out;int releases;
    assert(anchor_tsurami_native_capture(&s));s.root[TSU_PHASE]=29;s.root[TSU_FLASH]=511;
    anchor_tsurami_native_set_role(1,0,0);
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);
    anchor_tsurami_native_scheduler_end();assert(owned_flash_live());
    /* Global cursor can change; release must address only our attached node. */
    memset(foreign_flash,0,sizeof(foreign_flash));U8(foreign_flash,4)=1;U8(foreign_flash,5)=11;
    U8(foreign_flash,0x10)=93;flash_pointer=foreign_flash;
    assert(anchor_tsurami_native_capture(&out)&&out.root[TSU_FLASH]==511);
    s.root[TSU_PHASE]=2;s.root[TSU_FLASH]=0;releases=flash_releases;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);
    anchor_tsurami_native_scheduler_end();assert(flash_releases==releases+1 && !s_flash);
    assert(flash_pointer==foreign_flash && U8(foreign_flash,0x10)==93);
    /* A foreign effect cannot stall the incoming boss phase or get replaced. */
    s.root[TSU_PHASE]=29;s.root[TSU_FLASH]=416;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);
    anchor_tsurami_native_scheduler_end();assert(!s_pending_valid && !s_flash && r->ai==func_08002EC8_6B6168);
    assert(flash_pointer==foreign_flash && U8(foreign_flash,0x10)==93);
    /* Handoff must not run global-alias fade code against a foreign effect. */
    anchor_tsurami_native_set_role(1,1,0);anchor_tsurami_native_scheduler_begin();
    assert(r->ai==func_08002F50_6B61F0 && U8(foreign_flash,0x10)==93);
    anchor_tsurami_native_scheduler_end();anchor_tsurami_native_set_role(1,0,0);
    /* A stale freed alias permits a fresh native allocation for another hit. */
    flash_pointer=flash_data;
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);
    anchor_tsurami_native_scheduler_end();assert(owned_flash_live()&&U8(flash_data,0x10)==160);
    anchor_tsurami_native_set_role(1,1,0);anchor_tsurami_native_scheduler_begin();
    assert(r->ai==func_08002EC8_6B6168);anchor_tsurami_native_scheduler_end();
    r->ai=func_08002F0C_6B61AC;flash_pointer=foreign_flash;releases=flash_releases;
    anchor_tsurami_native_scheduler_begin();anchor_tsurami_native_scheduler_end();
    assert(r->ai==func_08002F50_6B61F0 && flash_releases==releases+1);
    assert(!s_flash && U8(foreign_flash,0x10)==93);
    flash_pointer=flash_data;anchor_tsurami_native_set_role(1,0,0);
    assert(anchor_tsurami_native_apply(&s));anchor_tsurami_native_adopt_before_pre(r->task_store.bytes);
    anchor_tsurami_native_scheduler_end();assert(owned_flash_live());
    releases=flash_releases;anchor_tsurami_native_reset();
    assert(flash_releases==releases+1 && U8(flash_data,4)==0x81 && !s_flash);
    /* Pool address reuse cannot resurrect the effect after its release hook. */
    U8(flash_data,4)=1;U8(flash_data,5)=11;U8(flash_data,0x10)=255;
    assert(capture_flash()==0);U8(flash_data,4)=0x81;
}
int main(void){test_adoption_and_pause();test_hazards_and_validation();test_constructor_groups_and_cap();test_colliding_ids_recreate_native_kind();test_native_projectile_orientation_capture_and_adoption();test_native_ring_inherited_orientation();test_flash_and_terminal();test_native_flash_free_does_not_publish_opaque_blue();test_flash_reuse_and_foreign_alias();puts("Tsurami native checkpoint tests passed");return 0;}
#endif
