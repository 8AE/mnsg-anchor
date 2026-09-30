#include "bosses/control_machine/anchor_control_machine_hud.h"
#include "bosses/anchor_boss_invite_world.h"

#ifndef ANCHOR_CONTROL_MACHINE_HUD_HOST_TEST
#include "platform/modding.h"
#else
#define RECOMP_HOOK_RETURN(name)
#endif

#define CM_ROOM 0x155u
#define CM_ENTITY 0x1b0u
#define CM_REMOVE_PENDING 2u
#ifndef CM_CALLBACK_DISABLED
#define CM_CALLBACK_DISABLED 0x00800000ul
#endif

#define CM_U8(p, o) (*(volatile unsigned char *)((unsigned char *)(p) + (o)))
#define CM_U16(p, o) (*(volatile unsigned short *)((unsigned char *)(p) + (o)))
#define CM_U32(p, o) (*(volatile unsigned int *)((unsigned char *)(p) + (o)))
#ifndef CM_PTR
#define CM_PTR(p, o) (*(void *volatile *)((unsigned char *)(p) + (o)))
#endif
#ifndef CM_AI
#define CM_AI(p) (*(ControlMachineCallback volatile *)((unsigned char *)(p) + 0x0cu))
#endif

typedef void (*ControlMachineCallback)(void *, void *);
typedef struct ControlMachineActor {
    void *task;
    void *object;
    unsigned char generation;
} ControlMachineActor;

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void func_080031D8_7042D8(void *, void *);
extern void func_08003634_704734(void *, void *);
extern void func_08003678_704778(void *, void *);
extern void func_080036CC_7047CC(void *, void *);

static ControlMachineActor s_child;
static ControlMachineActor s_root;
static unsigned int s_visit;

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_CONTROL_MACHINE_HUD_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           address >= 0x80001000u && address < 0x80800000u;
#endif
}

static int actor_live(const ControlMachineActor *actor)
{
    void *backlink;
    if (!pointer_valid(actor->task) || !pointer_valid(actor->object) ||
        CM_PTR(actor->task, 0x18) != actor->object ||
        CM_U8(actor->task, 0x74) != actor->generation ||
        (CM_U32(actor->task, 0x68) & CM_REMOVE_PENDING))
        return 0;
    /* +0x04 points to the current list slot. Native sibling insertion may
     * rewrite it, so inspect the live slot instead of caching its address. */
    backlink = CM_PTR(actor->task, 0x04);
    return pointer_valid(backlink) && CM_PTR(backlink, 0) == actor->task;
}

static int death_callback(ControlMachineCallback callback)
{
    unsigned long ai = (unsigned long)callback & ~CM_CALLBACK_DISABLED;
    return ai == ((unsigned long)func_080031D8_7042D8 &
                  ~CM_CALLBACK_DISABLED) ||
           ai == ((unsigned long)func_08003634_704734 &
                  ~CM_CALLBACK_DISABLED) ||
           ai == ((unsigned long)func_08003678_704778 &
                  ~CM_CALLBACK_DISABLED) ||
           ai == ((unsigned long)func_080036CC_7047CC &
                  ~CM_CALLBACK_DISABLED);
}

static int battle_child_bound(void)
{
    return D_800C7AB2 == CM_ROOM && s_visit &&
           s_visit == anchor_boss_invite_world_visit() &&
           actor_live(&s_root) && actor_live(&s_child) &&
           CM_U16(s_root.task, 0x5c) == CM_ENTITY &&
           CM_U16(s_root.task, 0x5e) == CM_ENTITY &&
           CM_U16(s_child.task, 0x5e) == CM_ENTITY &&
           CM_PTR(s_child.task, 0xd0) == s_root.task;
}

/* The File_46 initializer is scheduled after its owner has stored the placed
 * root at child+0xD0; its return has initialized the battle child's HP. */
RECOMP_HOOK_RETURN("func_08002D18_703E18")
void anchor_control_machine_hud_bind(void)
{
    void *task = D_8016DAB4_16E6B4;
    void *root;
    ControlMachineActor child, parent;
    if (D_800C7AB2 != CM_ROOM || !pointer_valid(task))
        return;
    root = CM_PTR(task, 0xd0);
    if (!pointer_valid(root))
        return;
    child.task = task;
    child.object = CM_PTR(task, 0x18);
    child.generation = CM_U8(task, 0x74);
    parent.task = root;
    parent.object = CM_PTR(root, 0x18);
    parent.generation = CM_U8(root, 0x74);
    if (!actor_live(&child) || !actor_live(&parent) ||
        CM_U16(root, 0x5c) != CM_ENTITY ||
        CM_U16(root, 0x5e) != CM_ENTITY ||
        CM_U16(task, 0x5e) != CM_ENTITY)
        return;
    s_child = child;
    s_root = parent;
    s_visit = anchor_boss_invite_world_visit();
}

void *anchor_control_machine_bound_task(void)
{
    return battle_child_bound() ? s_child.task : 0;
}

void *anchor_control_machine_bound_root(void)
{
    return battle_child_bound() ? s_root.task : 0;
}

void *anchor_control_machine_hud_task(void)
{
    unsigned int hp;
    if (!battle_child_bound() || death_callback(CM_AI(s_child.task)))
        return 0;
    hp = CM_U8(s_child.task, 0x8d);
    return hp >= 1u && hp <= 5u ? s_child.task : 0;
}

int anchor_control_machine_hud_health(unsigned int *health)
{
    void *task;
    if (!health || !(task = anchor_control_machine_hud_task()))
        return 0;
    *health = CM_U8(task, 0x8d);
    return 1;
}
