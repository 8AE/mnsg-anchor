/* Remote Ryo pickup visuals keep native motion/lifetime but mirror the native
 * 24-byte object-owned material into renderer-visible low RDRAM at draw time.
 * The category-8 child's cleanup callback and all gameplay callbacks remain
 * native. Only a verified remote tombstone calls the spawn entry point. */
#include "world/anchor_world_pickup_effects.h"
#include "player/anchor_remote_model_pool.h"
#ifndef ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
#include "platform/modding.h"
#endif

#define PICKUP_SLOT_LIMIT 8
#define PICKUP_MATERIAL_BYTES 32u
#define PICKUP_ARENA_BYTES (PICKUP_SLOT_LIMIT * 2u * PICKUP_MATERIAL_BYTES)
#define PICKUP_TASK_SCAN_LIMIT 256

extern void *D_8015CCBC;
extern short D_800C7A72_C8672;
extern unsigned short D_800C7AB2;
extern void *func_8021804C_5D351C(void *actor, int variant);
extern void func_80034EF8_35AF8(void *task);
extern void func_80211EF8_5CD3C8(void *, void *);
extern void func_80211F70_5CD440(void *, void *);
extern void func_80212020_5CD4F0(void *, void *);
extern void func_80218F30_5D4400(void *);

#ifndef ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
typedef struct PickupSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} PickupSceneResource;
extern PickupSceneResource D_80167FC0_168BC0[48];
#if DEBUG_BUTTON_ENABLED
extern void recomp_printf(const char *, ...);
#define PICKUP_LOG(...) recomp_printf(__VA_ARGS__)
#else
#define PICKUP_LOG(...) ((void)0)
#endif
#else
#define PICKUP_LOG(...) ((void)0)
extern unsigned int pickup_test_encode_pointer(const void *);
extern void *pickup_test_decode_pointer(unsigned int);
#define RECOMP_HOOK(name)
#endif

typedef struct PickupVisual
{
    void *task;
    void *object;
    unsigned int generation;
    unsigned short room;
} PickupVisual;

static PickupVisual s_visuals[PICKUP_SLOT_LIMIT];
static unsigned char *s_material_arena;
static unsigned int s_generation;

static void *decode_pointer(unsigned int value)
{
#ifdef ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
    return pickup_test_decode_pointer(value);
#else
    return (void *)(unsigned long)value;
#endif
}

static void *read_pointer(const void *record, unsigned int offset)
{
    unsigned int value;
    __builtin_memcpy(&value, (const unsigned char *)record + offset,
                     sizeof(value));
    return decode_pointer(value);
}

static unsigned short read_depth(const void *task)
{
    unsigned short depth;
    __builtin_memcpy(&depth, (const unsigned char *)task + 0x20,
                     sizeof(depth));
    return depth;
}

static int low_graphics_pointer(const void *pointer)
{
#ifdef ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
    return pointer && pointer != (void *)-1 &&
           !anchor_remote_model_pool_contains(pointer);
#else
    unsigned int phys = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return phys >= 0x1000u && phys < 0x800000u;
#endif
}

static int cpu_record_pointer(const void *pointer)
{
    return low_graphics_pointer(pointer) ||
           anchor_remote_model_pool_contains(pointer);
}

static int pickup_callback(const void *task)
{
    void *callback = read_pointer(task, 0x0c);
    return callback == (void *)func_80211EF8_5CD3C8 ||
           callback == (void *)func_80211F70_5CD440 ||
           callback == (void *)func_80212020_5CD4F0;
}

static int linked_pickup_task(const void *task)
{
    unsigned char *root = D_8015CCBC;
    unsigned char *current;
    unsigned short depth;
    int i;
    if (!cpu_record_pointer(root) || !cpu_record_pointer(task))
        return 0;
    depth = read_depth(root);
    current = read_pointer(root, 0);
    for (i = 0; current && i < PICKUP_TASK_SCAN_LIMIT; ++i)
    {
        void *backlink;
        if (!cpu_record_pointer(current) || read_depth(current) <= depth ||
            !cpu_record_pointer(backlink = read_pointer(current, 4)) ||
            read_pointer(backlink, 0) != current)
            return 0;
        if (current == task)
            return read_depth(current) == depth + 1 &&
                   read_pointer(current, 0x10) ==
                       (void *)func_80218F30_5D4400 &&
                   pickup_callback(current);
        current = read_pointer(current, 0);
    }
    return 0;
}

static int live_visual(const PickupVisual *visual)
{
    return visual->task && visual->generation == s_generation &&
           cpu_record_pointer(visual->object) &&
           linked_pickup_task(visual->task) &&
           read_pointer(visual->task, 0x18) == visual->object;
}

void anchor_world_pickup_effects_reset(void)
{
    int i;
    for (i = 0; i < PICKUP_SLOT_LIMIT; ++i)
    {
        if (live_visual(&s_visuals[i]))
            func_80034EF8_35AF8(s_visuals[i].task);
        s_visuals[i].task = 0;
        s_visuals[i].object = 0;
    }
    ++s_generation;
    if (!s_generation)
        ++s_generation;
}

void anchor_world_pickup_effects_load_resources(void)
{
    anchor_world_pickup_effects_reset();
    s_material_arena = 0;
#ifdef ANCHOR_WORLD_PICKUP_EFFECTS_HOST_TEST
    {
        static unsigned char materials[PICKUP_ARENA_BYTES];
        s_material_arena = materials;
    }
#else
    {
        unsigned int start, end;
        int i;
        for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
            ;
        if (i == 48 || !low_graphics_pointer(D_80167FC0_168BC0[i].data))
        {
            PICKUP_LOG("[pickup_fx] scene cursor unavailable\n");
            return;
        }
        start = ((unsigned int)(unsigned long)D_80167FC0_168BC0[i].data +
                 15u) & ~15u;
        end = start + PICKUP_ARENA_BYTES;
        if (end < start || (end & 0x1fffffffu) > 0x800000u)
        {
            PICKUP_LOG("[pickup_fx] low material arena unavailable\n");
            return;
        }
        s_material_arena = (unsigned char *)(unsigned long)start;
        D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)end;
        PICKUP_LOG("[pickup_fx] material arena %x..%x\n", start, end);
    }
#endif
}

void anchor_world_pickup_effects_spawn(void *actor, unsigned short room)
{
    void *child, *object;
    int i, free_slot = -1;
    if (!actor || !s_material_arena || room != D_800C7AB2)
        return;
    for (i = 0; i < PICKUP_SLOT_LIMIT; ++i)
    {
        if (s_visuals[i].task && !live_visual(&s_visuals[i]))
            s_visuals[i].task = s_visuals[i].object = 0;
        if (free_slot < 0 && !s_visuals[i].task)
            free_slot = i;
    }
    if (free_slot < 0)
    {
        PICKUP_LOG("[pickup_fx] active visual cap reached\n");
        return;
    }
    child = func_8021804C_5D351C(actor, 0);
    if (!cpu_record_pointer(child) || !linked_pickup_task(child))
    {
        PICKUP_LOG("[pickup_fx] native child allocation/link failed\n");
        return;
    }
    object = read_pointer(child, 0x18);
    if (!cpu_record_pointer(object))
    {
        PICKUP_LOG("[pickup_fx] native child has no model object\n");
        return;
    }
    s_visuals[free_slot].task = child;
    s_visuals[free_slot].object = object;
    s_visuals[free_slot].room = room;
    s_visuals[free_slot].generation = s_generation;
}

/* Native allocator reset precedes reuse of this task address. Forget its
 * remote-only identity before a new local pickup can inherit both task and
 * model addresses and callback identities. */
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_pickup_effects_task_reset(void *task)
{
    int i;
    for (i = 0; i < PICKUP_SLOT_LIMIT; ++i)
        if (s_visuals[i].task == task)
            s_visuals[i].task = s_visuals[i].object = 0;
}

/* This hook runs immediately before the stock renderer reads object+0x30.
 * The native fade callback rewrites object+0x80 each tick; select only the
 * graphics bank currently owned by native submission, even if frames skip. */
RECOMP_HOOK("func_80016C44_17844")
void anchor_world_pickup_effects_before_draw(void *object)
{
    int i, bank = D_800C7A72_C8672;
    if (!s_material_arena || bank < 0 || bank > 1)
        return;
    for (i = 0; i < PICKUP_SLOT_LIMIT; ++i)
    {
        PickupVisual *visual = &s_visuals[i];
        unsigned char *target;
        if (visual->object != object)
            continue;
        if (!live_visual(visual))
        {
            visual->task = visual->object = 0;
            return;
        }
        if (visual->room != D_800C7AB2)
        {
            *(unsigned int *)((unsigned char *)object + 0x2c) = 0;
            return;
        }
        if (read_pointer(visual->task, 0x0c) ==
            (void *)func_80211EF8_5CD3C8)
            return; /* Native initializer has not written object+0x80 yet. */
        target = s_material_arena +
            (i * 2 + bank) * PICKUP_MATERIAL_BYTES;
        __builtin_memcpy(target, (unsigned char *)object + 0x80, 24);
        *(unsigned int *)((unsigned char *)object + 0x30) =
            (unsigned int)(unsigned long)target | 0x60000000u;
        return;
    }
}
