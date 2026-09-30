#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*TestCallback)(void *, void *);
typedef struct Fixture {
    union { void *alignment; unsigned char bytes[0x100]; } task_store;
    union { void *alignment; unsigned char bytes[0x100]; } object_store;
    union { void *alignment; unsigned char bytes[0x20]; } backlink_store;
    void *backlink;
    void *object;
    void *parent;
    void *backlink_target;
    TestCallback ai;
} Fixture;

static Fixture actors[3];
static unsigned int current_visit;

static Fixture *fixture_for(const void *task)
{
    unsigned int i;
    for (i = 0; i < 3u; ++i)
        if (task == actors[i].task_store.bytes)
            return &actors[i];
    assert(!"unexpected task");
    return 0;
}

static void **fixture_pointer_slot(void *record, unsigned int offset)
{
    unsigned int i;
    for (i = 0; i < 3u; ++i)
    {
        Fixture *actor = &actors[i];
        if (record == actor->backlink_store.bytes && offset == 0)
            return &actor->backlink_target;
        if (record != actor->task_store.bytes)
            continue;
        if (offset == 0x04) return &actor->backlink;
        if (offset == 0x18) return &actor->object;
        if (offset == 0xd0) return &actor->parent;
    }
    assert(!"unexpected pointer slot");
    return &actors[0].backlink;
}

#define CM_PTR(p, o) (*fixture_pointer_slot((void *)(p), (o)))
#define CM_AI(p) (fixture_for(p)->ai)
#define CM_CALLBACK_DISABLED (1ul << (sizeof(unsigned long) * 8 - 1))
#define ANCHOR_CONTROL_MACHINE_HUD_HOST_TEST
#include "../src/bosses/control_machine/anchor_control_machine_hud.c"

unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4;
unsigned int anchor_boss_invite_world_visit(void) { return current_visit; }

#define EMPTY_CALLBACK(name) \
    void name(void *task, void *object) { (void)task; (void)object; }
EMPTY_CALLBACK(func_08002EB4_703FB4)
EMPTY_CALLBACK(func_080031D8_7042D8)
EMPTY_CALLBACK(func_08003634_704734)
EMPTY_CALLBACK(func_08003678_704778)
EMPTY_CALLBACK(func_080036CC_7047CC)

static void init_actor(Fixture *actor)
{
    actor->backlink = actor->backlink_store.bytes;
    actor->backlink_target = actor->task_store.bytes;
    actor->object = actor->object_store.bytes;
    CM_U16(actor->task_store.bytes, 0x5c) = 0x1b0;
    CM_U16(actor->task_store.bytes, 0x5e) = 0x1b0;
    CM_U8(actor->task_store.bytes, 0x74) = 7;
}

int main(void)
{
    Fixture *root = &actors[0], *child = &actors[1], *other = &actors[2];
    void *task = child->task_store.bytes;
    unsigned int health = 99u, visit;
    unsigned char generation;

    assert(!anchor_control_machine_hud_task());
    assert(!anchor_control_machine_hud_health(&health));
    memset(actors, 0, sizeof(actors));
    init_actor(root); init_actor(child); init_actor(other);
    child->parent = root->task_store.bytes;
    child->ai = func_08002EB4_703FB4;
    CM_U8(task, 0x8d) = 5;
    D_800C7AB2 = 0x155;
    current_visit = 1;

    /* The placed body and unrelated 0x1B0 child cannot bind this HUD. */
    D_8016DAB4_16E6B4 = root->task_store.bytes;
    anchor_control_machine_hud_bind();
    assert(!anchor_control_machine_hud_task());
    D_8016DAB4_16E6B4 = other->task_store.bytes;
    anchor_control_machine_hud_bind();
    assert(!anchor_control_machine_hud_task());

    D_8016DAB4_16E6B4 = task;
    anchor_control_machine_hud_bind();
    assert(anchor_control_machine_bound_task() == task);
    assert(anchor_control_machine_bound_root() == root->task_store.bytes);
    assert(anchor_control_machine_hud_task() == task);
    assert(!anchor_control_machine_hud_health(0));
    assert(anchor_control_machine_hud_health(&health) && health == 5u);
    CM_U8(task, 0x8d) = 3;
    assert(anchor_control_machine_hud_health(&health) && health == 3u);
    CM_U8(task, 0x8d) = 1;
    assert(anchor_control_machine_hud_health(&health) && health == 1u);
    CM_U8(task, 0x8d) = 0;
    /* HP zero is still a live native combat state before the final hit. */
    assert(anchor_control_machine_bound_task() == task);
    assert(!anchor_control_machine_hud_task() &&
           !anchor_control_machine_hud_health(&health) && health == 1u);
    CM_U8(task, 0x8d) = 6;
    assert(!anchor_control_machine_hud_task());
    CM_U8(task, 0x8d) = 5;

    child->ai = (TestCallback)((unsigned long)func_080031D8_7042D8 |
                               CM_CALLBACK_DISABLED);
    CM_U8(task, 0x8d) = 255;
    assert(anchor_control_machine_bound_task() == task);
    assert(!anchor_control_machine_hud_task());
    child->ai = func_08003634_704734;
    assert(!anchor_control_machine_hud_task());
    child->ai = func_08003678_704778;
    assert(!anchor_control_machine_hud_task());
    child->ai = func_080036CC_7047CC;
    assert(!anchor_control_machine_hud_task());
    child->ai = func_08002EB4_703FB4;
    CM_U8(task, 0x8d) = 5;

    /* A changed list head rewrites +0x04; the current backlink stays valid. */
    child->backlink = other->backlink_store.bytes;
    other->backlink_target = task;
    assert(anchor_control_machine_hud_task() == task);
    other->backlink_target = 0;
    assert(!anchor_control_machine_bound_task());
    assert(!anchor_control_machine_hud_task());
    child->backlink = child->backlink_store.bytes;

    child->parent = other->task_store.bytes;
    assert(!anchor_control_machine_hud_task());
    child->parent = root->task_store.bytes;
    generation = CM_U8(task, 0x74);
    CM_U8(task, 0x74) = generation + 1u;
    assert(!anchor_control_machine_hud_task());
    CM_U8(task, 0x74) = generation;
    child->object = other->object_store.bytes;
    assert(!anchor_control_machine_hud_task());
    child->object = child->object_store.bytes;
    generation = CM_U8(root->task_store.bytes, 0x74);
    CM_U8(root->task_store.bytes, 0x74) = generation + 1u;
    assert(!anchor_control_machine_hud_task());
    CM_U8(root->task_store.bytes, 0x74) = generation;
    CM_U32(task, 0x68) |= 2u;
    assert(!anchor_control_machine_hud_task());
    CM_U32(task, 0x68) &= ~2u;

    visit = current_visit;
    current_visit++;
    assert(!anchor_control_machine_bound_task());
    assert(!anchor_control_machine_hud_task());
    current_visit = visit;
    D_800C7AB2 = 0;
    assert(!anchor_control_machine_hud_task());
    puts("Control Machine native HUD child lifecycle tests passed");
    return 0;
}
