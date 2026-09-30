#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*TestCallback)(void *, void *);
typedef struct TestTask {
    union { void *alignment; unsigned char bytes[0x100]; } raw;
    union { void *alignment; unsigned char bytes[0x100]; } object_raw;
    void *object;
    void *backlink;
    void *backlink_target;
    void *resource;
    TestCallback ai;
    TestCallback post;
} TestTask;

static TestTask child;
static TestTask shots[8];
static unsigned int allocated;
static unsigned int fail_after;
static unsigned int native_initializations;
static int resource_present = 1;
static void *bound_child;
static void *resource_value;
static int expect_reconstructed_ctor;

int anchor_control_machine_projectile_test_pointer_valid(const void *pointer)
{
    const unsigned long address = (unsigned long)pointer;
    unsigned int i;
    if (!pointer) return 0;
    if (address >= (unsigned long)child.raw.bytes &&
        address < (unsigned long)(child.raw.bytes + sizeof(child.raw.bytes)))
        return 1;
    if (address >= (unsigned long)child.object_raw.bytes &&
        address < (unsigned long)(child.object_raw.bytes +
                                  sizeof(child.object_raw.bytes))) return 1;
    for (i = 0; i < 8u; ++i) {
        if (address >= (unsigned long)shots[i].raw.bytes &&
            address < (unsigned long)(shots[i].raw.bytes +
                                      sizeof(shots[i].raw.bytes))) return 1;
        if (address >= (unsigned long)shots[i].object_raw.bytes &&
            address < (unsigned long)(shots[i].object_raw.bytes +
                                      sizeof(shots[i].object_raw.bytes))) return 1;
        if (pointer == &shots[i].backlink_target) return 1;
    }
    return 0;
}

static TestTask *find_task(const void *task)
{
    unsigned int i;
    if (task == child.raw.bytes) return &child;
    for (i = 0; i < 8u; ++i)
        if (task == shots[i].raw.bytes) return &shots[i];
    assert(!"unrecognized task");
    return 0;
}

static void **pointer_slot(void *record, unsigned int offset)
{
    TestTask *task;
    unsigned int i;
    for (i = 0; i < 8u; ++i)
        if (record == &shots[i].backlink_target && offset == 0)
            return &shots[i].backlink_target;
    task = find_task(record);
    if (offset == 0x04) return &task->backlink;
    if (offset == 0x18) return &task->object;
    if (offset == 0x2c) return &task->resource;
    assert(!"unexpected pointer field");
    return &task->object;
}

#define CM_PROJECTILE_PTR(p,o) (*pointer_slot((void *)(p),(o)))
#define CM_PROJECTILE_AI(p) (find_task(p)->ai)
#define CM_PROJECTILE_POST(p) (find_task(p)->post)
#define ANCHOR_CONTROL_MACHINE_PROJECTILES_HOST_TEST
#include "../src/bosses/control_machine/anchor_control_machine_projectiles.c"

unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4;

void *anchor_control_machine_bound_task(void) { return bound_child; }
int anchor_control_machine_snapshot_valid(const AnchorControlMachineSnapshot *s)
{
    unsigned int i, j;
    if (!s || s->projectile_count > ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES)
        return 0;
    for (i = 0; i < s->projectile_count; ++i) {
        if (!s->projectile[i][CM_PROJECTILE_ID] ||
            s->projectile[i][CM_PROJECTILE_TIMER] > 80u) return 0;
        for (j = 0; j < i; ++j)
            if (s->projectile[j][CM_PROJECTILE_ID] ==
                s->projectile[i][CM_PROJECTILE_ID]) return 0;
    }
    return 1;
}
void *func_800141C4_14DC4(unsigned int file)
{
    return resource_present && file == 0x2eu ? resource_value : 0;
}
void func_80218F30_5D4400(void *task, void *object)
{ (void)task; (void)object; }
void func_08004214_705314(void *task, void *object)
{ (void)task; (void)object; }
void func_0800413C_70523C(void *task, void *object)
{
    (void)object;
    if (expect_reconstructed_ctor) {
        assert(D_8016DAB4_16E6B4 == task);
        assert(CM_P_U16(task, 0x28) == 0x2eu);
        assert(CM_PROJECTILE_PTR(task, 0x2c) == resource_value);
    }
    ++native_initializations;
    CM_P_U16(task, 0x5e) = 0x7e;
    CM_P_U16(task, 0x8a) = 80;
    CM_PROJECTILE_AI(task) = func_08004214_705314;
    CM_PROJECTILE_POST(task) = func_80218F30_5D4400;
}

static void init_test_task(TestTask *task)
{
    memset(task, 0, sizeof(*task));
    task->object = task->object_raw.bytes;
    task->backlink = &task->backlink_target;
    task->backlink_target = task->raw.bytes;
    CM_P_U8(task->raw.bytes, 0x74) = 7;
    CM_P_U16(task->raw.bytes, 0x28) = 0x2e;
    task->resource = child.object_raw.bytes;
    func_0800413C_70523C(task->raw.bytes, task->object);
}

static void allocate_test_task(TestTask *task)
{
    memset(task, 0, sizeof(*task));
    task->object = task->object_raw.bytes;
    task->backlink = &task->backlink_target;
    task->backlink_target = task->raw.bytes;
    CM_P_U8(task->raw.bytes, 0x74) = 7;
    CM_P_U16(task->raw.bytes, 0x28) = 0xfffeu;
    task->resource = (void *)(unsigned long)0xffffffffu;
    task->ai = func_0800413C_70523C;
    task->post = func_80218F30_5D4400;
}

void *func_802171A8_5D2678(void *parent, TestCallback initializer,
                            unsigned char group)
{
    TestTask *task;
    assert(parent == child.raw.bytes);
    assert(initializer == func_0800413C_70523C);
    assert(group == 10u);
    if (allocated == fail_after || allocated >= 8u) return 0;
    task = &shots[allocated++];
    allocate_test_task(task);
    return task->raw.bytes;
}

static unsigned int bits(float f)
{ union { float f; unsigned int u; } v; v.f = f; return v.u; }

static void add_row(AnchorControlMachineSnapshot *s, unsigned int id,
                    float x)
{
    unsigned int *row = s->projectile[s->projectile_count++];
    memset(row, 0, ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS * sizeof(*row));
    row[CM_PROJECTILE_ID] = id;
    row[CM_PROJECTILE_BORN] = 12u;
    row[CM_PROJECTILE_TIMER] = 55u;
    row[CM_PROJECTILE_X] = bits(x);
    row[CM_PROJECTILE_Y] = bits(2.f);
    row[CM_PROJECTILE_Z] = bits(3.f);
    row[CM_PROJECTILE_VX] = bits(0.5f);
    row[CM_PROJECTILE_VY] = bits(0.25f);
    row[CM_PROJECTILE_VZ] = bits(-0.5f);
}

int main(void)
{
    AnchorControlMachineSnapshot snapshot;
    unsigned int first_id;
    TestCallback post;
    void *first_task;
    memset(&snapshot, 0, sizeof(snapshot));
    resource_value = child.object_raw.bytes;
    init_test_task(&child);
    bound_child = child.raw.bytes;
    D_800C7AB2 = 0x155;
    fail_after = 8u;
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 0, 0, 0);

    /* Before election, the local native encounter can seed an owner-ready
     * checkpoint. A birth gets one stable identity despite repeat hooks. */
    init_test_task(&shots[allocated++]);
    D_8016DAB4_16E6B4 = shots[0].raw.bytes;
    anchor_control_machine_projectiles_birth();
    anchor_control_machine_projectiles_birth();
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 1u);
    first_id = snapshot.projectile[0][CM_PROJECTILE_ID];
    assert(first_id == 1u);
    first_task = shots[0].raw.bytes;
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 1, 0, 1);
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 1u);
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 1, 1, 0);

    /* Demotion retires local shots before the remote serial is adopted. */
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 1, 0, 1);
    assert(CM_P_U32(first_task, 0x68) & 2u);
    snapshot.projectile_count = 0;
    add_row(&snapshot, 9u, 18.f);
    assert(anchor_control_machine_projectiles_apply(&snapshot));
    assert(allocated == 2u);
    assert(native_initializations >= 3u);
    post = CM_PROJECTILE_POST(shots[1].raw.bytes);
    assert(post == func_80218F30_5D4400);
    assert(CM_PROJECTILE_AI(shots[1].raw.bytes) == noop);
    assert(CM_P_F32(shots[1].object, 8) == 18.f);
    assert(CM_P_U16(shots[1].raw.bytes, 0x8a) == 55u);
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 1u);
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 1, 0, 0);

    /* Reordering and duplicate delivery correct the same native task. */
    snapshot.projectile[0][CM_PROJECTILE_X] = bits(22.f);
    CM_PROJECTILE_AI(shots[1].raw.bytes) = (TestCallback)(
        (unsigned long)CM_PROJECTILE_AI(shots[1].raw.bytes) |
        CM_P_DISABLED);
    assert(anchor_control_machine_projectiles_apply(&snapshot));
    assert(allocated == 2u && CM_P_F32(shots[1].object, 8) == 22.f);
    assert(CM_PROJECTILE_POST(shots[1].raw.bytes) == post);
    assert((unsigned long)CM_PROJECTILE_AI(shots[1].raw.bytes) &
           CM_P_DISABLED);
    snapshot.projectile_count = 2;
    memcpy(snapshot.projectile[1], snapshot.projectile[0],
           sizeof(snapshot.projectile[1]));
    assert(!anchor_control_machine_projectiles_apply(&snapshot));
    assert(allocated == 2u);

    /* A failed second allocation rolls back new replicas and leaves the
     * previously synchronized shot at its old checkpoint. */
    snapshot.projectile_count = 1;
    add_row(&snapshot, 10u, 25.f);
    add_row(&snapshot, 11u, 26.f);
    fail_after = 3u;
    assert(!anchor_control_machine_projectiles_apply(&snapshot));
    assert(CM_P_U32(shots[2].raw.bytes, 0x68) & 2u);
    assert(CM_P_F32(shots[1].object, 8) == 22.f);
    snapshot.projectile_count = 1;
    assert(anchor_control_machine_projectiles_apply(&snapshot));

    /* A removed snapshot retires the shot; local native post remains intact
     * on surviving shots, with only AI held while following. */
    snapshot.projectile_count = 0;
    resource_present = 0;
    assert(anchor_control_machine_projectiles_apply(&snapshot));
    resource_present = 1;
    assert(CM_P_U32(shots[1].raw.bytes, 0x68) & 2u);
    assert(CM_PROJECTILE_POST(shots[1].raw.bytes) == post);

    /* Promotion retains adopted serials and releases AI for new owner work. */
    add_row(&snapshot, 19u, 30.f);
    fail_after = 8u;
    assert(anchor_control_machine_projectiles_apply(&snapshot));
    anchor_control_machine_projectiles_set_context(bound_child, 1u, 1, 1, 0);
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 1u);
    assert(snapshot.projectile[0][CM_PROJECTILE_ID] == 19u);
    assert(CM_PROJECTILE_AI(shots[3].raw.bytes) == func_08004214_705314);
    init_test_task(&shots[allocated++]);
    D_8016DAB4_16E6B4 = shots[4].raw.bytes;
    anchor_control_machine_projectiles_birth();
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 2u);
    assert(snapshot.projectile[1][CM_PROJECTILE_ID] == 20u);

    /* Wrong room, stale generation and a new binding all reject old tasks. */
    D_800C7AB2 = 0x154;
    assert(!anchor_control_machine_projectiles_capture(&snapshot));
    D_800C7AB2 = 0x155;
    CM_P_U8(child.raw.bytes, 0x74) = 8;
    assert(!anchor_control_machine_projectiles_capture(&snapshot));
    anchor_control_machine_projectiles_set_context(bound_child, 2u, 0, 0, 0);
    assert(anchor_control_machine_projectiles_capture(&snapshot));
    assert(snapshot.projectile_count == 0u);

    /* A file registry handle is opaque and may carry native cache bits.
     * The allocator is uninitialized until spawn invokes the constructor
     * once under the saved scheduler-current task. */
    resource_value = (void *)(unsigned long)0xc0315000u;
    assert(!anchor_control_machine_projectile_test_pointer_valid(
        resource_value));
    anchor_control_machine_projectiles_set_context(bound_child, 2u, 1, 0, 0);
    add_row(&snapshot, 25u, 41.f);
    D_8016DAB4_16E6B4 = child.raw.bytes;
    {
        unsigned int before = native_initializations;
        expect_reconstructed_ctor = 1;
        assert(anchor_control_machine_projectiles_apply(&snapshot));
        expect_reconstructed_ctor = 0;
        assert(native_initializations == before + 1u);
        CM_PROJECTILE_AI(shots[allocated - 1u].raw.bytes)(
            shots[allocated - 1u].raw.bytes, shots[allocated - 1u].object);
        assert(native_initializations == before + 1u);
    }
    assert(D_8016DAB4_16E6B4 == child.raw.bytes);
    assert(shots[allocated - 1u].resource == resource_value);
    assert(CM_P_U16(shots[allocated - 1u].raw.bytes, 0x5e) == 0x7eu);
    assert(CM_PROJECTILE_AI(shots[allocated - 1u].raw.bytes) == noop);
    assert(CM_PROJECTILE_POST(shots[allocated - 1u].raw.bytes) ==
           func_80218F30_5D4400);
    snapshot.projectile_count = 0;
    assert(anchor_control_machine_projectiles_apply(&snapshot));
    add_row(&snapshot, 26u, 42.f);
    resource_value = 0;
    assert(!anchor_control_machine_projectiles_apply(&snapshot));
    resource_value = (void *)(unsigned long)0xffffffffu;
    assert(!anchor_control_machine_projectiles_apply(&snapshot));
    anchor_control_machine_projectiles_reset();
    puts("Control Machine projectile identity, rollback, and AI hold tests passed");
    return 0;
}
