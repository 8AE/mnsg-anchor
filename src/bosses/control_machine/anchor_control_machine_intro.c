#include "bosses/control_machine/anchor_control_machine_intro.h"
#include "bosses/control_machine/anchor_control_machine_native.h"
#include "world/anchor_world_quest.h"

#ifndef ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST
#include "platform/modding.h"
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
#include "platform/recomputils.h"
#endif
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

#define BYTE(p,o) (*(volatile unsigned char *)((unsigned char *)(p)+(o)))
#define HALF(p,o) (*(volatile unsigned short *)((unsigned char *)(p)+(o)))
#ifndef CM_INTRO_PTR
#define CM_INTRO_PTR(p,o) (*(void *volatile *)((unsigned char *)(p)+(o)))
#endif

typedef void (*ControlMachineIntroCallback)(void *, void *);
#ifndef CM_INTRO_AI
#define CM_INTRO_AI(p) \
    (*(ControlMachineIntroCallback volatile *)((unsigned char *)(p)+0x0c))
#endif
#ifdef ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST
#define CM_INTRO_DISABLED 0ul
#else
#define CM_INTRO_DISABLED 0x00800000ul
#endif
#define CM_INTRO_ROOM 0x155u
#define CM_INTRO_WAIT_TICKS 60u

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern int func_8003F1D8_3FDD8(void);
extern void func_801FC590_5B84A0(void);
extern void func_080004F8_71AA08(void *, void *);

typedef struct ControlMachineIntroWait {
    void *task;
    void *private_state;
    ControlMachineIntroCallback native_ai;
    unsigned int visit;
    unsigned int ticks;
    unsigned char generation;
} ControlMachineIntroWait;

static ControlMachineIntroWait s_wait;
static unsigned int s_preview_visit, s_hint_visit;

static int callback_is(ControlMachineIntroCallback current,
                       ControlMachineIntroCallback expected)
{
    return ((unsigned long)current & ~CM_INTRO_DISABLED) ==
           ((unsigned long)expected & ~CM_INTRO_DISABLED);
}

static void wait_callback(void *task, void *object);

static int wait_controller_live(void)
{
    return s_wait.task && D_800C7AB2 == CM_INTRO_ROOM &&
           s_wait.task == anchor_world_quest_koryuta_controller_task() &&
           BYTE(s_wait.task, 0x74) == s_wait.generation &&
           CM_INTRO_PTR(s_wait.task, 0xd0) == s_wait.private_state;
}

static int wait_binding_valid(void)
{
    return wait_controller_live() && anchor_control_machine_native_bound() &&
           s_wait.visit == anchor_control_machine_native_visit();
}

/* Restore only the same live controller and the callback installed by us.
 * Recycled task storage or a native replacement callback is never written. */
static int restore_wait(void)
{
    int restored = wait_controller_live() &&
        callback_is(CM_INTRO_AI(s_wait.task), wait_callback);
    if (restored) {
        unsigned long current = (unsigned long)CM_INTRO_AI(s_wait.task);
        CM_INTRO_AI(s_wait.task) = (ControlMachineIntroCallback)(
            ((unsigned long)s_wait.native_ai & ~CM_INTRO_DISABLED) |
            (current & CM_INTRO_DISABLED));
    }
    s_wait.task = 0;
    s_wait.private_state = 0;
    s_wait.native_ai = 0;
    s_wait.visit = s_wait.ticks = 0;
    s_wait.generation = 0;
    return restored;
}

void anchor_control_machine_intro_owner_hint(unsigned int visit,
                                              unsigned int owner)
{
    if (owner && visit && D_800C7AB2 == CM_INTRO_ROOM &&
        anchor_control_machine_native_bound() &&
        visit == anchor_control_machine_native_visit() &&
        !anchor_world_quest_koryuta_local_ready())
        s_hint_visit = visit;
    else {
        s_hint_visit = 0;
        if (s_wait.task) (void)restore_wait();
    }
}

void anchor_control_machine_intro_preview(unsigned int visit, int fresh)
{
    if (fresh && visit && D_800C7AB2 == CM_INTRO_ROOM &&
        anchor_control_machine_native_bound() &&
        visit == anchor_control_machine_native_visit() &&
        !anchor_world_quest_koryuta_local_ready())
        s_preview_visit = visit;
    else
        s_preview_visit = 0;
}

static int skip_ready(void *task)
{
    void *private_state;
    if (D_800C7AB2 != CM_INTRO_ROOM || !task ||
        task != anchor_world_quest_koryuta_controller_task() ||
        !anchor_control_machine_native_bound() ||
        s_preview_visit != anchor_control_machine_native_visit() ||
        anchor_world_quest_koryuta_local_ready()) return 0;
    private_state = CM_INTRO_PTR(task, 0xd0);
    if (!private_state || BYTE(private_state, 0) != 7u ||
        HALF(task, 0x8a) != 1u) return 0;
    if (func_8003F1D8_3FDD8()) {
        s_preview_visit = 0;
        return 0;
    }
    return 1;
}

static void skip_dialogue(void *task, int delayed)
{
    void *private_state = CM_INTRO_PTR(task, 0xd0);
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST)
    recomp_printf("[CM] intro skip visit=%u delayed=%d\n",
                  anchor_control_machine_native_visit(), delayed);
#else
    (void)delayed;
#endif
    func_801FC590_5B84A0();
    BYTE(private_state, 0) = 6u;
    HALF(task, 0x8a) = 0;
    s_preview_visit = s_hint_visit = 0;
}

RECOMP_HOOK("func_080004F8_71AA08")
void anchor_control_machine_intro_skip(void *task, void *unused)
{
    (void)unused;
    if (skip_ready(task)) skip_dialogue(task, 0);
}

/* File_58 phase 8 has just consumed this player's local proximity flag and
 * left phase 7 at timer 1. Delay only this controller's next AI callback so
 * a requested owner checkpoint can arrive before native opens dialogue. */
RECOMP_HOOK_RETURN("func_080004F8_71AA08")
void anchor_control_machine_intro_wait_boundary(void)
{
    void *task = D_8016DAB4_16E6B4;
    void *private_state;
    ControlMachineIntroCallback native_ai;
    unsigned int visit;
    if (!task || D_800C7AB2 != CM_INTRO_ROOM ||
        task != anchor_world_quest_koryuta_controller_task() ||
        !anchor_control_machine_native_bound() ||
        anchor_world_quest_koryuta_local_ready()) return;
    visit = anchor_control_machine_native_visit();
    if (s_hint_visit != visit || s_preview_visit == visit ||
        func_8003F1D8_3FDD8()) return;
    private_state = CM_INTRO_PTR(task, 0xd0);
    if (!private_state || BYTE(private_state, 0) != 7u ||
        HALF(task, 0x8a) != 1u) return;
    native_ai = CM_INTRO_AI(task);
    if (!callback_is(native_ai, func_080004F8_71AA08) ||
        ((unsigned long)native_ai & CM_INTRO_DISABLED)) return;
    if (s_wait.task) (void)restore_wait();
    s_wait.task = task;
    s_wait.private_state = private_state;
    s_wait.native_ai = native_ai;
    s_wait.visit = visit;
    s_wait.generation = BYTE(task, 0x74);
    s_wait.ticks = 0;
    CM_INTRO_AI(task) = wait_callback;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST)
    recomp_printf("[CM] intro wait start visit=%u limit=%u\n",
                  visit, CM_INTRO_WAIT_TICKS);
#endif
}

static void wait_callback(void *task, void *object)
{
    int scenario_active;
    (void)object;
    if (task != s_wait.task || !wait_binding_valid() ||
        !callback_is(CM_INTRO_AI(task), wait_callback)) {
        if (s_wait.task) (void)restore_wait();
        return;
    }
    scenario_active = func_8003F1D8_3FDD8();
    if (s_hint_visit != s_wait.visit ||
        BYTE(s_wait.private_state, 0) != 7u ||
        HALF(task, 0x8a) != 1u ||
        anchor_world_quest_koryuta_local_ready() ||
        scenario_active) {
        if (scenario_active) s_preview_visit = 0;
        (void)restore_wait();
        return;
    }
    if (skip_ready(task)) {
        if (restore_wait()) skip_dialogue(task, 1);
        return;
    }
    if (++s_wait.ticks >= CM_INTRO_WAIT_TICKS) {
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_INTRO_HOST_TEST)
        recomp_printf("[CM] intro wait timeout visit=%u\n", s_wait.visit);
#endif
        (void)restore_wait();
    }
}
