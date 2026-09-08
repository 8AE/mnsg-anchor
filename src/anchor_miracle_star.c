#include "anchor_miracle_star.h"
#include "anchor_remote_model_pool.h"
#include "anchor_tsurami_native.h"
#include "boss_sync.h"

#ifndef ANCHOR_MIRACLE_STAR_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern unsigned short D_800C7AB2;
extern int func_800240DC_24CDC(int flag_id);
extern void func_80024038_24C38(unsigned short flag_id);
extern void func_80024088_24C88(int flag_id);
extern void boss_sync_finish_tsurami_reward_scene(void);

#define STAR_ROOM 0x71u
#define STAR_REMOVE_PENDING 2u
#define STAR_CONTROLLER_TERMINAL 23u
#define STAR_BYTE(p, off) (*(unsigned char *)((unsigned char *)(p) + (off)))
#define STAR_HALF(p, off) (*(unsigned short *)((unsigned char *)(p) + (off)))
#define STAR_WORD(p, off) (*(unsigned int *)((unsigned char *)(p) + (off)))
#ifndef STAR_READ_POINTER
#define STAR_READ_POINTER(p, off) (*(void **)((unsigned char *)(p) + (off)))
#endif

static void *s_controller;
static void *s_scene;
static void *s_object;
static unsigned short s_actor;
static unsigned short s_entity;
static unsigned char s_generation;
static unsigned short s_room;
static int s_room_valid;
static int s_terminal_entered;
static void *s_constructor_actor;
static int s_restore_completion_flag;

static int star_pointer_valid(const void *pointer)
{
#ifdef ANCHOR_MIRACLE_STAR_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           ((address >= 0x80001000u && address < 0x80800000u) ||
            anchor_remote_model_pool_contains(pointer));
#endif
}

static void clear_binding(void)
{
    s_controller = s_scene = s_object = 0;
    s_actor = s_entity = 0;
    s_generation = 0;
    s_terminal_entered = 0;
    s_constructor_actor = 0;
    s_restore_completion_flag = 0;
}

void anchor_miracle_star_reset(void)
{
    clear_binding();
    s_room = 0;
    s_room_valid = 0;
}

void anchor_miracle_star_update_room(unsigned short current_room)
{
    if (!s_room_valid || s_room != current_room)
    {
        clear_binding();
        s_room = current_room;
        s_room_valid = 1;
    }
}

static int is_bound_controller(const void *actor)
{
    void *backlink;

    if (!actor || actor != s_controller || D_800C7AB2 != STAR_ROOM ||
        !s_room_valid || s_room != STAR_ROOM ||
        !star_pointer_valid(actor) || !star_pointer_valid(s_scene) ||
        (s_object && !star_pointer_valid(s_object)))
        return 0;
    backlink = STAR_READ_POINTER(actor, 0x04);
    return star_pointer_valid(backlink) &&
           STAR_READ_POINTER(backlink, 0) == actor &&
           STAR_READ_POINTER(actor, 0x18) == s_object &&
           STAR_READ_POINTER(actor, 0xd0) == s_scene &&
           STAR_HALF(actor, 0x5c) == s_actor &&
           STAR_HALF(actor, 0x5e) == s_entity &&
           STAR_BYTE(actor, 0x74) == s_generation &&
           !(STAR_WORD(actor, 0x68) & STAR_REMOVE_PENDING);
}

int anchor_miracle_star_local_scene_active(void)
{
    /* Phase zero is already the local owner of the future reward scene. An
     * incoming teammate's post-Tsurami dialogue flag must not skip ahead as
     * soon as this controller begins its destruction/reward presentation. */
    return is_bound_controller(s_controller) &&
           STAR_WORD(s_scene, 0) <= STAR_CONTROLLER_TERMINAL;
}

RECOMP_HOOK("func_08000000_732F30")
void anchor_miracle_star_before_constructor(void *actor, void *object)
{
    int completed;
    (void)object;
    anchor_miracle_star_update_room(D_800C7AB2);
    /* The native constructor destroys the current task immediately when
     * flag 0x40 is already set. Never inspect that task after its return. */
    s_constructor_actor = 0;
    s_restore_completion_flag = 0;
    if (D_800C7AB2 != STAR_ROOM)
        return;
    completed = func_800240DC_24CDC(0x40);
    if (completed && (boss_sync_has_active_encounter("fl_tsurami") ||
                      anchor_tsurami_native_root_task() != 0))
    {
        /* A team snapshot can set durable progression while a root has
         * already been instantiated locally. Keep that living encounter's
         * required reward controller; restore the saved bit synchronously. */
        func_80024088_24C88(0x40);
        s_restore_completion_flag = 1;
        completed = 0;
    }
    if (!completed)
        s_constructor_actor = actor;
}

RECOMP_HOOK_RETURN("func_08000000_732F30")
void anchor_miracle_star_after_constructor(void)
{
    void *actor = s_constructor_actor;
    void *scene;
    void *object;
    void *backlink;

    if (s_restore_completion_flag)
        func_80024038_24C38(0x40);
    s_restore_completion_flag = 0;
    s_constructor_actor = 0;
    if (D_800C7AB2 != STAR_ROOM || !star_pointer_valid(actor) ||
        (STAR_WORD(actor, 0x68) & STAR_REMOVE_PENDING))
        return;
    scene = STAR_READ_POINTER(actor, 0xd0);
    object = STAR_READ_POINTER(actor, 0x18);
    backlink = STAR_READ_POINTER(actor, 0x04);
    /* This controller does not dereference its supplied display object;
     * preserve identity even if the native task has no display object. */
    if (!star_pointer_valid(scene) ||
        (object && !star_pointer_valid(object)) ||
        !star_pointer_valid(backlink) ||
        STAR_READ_POINTER(backlink, 0) != actor || STAR_WORD(scene, 0) != 0)
        return;
    clear_binding();
    s_controller = actor;
    s_scene = scene;
    s_object = object;
    s_actor = STAR_HALF(actor, 0x5c);
    s_entity = STAR_HALF(actor, 0x5e);
    s_generation = STAR_BYTE(actor, 0x74);
}

RECOMP_HOOK("func_08000090_732FC0")
void anchor_miracle_star_before_controller(void *actor, void *object)
{
    (void)object;
    s_terminal_entered = is_bound_controller(actor) &&
                         STAR_WORD(s_scene, 0) == STAR_CONTROLLER_TERMINAL &&
                         STAR_WORD(s_scene, 4) == 0;
}

RECOMP_HOOK_RETURN("func_08000090_732FC0")
void anchor_miracle_star_after_controller(void)
{
    if (!s_terminal_entered)
        return;
    /* Phase 23, timer zero, releases native control and requests the Festival
     * Temple exit transition. Do not dereference its task/scene after that
     * transition or directly invoke cleanup on the scene's reward child. */
    clear_binding();
    boss_sync_finish_tsurami_reward_scene();
}
