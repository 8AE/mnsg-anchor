#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*TestCallback)(void *, void *);
static union { void *align; unsigned char bytes[0x200]; } controller_store;
static union { void *align; unsigned char bytes[16]; } private_store;
static unsigned int binding_visit = 17u;
static int bound = 1, ready, scenario_active, clear_calls;
static void *controller_bound;
static TestCallback controller_ai;

#define CM_INTRO_AI(p) (controller_ai)
#define ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST
#include "../src/bosses/control_machine/anchor_control_machine_intro.c"

unsigned short D_800C7AB2 = CM_INTRO_ROOM;
void *D_8016DAB4_16E6B4;
int anchor_control_machine_native_bound(void) { return bound; }
unsigned int anchor_control_machine_native_visit(void) { return binding_visit; }
int anchor_world_quest_koryuta_local_ready(void) { return ready; }
void *anchor_world_quest_koryuta_controller_task(void)
{ return controller_bound; }
int func_8003F1D8_3FDD8(void) { return scenario_active; }
void func_801FC590_5B84A0(void) { ++clear_calls; }
void func_080004F8_71AA08(void *task, void *object)
{ (void)task; (void)object; }
static void replacement_callback(void *task, void *object)
{ (void)task; (void)object; }

static void prepare(void)
{
    D_800C7AB2 = CM_INTRO_ROOM;
    bound = 1;
    ready = scenario_active = 0;
    anchor_control_machine_intro_owner_hint(0, 0);
    anchor_control_machine_intro_preview(0, 0);
    ++binding_visit;
    memset(&controller_store, 0, sizeof(controller_store));
    memset(&private_store, 0, sizeof(private_store));
    controller_bound = D_8016DAB4_16E6B4 = controller_store.bytes;
    CM_INTRO_PTR(controller_bound, 0xd0) = private_store.bytes;
    BYTE(controller_bound, 0x74) = 7u;
    BYTE(private_store.bytes, 0) = 7u;
    HALF(controller_bound, 0x8a) = 1u;
    controller_ai = func_080004F8_71AA08;
    assert(!s_wait.task);
}

static void tick_wait(void)
{
    assert(controller_ai == wait_callback);
    controller_ai(controller_bound, 0);
}

int main(void)
{
    unsigned int i;
    void *controller;

    prepare();
    controller = controller_bound;
    /* Solo entrance: no observer owner hint, so native dialogue remains. */
    anchor_control_machine_intro_wait_boundary();
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task);
    anchor_control_machine_intro_skip(controller, 0);
    assert(BYTE(private_store.bytes, 0) == 7u && !clear_calls);

    /* A fresh checkpoint already present needs no hold and skips at entry. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_preview(binding_visit, 1);
    anchor_control_machine_intro_wait_boundary();
    assert(controller_ai == func_080004F8_71AA08);
    anchor_control_machine_intro_skip(controller, 0);
    assert(BYTE(private_store.bytes, 0) == 6u &&
           HALF(controller, 0x8a) == 0u && clear_calls == 1);
    anchor_control_machine_intro_skip(controller, 0);
    assert(clear_calls == 1);

    /* Owner hint alone waits; only a later fresh preview permits the skip. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    assert(controller_ai == wait_callback && s_wait.ticks == 0u);
    for (i = 0; i < 12u; ++i) tick_wait();
    assert(BYTE(private_store.bytes, 0) == 7u &&
           HALF(controller, 0x8a) == 1u && clear_calls == 1);
    anchor_control_machine_intro_preview(binding_visit, 1);
    tick_wait();
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task &&
           BYTE(private_store.bytes, 0) == 6u &&
           HALF(controller, 0x8a) == 0u && clear_calls == 2);

    /* The 60th scheduler callback releases native phase 7 unchanged. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    for (i = 0; i < CM_INTRO_WAIT_TICKS - 1u; ++i) tick_wait();
    assert(controller_ai == wait_callback && s_wait.ticks == 59u);
    tick_wait();
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task &&
           BYTE(private_store.bytes, 0) == 7u && HALF(controller, 0x8a) == 1u);

    /* A withdrawn hint restores promptly without opening/cancelling script. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    anchor_control_machine_intro_owner_hint(binding_visit, 0);
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task);

    /* Active scenario never waits or skips; the owner hint is insufficient. */
    prepare();
    scenario_active = 1;
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_preview(binding_visit, 1);
    anchor_control_machine_intro_wait_boundary();
    anchor_control_machine_intro_skip(controller, 0);
    assert(controller_ai == func_080004F8_71AA08 &&
           BYTE(private_store.bytes, 0) == 7u && clear_calls == 2);
    scenario_active = 0;

    /* A hint for an earlier binding cannot delay this controller. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    ++binding_visit;
    anchor_control_machine_intro_wait_boundary();
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task);

    /* Losing the battle-child binding must restore a still-live controller. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    bound = 0;
    anchor_control_machine_intro_owner_hint(0, 0);
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task &&
           BYTE(private_store.bytes, 0) == 7u);
    bound = 1;

    /* A new combat visit with this same controller also restores its AI. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    ++binding_visit;
    anchor_control_machine_intro_owner_hint(0, 0);
    assert(controller_ai == func_080004F8_71AA08 && !s_wait.task);

    /* No write into a recycled task or callback replaced by native code. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    BYTE(controller, 0x74) = 8u;
    controller_ai = replacement_callback;
    anchor_control_machine_intro_owner_hint(0, 0);
    assert(controller_ai == replacement_callback && !s_wait.task);

    /* Room departure also leaves the obsolete controller storage alone. */
    prepare();
    anchor_control_machine_intro_owner_hint(binding_visit, 7u);
    anchor_control_machine_intro_wait_boundary();
    D_800C7AB2 = 0;
    anchor_control_machine_intro_owner_hint(0, 0);
    assert(controller_ai == wait_callback && !s_wait.task);
    D_800C7AB2 = CM_INTRO_ROOM;

    puts("Control Machine unopened intro wait and skip: PASS");
    return 0;
}
