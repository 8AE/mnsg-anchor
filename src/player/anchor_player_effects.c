/**
 * @file anchor_player_effects.c
 * @brief Reproduce verified, visual-only remote transform particles.
 *
 * Native file_11 selector 0x22 creates four purple display tasks for both
 * Ebisumaru Mini transitions and Yae Mermaid transitions. Its child callback
 * reads only the owner's model pose and updates its own color/scale/lifetime.
 * We give it an actual scheduler manager under the remote render task, with
 * manager+0x5C pointing back to that task. No playable player constructor or
 * transform action callback runs for a remote peer.
 */

#include "player/anchor_player_effects.h"
#include "player/anchor_remote_model_pool.h"

#define CHARACTER_EBISUMARU 1
#define CHARACTER_YAE 3
#define ACTION_MINI_SHRINK 0x8e
#define ACTION_MINI_GROW 0x8f
#define ACTION_MERMAID_ENTER 0xaa
#define ACTION_MERMAID_EXIT 0xac
#define ACTION_SUDDEN_IMPACT_START 0x82
#define ACTION_SUDDEN_IMPACT_ACTIVE 0x83
#define TRANSFORM_RETRY_FRAMES 30
#define NATIVE_PURPLE_EFFECT_KIND 0x22u
#define NATIVE_SUDDEN_AURA_KIND 0x23u
#define CHARGE_OBJECT_COUNT 3
#define CHARGE_MATERIAL_WORDS 8
#define CHARGE_MATERIAL_BYTES \
    (ANCHOR_REMOTE_CHARGE_LIMIT * CHARGE_OBJECT_COUNT * 2 * 32u)
#define EFFECT_CHILD_LIMIT 10
#define EFFECT_MATERIAL_BYTES \
    (ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT * EFFECT_CHILD_LIMIT * 2 * 32u)
#if DEBUG_BUTTON_ENABLED && !defined(ANCHOR_PLAYER_EFFECTS_HOST_TEST)
extern void recomp_printf(const char *, ...);
#define EFFECT_LOG(...) recomp_printf(__VA_ARGS__)
#else
#define EFFECT_LOG(...) ((void)0)
#endif

/* Native manager/task ABI: +0x18 owns the render object, +0x5C is copied
 * into each effect child as its source owner. */
extern void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                                 unsigned short flags);
extern void func_80034EF8_35AF8(void *task);
extern void *func_800141C4_14DC4(unsigned int file_id);
extern short D_800C7A72_C8672;
extern int func_801E8C44_5A4B54(void *manager, unsigned char kind);
extern void func_801F11F0_5AD100(void *manager, unsigned int kind);
extern void func_801F15E0_5AD4F0(void *manager, unsigned int kind);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model,
    unsigned int material, float x, float y, float z,
    short rx, short ry, short rz, float sx, float sy, float sz,
    short file8, short file9);
#ifndef ANCHOR_PLAYER_EFFECTS_HOST_TEST
extern void *func_80013B14_14714(unsigned int file_id);
typedef struct EffectSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} EffectSceneResource;
extern EffectSceneResource D_80167FC0_168BC0[48];
#endif

static int s_manager_count;
static int s_aura_count;
static unsigned int s_charge_banks;
static unsigned int s_material_banks;
static unsigned int s_material_generation;
static unsigned int *s_charge_materials;
static unsigned char *s_effect_materials;
static unsigned char *s_charge_models;
static unsigned char *s_charge_textures;

typedef void (*EffectPostCallback)(void *, void *);
typedef struct EffectMaterialRecord
{
    void *task;
    void *object;
    void *manager;
    void *owner;
    EffectPostCallback old_post;
} EffectMaterialRecord;
static EffectMaterialRecord s_material_records
    [ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT][EFFECT_CHILD_LIMIT];

static void clear_material_records(void *storage, unsigned int bytes)
{
    volatile unsigned char *cursor = storage;
    unsigned int i;
    /* Volatile stores keep this bounded stage/manager cleanup inline on MIPS;
     * the mod ELF has no libc memset import for RecompModTool to resolve. */
    for (i = 0; i < bytes; ++i)
        cursor[i] = 0;
}

#ifndef ANCHOR_PLAYER_EFFECTS_HOST_TEST
static unsigned int physical(unsigned int address)
{
    return address & 0x1fffffffu;
}
#endif

static int resident_pointer(const void *pointer)
{
#ifdef ANCHOR_PLAYER_EFFECTS_HOST_TEST
    return pointer && pointer != (void *)-1 &&
           !anchor_remote_model_pool_contains(pointer);
#else
    unsigned int address = physical((unsigned int)(unsigned long)pointer);
    return address >= 0x1000u && address < 0x800000u;
#endif
}

static int cpu_record_pointer(const void *pointer)
{
    return resident_pointer(pointer) ||
           anchor_remote_model_pool_contains(pointer);
}

void anchor_player_effects_load_resources(void)
{
    void *models;
    void *textures;
    ++s_material_generation;
    if (!s_material_generation)
        ++s_material_generation;
    clear_material_records(s_material_records, sizeof(s_material_records));
    s_manager_count = 0;
    s_aura_count = 0;
    s_charge_banks = 0;
    s_material_banks = 0;
    s_charge_materials = 0;
    s_effect_materials = 0;
    s_charge_models = 0;
    s_charge_textures = 0;
#ifndef ANCHOR_PLAYER_EFFECTS_HOST_TEST
    func_80013B14_14714(0x80u);
    func_80013B14_14714(0x152u);
#endif
    models = func_800141C4_14DC4(0x80u);
    textures = func_800141C4_14DC4(0x152u);
    if (!resident_pointer(models) || !resident_pointer(textures))
    {
        EFFECT_LOG("[remote_effects] effect resources not resident\n");
        return;
    }
#ifdef ANCHOR_PLAYER_EFFECTS_HOST_TEST
    {
        static unsigned int host_materials
            [(CHARGE_MATERIAL_BYTES + EFFECT_MATERIAL_BYTES) / 4];
        s_charge_materials = host_materials;
        s_effect_materials = (unsigned char *)host_materials +
            CHARGE_MATERIAL_BYTES;
        s_charge_models = models;
        s_charge_textures = textures;
        EFFECT_LOG("[remote_effects] host effect materials ready\n");
    }
#else
    {
        unsigned int start, end;
        int i;
        for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
            ;
        if (i == 48 || !resident_pointer(D_80167FC0_168BC0[i].data))
            return;
        start = (physical((unsigned int)(unsigned long)
            D_80167FC0_168BC0[i].data) + 15u) & ~15u;
        end = start + CHARGE_MATERIAL_BYTES + EFFECT_MATERIAL_BYTES;
        if (end < start || end > 0x800000u)
        {
            EFFECT_LOG("[remote_effects] low material arena unavailable\n");
            return;
        }
        s_charge_materials = (unsigned int *)(unsigned long)(start | 0x80000000u);
        s_effect_materials = (unsigned char *)(unsigned long)
            ((start + CHARGE_MATERIAL_BYTES) | 0x80000000u);
        D_80167FC0_168BC0[i].data =
            (unsigned char *)(unsigned long)(end | 0x80000000u);
        s_charge_models = (unsigned char *)(unsigned long)
            (physical((unsigned int)(unsigned long)models) | 0x80000000u);
        s_charge_textures = (unsigned char *)(unsigned long)
            (physical((unsigned int)(unsigned long)textures) | 0x80000000u);
        EFFECT_LOG("[remote_effects] effect material arena %x..%x\n",
                   start, end);
    }
#endif
}

static void release_manager(AnchorPlayerEffectState *state, int owner_live)
{
    int i;
    int current;
    if (!state->manager)
        return;
    current = state->material_generation == s_material_generation;
    if (current && state->charge_bank >= 0 &&
        state->charge_bank < ANCHOR_REMOTE_CHARGE_LIMIT)
        s_charge_banks &= ~(1u << state->charge_bank);
    if (current && state->material_bank >= 0 &&
        state->material_bank < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT)
    {
        clear_material_records(s_material_records[state->material_bank],
                               sizeof(s_material_records[state->material_bank]));
        s_material_banks &= ~(1u << state->material_bank);
    }
    if (current && state->aura_occupied && s_aura_count > 0)
        --s_aura_count;
    if (current && owner_live)
        func_80034EF8_35AF8(state->manager);
    state->manager = 0;
    for (i = 0; i < CHARGE_OBJECT_COUNT; ++i)
        state->charge_object[i] = 0;
    state->charge_bank = -1;
    state->material_bank = -1;
    state->charge_alpha = 0;
    state->charge_scale = 0;
    state->aura_drop_pending = 0;
    state->aura_occupied = 0;
    if (current && s_manager_count > 0)
        --s_manager_count;
}

static void effect_manager_update(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void *read_task_pointer(const void *task, unsigned int offset)
{
    void *pointer;
    __builtin_memcpy(&pointer, (const unsigned char *)task + offset,
                     sizeof(pointer));
    return pointer;
}

static void write_task_pointer(void *task, unsigned int offset, void *pointer)
{
    __builtin_memcpy((unsigned char *)task + offset, &pointer,
                     sizeof(pointer));
}

static unsigned short task_depth(const void *task)
{
    unsigned short depth;
    __builtin_memcpy(&depth, (const unsigned char *)task + 0x20,
                     sizeof(depth));
    return depth;
}

/* Task +0 is a flat scheduler successor, not a null-terminated child list. */
static int manager_descendant(void *manager, void *task)
{
    unsigned char *child;
    unsigned short depth;
    int i;
    if (!cpu_record_pointer(manager))
        return 0;
    depth = task_depth(manager);
    child = read_task_pointer(manager, 0);
    for (i = 0; child && i < EFFECT_CHILD_LIMIT; ++i)
    {
        if (!cpu_record_pointer(child) || task_depth(child) <= depth)
            break;
        if (child == task)
            return 1;
        child = read_task_pointer(child, 0);
    }
    return 0;
}

static void material_post_update(void *task, void *object)
{
    int bank, i;
    for (bank = 0; bank < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT; ++bank)
        for (i = 0; i < EFFECT_CHILD_LIMIT; ++i)
        {
            EffectMaterialRecord *record = &s_material_records[bank][i];
            unsigned char *target;
            if (record->task != task)
                continue;
            if (!s_effect_materials || !cpu_record_pointer(task) ||
                !cpu_record_pointer(object) || record->object != object ||
                !cpu_record_pointer(record->manager) ||
                read_task_pointer(task, 0x18) != object ||
                read_task_pointer(task, 0x5c) != record->owner ||
                read_task_pointer(task, 0x84) != record->manager ||
                read_task_pointer(record->manager, 0x5c) != record->owner ||
                !manager_descendant(record->manager, task))
                return;
            if (record->old_post)
                record->old_post(task, object);
            if (!cpu_record_pointer(task) || !cpu_record_pointer(object) ||
                read_task_pointer(task, 0x18) != object ||
                read_task_pointer(task, 0x5c) != record->owner ||
                read_task_pointer(task, 0x84) != record->manager ||
                !manager_descendant(record->manager, task))
                return;
            if (D_800C7A72_C8672 < 0 || D_800C7A72_C8672 > 1)
                return;
            target = s_effect_materials +
                ((bank * EFFECT_CHILD_LIMIT + i) * 2 +
                 D_800C7A72_C8672) * 32u;
            __builtin_memcpy(target, (unsigned char *)task + 0xa4, 32);
            *(unsigned int *)((unsigned char *)object + 0x30) =
                (unsigned int)(unsigned long)target | 0x60000000u;
            return;
        }
}

static int attach_effect_material(AnchorPlayerEffectState *state,
                                  void *task, void *object)
{
    int i;
    EffectMaterialRecord *record;
    unsigned char *target;
    if (!s_effect_materials || state->material_bank < 0 ||
        state->material_bank >= ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT ||
        !cpu_record_pointer(task) || !cpu_record_pointer(object) ||
        read_task_pointer(task, 0x18) != object ||
        read_task_pointer(task, 0x5c) != state->owner_task ||
        read_task_pointer(task, 0x84) != state->manager ||
        !manager_descendant(state->manager, task))
        return 0;
    for (i = 0; i < EFFECT_CHILD_LIMIT; ++i)
    {
        record = &s_material_records[state->material_bank][i];
        if (record->task == task)
        {
            if (record->object == object &&
                read_task_pointer(task, 0x10) == (void *)material_post_update)
                return 1;
            break; /* Reused native task slot, reset by a new constructor. */
        }
        if (!record->task ||
            !manager_descendant(state->manager, record->task) ||
            ((unsigned char *)record->task)[0x65])
            break;
    }
    if (i == EFFECT_CHILD_LIMIT)
        return 0;
    if (D_800C7A72_C8672 < 0 || D_800C7A72_C8672 > 1)
        return 0;
    record->task = task;
    record->object = object;
    record->manager = state->manager;
    record->owner = state->owner_task;
    record->old_post = read_task_pointer(task, 0x10);
    if (record->old_post == material_post_update)
        return 0;
    target = s_effect_materials +
        ((state->material_bank * EFFECT_CHILD_LIMIT + i) * 2 +
         D_800C7A72_C8672) * 32u;
    __builtin_memcpy(target, (unsigned char *)task + 0xa4, 32);
    *(unsigned int *)((unsigned char *)object + 0x30) =
        (unsigned int)(unsigned long)target | 0x60000000u;
    write_task_pointer(task, 0x10, (void *)material_post_update);
    return 1;
}

static void hide_effect_child(void *task)
{
    unsigned char *child = task;
    unsigned char *object;
    if (!cpu_record_pointer(child))
        return;
    object = read_task_pointer(child, 0x18);
    if (cpu_record_pointer(object))
    {
        *(unsigned int *)(object + 0x2c) = 0;
        object[0x64] |= 1u;
    }
}

void anchor_player_effects_suspend(AnchorPlayerEffectState *state)
{
    unsigned char *child;
    unsigned short depth;
    int i;
    if (!state || !state->initialized ||
        state->material_generation != s_material_generation)
        return;
    if (cpu_record_pointer(state->owner_task))
        ((unsigned char *)state->owner_task)[0xcc] = 0;
    for (i = 0; i < CHARGE_OBJECT_COUNT; ++i)
        if (state->charge_object[i])
        {
            unsigned char *object = state->charge_object[i];
            *(unsigned int *)(object + 0x2c) = 0;
            object[0x64] |= 1u;
        }
    state->charge_alpha = 0;
    state->purple_pending_action = -1;
    state->purple_retry_frames = 0;
    if (!state->manager)
        return;
    if (!cpu_record_pointer(state->manager))
        return;
    depth = task_depth(state->manager);
    child = read_task_pointer(state->manager, 0);
    for (i = 0; child && i < EFFECT_CHILD_LIMIT; ++i)
    {
        unsigned char *next;
        if (!cpu_record_pointer(child) || task_depth(child) <= depth)
            break;
        next = read_task_pointer(child, 0);
        if (read_task_pointer(child, 0x84) == state->manager &&
            read_task_pointer(child, 0x5c) == state->owner_task)
            hide_effect_child(child);
        child = next;
    }
    state->aura_drop_pending = 1;
}

static int purple_transform_action(int ch, int action)
{
    return (ch == CHARACTER_EBISUMARU &&
            (action == ACTION_MINI_SHRINK || action == ACTION_MINI_GROW)) ||
           (ch == CHARACTER_YAE &&
            (action == ACTION_MERMAID_ENTER || action == ACTION_MERMAID_EXIT));
}

void anchor_player_effects_reset(AnchorPlayerEffectState *state, int owner_live)
{
    if (!state)
        return;
    /* A live manager never self-deletes; its inert callback leaves the exact
     * owned subtree to the native destructor. Once the remote owner is gone,
     * the engine has already reclaimed that subtree. */
    release_manager(state, owner_live);
    state->owner_task = 0;
    state->cid = 0;
    state->session = 0;
    state->epoch = 0;
    state->ch = -1;
    state->last_action = -1;
    state->purple_pending_action = -1;
    state->purple_retry_frames = 0;
    state->charge_bank = -1;
    state->material_bank = -1;
    state->material_generation = 0;
    state->charge_alpha = 0;
    state->charge_scale = 0;
    state->charge_spin = 0;
    state->aura_spawned = 0;
    state->aura_drop_pending = 0;
    state->aura_occupied = 0;
    state->aura_phase_seen = 0;
    state->aura_retry_frames = 0;
    state->room = 0;
    state->initialized = 0;
}

void anchor_player_effects_set_context(AnchorPlayerEffectState *state,
                                       void *remote_task,
                                       const AnchorPlayerModelRemote *remote,
                                       unsigned short room)
{
    if (!state || !remote || !remote_task)
        return;
    if (state->initialized &&
        state->material_generation != s_material_generation)
        anchor_player_effects_reset(state, 0);
    /* The native purple children mark themselves retired with task+0x65.
     * Reclaim their now-empty manager at frame end, freeing the shared cap
     * for another peer without changing the scheduler during a callback. */
    if (state->manager && state->owner_task == remote_task &&
        state->aura_drop_pending)
        release_manager(state, 1);
    if (state->manager && state->owner_task == remote_task &&
        !(remote->appearance_flags & ANCHOR_APPEARANCE_WEAPON_CHARGE) &&
        state->charge_alpha == 0 &&
        func_801E8C44_5A4B54(state->manager,
                             NATIVE_PURPLE_EFFECT_KIND) == 0 &&
        func_801E8C44_5A4B54(state->manager,
                             NATIVE_SUDDEN_AURA_KIND) == 0)
        release_manager(state, 1);
    if (state->initialized && state->owner_task == remote_task &&
        state->cid == remote->cid &&
        state->session == remote->interaction_session &&
        state->epoch == remote->player_epoch &&
        state->room == room && state->ch == remote->ch)
        return;

    /* The caller verifies that remote_task is still Anchor's linked render
     * child. This runs at frame end so deleting an old manager never mutates
     * the scheduler list while it is visiting that child's callback. */
    anchor_player_effects_reset(state,
        state->manager && state->owner_task == remote_task);
    state->owner_task = remote_task;
    state->cid = remote->cid;
    state->session = remote->interaction_session;
    state->epoch = remote->player_epoch;
    state->room = room;
    state->ch = remote->ch;
    state->material_generation = s_material_generation;
    state->initialized = 1;
}

static int purple_resources_resident(void)
{
    void *sheet = func_800141C4_14DC4(0x80u);
    void *textures = func_800141C4_14DC4(0x152u);

    /* file 0x80 is the shared native effect sheet, and 0x152 supplies its
     * animated texture segment. Both are staged during normal stage load by
     * the existing projectile/resource modules. Never load them per frame. */
#ifdef ANCHOR_PLAYER_EFFECTS_HOST_TEST
    return sheet && sheet != (void *)-1 &&
           textures && textures != (void *)-1;
#else
    {
        unsigned int a = (unsigned int)(unsigned long)sheet & 0x1fffffffu;
        unsigned int b = (unsigned int)(unsigned long)textures & 0x1fffffffu;

        return a >= 0x1000u && a < 0x800000u &&
               b >= 0x1000u && b < 0x800000u;
    }
#endif
}

static int ensure_manager(AnchorPlayerEffectState *state, void *remote_task)
{
    int bank;
    if (state->manager)
        return 1;
    if (s_manager_count >= ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT ||
        !s_effect_materials)
        return 0;
    for (bank = 0; bank < ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT; ++bank)
        if (!(s_material_banks & (1u << bank)))
            break;
    if (bank == ANCHOR_REMOTE_EFFECT_MANAGER_LIMIT)
        return 0;
    state->manager = func_80034E08_35A08(remote_task,
                                          effect_manager_update, 0);
    if (!state->manager)
        return 0;
    write_task_pointer(state->manager, 0x5c, remote_task);
    state->material_bank = bank;
    s_material_banks |= 1u << bank;
    ++s_manager_count;
    return 1;
}

static void charge_material(unsigned int commands[CHARGE_MATERIAL_WORDS],
                            int alpha)
{
    /* Exact file_11 func_801DC554 wrapper used by selector 0x11:
     * primitive FF F5 B8, environment FF FA 24 <alpha>. */
    commands[0] = 0x06000000u;
    commands[1] = 0xe0204c28u;
    commands[2] = 0xfa000000u;
    commands[3] = 0xfff5b800u;
    commands[4] = 0xfb000000u;
    commands[5] = 0xfffa2400u | (unsigned int)alpha;
    commands[6] = 0xb8000000u;
    commands[7] = 0;
}

static void apply_charge(AnchorPlayerEffectState *state, void *remote_task,
                         const void *remote_object,
                         const AnchorPlayerModelRemote *remote)
{
    static const unsigned int models[CHARGE_OBJECT_COUNT] = {
        0x1900012cu, 0x1900025cu, 0x1900037cu
    };
    static const unsigned short full_spin[CHARGE_OBJECT_COUNT] = {
        0x1cu, 0x08u, 0x10u
    };
    static const unsigned short fade_spin[CHARGE_OBJECT_COUNT] = {
        0x24u, 0x0cu, 0x14u
    };
    int active = (remote->appearance_flags & ANCHOR_APPEARANCE_WEAPON_CHARGE) != 0;
    int full = (remote->appearance_flags &
        ANCHOR_APPEARANCE_WEAPON_CHARGE_FULL) != 0;
    float owner_scale;
    int graphics_bank = D_800C7A72_C8672;
    int i;

    if (remote->ch != 0 && remote->ch != CHARACTER_EBISUMARU &&
        remote->ch != 2)
        active = full = 0;
    if (!s_charge_materials || !s_charge_models || !s_charge_textures ||
        graphics_bank < 0 || graphics_bank > 1)
    {
        state->charge_alpha = 0;
        for (i = 0; i < CHARGE_OBJECT_COUNT; ++i)
            if (state->charge_object[i])
            {
                unsigned char *object = state->charge_object[i];
                *(unsigned int *)(object + 0x2c) = 0;
                object[0x64] |= 1u;
            }
        return;
    }
    if (active && !ensure_manager(state, remote_task))
        return;
    if (active && state->charge_bank < 0)
    {
        for (i = 0; i < ANCHOR_REMOTE_CHARGE_LIMIT; ++i)
            if (!(s_charge_banks & (1u << i)))
            {
                state->charge_bank = i;
                s_charge_banks |= 1u << i;
                break;
            }
        if (state->charge_bank < 0)
            return;
    }
    if (state->charge_bank < 0)
        return;

    owner_scale = *(const float *)((const unsigned char *)remote_object + 0x1c);
    if (!(owner_scale > 0.0f && owner_scale <= 2.0f))
        return;
    if (active)
    {
        state->charge_alpha = 0xff;
        state->charge_scale += 0.01f;
        if (state->charge_scale > owner_scale)
            state->charge_scale = owner_scale;
    }
    else
    {
        state->charge_alpha -= 0x20;
        if (state->charge_alpha < 0)
            state->charge_alpha = 0;
        state->charge_scale += 0.002f;
    }
    for (i = 0; i < CHARGE_OBJECT_COUNT; ++i)
    {
        unsigned char *object = state->charge_object[i];
        unsigned int *commands;
        unsigned short yaw;
        if (!object && active)
        {
            object = func_8000DBF0_E7F0(state->manager, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            state->charge_object[i] = object;
            if (object)
                *(unsigned short *)(object + 0x16) =
                    *(const unsigned short *)((const unsigned char *)remote_object + 0x16);
        }
        if (!object)
            continue;
        if (state->charge_alpha == 0)
        {
            *(unsigned int *)(object + 0x2c) = 0;
            object[0x64] |= 1u;
            continue;
        }
        commands = s_charge_materials +
            ((state->charge_bank * CHARGE_OBJECT_COUNT + i) * 2 +
             graphics_bank) * CHARGE_MATERIAL_WORDS;
        charge_material(commands, state->charge_alpha);
        *(unsigned int *)(object + 0x2c) = models[i];
        *(unsigned int *)(object + 0x30) =
            (unsigned int)(unsigned long)commands | 0x60000000u;
        *(unsigned short *)(object + 0x3c) = 0x80;
        *(void **)(object + 0x40) = s_charge_models;
        *(unsigned short *)(object + 0x44) = 0x152;
        *(void **)(object + 0x48) = s_charge_textures;
        *(float *)(object + 8) =
            *(const float *)((const unsigned char *)remote_object + 8);
        *(float *)(object + 0xc) =
            *(const float *)((const unsigned char *)remote_object + 0xc) +
            (remote->ch == 2 ? 30.0f : 50.0f) * owner_scale;
        *(float *)(object + 0x10) =
            *(const float *)((const unsigned char *)remote_object + 0x10);
        *(unsigned short *)(object + 0x14) =
            *(const unsigned short *)((const unsigned char *)remote_object + 0x14);
        *(unsigned short *)(object + 0x18) =
            *(const unsigned short *)((const unsigned char *)remote_object + 0x18);
        yaw = *(unsigned short *)(object + 0x16);
        if (full)
            yaw = (unsigned short)((yaw + full_spin[i]) & 0x3ffu);
        else if (!active)
            yaw = (unsigned short)((yaw + fade_spin[i]) & 0x3ffu);
        *(unsigned short *)(object + 0x16) = yaw;
        *(float *)(object + 0x1c) = state->charge_scale;
        *(float *)(object + 0x20) = state->charge_scale;
        *(float *)(object + 0x24) = state->charge_scale;
        object[5] = 7;
        object[0x64] &= ~1u;
        object[0x65] = 0;
    }
}

static int aura_child_object(void *task, void *owner_task, void *broad,
                             AnchorPlayerEffectState *state)
{
    unsigned char *object;
    if (!cpu_record_pointer(task) ||
        read_task_pointer(task, 0x5c) != owner_task)
        return 0;
    object = read_task_pointer(task, 0x18);
    if (!cpu_record_pointer(object))
        return 0;
#ifndef ANCHOR_PLAYER_EFFECTS_HOST_TEST
    if (*(unsigned short *)(object + 0x3c) != 0x120u ||
        *(unsigned short *)(object + 0x44) != 0x152u)
        return 0;
#endif
    *(void **)(object + 0x40) = broad;
    return attach_effect_material(state, task, object);
}

static int rebind_aura_tree(AnchorPlayerEffectState *state, void *broad)
{
    unsigned char *child = read_task_pointer(state->manager, 0);
    unsigned short depth = task_depth(state->manager);
    int scanned = 0, mains = 0, satellites = 0;
    while (child && scanned++ < EFFECT_CHILD_LIMIT)
    {
        unsigned short child_depth;
        if (!cpu_record_pointer(child))
            return 0;
        child_depth = task_depth(child);
        if (child_depth <= depth)
            break;
        if (mains && child_depth <= depth + 1)
            break; /* The five satellites must remain below this main. */
        if (child[0x64] == NATIVE_SUDDEN_AURA_KIND &&
            child_depth == depth + 1)
        {
            if (++mains != 1 ||
                !aura_child_object(child, state->owner_task, broad, state))
                return 0;
        }
        else if (mains && child_depth == depth + 2 &&
                 child[0x64] == 0 && satellites < 5)
        {
            if (!aura_child_object(child, state->owner_task, broad, state))
                return 0;
            ++satellites;
        }
        else if (mains)
            return 0;
        child = read_task_pointer(child, 0);
    }
    return mains == 1 && satellites == 5;
}

static int attach_purple_tree(AnchorPlayerEffectState *state)
{
    unsigned char *child = read_task_pointer(state->manager, 0);
    unsigned short depth = task_depth(state->manager);
    int scanned = 0, purple = 0;
    while (child && scanned++ < EFFECT_CHILD_LIMIT)
    {
        unsigned char *object;
        if (!cpu_record_pointer(child))
            return 0;
        if (task_depth(child) <= depth)
            break;
        if (task_depth(child) == depth + 1 &&
            child[0x64] == NATIVE_PURPLE_EFFECT_KIND)
        {
            object = read_task_pointer(child, 0x18);
            if (!attach_effect_material(state, child, object))
                return 0;
            ++purple;
        }
        child = read_task_pointer(child, 0);
    }
    return purple == 4;
}

static void apply_sudden_aura(AnchorPlayerEffectState *state,
                              void *remote_task, const void *remote_object,
                              const AnchorPlayerModelRemote *remote)
{
    void *broad;
    unsigned short broad_id;
    unsigned char *task = remote_task;
    int active_phase;
    /* Raw aura children read only owner character/action and its object pose.
     * The remote cutscene task has no gameplay callback, and we provide those
     * two synthetic bytes for this verified native visual constructor. */
    task[0x90] = (unsigned char)remote->ch;
    task[0xcc] = (unsigned char)remote->action;
    active_phase = remote->ch == 0 &&
        (remote->action == ACTION_SUDDEN_IMPACT_START ||
         (remote->action == ACTION_SUDDEN_IMPACT_ACTIVE &&
          ((remote->appearance_flags & ANCHOR_APPEARANCE_SUDDEN_IMPACT) ||
           state->aura_phase_seen)));
    if (!active_phase)
    {
        state->aura_spawned = 0;
        state->aura_phase_seen = 0;
        state->aura_retry_frames = 0;
        return;
    }
    if (!state->aura_phase_seen)
    {
        state->aura_phase_seen = 1;
        state->aura_retry_frames = TRANSFORM_RETRY_FRAMES;
    }
    /* Native action 0x82 starts the aura at frame 4. A latest-state packet
     * can miss that phase and first show action 0x83 with gold already set;
     * its native aura callbacks explicitly support that active action. */
    if (remote->action == ACTION_SUDDEN_IMPACT_START &&
        *(const float *)((const unsigned char *)remote_object + 0x28) < 4.0f)
        return;
    if (state->aura_spawned || state->aura_drop_pending ||
        state->aura_retry_frames == 0)
        return;
    --state->aura_retry_frames;
    if ((!state->aura_occupied && s_aura_count >= ANCHOR_REMOTE_AURA_LIMIT) ||
        !resident_pointer(func_800141C4_14DC4(0x152u)))
        return;
    broad = read_task_pointer(remote_object, 0x40);
    broad_id = *(const unsigned short *)((const unsigned char *)remote_object + 0x3c);
    if (broad_id != 0x120u || !resident_pointer(broad) ||
        !ensure_manager(state, remote_task))
        return;
    if (func_801E8C44_5A4B54(state->manager,
                             NATIVE_SUDDEN_AURA_KIND) != 0)
        return;
    if (!state->aura_occupied)
    {
        ++s_aura_count;
        state->aura_occupied = 1;
    }
    func_801F15E0_5AD4F0(state->manager, NATIVE_SUDDEN_AURA_KIND);
    EFFECT_LOG("[remote_effects] aura constructor children=%d\n",
               func_801E8C44_5A4B54(state->manager,
                                     NATIVE_SUDDEN_AURA_KIND));
    if (rebind_aura_tree(state, broad))
    {
        state->aura_spawned = 1;
        state->aura_retry_frames = 0;
    }
    else
    {
        EFFECT_LOG("[remote_effects] aura tree/material unavailable\n");
        /* Keep the native scheduler untouched inside this callback. Hide any
         * partial tree now and retire its manager at the next frame end. */
        anchor_player_effects_suspend(state);
        state->aura_drop_pending = 1;
    }
}

void anchor_player_effects_apply(AnchorPlayerEffectState *state,
                                  void *remote_task, void *remote_object,
                                  const AnchorPlayerModelRemote *remote,
                                  unsigned short room)
{
    int old_action;

    if (!state || !state->initialized || !remote || !remote_task ||
        state->material_generation != s_material_generation ||
        !remote_object || state->owner_task != remote_task ||
        state->cid != remote->cid ||
        state->session != remote->interaction_session ||
        state->epoch != remote->player_epoch ||
        state->room != room || state->ch != remote->ch ||
        read_task_pointer(remote_task, 0x18) != remote_object)
        return;

    apply_sudden_aura(state, remote_task, remote_object, remote);
    apply_charge(state, remote_task, remote_object, remote);
    old_action = state->last_action;
    if (remote->action != old_action)
    {
        state->last_action = remote->action;
        state->purple_pending_action = -1;
        state->purple_retry_frames = 0;
        if (purple_transform_action(remote->ch, remote->action))
        {
            /* Position snapshots can first arrive well after native frame 3.
             * The native burst belongs to the action entry, so start it once
             * while this observed transition action is still current. */
            state->purple_pending_action = remote->action;
            state->purple_retry_frames = TRANSFORM_RETRY_FRAMES;
        }
    }
    if (state->purple_pending_action != remote->action ||
        state->purple_retry_frames == 0)
        return;
    --state->purple_retry_frames;
    if (!purple_resources_resident() || !ensure_manager(state, remote_task) ||
        func_801E8C44_5A4B54(state->manager,
                             NATIVE_PURPLE_EFFECT_KIND) != 0)
        return;
    /* The native constructor checks its own manager child list and refuses
     * another kind-0x22 burst until the prior four visual children retire. */
    func_801F11F0_5AD100(state->manager, NATIVE_PURPLE_EFFECT_KIND);
    EFFECT_LOG("[remote_effects] purple constructor children=%d\n",
               func_801E8C44_5A4B54(state->manager,
                                     NATIVE_PURPLE_EFFECT_KIND));
    if (func_801E8C44_5A4B54(state->manager,
                             NATIVE_PURPLE_EFFECT_KIND) > 0 &&
        attach_purple_tree(state))
    {
        state->purple_pending_action = -1;
        state->purple_retry_frames = 0;
    }
    else
    {
        EFFECT_LOG("[remote_effects] purple constructor/tree failed\n");
        anchor_player_effects_suspend(state);
    }
}
