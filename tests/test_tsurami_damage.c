#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANCHOR_TSURAMI_DAMAGE_HOST_TEST
static void *read_pointer(const void *record, unsigned int offset)
{
    void *value;
    memcpy(&value, (const unsigned char *)record + offset, sizeof(value));
    return value;
}
static void write_pointer(void *record, unsigned int offset, void *value)
{
    memcpy((unsigned char *)record + offset, &value, sizeof(value));
}
#define TSURAMI_READ_POINTER(p, off) read_pointer(p, off)
#define TSURAMI_WRITE_POINTER(p, off, value) write_pointer(p, off, value)
#include "../src/anchor_tsurami_damage.c"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #test); return 1; \
} } while (0)

unsigned short D_800C7AB2;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
void *D_8016DAB4_16E6B4;
static union { void *alignment; unsigned char bytes[8192]; } s_storage;
static unsigned char *s_boss;
static unsigned char *s_player;
static unsigned char *s_projectile;
static unsigned char *s_other;
static int s_epoch;
static int s_scripted;
static int s_allocation_fails;
static int s_native_calls;
static int s_native_context_correct;
static int s_reentrant_result;
static int s_native_deaths;
static unsigned int s_reflect_id;
static int s_native_returning;
static int s_reflection_callbacks;
#define TRAVELLING_AI 0x08004ed0u
#define REFLECTION_REACTION 0x08005b1cu
#define REFLECTION_AI 0x08005b50u
unsigned int anchor_tsurami_native_projectile_id(const void *task)
{return task==s_projectile && WORD(task,0x0c)==TRAVELLING_AI ? s_reflect_id:0;}
void *anchor_tsurami_native_projectile_task(unsigned int id)
{return id&&id==anchor_tsurami_native_projectile_id(s_projectile)?s_projectile:0;}
int anchor_tsurami_native_is_projectile(const void *task)
{return task==s_projectile&&(s_reflect_id||s_native_returning);}


int anchor_remote_model_pool_contains(const void *pointer)
{
    unsigned long p = (unsigned long)pointer;
    unsigned long begin = (unsigned long)s_storage.bytes;
    return p >= begin && p < begin + sizeof(s_storage.bytes);
}
int anchor_player_models_get_epoch(void) { return s_epoch; }
int anchor_remote_collision_is_scripted(void) { return s_scripted; }
int mnsg_array_reserve(void **data, int *capacity, int needed, unsigned int size)
{
    void *next;
    if (needed <= *capacity) return 1;
    if (s_allocation_fails) return 0;
    next = realloc(*data, (size_t)needed * size);
    if (!next) return 0;
    memset((char *)next + (size_t)*capacity * size, 0,
           (size_t)(needed - *capacity) * size);
    *data = next;
    *capacity = needed;
    return 1;
}

/* Native 80218350 semantics relevant to Tsurami: the synthetic branch bypasses
 * gates, normal hits use attacker+4C, and accepted hits invoke +90 immediately
 * through the current task's AI slot (802184C0..802184F4). Tsurami's 05B1C
 * reaction installs 05B50 there for the next scheduler tick (08005B30..38).
 * Calling the real entry hook here also tests that scoped owner delivery is
 * not accidentally captured and suppressed as another local physical hit. */
void func_80218350_5D3820(void *actor)
{
    unsigned int status;
    void *attacker;
    int amount;
    anchor_tsurami_damage_before_native(actor);
    ++s_native_calls;
    status = WORD(actor, 0x68);
    attacker = TSURAMI_READ_POINTER(actor, 0x38);
    if (s_applying)
    {
        s_native_context_correct = D_8016DAB4_16E6B4 == actor &&
            attacker && attacker != s_player && !(status & SYNTHETIC_DAMAGE);
        s_reentrant_result = anchor_tsurami_damage_apply(1,0);
    }
    if (status & SYNTHETIC_DAMAGE)
        amount = 1;
    else
    {
        if (!(WORD(actor, 0x60) & CAN_RECEIVE_DAMAGE) ||
            !(status & CONTACT) || !attacker)
            return;
        WORD(actor, 0x68) &= ~CONTACT;
        if (WORD(actor, 0x60) & DEFLECT_DAMAGE)
        {
            WORD(actor, 0x68) |= RECOVERY;
            return;
        }
        if (status & RECOVERY) return;
        amount = attack_amount(BYTE(attacker, 0x4c));
    }
    BYTE(actor, 0x8d) = amount >= BYTE(actor, 0x8d) ? 0 :
                       BYTE(actor, 0x8d) - amount;
    WORD(actor, 0x68) |= RECOVERY;
    if (BYTE(actor, 0x8d))
    {
        if (WORD(actor, 0x90)==REFLECTION_REACTION)
        {
            WORD(D_8016DAB4_16E6B4, 0x0c)=REFLECTION_REACTION;
            if (WORD(actor, 0x0c)==REFLECTION_REACTION)
            {
                ++s_reflection_callbacks;
                WORD(D_8016DAB4_16E6B4, 0x0c)=REFLECTION_AI;
            }
        }
        else BYTE(actor, 0x8c) = 180;
    }
    else ++s_native_deaths;
}

static void initialize_task(unsigned char *task, unsigned char *object,
                            unsigned char *backlink)
{
    write_pointer(task, 4, backlink);
    write_pointer(backlink, 0, task);
    write_pointer(task, 0x18, object);
    object[4] = 2;
    WORD(task, 0x48) = 1234;
    BYTE(task, 0x30) = 2;
    WORD(object, 0x2c) = 27;
    FLOAT(object, 0x28) = 1.0f;
}

static void reset_case(int owner)
{
    anchor_tsurami_damage_reset();
    memset(s_storage.bytes, 0, sizeof(s_storage.bytes));
    s_boss = s_storage.bytes;
    s_player = s_storage.bytes + 1024;
    s_projectile = s_storage.bytes + 2048;
    s_other = s_storage.bytes + 3072;
    initialize_task(s_boss, s_boss + 256, s_boss + 512);
    initialize_task(s_player, s_player + 256, s_player + 512);
    initialize_task(s_projectile, s_projectile + 256, s_projectile + 512);
    initialize_task(s_other, s_other + 256, s_other + 512);
    D_800C7AB2 = TSURAMI_ROOM;
    D_801FC604_5B8514 = s_player;
    D_801FC60C_5B851C = s_player + 256;
    D_8016DAB4_16E6B4 = s_other;
    HALF(s_boss, 0x5c) = TSURAMI_ACTOR;
    WORD(s_boss, 0xe8) = ROOT_HEALTH_CONTROLLER;
    WORD(s_boss, 0x60) = CAN_RECEIVE_DAMAGE | 0x6e1;
    WORD(s_boss, 0x64) = 0x8000;
    BYTE(s_boss, 0x8d) = 12;
    BYTE(s_player, 0x4c) = 0x15;
    write_pointer(s_projectile, 0x5c, s_player);
    BYTE(s_projectile, 0x4c) = 0x22;
    s_epoch = 7;
    s_scripted = s_allocation_fails = s_native_calls = 0;
    s_native_deaths = s_native_context_correct = 0;
    s_reentrant_result = -1;
    s_reflect_id=0;s_native_returning=0;s_reflection_callbacks=0;
    anchor_tsurami_damage_bind_root(s_boss);
    anchor_tsurami_damage_set_context(1, owner, 0, 42);
    anchor_tsurami_damage_scene_frame();
}

static void contact(void *attacker)
{
    write_pointer(attacker, 0x34, s_boss);
    write_pointer(s_boss, 0x38, attacker);
    WORD(s_boss, 0x68) |= CONTACT;
    func_80218350_5D3820(s_boss);
}

static void reflectable_projectile(unsigned int id)
{
    s_reflect_id=id;
    BYTE(s_projectile,0x8d)=20;
    WORD(s_projectile,0x60)=CAN_RECEIVE_DAMAGE;
    WORD(s_projectile,0x64)=NO_GENERIC_DROPS;
    WORD(s_projectile,0x0c)=TRAVELLING_AI;
    WORD(s_projectile,0x90)=REFLECTION_REACTION;
}

static int test_contacts_and_reflection(void)
{
    AnchorTsuramiHit hit;
    int owner;
    for(owner=0;owner<=1;++owner)
    {
        reset_case(owner);
        contact(s_player);
        CHECK(BYTE(s_boss,0x8d)==12);
        CHECK(anchor_tsurami_damage_take_local_hit(&hit));
        CHECK(hit.amount==1&&hit.target==0&&hit.sequence>0);
        contact(s_player);CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
        CHECK(!(WORD(s_boss,0x68)&CONTACT));
        CHECK(read_pointer(s_player,0x34)==s_boss);

        /* The same swing can reflect a distinct native projectile. */
        reflectable_projectile(81);
        write_pointer(s_projectile,0x38,s_player);
        WORD(s_projectile,0x68)=CONTACT;
        func_80218350_5D3820(s_projectile);
        CHECK(BYTE(s_projectile,0x8d)==20);
        CHECK(anchor_tsurami_damage_take_local_hit(&hit));
        CHECK(hit.target==81&&hit.amount==1);
        write_pointer(s_projectile,0x38,s_player);
        WORD(s_projectile,0x68)=CONTACT;
        func_80218350_5D3820(s_projectile);
        CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    }
    return 0;
}
static int test_mapped_delivery_and_strength(void)
{
    int amounts[]={1,2,3,4,8}; unsigned i;
    for(i=0;i<sizeof(amounts)/sizeof(*amounts);++i)
    {
        reset_case(1);
        CHECK(anchor_tsurami_damage_apply(amounts[i],0));
        CHECK(s_native_calls==0&&BYTE(s_boss,0x8d)==12);
        anchor_tsurami_damage_flush();
        CHECK(BYTE(s_boss,0x8d)==12-amounts[i]);
        CHECK(s_native_calls==1&&s_native_context_correct&&s_reentrant_result==0);
        CHECK(D_8016DAB4_16E6B4==s_other);
        CHECK(anchor_tsurami_damage_apply(1,0));anchor_tsurami_damage_flush();
        CHECK(s_native_calls==1); /* native recovery refuses another hit */
    }
    reset_case(1);BYTE(s_boss,0x8d)=4;
    CHECK(anchor_tsurami_damage_apply(8,0));anchor_tsurami_damage_flush();
    CHECK(BYTE(s_boss,0x8d)==1&&s_native_deaths==0);
    CHECK(!anchor_tsurami_damage_apply(1,0)&&!anchor_tsurami_damage_pending());
    reset_case(1);CHECK(anchor_tsurami_damage_apply(1,0));
    anchor_tsurami_damage_discard_pending();
    CHECK(!anchor_tsurami_damage_pending());
    reset_case(1);reflectable_projectile(81);
    CHECK(anchor_tsurami_damage_apply(4,81));anchor_tsurami_damage_flush();
    CHECK(BYTE(s_projectile,0x8d)==16&&BYTE(s_boss,0x8d)==12);
    CHECK(s_reflection_callbacks==1&&WORD(s_projectile,0x0c)==REFLECTION_AI);
    CHECK(!anchor_tsurami_native_projectile_id(s_projectile));
    CHECK(!anchor_tsurami_native_projectile_task(81));
    CHECK(D_8016DAB4_16E6B4==s_other);
    CHECK(WORD(s_boss,0x0c)==0&&WORD(s_other,0x0c)==0);
    CHECK(anchor_tsurami_damage_apply(4,81));anchor_tsurami_damage_flush();
    CHECK(s_native_calls==1&&s_reflection_callbacks==1);
    CHECK(anchor_tsurami_damage_apply(1,82));anchor_tsurami_damage_flush();
    CHECK(s_native_calls==1); /* stale projectile ID is not relabelled */
    return 0;
}
static int test_identity_pause_and_epoch(void)
{
    AnchorTsuramiHit hit;unsigned i;
    reset_case(0);CHECK(!anchor_tsurami_damage_apply(1,0));
    reset_case(1);anchor_tsurami_damage_set_context(1,1,1,42);
    CHECK(!anchor_tsurami_damage_apply(1,0));contact(s_player);
    CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(1);CHECK(anchor_tsurami_damage_apply(1,0));
    anchor_tsurami_damage_set_context(1,0,0,42);
    anchor_tsurami_damage_set_context(1,1,0,42);
    anchor_tsurami_damage_flush();CHECK(s_native_calls==0);
    reset_case(1);contact(s_player);++s_epoch;
    CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(1);BYTE(s_boss,0x74)++;
    CHECK(!anchor_tsurami_damage_is_root(s_boss));
    reset_case(1);contact(s_other);
    CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(1);BYTE(s_player,0x4c)=0x1a;contact(s_player);
    CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(1);BYTE(s_player,0x4c)=0x1c;contact(s_player);
    CHECK(!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(1);
    for(i=0;i<32;++i)CHECK(anchor_tsurami_damage_apply(1,0));
    CHECK(!anchor_tsurami_damage_apply(1,0));
    CHECK(!anchor_tsurami_damage_apply(9,0));
    CHECK(!anchor_tsurami_damage_apply(1,0x80000000u));
    return 0;
}
static int test_native_owner_hazards_and_terminal(void)
{
    AnchorTsuramiHit hit;
    reset_case(1);s_native_returning=1;BYTE(s_projectile,0x4c)=1;
    contact(s_projectile);
    CHECK(BYTE(s_boss,0x8d)==11&&!anchor_tsurami_damage_take_local_hit(&hit));
    reset_case(0);s_native_returning=1;BYTE(s_projectile,0x4c)=1;
    contact(s_projectile);
    CHECK(BYTE(s_boss,0x8d)==12);
    reset_case(0);BYTE(s_boss,0x8d)=2;
    WORD(s_boss,0x68)=SYNTHETIC_DAMAGE;
    func_80218350_5D3820(s_boss);
    CHECK(BYTE(s_boss,0x8d)==1&&s_native_deaths==0);
    return 0;
}
int main(void)
{
    if(test_contacts_and_reflection()||test_mapped_delivery_and_strength()||
       test_identity_pause_and_epoch()||test_native_owner_hazards_and_terminal())return 1;
    puts("Tsurami damage tests passed");return 0;
}
