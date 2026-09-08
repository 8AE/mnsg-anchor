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
    void *ptr04;
    void *ptr18;
    void *ptr2c;
    void *ptr5c;
    void *ptr84;
    void *ptrdc;
    void *backlink_ptr0;
} Fixture;

#define TEST_CAPACITY 48
static Fixture pool[TEST_CAPACITY];
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
    for (index = 0; index < pool_count; ++index)
    {
        fixture = &pool[index];
        if (record == fixture->backlink_store.bytes && offset == 0)
            return &fixture->backlink_ptr0;
        if (record != fixture->task_store.bytes)
            continue;
        if (offset == 0x04) return &fixture->ptr04;
        if (offset == 0x18) return &fixture->ptr18;
        if (offset == 0x2c) return &fixture->ptr2c;
        if (offset == 0x5c) return &fixture->ptr5c;
        if (offset == 0x84) return &fixture->ptr84;
        if (offset == 0xdc) return &fixture->ptrdc;
    }
    assert(!"unexpected pointer slot");
    return &pool[0].ptr04;
}

#define DHARUMANYO_PTR(p, o) \
    (*fixture_pointer_slot((void *)(p), (o)))
#define DHARUMANYO_AI(p) (fixture_for(p)->ai)
#define DHARUMANYO_POST(p) (fixture_for(p)->post)
/* The native tag bit can occur naturally in a host function address. */
#define CALLBACK_DISABLED (1ul << (sizeof(unsigned long) * 8 - 1))
#define ANCHOR_DHARUMANYO_NATIVE_HOST_TEST
#include "../src/anchor_dharumanyo_native.c"

unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4;
unsigned char D_8015CC30_15D830[0x300];
static unsigned int current_visit;
static int native_pause;
static int terminal_accept;
static int terminal_calls;
static int damage_binds;
static float observed_target_x;
static float observed_target_z;
static unsigned char resource_byte;

int anchor_remote_model_pool_contains(const void *pointer)
{
    const unsigned char *p = pointer;
    const unsigned char *begin = (const unsigned char *)pool;
    const unsigned char *end = begin + sizeof(pool);
    return p >= begin && p < end;
}

unsigned int anchor_boss_invite_world_visit(void)
{
    return current_visit;
}

void anchor_dharumanyo_damage_bind(void *root, void *carrier)
{
    (void)root;
    (void)carrier;
    ++damage_binds;
}

int boss_sync_queue_darumanyo_shared_terminal(void)
{
    ++terminal_calls;
    return terminal_accept;
}

int func_800240DC_24CDC(int flag_id)
{
    return flag_id == 0x16c && native_pause;
}

void *func_800141C4_14DC4(unsigned int file)
{
    return file == 0x1f ? &resource_byte : (void *)-1;
}

float func_8001B5AC_1C1AC(void *object)
{
    (void)object;
    return 100.0f;
}

void func_8021664C_5D1B1C(void *task, unsigned int clip, float rate,
                          unsigned int flags)
{
    (void)rate;
    (void)flags;
    if (task == s_root.task)
        s_root_clip = clip;
}

static Fixture *new_fixture(void)
{
    Fixture *fixture = &pool[pool_count++];
    memset(fixture, 0, sizeof(*fixture));
    DHARUMANYO_PTR(fixture->task_store.bytes, 0x04) =
        fixture->backlink_store.bytes;
    DHARUMANYO_PTR(fixture->backlink_store.bytes, 0) =
        fixture->task_store.bytes;
    DHARUMANYO_PTR(fixture->task_store.bytes, 0x18) =
        fixture->object_store.bytes;
    U16(fixture->task_store.bytes, 0x5c) = 0xcc;
    U8(fixture->task_store.bytes, 0x74) = (unsigned char)pool_count;
    F32(fixture->object_store.bytes, 0x1c) = 1.0f;
    F32(fixture->object_store.bytes, 0x20) = 1.0f;
    F32(fixture->object_store.bytes, 0x24) = 1.0f;
    return fixture;
}

void *func_8021DDE8_5D92B8(void *parent, TestCallback initializer,
                           unsigned char priority, float x, float y, float z,
                           int flags)
{
    Fixture *fixture;
    (void)priority;
    (void)flags;
    assert(pool_count < TEST_CAPACITY);
    fixture = new_fixture();
    fixture->ai = initializer;
    F32(fixture->object_store.bytes, 8) = x;
    F32(fixture->object_store.bytes, 12) = y;
    F32(fixture->object_store.bytes, 16) = z;
    DHARUMANYO_PTR(fixture->task_store.bytes, 0xdc) = parent;
    return fixture->task_store.bytes;
}

#define EMPTY_CALLBACK(name) \
    void name(void *task, void *object) { (void)task; (void)object; }
EMPTY_CALLBACK(func_08000104_6C8314)
EMPTY_CALLBACK(func_08000970_6C8B80)
void func_08000A0C_6C8C1C(void *task, void *object)
{
    /* Observable AI work for the scheduler-order regression. */
    ++U16(task, 0x8a);
    (void)object;
}
EMPTY_CALLBACK(func_08000AB0_6C8CC0)
EMPTY_CALLBACK(func_08000BD8_6C8DE8)
EMPTY_CALLBACK(func_08000CCC_6C8EDC)
EMPTY_CALLBACK(func_08000E98_6C90A8)
EMPTY_CALLBACK(func_08000F70_6C9180)
EMPTY_CALLBACK(func_080010A0_6C92B0)
EMPTY_CALLBACK(func_08001148_6C9358)
EMPTY_CALLBACK(func_080011C8_6C93D8)
EMPTY_CALLBACK(func_08001260_6C9470)
EMPTY_CALLBACK(func_08001360_6C9570)
EMPTY_CALLBACK(func_08001540_6C9750)
EMPTY_CALLBACK(func_08001618_6C9828)
EMPTY_CALLBACK(func_08001730_6C9940)
EMPTY_CALLBACK(func_080017B8_6C99C8)
EMPTY_CALLBACK(func_08001810_6C9A20)
EMPTY_CALLBACK(func_080018B4_6C9AC4)
EMPTY_CALLBACK(func_0800195C_6C9B6C)
EMPTY_CALLBACK(func_08001A24_6C9C34)
EMPTY_CALLBACK(func_08001B1C_6C9D2C)
EMPTY_CALLBACK(func_08001C1C_6C9E2C)
EMPTY_CALLBACK(func_08001CC8_6C9ED8)
EMPTY_CALLBACK(func_08001E4C_6CA05C)
EMPTY_CALLBACK(func_08001F08_6CA118)
EMPTY_CALLBACK(func_080020B4_6CA2C4)
EMPTY_CALLBACK(func_0800218C_6CA39C)
EMPTY_CALLBACK(func_080022BC_6CA4CC)
EMPTY_CALLBACK(func_08002380_6CA590)
EMPTY_CALLBACK(func_08002410_6CA620)
EMPTY_CALLBACK(func_080025C8_6CA7D8)
EMPTY_CALLBACK(func_0800284C_6CAA5C)
EMPTY_CALLBACK(func_08002A40_6CAC50)
void func_08003810_6CBA20(void *task, void *object)
{
    anchor_dharumanyo_native_observe_post(task);
    /* Native 08003844-0800388C mirrors this script gate each post pass. */
    if (native_pause)
        U32(D_8015CC30_15D830, 0xd4) |= 1u;
    else
        U32(D_8015CC30_15D830, 0xd4) &= ~1u;
    (void)object;
}
EMPTY_CALLBACK(func_08003F78_6CC188)
EMPTY_CALLBACK(func_08003F84_6CC194)
EMPTY_CALLBACK(func_0800432C_6CC53C)
EMPTY_CALLBACK(func_80218F30_5D4400)

void func_08002620_6CA830(void *task, void *object)
{
    void *target;
    anchor_dharumanyo_native_projectile_begin(task);
    U16(task, 0x5e) = 0xcc;
    fixture_for(task)->ai = U8(task, 0xd3)
                                ? func_08002A40_6CAC50
                                : func_0800284C_6CAA5C;
    fixture_for(task)->post = func_80218F30_5D4400;
    target = DHARUMANYO_PTR(s_root.task, 0x84);
    observed_target_x = target ? F32(target, 8) : -9999.0f;
    observed_target_z = target ? F32(target, 16) : -9999.0f;
    (void)object;
    anchor_dharumanyo_native_projectile_end();
}

static Fixture *setup(void)
{
    Fixture *root;
    Fixture *carrier;
    pool_count = 0;
    memset(pool, 0, sizeof(pool));
    memset(D_8015CC30_15D830, 0, sizeof(D_8015CC30_15D830));
    D_800C7AB2 = 0x49;
    current_visit++;
    native_pause = 0;
    terminal_accept = 1;
    terminal_calls = 0;
    observed_target_x = observed_target_z = 0.0f;
    root = new_fixture();
    carrier = new_fixture();
    root->post = func_08003810_6CBA20;
    root->ai = func_08000970_6C8B80;
    carrier->post = func_08003F84_6CC194;
    carrier->ai = func_08003F78_6CC188;
    U16(carrier->task_store.bytes, 0x5e) = 0xcc;
    U8(carrier->task_store.bytes, 0xd1) = 12;
    U8(carrier->task_store.bytes, 0x8d) = 10;
    DHARUMANYO_PTR(root->task_store.bytes, 0xdc) =
        carrier->task_store.bytes;
    DHARUMANYO_PTR(carrier->task_store.bytes, 0xdc) =
        root->task_store.bytes;
    D_8016DAB4_16E6B4 = root->task_store.bytes;
    anchor_dharumanyo_native_bind_root();
    assert(!s_active && !s_owner && !s_paused);
    D_8016DAB4_16E6B4 = carrier->task_store.bytes;
    anchor_dharumanyo_native_bind_carrier();
    D_8016DAB4_16E6B4 = root->task_store.bytes;
    anchor_dharumanyo_native_start_combat(root->task_store.bytes);
    F32(root->object_store.bytes, 0x1c) = 1.0f;
    F32(root->object_store.bytes, 0x20) = 1.0f;
    F32(root->object_store.bytes, 0x24) = 1.0f;
    U32(root->task_store.bytes, 0x60) = 0x006002e1u;
    U32(root->task_store.bytes, 0x64) = 2;
    U16(root->task_store.bytes, 0x3c) = 150;
    U16(root->task_store.bytes, 0x3e) = 300;
    U16(root->task_store.bytes, 0x40) = 230;
    U32(carrier->task_store.bytes, 0x60) = 0x002002a0u;
    assert(anchor_dharumanyo_native_ready());
    return root;
}

static void lifecycle_and_snapshot_test(void)
{
    Fixture *root = setup();
    AnchorDharumanyoNativeSnapshot snapshot;
    AnchorDharumanyoNativeSnapshot invalid;
    void *original_target = root->object_store.bytes;
    DHARUMANYO_PTR(root->task_store.bytes, 0x84) = original_target;
    F32(root->object_store.bytes, 8) = 10.0f;
    F32(root->object_store.bytes, 12) = 20.0f;
    F32(root->object_store.bytes, 16) = 30.0f;
    assert(anchor_dharumanyo_native_capture(&snapshot));
    assert(snapshot.root[DHAR_PHASE] == 1);
    assert(snapshot.carrier[DHAR_CARRIER_LIVES] == 12);
    invalid = snapshot;
    invalid.root[DHAR_X] = 0x7fc00000u;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.projectile_count = 17;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_D0] = 5;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_D0] = 3u << 8;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_D4] = 0x181;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_D8] = float_bits(101.0f);
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_E0] = float_bits(4097.0f);
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_E8] = 0x100;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_PRIVATE_EC] = float_bits(-65.0f);
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = snapshot;
    invalid.root[DHAR_COLLIDER_Z] = 0xffffffffu;
    assert(!anchor_dharumanyo_native_apply(&invalid));

    snapshot.root[DHAR_PHASE] = 2;
    snapshot.root[DHAR_X] = float_bits(44.0f);
    snapshot.carrier[DHAR_CARRIER_LIVES] = 9;
    anchor_dharumanyo_native_set_role(1, 0, 0);
    assert(anchor_dharumanyo_native_apply(&snapshot));
    anchor_dharumanyo_native_scheduler_begin();
    assert(s_root.held && s_carrier.held);
    /* The native pre callback runs inside the scheduler, while both tasks
     * still carry the temporary hold callbacks. */
    D_8016DAB4_16E6B4 = root->task_store.bytes;
    anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
    assert(!s_pending_valid);
    anchor_dharumanyo_native_scheduler_end();
    assert(root->ai == func_08000A0C_6C8C1C);
    assert(F32(root->object_store.bytes, 8) == 44.0f);
    assert(U8(s_carrier.task, 0xd1) == 9);
    assert(anchor_dharumanyo_native_capture(&snapshot));

    anchor_dharumanyo_native_set_role(1, 1, 0);
    anchor_dharumanyo_native_set_target(101.0f, 202.0f, 303.0f);
    anchor_dharumanyo_native_scheduler_begin();
    assert(DHARUMANYO_PTR(root->task_store.bytes, 0x84) == s_target_object);
    anchor_dharumanyo_native_scheduler_end();
    assert(DHARUMANYO_PTR(root->task_store.bytes, 0x84) == original_target);
    native_pause = 1;
    assert(anchor_dharumanyo_native_world_paused());
    native_pause = 0;
}

static void projectile_checkpoint_test(void)
{
    Fixture *root = setup();
    AnchorDharumanyoNativeSnapshot state;
    AnchorDharumanyoNativeSnapshot invalid;
    DharumanyoProjectile *projectile;
    assert(anchor_dharumanyo_native_capture(&state));
    state.projectile_count = 1;
    state.projectile_serial = 7;
    state.tick = 100;
    state.projectile[0][DHAR_PROJECTILE_ID] = 7;
    state.projectile[0][DHAR_PROJECTILE_BORN] = 95;
    state.projectile[0][DHAR_PROJECTILE_VARIANT] = 1;
    state.projectile[0][DHAR_PROJECTILE_X] = float_bits(11.0f);
    state.projectile[0][DHAR_PROJECTILE_Y] = float_bits(22.0f);
    state.projectile[0][DHAR_PROJECTILE_Z] = float_bits(33.0f);
    state.projectile[0][DHAR_PROJECTILE_YAW] = 123;
    state.projectile[0][DHAR_PROJECTILE_DEST_X] = float_bits(111.0f);
    state.projectile[0][DHAR_PROJECTILE_DEST_Z] = float_bits(333.0f);
    state.projectile[0][DHAR_PROJECTILE_VY] = float_bits(4.0f);
    state.projectile[0][DHAR_PROJECTILE_TIMER] = 12;
    invalid = state;
    invalid.projectile_serial = 6;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    invalid = state;
    invalid.projectile_serial = 0;
    assert(!anchor_dharumanyo_native_apply(&invalid));
    anchor_dharumanyo_native_set_role(1, 0, 0);
    assert(apply_now(&state));
    projectile = find_projectile(7);
    assert(projectile && projectile_live(projectile));
    assert(F32(projectile->actor.object, 8) == 11.0f);
    assert(F32(projectile->actor.object, 12) == 22.0f);
    assert(F32(projectile->actor.object, 16) == 33.0f);
    assert(observed_target_x == 111.0f && observed_target_z == 333.0f);
    assert(callback_is(DHARUMANYO_AI(projectile->actor.task),
                       func_08002A40_6CAC50));
    assert(anchor_dharumanyo_native_capture(&state));
    assert(state.projectile_count == 1 &&
           state.projectile[0][DHAR_PROJECTILE_ID] == 7);
    anchor_dharumanyo_native_set_role(1, 0, 1);
    anchor_dharumanyo_native_scheduler_begin();
    assert(projectile->actor.held && projectile_live(projectile));
    anchor_dharumanyo_native_tick();
    assert(projectile->id == 7);
    anchor_dharumanyo_native_scheduler_end();
    assert(projectile_live(projectile));
    anchor_dharumanyo_native_set_role(1, 0, 0);
    state.projectile_count = 0;
    assert(apply_now(&state));
    assert(U32(projectile->actor.task, 0x68) & REMOVE_PENDING);
    (void)root;
}

static void projectile_capacity_cleanup_test(void)
{
    Fixture *root = setup();
    AnchorDharumanyoNativeSnapshot state;
    DharumanyoActor actor;
    Fixture *fixture;
    unsigned int index;
    assert(anchor_dharumanyo_native_capture(&state));
    for (index = 0; index < ANCHOR_DHARUMANYO_MAX_PROJECTILES; ++index)
    {
        fixture = new_fixture();
        fixture->ai = func_0800284C_6CAA5C;
        fixture->post = func_80218F30_5D4400;
        U16(fixture->task_store.bytes, 0x5e) = 0xcc;
        DHARUMANYO_PTR(fixture->task_store.bytes, 0xdc) =
            root->task_store.bytes;
        bind(&actor, fixture->task_store.bytes);
        s_projectiles[index].actor = actor;
        s_projectiles[index].id = index + 1;
        s_projectiles[index].born = 0;
    }
    state.projectile[0][DHAR_PROJECTILE_ID] = 17;
    state.projectile[0][DHAR_PROJECTILE_BORN] = 0;
    state.projectile[0][DHAR_PROJECTILE_VARIANT] = 0;
    state.projectile[0][DHAR_PROJECTILE_X] = float_bits(1.0f);
    state.projectile[0][DHAR_PROJECTILE_Y] = float_bits(2.0f);
    state.projectile[0][DHAR_PROJECTILE_Z] = float_bits(3.0f);
    state.projectile[0][DHAR_PROJECTILE_DEST_X] = float_bits(4.0f);
    state.projectile[0][DHAR_PROJECTILE_DEST_Z] = float_bits(5.0f);
    state.projectile[0][DHAR_PROJECTILE_VY] = float_bits(1.0f);
    assert(!spawn_projectile(state.projectile[0]));
    fixture = &pool[pool_count - 1];
    assert(U32(fixture->task_store.bytes, 0x68) & REMOVE_PENDING);
}

static void pause_tick_and_terminal_test(void)
{
    Fixture *root = setup();
    AnchorDharumanyoNativeSnapshot state;
    unsigned int before;
    assert(anchor_dharumanyo_native_capture(&state));
    anchor_dharumanyo_native_set_role(1, 1, 1);
    anchor_dharumanyo_native_scheduler_begin();
    assert(s_root.held && s_carrier.held &&
           root->post == hold_noop && fixture_for(s_carrier.task)->post == hold_noop);
    anchor_dharumanyo_native_scheduler_end();
    before = s_tick;
    anchor_dharumanyo_native_observe_post(root->task_store.bytes);
    anchor_dharumanyo_native_tick();
    assert(s_tick == before);
    anchor_dharumanyo_native_set_role(1, 1, 0);
    anchor_dharumanyo_native_observe_post(root->task_store.bytes);
    anchor_dharumanyo_native_tick();
    assert(s_tick == before + 1);

    state.root[DHAR_PHASE] = DHAR_PHASE_TERMINAL;
    terminal_accept = 0;
    assert(!apply_now(&state));
    assert(!s_terminal_started && terminal_calls == 1);
    terminal_accept = 1;
    assert(apply_now(&state));
    assert(s_terminal_started && terminal_calls == 2);
    assert(apply_now(&state) && terminal_calls == 2);

    root = setup();
    U8(s_carrier.task, 0xd1) = 1;
    U16(s_carrier.task, 0xd6) = 0;
    anchor_dharumanyo_native_terminal(s_carrier.task);
    assert(s_terminal_started && s_terminal_snapshot_valid);
    assert(anchor_dharumanyo_native_snapshot_ready());
    U8(s_carrier.task, 0xd1) = 0;
    U32(root->task_store.bytes, 0x68) |= REMOVE_PENDING;
    U32(s_carrier.task, 0x68) |= REMOVE_PENDING;
    assert(!anchor_dharumanyo_native_ready());
    assert(anchor_dharumanyo_native_snapshot_ready());
    assert(anchor_dharumanyo_native_capture(&state));
    assert(state.root[DHAR_PHASE] == DHAR_PHASE_TERMINAL);
    assert(state.carrier[DHAR_CARRIER_LIVES] == 1);
    assert(anchor_dharumanyo_native_apply(&state));
    anchor_dharumanyo_native_finish_terminal();
    assert(!anchor_dharumanyo_native_snapshot_ready());
    (void)root;
}

static void checkpoint_scheduler_order_test(void)
{
    int owner;
    for (owner = 0; owner <= 1; ++owner)
    {
        Fixture *root = setup();
        AnchorDharumanyoNativeSnapshot state;
        unsigned int frame;
        assert(anchor_dharumanyo_native_capture(&state));
        state.root[DHAR_PHASE] = 2;
        anchor_dharumanyo_native_set_role(1, owner, 0);
        for (frame = 0; frame < 3; ++frame)
        {
            state.root[DHAR_TIMER] = 100 + frame;
            assert(anchor_dharumanyo_native_apply(&state));
            anchor_dharumanyo_native_scheduler_begin();
            assert(s_root.held && s_carrier.held);
            assert(anchor_dharumanyo_native_ready());
            assert(!anchor_dharumanyo_native_world_paused());
            D_8016DAB4_16E6B4 = root->task_store.bytes;
            if (frame == 0)
            {
                native_pause = 1;
                assert(anchor_dharumanyo_native_world_paused());
                anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
                assert(s_pending_valid);
                anchor_dharumanyo_native_scheduler_end();
                native_pause = 0;
                anchor_dharumanyo_native_scheduler_begin();
            }
            anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
            assert(!s_pending_valid && anchor_dharumanyo_native_ready());
            assert(!s_carrier.held && s_root.held == !owner);
            assert(root->post == func_08003810_6CBA20);
            root->ai(root->task_store.bytes, root->object_store.bytes);
            assert(U16(root->task_store.bytes, 0x8a) ==
                   state.root[DHAR_TIMER] + (unsigned int)owner);
            anchor_dharumanyo_native_scheduler_end();
            assert(!s_root.held && !s_carrier.held);
            assert(root->ai == func_08000A0C_6C8C1C);
        }
        /* Saved callbacks must not bypass the native lifetime fence. */
        assert(anchor_dharumanyo_native_apply(&state));
        anchor_dharumanyo_native_scheduler_begin();
        ++U8(root->task_store.bytes, 0x74);
        assert(!anchor_dharumanyo_native_is_root(root->task_store.bytes));
        anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
        assert(s_pending_valid);
        anchor_dharumanyo_native_scheduler_end();
    }
}

static void scripted_pause_release_test(void)
{
    Fixture *root = setup();
    AnchorDharumanyoNativeSnapshot state;
    assert(anchor_dharumanyo_native_capture(&state));
    native_pause = 1;
    root->post(root->task_store.bytes, root->object_store.bytes);
    assert(s_root_pause_mirror && anchor_dharumanyo_native_world_paused());
    anchor_dharumanyo_native_set_role(1, 1, 1);
    anchor_dharumanyo_native_scheduler_begin();
    assert(s_root.held);
    anchor_dharumanyo_native_scheduler_end();

    /* The controller releases 0x16C; only the root's next native post clears
     * its D4 mirror. Sync must allow that post instead of latching the pause. */
    native_pause = 0;
    assert(U32(D_8015CC30_15D830, 0xd4) & 1u);
    assert(!anchor_dharumanyo_native_world_paused());
    anchor_dharumanyo_native_set_role(1, 1, 0);
    assert(anchor_dharumanyo_native_apply(&state));
    anchor_dharumanyo_native_scheduler_begin();
    anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
    assert(!s_pending_valid && !s_root.held);
    root->post(root->task_store.bytes, root->object_store.bytes);
    assert(!s_root_pause_mirror);
    assert(!(U32(D_8015CC30_15D830, 0xd4) & 1u));
    anchor_dharumanyo_native_scheduler_end();

    /* A pause set by another native system remains a gate. */
    U32(D_8015CC30_15D830, 0xd4) |= 1u;
    assert(anchor_dharumanyo_native_world_paused());
    assert(anchor_dharumanyo_native_apply(&state));
    anchor_dharumanyo_native_scheduler_begin();
    anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
    assert(s_pending_valid);
    anchor_dharumanyo_native_scheduler_end();
    U32(D_8015CC30_15D830, 0xd4) &= ~1u;
    anchor_dharumanyo_native_scheduler_begin();
    anchor_dharumanyo_native_adopt_before_pre(root->task_store.bytes);
    assert(!s_pending_valid);
    anchor_dharumanyo_native_scheduler_end();

    root->ai = (TestCallback)((unsigned long)root->ai | CALLBACK_DISABLED);
    assert(anchor_dharumanyo_native_world_paused());
    anchor_dharumanyo_native_reset();
    assert(!s_root_pause_mirror);
}

int main(void)
{
    lifecycle_and_snapshot_test();
    checkpoint_scheduler_order_test();
    scripted_pause_release_test();
    projectile_checkpoint_test();
    projectile_capacity_cleanup_test();
    pause_tick_and_terminal_test();
    puts("Dharumanyo native snapshot, authority, projectile and terminal contracts passed");
    return 0;
}
