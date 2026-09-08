#include <stdlib.h>
#define ANCHOR_TSURAMI_REFLECTION_INTEGRATION
#include "test_anchor_tsurami_native.c"

void anchor_tsurami_damage_before_native(void *actor);
void anchor_tsurami_damage_scene_frame(void);
void *D_801FC604_5B8514;
static int reflections, returning_births;

void *reflection_read_pointer(const void *record,unsigned int offset)
{ return *fixture_pointer_slot((void *)record,offset); }
void reflection_write_pointer(void *record,unsigned int offset,void *value)
{ *fixture_pointer_slot(record,offset)=value; }
int anchor_player_models_get_epoch(void) {return 7;}
int anchor_remote_collision_is_scripted(void) {return 0;}
int mnsg_array_reserve(void **data,int *capacity,int needed,unsigned int size)
{
    void *next;
    if(needed<=*capacity)return 1;
    next=realloc(*data,(size_t)needed*size);if(!next)return 0;
    memset((char *)next+(size_t)*capacity*size,0,(size_t)(needed-*capacity)*size);
    *data=next;*capacity=needed;return 1;
}

/* 80218350 accepts normal contacts, reduces HP, installs +90 on the current
 * task, then immediately invokes that AI with the victim and its object.
 * Use the real damage hook and native projectile lookup/hold implementation. */
void func_80218350_5D3820(void *task)
{
    void *attacker;
    unsigned int amount,type,status;
    anchor_tsurami_damage_before_native(task);
    status=U32(task,0x68);attacker=TSURAMI_PTR(task,0x38);
    if(!(U32(task,0x60)&0x200000u) || !(status&0x80u) || !attacker)return;
    U32(task,0x68)&=~0x80u;
    if((U32(task,0x60)&0x400000u) || (status&1u))return;
    type=U8(attacker,0x4c);
    amount=type==0x22?8:type==0x17||type==0x24?4:type==0x1b?3:
           type==0x16||type==0x23?2:1;
    assert(amount<U8(task,0x8d));U8(task,0x8d)-=amount;U32(task,0x68)|=1u;
    if(fixture_for(task)->reaction) {
        TSURAMI_AI(D_8016DAB4_16E6B4)=fixture_for(task)->reaction;
        TSURAMI_AI(task)(task,TSURAMI_PTR(task,0x18));
    }
}
void func_80218F30_5D4400(void *task,void *object)
{
    (void)object;func_80218350_5D3820(task);
    TSURAMI_PTR(task,0x38)=0;
}
void func_08005B1C_6B8DBC(void *task,void *object)
{
    (void)task;(void)object;++reflections;
    TSURAMI_AI(D_8016DAB4_16E6B4)=func_08005B50_6B8DF0;
}
void func_08005B50_6B8DF0(void *task,void *object)
{
    void *root=TSURAMI_PTR(task,0xd4),*child;
    (void)object;
    child=func_8021DDE8_5D92B8(task,func_080049A4_6B7C44,5,0.f,0.f,0.f,0);
    U32(child,0xe8)=0x10;TSURAMI_PTR(child,0xd4)=root;
    D_8016DAB4_16E6B4=child;func_080049A4_6B7C44(child,TSURAMI_PTR(child,0x18));
    anchor_tsurami_native_projectile();D_8016DAB4_16E6B4=task;
    fixture_for(task)->backlink_ptr0=0;++returning_births;
}
static void native_pre(void *task)
{
    if(anchor_tsurami_native_is_root(task))anchor_tsurami_native_adopt_before_pre(task);
    /* 80218FE8 maps receive capability to task+30 and then converts a
     * collision's +38 pointer to the CONTACT bit before AI/post execution. */
    if(U32(task,0x60)&0x200000u)U8(task,0x30)|=1;
    if(TSURAMI_PTR(task,0x38) && U8(TSURAMI_PTR(task,0x38),0x4c)!=0x14)
        U32(task,0x68)|=0x80;
}
static void native_step(void *task)
{
    D_8016DAB4_16E6B4=task;native_pre(task);
    TSURAMI_AI(task)(task,TSURAMI_PTR(task,0x18));
    if(fixture_for(task)->backlink_ptr0)TSURAMI_POST(task)(task,TSURAMI_PTR(task,0x18));
}
static void set_damage_context(int owner)
{
    anchor_tsurami_native_set_role(1,owner,0);
    anchor_tsurami_damage_set_context(1,owner,0,42);
    anchor_tsurami_damage_scene_frame();
}
int main(void)
{
    AnchorTsuramiNativeSnapshot owner_state,out,flash_state;
    AnchorTsuramiHit intent;
    Fixture *root,*player;
    void *shot;
    unsigned int id;
    anchor_tsurami_damage_reset();root=setup();set_damage_context(1);
    shot=func_8021DDE8_5D92B8(root->task_store.bytes,func_080049A4_6B7C44,6,0,0,0,0);
    U32(shot,0xe8)=1;TSURAMI_PTR(shot,0xd4)=root->task_store.bytes;
    D_8016DAB4_16E6B4=shot;func_080049A4_6B7C44(shot,TSURAMI_PTR(shot,0x18));
    anchor_tsurami_native_projectile();id=anchor_tsurami_native_projectile_id(shot);
    assert(id&&anchor_tsurami_native_capture(&owner_state));

    /* Reconstruct a fresh follower from the owner's pointer-free snapshot. */
    anchor_tsurami_damage_reset();root=setup();set_damage_context(0);
    assert(anchor_tsurami_native_apply(&owner_state));native_step(root->task_store.bytes);
    shot=anchor_tsurami_native_projectile_task(id);assert(shot);
    player=new_fixture();D_801FC604_5B8514=player->task_store.bytes;D_801FC60C_5B851C=player->ptr18;
    U32(player->task_store.bytes,0x48)=1234;U8(player->task_store.bytes,0x4c)=0x15;
    U32(player->ptr18,0x2c)=27;F32(player->ptr18,0x28)=1.f;
    assert(TSURAMI_AI(shot)==hold_noop);
    TSURAMI_PTR(shot,0x38)=player->task_store.bytes;
    /* Native fade release leaves its global cursor at a freed kind1 record
     * with alpha255. An incoming flash checkpoint must still adopt gameplay
     * and release this shot's post, or reflection contacts never reach18350. */
    flash_pointer=flash_data;U8(flash_data,4)=0x81;U8(flash_data,5)=0;
    U8(flash_data,0x10)=255;assert(!s_flash);
    flash_state=owner_state;flash_state.root[TSU_PHASE]=29;
    flash_state.root[TSU_FLASH]=416;
    assert(anchor_tsurami_native_apply(&flash_state));native_step(root->task_store.bytes);
    assert(!s_pending_valid&&TSURAMI_POST(shot)==func_80218F30_5D4400);
    assert(owned_flash_live()&&U8(flash_data,0x10)==160);
    native_step(shot);
    assert(U8(shot,0x8d)==20&&reflections==0&&returning_births==0);
    assert(anchor_tsurami_damage_take_local_hit(&intent));
    assert(intent.target==id&&intent.amount==1);
    anchor_tsurami_native_scheduler_end();

    /* Owner receives the same ID; mapped root pre delivers its reaction,
     * and the next shot AI births the native mode16 return toward this root. */
    anchor_tsurami_damage_reset();root=setup();set_damage_context(1);
    assert(anchor_tsurami_native_apply(&owner_state));native_step(root->task_store.bytes);
    anchor_tsurami_native_scheduler_end();shot=anchor_tsurami_native_projectile_task(id);assert(shot);
    assert(anchor_tsurami_damage_apply(intent.amount,intent.target));
    native_step(root->task_store.bytes);
    assert(reflections==1&&TSURAMI_AI(shot)==func_08005B50_6B8DF0);
    native_step(shot);anchor_tsurami_native_scheduler_end();
    assert(returning_births==1&&observed_group==5);
    assert(!anchor_tsurami_native_projectile_task(id));
    assert(anchor_tsurami_native_capture(&out)&&out.projectile_count==1);
    assert(out.projectile[0][TSU_PROJECTILE_FLAGS]==0x10);
    assert(out.projectile[0][TSU_PROJECTILE_ID]!=id);
    assert(TSURAMI_PTR(find_projectile(out.projectile[0][TSU_PROJECTILE_ID])->actor.task,0xd4)==root->task_store.bytes);

    /* The following checkpoint retires the incoming shot on its follower
     * and reconstructs the returning shot with the follower's local root. */
    owner_state=out;anchor_tsurami_damage_reset();root=setup();set_damage_context(0);
    assert(anchor_tsurami_native_apply(&owner_state));native_step(root->task_store.bytes);
    anchor_tsurami_native_scheduler_end();
    assert(!anchor_tsurami_native_projectile_task(id));
    assert(anchor_tsurami_native_capture(&out)&&out.projectile_count==1);
    assert(out.projectile[0][TSU_PROJECTILE_FLAGS]==0x10);
    shot=find_projectile(out.projectile[0][TSU_PROJECTILE_ID])->actor.task;
    assert(TSURAMI_PTR(shot,0xd4)==root->task_store.bytes);
    assert(reflections==1&&returning_births==1);
    puts("Tsurami reflection native/damage integration passed");return 0;
}
