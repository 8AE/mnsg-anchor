#include "anchor_miracle_moon.h"
#include "anchor_remote_model_pool.h"

#ifndef ANCHOR_MIRACLE_MOON_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void func_0800532C_6C4A7C(void *actor, void *object);
extern void func_080053A4_6C4AF4(void *actor, void *object);

#define MOON_ROOM 0x16u
#define MOON_ENTITY 0x350u
#define MOON_REMOVE_PENDING 2u
#define MOON_CALLBACK_DISABLED 0x00800000ul
#define MOON_BYTE(p, off) (*(unsigned char *)((unsigned char *)(p) + (off)))
#define MOON_HALF(p, off) (*(unsigned short *)((unsigned char *)(p) + (off)))
#define MOON_WORD(p, off) (*(unsigned int *)((unsigned char *)(p) + (off)))
#ifndef MOON_READ_POINTER
#define MOON_READ_POINTER(p, off) (*(void **)((unsigned char *)(p) + (off)))
#endif

static void *s_moon;
static void *s_object;
static unsigned short s_actor;
static unsigned char s_generation;
static unsigned short s_room;
static int s_room_valid;
static int s_remote_completed;
static int s_local_pickup;
/* Resolve these overlay addresses while the constructor's file is loaded. */
static unsigned long volatile s_pickup_callback;
static unsigned long volatile s_completion_callback;

static int moon_pointer_valid(const void *pointer)
{
#ifdef ANCHOR_MIRACLE_MOON_HOST_TEST
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
    s_moon = s_object = 0;
    s_actor = s_generation = 0;
    s_local_pickup = 0;
    s_pickup_callback = s_completion_callback = 0;
}

void anchor_miracle_moon_reset(void)
{
    clear_binding();
    s_room = 0;
    s_room_valid = s_remote_completed = 0;
}

void anchor_miracle_moon_update_room(unsigned short current_room)
{
    if (!s_room_valid || s_room != current_room)
    {
        clear_binding();
        s_remote_completed = 0;
        s_room = current_room;
        s_room_valid = 1;
    }
}

void anchor_miracle_moon_remote_completed(unsigned short current_room)
{
    anchor_miracle_moon_update_room(current_room);
    if (current_room == MOON_ROOM)
        s_remote_completed = 1;
}

static int is_bound_moon(const void *actor)
{
    void *backlink;

    if (!actor || actor != s_moon || D_800C7AB2 != MOON_ROOM ||
        !s_room_valid || s_room != MOON_ROOM ||
        !moon_pointer_valid(actor) || !moon_pointer_valid(s_object))
        return 0;
    backlink = MOON_READ_POINTER(actor, 0x04);
    return moon_pointer_valid(backlink) &&
           MOON_READ_POINTER(backlink, 0) == actor &&
           MOON_READ_POINTER(actor, 0x18) == s_object &&
           MOON_HALF(actor, 0x5c) == s_actor &&
           MOON_HALF(actor, 0x5e) == MOON_ENTITY &&
           MOON_BYTE(actor, 0x74) == s_generation;
}

int anchor_miracle_moon_local_pickup_active(void)
{
    unsigned long callback;
    if (!is_bound_moon(s_moon) ||
        (MOON_WORD(s_moon, 0x68) & MOON_REMOVE_PENDING))
        return 0;
    if (s_local_pickup)
        return 1;
    /* 051F4/052A0 select 0532C through 8003521C, which immediately writes
     * task+C. The pickup entry hook runs on the next actor tick; packet
     * application and the current tick's finalizer must see this selection. */
    callback = (unsigned long)MOON_READ_POINTER(s_moon, 0x0c) &
               ~MOON_CALLBACK_DISABLED;
    return callback == s_pickup_callback || callback == s_completion_callback;
}

RECOMP_HOOK_RETURN("func_08005018_6C4768")
void anchor_miracle_moon_after_constructor(void)
{
    void *actor = D_8016DAB4_16E6B4;
    void *object;
    void *backlink;
    anchor_miracle_moon_update_room(D_800C7AB2);
    if (D_800C7AB2 != MOON_ROOM || !moon_pointer_valid(actor) ||
        MOON_HALF(actor, 0x5e) != MOON_ENTITY ||
        (MOON_WORD(actor, 0x68) & MOON_REMOVE_PENDING))
        return;
    object = MOON_READ_POINTER(actor, 0x18);
    backlink = MOON_READ_POINTER(actor, 0x04);
    if (!moon_pointer_valid(object) || !moon_pointer_valid(backlink) ||
        MOON_READ_POINTER(backlink, 0) != actor)
        return;
    clear_binding();
    s_moon = actor;
    s_object = object;
    s_actor = MOON_HALF(actor, 0x5c);
    s_generation = MOON_BYTE(actor, 0x74);
    s_pickup_callback = (unsigned long)func_0800532C_6C4A7C &
                        ~MOON_CALLBACK_DISABLED;
    s_completion_callback = (unsigned long)func_080053A4_6C4AF4 &
                            ~MOON_CALLBACK_DISABLED;
}

RECOMP_HOOK("func_0800532C_6C4A7C")
void anchor_miracle_moon_before_pickup(void *actor, void *object)
{
    (void)object;
    if (is_bound_moon(actor))
        s_local_pickup = 1;
}

RECOMP_HOOK_RETURN("func_080053A4_6C4AF4")
void anchor_miracle_moon_after_pickup(void)
{
    void *actor = D_8016DAB4_16E6B4;
    if (is_bound_moon(actor) &&
        (MOON_WORD(actor, 0x68) & MOON_REMOVE_PENDING))
    {
        clear_binding();
        s_remote_completed = 0;
    }
}

/* This finalizer immediately consumes removal bit 2 using native task cleanup.
 * Do not run the reward's completion callback: it releases global input and
 * must remain owned by a pickup actually started on this client. */
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_miracle_moon_before_finalizer(void *actor, void *object)
{
    (void)object;
    if (!s_remote_completed || !is_bound_moon(actor) ||
        anchor_miracle_moon_local_pickup_active())
        return;
    MOON_WORD(actor, 0x68) |= MOON_REMOVE_PENDING;
    clear_binding();
    s_remote_completed = 0;
}
