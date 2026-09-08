#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*TestDamageCallback)(void *, void *);

typedef struct DamageFixture
{
    union { void *alignment; unsigned char bytes[0x200]; } task_store;
    union { void *alignment; unsigned char bytes[0x200]; } object_store;
    union { void *alignment; unsigned char bytes[0x20]; } backlink_store;
    TestDamageCallback ai;
    TestDamageCallback post;
    void *ptr04;
    void *ptr18;
    void *ptr38;
    void *ptr5c;
    void *ptrdc;
    void *backlink_ptr0;
} DamageFixture;

#define FIXTURE_COUNT 8
static DamageFixture fixtures[FIXTURE_COUNT];

static DamageFixture *fixture_for(const void *task)
{
    unsigned int index;
    for (index = 0; index < FIXTURE_COUNT; ++index)
        if (task == fixtures[index].task_store.bytes)
            return &fixtures[index];
    return 0;
}

static void **pointer_slot(void *record, unsigned int offset)
{
    unsigned int index;
    DamageFixture *fixture;
    for (index = 0; index < FIXTURE_COUNT; ++index)
    {
        fixture = &fixtures[index];
        if (record == fixture->backlink_store.bytes && offset == 0)
            return &fixture->backlink_ptr0;
        if (record != fixture->task_store.bytes)
            continue;
        if (offset == 0x04) return &fixture->ptr04;
        if (offset == 0x18) return &fixture->ptr18;
        if (offset == 0x38) return &fixture->ptr38;
        if (offset == 0x5c) return &fixture->ptr5c;
        if (offset == 0xdc) return &fixture->ptrdc;
    }
    assert(!"unexpected pointer slot");
    return &fixtures[0].ptr04;
}

static void *read_pointer(const void *record, unsigned int offset)
{
    return *pointer_slot((void *)record, offset);
}

static void write_pointer(void *record, unsigned int offset, void *value)
{
    *pointer_slot(record, offset) = value;
}

#define DHARUMANYO_DAMAGE_READ_POINTER(p, o) read_pointer((p), (o))
#define DHARUMANYO_DAMAGE_WRITE_POINTER(p, o, value) \
    write_pointer((p), (o), (value))
#define DHARUMANYO_DAMAGE_AI(p) (fixture_for(p)->ai)
#define DHARUMANYO_DAMAGE_POST(p) (fixture_for(p)->post)
#define DHARUMANYO_DAMAGE_CALLBACKS_DISABLED(p) 0
#define ANCHOR_DHARUMANYO_DAMAGE_HOST_TEST
#include "../src/anchor_dharumanyo_damage.c"

unsigned short D_800C7AB2;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
void *D_8016DAB4_16E6B4;
static int player_epoch;
static int scripted;
static int allocation_fails;
static int native_calls;

int anchor_remote_model_pool_contains(const void *pointer)
{
    const unsigned char *p = pointer;
    const unsigned char *begin = (const unsigned char *)fixtures;
    const unsigned char *end = begin + sizeof(fixtures);
    return p >= begin && p < end;
}

int anchor_player_models_get_epoch(void)
{
    return player_epoch;
}

int anchor_remote_collision_is_scripted(void)
{
    return scripted;
}

int mnsg_array_reserve(void **data, int *capacity, int needed,
                       unsigned int element_size)
{
    void *next;
    if (needed <= *capacity)
        return 1;
    if (allocation_fails)
        return 0;
    next = realloc(*data, (size_t)needed * element_size);
    if (!next)
        return 0;
    memset((unsigned char *)next + (size_t)*capacity * element_size, 0,
           (size_t)(needed - *capacity) * element_size);
    *data = next;
    *capacity = needed;
    return 1;
}

void func_08003F84_6CC194(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void carrier_ai(void *task, void *object)
{
    (void)task;
    (void)object;
}

void func_80218350_5D3820(void *actor)
{
    unsigned int status;
    void *attacker;
    anchor_dharumanyo_damage_before_native(actor);
    ++native_calls;
    status = WORD(actor, 0x68);
    if (status & SYNTHETIC_DAMAGE)
    {
        if (BYTE(actor, 0x8d))
            --BYTE(actor, 0x8d);
        WORD(actor, 0x68) |= RECOVERY;
        return;
    }
    attacker = read_pointer(actor, 0x38);
    if ((status & CONTACT) && attacker &&
        (WORD(actor, 0x60) & CAN_RECEIVE_DAMAGE) &&
        !(WORD(actor, 0x60) & DEFLECT_DAMAGE) &&
        !(status & RECOVERY))
    {
        if (BYTE(actor, 0x8d))
            --BYTE(actor, 0x8d);
        WORD(actor, 0x68) |= RECOVERY;
    }
}

static void initialize_fixture(DamageFixture *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->ptr04 = fixture->backlink_store.bytes;
    fixture->backlink_ptr0 = fixture->task_store.bytes;
    fixture->ptr18 = fixture->object_store.bytes;
    BYTE(fixture->object_store.bytes, 4) = 2;
    WORD(fixture->task_store.bytes, 0x48) = 1234;
    BYTE(fixture->task_store.bytes, 0x30) = 2;
    WORD(fixture->object_store.bytes, 0x2c) = 7;
    FLOAT(fixture->object_store.bytes, 0x28) = 1.0f;
}

static void reset_case(int owner)
{
    DamageFixture *root = &fixtures[0];
    DamageFixture *carrier = &fixtures[1];
    DamageFixture *player = &fixtures[2];
    unsigned int index;
    anchor_dharumanyo_damage_reset();
    for (index = 0; index < FIXTURE_COUNT; ++index)
        initialize_fixture(&fixtures[index]);
    D_800C7AB2 = 0x49;
    D_801FC604_5B8514 = player->task_store.bytes;
    D_801FC60C_5B851C = player->object_store.bytes;
    D_8016DAB4_16E6B4 = fixtures[7].task_store.bytes;
    HALF(root->task_store.bytes, 0x5c) = 0xcc;
    HALF(carrier->task_store.bytes, 0x5c) = 0xcc;
    HALF(carrier->task_store.bytes, 0x5e) = 0xcc;
    root->ptrdc = carrier->task_store.bytes;
    carrier->ptrdc = root->task_store.bytes;
    carrier->ai = carrier_ai;
    carrier->post = func_08003F84_6CC194;
    WORD(carrier->task_store.bytes, 0x60) = CAN_RECEIVE_DAMAGE | 0x2a0u;
    BYTE(carrier->task_store.bytes, 0x8d) = 10;
    BYTE(carrier->task_store.bytes, 0xd1) = 12;
    BYTE(player->task_store.bytes, 0x4c) = 0x22;
    player_epoch = 3;
    scripted = 0;
    allocation_fails = 0;
    native_calls = 0;
    anchor_dharumanyo_damage_bind(root->task_store.bytes,
                                   carrier->task_store.bytes);
    anchor_dharumanyo_damage_set_context(1, owner, 0, 77);
    anchor_dharumanyo_damage_scene_frame();
}

static void contact(DamageFixture *attacker)
{
    DamageFixture *carrier = &fixtures[1];
    carrier->ptr38 = attacker->task_store.bytes;
    WORD(carrier->task_store.bytes, 0x68) |= CONTACT;
    func_80218350_5D3820(carrier->task_store.bytes);
}

static void physical_hit_test(void)
{
    AnchorDharumanyoHit hit;
    int owner;
    for (owner = 0; owner <= 1; ++owner)
    {
        reset_case(owner);
        contact(&fixtures[2]);
        assert(BYTE(fixtures[1].task_store.bytes, 0x8d) == 10);
        assert(!(WORD(fixtures[1].task_store.bytes, 0x68) & CONTACT));
        assert(fixtures[1].ptr38 == 0);
        assert(anchor_dharumanyo_damage_take_local_hit(&hit));
        assert(hit.sequence > 0 && hit.amount == 1);
        contact(&fixtures[2]);
        assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
    }
    reset_case(0);
    fixtures[3].ptr5c = fixtures[2].task_store.bytes;
    contact(&fixtures[3]);
    assert(anchor_dharumanyo_damage_take_local_hit(&hit));
    assert(hit.amount == 1);
    reset_case(0);
    contact(&fixtures[4]);
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
    assert(!(WORD(fixtures[1].task_store.bytes, 0x68) & CONTACT));
}

static void authoritative_delivery_test(void)
{
    void *previous;
    reset_case(1);
    previous = D_8016DAB4_16E6B4;
    assert(anchor_dharumanyo_damage_is_owner());
    assert(anchor_dharumanyo_damage_is_carrier(fixtures[1].task_store.bytes));
    assert(can_take_damage(fixtures[1].task_store.bytes));
    assert(anchor_dharumanyo_damage_apply(1));
    assert(BYTE(fixtures[1].task_store.bytes, 0x8d) == 1);
    assert(WORD(fixtures[1].task_store.bytes, 0x68) & RECOVERY);
    assert(!(WORD(fixtures[1].task_store.bytes, 0x68) & SYNTHETIC_DAMAGE));
    assert(D_8016DAB4_16E6B4 == previous);
    assert(!anchor_dharumanyo_damage_apply(1));
    reset_case(0);
    assert(!anchor_dharumanyo_damage_apply(1));
    reset_case(1);
    assert(!anchor_dharumanyo_damage_apply(2));
    HALF(fixtures[1].task_store.bytes, 0xd6) = 1;
    assert(!anchor_dharumanyo_damage_apply(1));
    HALF(fixtures[1].task_store.bytes, 0xd6) = 0;
    BYTE(fixtures[1].task_store.bytes, 0xd1) = 0;
    assert(!anchor_dharumanyo_damage_apply(1));
}

static void pause_synthetic_and_offline_test(void)
{
    AnchorDharumanyoHit hit;
    reset_case(1);
    anchor_dharumanyo_damage_set_context(1, 1, 1, 77);
    contact(&fixtures[2]);
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
    assert(BYTE(fixtures[1].task_store.bytes, 0x8d) == 10);
    reset_case(1);
    scripted = 1;
    contact(&fixtures[2]);
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
    reset_case(1);
    WORD(fixtures[1].task_store.bytes, 0x68) = SYNTHETIC_DAMAGE;
    func_80218350_5D3820(fixtures[1].task_store.bytes);
    assert(BYTE(fixtures[1].task_store.bytes, 0x8d) == 9);
    assert(WORD(fixtures[1].task_store.bytes, 0x68) & SYNTHETIC_DAMAGE);
    reset_case(1);
    anchor_dharumanyo_damage_set_context(0, 0, 0, 0);
    contact(&fixtures[2]);
    assert(BYTE(fixtures[1].task_store.bytes, 0x8d) == 9);
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
}

static void queue_and_identity_test(void)
{
    AnchorDharumanyoHit hit;
    unsigned int index;
    reset_case(0);
    for (index = 0; index < HIT_QUEUE_SIZE + 2u; ++index)
    {
        s_capture_cooldown = 0;
        WORD(fixtures[2].object_store.bytes, 0x2c) = index + 20;
        contact(&fixtures[2]);
    }
    for (index = 0; index < HIT_QUEUE_SIZE; ++index)
        assert(anchor_dharumanyo_damage_take_local_hit(&hit));
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
    reset_case(0);
    ++BYTE(fixtures[1].task_store.bytes, 0x74);
    assert(!anchor_dharumanyo_damage_is_carrier(
        fixtures[1].task_store.bytes));
    contact(&fixtures[2]);
    assert(!anchor_dharumanyo_damage_take_local_hit(&hit));
}

int main(void)
{
    physical_hit_test();
    authoritative_delivery_test();
    pause_synthetic_and_offline_test();
    queue_and_identity_test();
    free(s_episodes);
    puts("Dharumanyo unit-hit authority and native intake contracts passed");
    return 0;
}
