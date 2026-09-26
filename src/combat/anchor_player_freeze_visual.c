#include "combat/anchor_player_freeze_visual.h"
#include "combat/anchor_player_cube.h"
#include "combat/anchor_player_freeze.h"
#include "core/anchor_dialog.h"
#include "player/anchor_player_models.h"
#include "player/anchor_remote_model_pool.h"
#include "platform/modding.h"
#include "platform/recomputils.h"

extern void *D_801FC604_5B8514;
extern unsigned short D_800C7AB2;
extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned int *D_8015C5CC_15D1CC;
extern unsigned char *D_80168504_169104;
extern short D_800C7A72_C8672;
extern void *func_800141C4_14DC4(unsigned int file_id);
extern void *func_80013B14_14714(unsigned int file_id);
extern void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                                unsigned short flags);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                              float x, float y, float z,
                              short rx, short ry, short rz,
                              float sx, float sy, float sz,
                              short file8, short file9);

typedef struct FreezeSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} FreezeSceneResource;
extern FreezeSceneResource D_80167FC0_168BC0[48];

typedef struct FreezeVisualSlot
{
    void *task;
    void *object;
    AnchorFreezeVisualTarget target;
    unsigned short room;
    unsigned char active, seen, shard_frames, shattered, tombstoned;
    unsigned char impact_frames;
} FreezeVisualSlot;

/* The enemy ice child uses this immutable .file_12 material template. Copy
 * the game commands to owned low RDRAM so overlay changes cannot invalidate
 * a live cube. The wrapper supplies each cube's native translucent env RGBA. */
static const unsigned int s_cube_template[22] = {
    0xe7000000u, 0, 0xba001402u, 0, 0xba001301u, 0x00080000u,
    0xba000602u, 0x000000c0u, 0xba000903u, 0x00000c00u,
    0xb6000000u, 0x001f3205u, 0xb7000000u, 0x00002205u,
    0xb900031du, 0x004045d8u, 0xb9000002u, 1u,
    0xf9000000u, 1u, 0xb8000000u, 0,
};
#define CUBE_TEMPLATE_BYTES (sizeof(s_cube_template))
#define CUBE_WRAPPER_WORDS 8u
#define CUBE_ARENA_BYTES (CUBE_TEMPLATE_BYTES + ANCHOR_FREEZE_VISUAL_MAX * CUBE_WRAPPER_WORDS * 4u)
#define SHARD_FRAMES 40u
#define IMPACT_FRAMES 32u
#define NATIVE_BANK_BYTES 0x1d6d8u
#define NATIVE_COMMAND_BYTES 0x14c80u
#define NATIVE_MATRIX_END 0x1d380u
#define NATIVE_COMMAND_TAIL (512u * 8u)
#define NATIVE_DIALOG_TAIL (2048u * 8u)
#define ICE_DRAW_COMMAND_ALLOWANCE 1024u
#define ICE_MATRIX_RESERVE (16u * 64u)

static FreezeVisualSlot s_slots[ANCHOR_FREEZE_VISUAL_MAX];
static unsigned char *s_resources[3];
static unsigned int *s_material_arena;
static void *s_owner;
static unsigned char *s_guard_object;
static unsigned char s_guard_hidden;
static void freeze_visual_task_update(void *task, void *object);
#if DEBUG_BUTTON_ENABLED
static int s_resource_reported;
static int s_draw_skip_reported;
static int s_slot_skip_reported;
static void report_resources(int state)
{
    if (state != s_resource_reported)
    {
        s_resource_reported = state;
        recomp_printf("[player_ice] visual resources state=%d\n", state);
    }
}
#endif

static unsigned int physical(unsigned int address)
{
    return address & 0x1fffffffu;
}

static int rdram(const void *pointer)
{
    unsigned int address = physical((unsigned int)(unsigned long)pointer);
    return (address >= 0x1000u && address < 0x800000u) ||
           anchor_remote_model_pool_contains(pointer);
}

static int linked(const void *task)
{
    void *backlink;
    if (!rdram(task))
        return 0;
    backlink = *(void *const *)((const unsigned char *)task + 4);
    return rdram(backlink) && *(void *const *)backlink == task;
}

static int owned(const FreezeVisualSlot *slot)
{
    return linked(slot->task) && rdram(slot->object) &&
           *(void **)((unsigned char *)slot->task + 0xc) ==
               (void *)freeze_visual_task_update &&
           *(void **)((unsigned char *)slot->task + 0x18) == slot->object;
}

static int target_frozen(const AnchorFreezeVisualTarget *target)
{
    if (target->cid == 0)
        return target->epoch == anchor_player_models_get_epoch() &&
               anchor_player_freeze_active();
    return anchor_player_models_peer_frozen(target->cid, target->session,
                                            target->epoch);
}

static int cube_visible(const FreezeVisualSlot *slot)
{
    const unsigned char *object = slot->object;
    if (!slot->active || slot->room != D_800C7AB2 ||
        s_owner != D_801FC604_5B8514 || !linked(s_owner) ||
        !owned(slot) || !s_material_arena || !s_resources[0] ||
        !s_resources[1] || !s_resources[2] ||
        object[4] != 2 || (object[0x64] & 1u) ||
        (signed char)object[0x65] < 0 ||
        *(const unsigned int *)(object + 0x2c) != 0x48000500u)
        return 0;
    return target_frozen(&slot->target);
}

/* The native hit scan only needs the owned kind-2 object and its task attack
 * fields. Its display can stay hidden during this short landing window. */
static int impact_native(const FreezeVisualSlot *slot)
{
    if (!slot->shattered || !slot->tombstoned || !slot->impact_frames ||
        slot->room != D_800C7AB2 ||
        s_owner != D_801FC604_5B8514 || !linked(s_owner) ||
        !owned(slot) || ((const unsigned char *)slot->object)[4] != 2)
        return 0;
    if (slot->target.cid == 0)
        return slot->target.epoch == anchor_player_models_get_epoch();
    return anchor_player_models_peer_is_current(slot->target.cid,
                                                slot->target.session,
                                                slot->target.epoch);
}

static int cube_collision(const FreezeVisualSlot *slot,
                          AnchorFreezeCubeCollision *out)
{
    const unsigned char *object = slot->object;
    float x, y, z, sx, sy, sz;
    if (!cube_visible(slot) || slot->target.moving ||
        anchor_player_cube_visual_owned(slot->target.cid,
            slot->target.session, slot->target.epoch))
        return 0;
    x = *(const float *)(object + 8);
    y = *(const float *)(object + 0xc);
    z = *(const float *)(object + 0x10);
    sx = *(const float *)(object + 0x1c);
    sy = *(const float *)(object + 0x20);
    sz = *(const float *)(object + 0x24);
    if (!(x >= -10000000.0f && x <= 10000000.0f &&
          y >= -10000000.0f && y <= 10000000.0f &&
          z >= -10000000.0f && z <= 10000000.0f &&
          sx > 0.0f && sx <= 20.0f && sy > 0.0f && sy <= 20.0f &&
          sz > 0.0f && sz <= 20.0f))
        return 0;
    out->cid = slot->target.cid;
    out->session = slot->target.session;
    out->epoch = slot->target.epoch;
    out->cube.min.x = x - 50.0f * sx;
    out->cube.max.x = x + 50.0f * sx;
    out->cube.min.y = y - 50.0f * sy;
    out->cube.max.y = y + 50.0f * sy;
    out->cube.min.z = z - 50.0f * sz;
    out->cube.max.z = z + 50.0f * sz;
    return 1;
}

int anchor_player_freeze_visual_get_cubes(AnchorFreezeCubeCollision *out,
                                          int capacity)
{
    int i, count = 0;
    if (!out || capacity <= 0)
        return 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX && count < capacity; ++i)
        if (cube_collision(&s_slots[i], &out[count]))
            ++count;
    return count;
}

int anchor_player_freeze_visual_has_cube(int cid, int session, int epoch)
{
    int i;
    if (cid < 0 || epoch <= 0 || (cid > 0 && session <= 0))
        return 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (s_slots[i].target.cid == cid &&
            s_slots[i].target.session == session &&
            s_slots[i].target.epoch == epoch &&
            cube_visible(&s_slots[i]))
            return 1;
    return 0;
}

int anchor_player_freeze_visual_shatter(int cid, int session, int epoch)
{
    int i;
    if (cid < 0 || epoch <= 0 || (cid > 0 && session <= 0))
        return 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        FreezeVisualSlot *slot = &s_slots[i];
        unsigned char *object;
        if (slot->target.cid != cid || slot->target.session != session ||
            slot->target.epoch != epoch || !cube_visible(slot))
            continue;
        object = slot->object;
        slot->active = slot->shard_frames = 0;
        slot->shattered = 1;
        slot->tombstoned = 1;
        slot->impact_frames = IMPACT_FRAMES;
        *(unsigned int *)(object + 0x2c) = 0;
        object[0x64] |= 1u;
        return 1;
    }
    return 0;
}

int anchor_player_freeze_visual_get_native(int cid, int session, int epoch,
                                          void **task, void **object,
                                          float *scale)
{
    int i;
    if (!task || !object || !scale || epoch <= 0)
        return 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (s_slots[i].target.cid == cid &&
            s_slots[i].target.session == session &&
            s_slots[i].target.epoch == epoch &&
            (cube_visible(&s_slots[i]) || impact_native(&s_slots[i])))
        {
            *task = s_slots[i].task;
            *object = s_slots[i].object;
            *scale = s_slots[i].target.scale;
            return 1;
        }
    return 0;
}

int anchor_player_freeze_visual_owns_task(const void *task)
{
    int i;
    if (!task)
        return 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (s_slots[i].task == task && owned(&s_slots[i]))
            return 1;
    return 0;
}

static void hide(FreezeVisualSlot *slot)
{
    if (owned(slot))
    {
        unsigned char *object = slot->object;
        *(unsigned int *)(object + 0x2c) = 0;
        object[0x64] |= 1u;
    }
    slot->active = slot->shard_frames = slot->impact_frames = 0;
}

void anchor_player_freeze_visual_reset(void)
{
    int i;
    anchor_player_cube_reset();
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        hide(&s_slots[i]);
        s_slots[i].shattered = 0;
        s_slots[i].tombstoned = 0;
    }
    s_owner = 0;
}

/* The ice mesh uses the native direct-display-list path, which does not get
 * the remote character's scratch redirect. Suppress only this owned object
 * when the stock bank lacks headroom for its matrix and command setup. */
RECOMP_HOOK("func_80016C44_17844")
void anchor_player_freeze_visual_before_draw(void *pointer)
{
    unsigned int head, matrix, bank, command_end, matrix_start, matrix_end;
    int i;
    unsigned char *object = pointer;
    s_guard_object = 0;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (s_slots[i].object == object && owned(&s_slots[i]) &&
            (s_slots[i].active || s_slots[i].shard_frames ||
             s_slots[i].shattered))
            break;
    if (i == ANCHOR_FREEZE_VISUAL_MAX)
        return;
    s_guard_object = object;
    s_guard_hidden = object[0x64] & 1u;
    if (s_slots[i].shattered)
    {
        object[0x64] |= 1u;
        return;
    }
    bank = (unsigned int)D_800C7A72_C8672;
    if (bank > 1u || !rdram(D_8015C5C8_15D1C8))
        goto skip;
    bank = (unsigned int)(unsigned long)D_8015C5C8_15D1C8 + bank * NATIVE_BANK_BYTES;
    head = (unsigned int)(unsigned long)D_8015C5CC_15D1CC;
    matrix = (unsigned int)(unsigned long)D_80168504_169104;
    command_end = bank + NATIVE_COMMAND_BYTES - NATIVE_COMMAND_TAIL -
                  (anchor_dialog_busy() ? NATIVE_DIALOG_TAIL : 0u);
    matrix_start = bank + NATIVE_COMMAND_BYTES;
    matrix_end = bank + NATIVE_MATRIX_END - ICE_MATRIX_RESERVE;
    if (head >= bank && head <= command_end &&
        ICE_DRAW_COMMAND_ALLOWANCE <= command_end - head &&
        matrix >= matrix_start && matrix <= matrix_end &&
        64u <= matrix_end - matrix)
        return;
skip:
#if DEBUG_BUTTON_ENABLED
    if (!s_draw_skip_reported)
    {
        s_draw_skip_reported = 1;
        recomp_printf("[player_ice] visual skipped for graphics bank headroom\n");
    }
#endif
    object[0x64] |= 1u;
}

RECOMP_HOOK_RETURN("func_80016C44_17844")
void anchor_player_freeze_visual_after_draw(void)
{
    if (s_guard_object)
        s_guard_object[0x64] = (unsigned char)((s_guard_object[0x64] & ~1u) |
                                                  s_guard_hidden);
    s_guard_object = 0;
}

static unsigned char *resident(unsigned int file)
{
    void *pointer = func_800141C4_14DC4(file);
    if (!rdram(pointer))
        return 0;
    return (unsigned char *)(unsigned long)(physical((unsigned int)(unsigned long)pointer) |
                                            0x80000000u);
}

void anchor_player_freeze_visual_load_resources(void)
{
    unsigned int start, end;
    int i;
    anchor_player_freeze_visual_reset();
    s_material_arena = 0;
    func_80013B14_14714(0x191);
    func_80013B14_14714(0x192);
    func_80013B14_14714(0x152);
    s_resources[0] = resident(0x191);
    s_resources[1] = resident(0x192);
    s_resources[2] = resident(0x152);
    if (!s_resources[0] || !s_resources[1] || !s_resources[2])
    {
#if DEBUG_BUTTON_ENABLED
        report_resources(2); /* A required native file is not resident. */
#endif
        return;
    }
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !rdram(D_80167FC0_168BC0[i].data))
    {
#if DEBUG_BUTTON_ENABLED
        report_resources(3); /* No writable scene resource cursor. */
#endif
        return;
    }
    start = (physical((unsigned int)(unsigned long)D_80167FC0_168BC0[i].data) + 15u) & ~15u;
    end = start + CUBE_ARENA_BYTES;
    if (end < start || end > 0x800000u)
    {
#if DEBUG_BUTTON_ENABLED
        report_resources(4); /* Low-RDRAM arena cannot fit. */
#endif
        return;
    }
    D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)(end | 0x80000000u);
    s_material_arena = (unsigned int *)(unsigned long)(start | 0x80000000u);
    for (i = 0; i < 22; ++i)
        s_material_arena[i] = s_cube_template[i];
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        unsigned int *wrapper = s_material_arena + 22 + i * CUBE_WRAPPER_WORDS;
        wrapper[0] = 0x06000000u;
        wrapper[1] = (unsigned int)(unsigned long)s_material_arena;
        wrapper[2] = 0xfb000000u;
        wrapper[3] = 0x00000078u;
        wrapper[4] = 0xb8000000u;
        wrapper[5] = wrapper[6] = wrapper[7] = 0;
    }
#if DEBUG_BUTTON_ENABLED
    report_resources(1); /* Native files and owned material arena are ready. */
#endif
}

static int ensure_slot(FreezeVisualSlot *slot)
{
    if (!owned(slot))
        slot->task = slot->object = 0;
    if (!slot->task)
        slot->task = func_80034E08_35A08(s_owner, freeze_visual_task_update, 0);
    if (!slot->task)
        return 0;
    if (!slot->object)
        slot->object = func_8000DBF0_E7F0(slot->task, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!owned(slot))
    {
#if DEBUG_BUTTON_ENABLED
        if (!s_slot_skip_reported)
        {
            s_slot_skip_reported = 1;
            recomp_printf("[player_ice] visual task/object allocation failed\n");
        }
#endif
        return 0;
    }
    *(unsigned int *)((unsigned char *)slot->task + 0x30) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x34) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x38) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x48) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x5c) = 0;
    return 1;
}

static void draw(FreezeVisualSlot *slot, int index)
{
    unsigned char *object = slot->object;
    unsigned int *wrapper = s_material_arena + 22 + index * CUBE_WRAPPER_WORDS;
    float scale = slot->target.scale;
    unsigned int age = SHARD_FRAMES - slot->shard_frames;
    if ((!slot->active && !slot->shard_frames) || !owned(slot) || !s_material_arena ||
        !s_resources[0] || !s_resources[1] || !s_resources[2] ||
        !(scale > 0.00001f && scale <= 10.0f))
    {
        hide(slot);
        return;
    }
    *(unsigned int *)(object + 0x2c) = slot->active ? 0x48000500u : 0x480002c0u;
    wrapper[3] = slot->active ? 0x00000078u : 0x000000ffu;
    *(unsigned int *)(object + 0x30) =
        (unsigned int)(unsigned long)wrapper | 0x60000000u;
    *(unsigned short *)(object + 0x34) = 0x191;
    *(void **)(object + 0x38) = s_resources[0];
    *(unsigned short *)(object + 0x3c) = 0x192;
    *(void **)(object + 0x40) = s_resources[1];
    *(unsigned short *)(object + 0x44) = 0x152;
    *(void **)(object + 0x48) = s_resources[2];
    if (!slot->active || !anchor_player_cube_visual_native_pose(
            slot->target.cid, slot->target.session, slot->target.epoch))
    {
        *(float *)(object + 8) = slot->target.x +
                                 (slot->active ? 0.0f : age * 0.4f);
        *(float *)(object + 0xc) = slot->target.y + scale * 100.0f +
                                   (slot->active ? 0.0f : age * 0.8f);
        *(float *)(object + 0x10) = slot->target.z +
                                    (slot->active ? 0.0f : age * 0.3f);
    }
    *(unsigned short *)(object + 0x14) = 0;
    *(unsigned short *)(object + 0x16) = 0;
    *(unsigned short *)(object + 0x18) = 0;
    *(float *)(object + 0x1c) = scale * (slot->active ? 2.0f : 1.2f);
    *(float *)(object + 0x20) = scale * (slot->active ? 2.0f : 1.2f);
    *(float *)(object + 0x24) = scale * (slot->active ? 2.0f : 1.2f);
    object[5] = 2;
    object[0x64] &= (unsigned char)~1u;
    object[0x65] = 0;
}

static void freeze_visual_task_update(void *task, void *object)
{
    int i;
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        FreezeVisualSlot *slot = &s_slots[i];
        if (slot->task != task || slot->object != object)
            continue;
        if (slot->shattered)
        {
            if (!slot->impact_frames || slot->room != D_800C7AB2 ||
                s_owner != D_801FC604_5B8514 || !linked(s_owner) ||
                !owned(slot))
                hide(slot);
            else
            {
                unsigned char *ice = slot->object;
                *(unsigned int *)(ice + 0x2c) = 0;
                ice[0x64] |= 1u;
            }
        }
        else if ((!slot->active && !slot->shard_frames) ||
            slot->room != D_800C7AB2 ||
            s_owner != D_801FC604_5B8514 || !linked(s_owner) ||
            (slot->active && !target_frozen(&slot->target)))
            hide(slot);
        else
            draw(slot, i);
        return;
    }
}

void anchor_player_freeze_visual_tick(void *owner)
{
    AnchorFreezeVisualTarget targets[ANCHOR_FREEZE_VISUAL_MAX];
    int count, i, j;
    if (!linked(owner) || owner != D_801FC604_5B8514 || !s_material_arena)
    {
        anchor_player_freeze_visual_reset();
        return;
    }
    if (s_owner != owner)
    {
        anchor_player_freeze_visual_reset();
        s_owner = owner;
    }
    count = anchor_player_models_get_freeze_visual_targets(targets,
                                                          ANCHOR_FREEZE_VISUAL_MAX);
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
    {
        FreezeVisualSlot *slot = &s_slots[i];
        int still_frozen;
        slot->seen = 0;
        if (slot->shard_frames && !--slot->shard_frames)
            hide(slot);
        if (!slot->shattered)
            continue;
        still_frozen = target_frozen(&slot->target);
        if (slot->room != D_800C7AB2 || !still_frozen)
            slot->tombstoned = 0;
        if (slot->impact_frames && !--slot->impact_frames)
            hide(slot);
        if (!slot->impact_frames && !slot->tombstoned)
        {
            hide(slot);
            slot->shattered = 0;
        }
    }
    for (i = 0; i < count; ++i)
    {
        FreezeVisualSlot *slot = 0;
        int index = -1;
        int shattered = 0;
        /* A target sampled before the thaw must not revive its cube. */
        if (!target_frozen(&targets[i]))
            continue;
        for (j = 0; j < ANCHOR_FREEZE_VISUAL_MAX; ++j)
            if (s_slots[j].tombstoned &&
                s_slots[j].target.cid == targets[i].cid &&
                s_slots[j].target.session == targets[i].session &&
                s_slots[j].target.epoch == targets[i].epoch)
            {
                shattered = 1;
                break;
            }
        if (shattered)
            continue;
        for (j = 0; j < ANCHOR_FREEZE_VISUAL_MAX; ++j)
            if (s_slots[j].active && s_slots[j].target.cid == targets[i].cid &&
                s_slots[j].target.session == targets[i].session &&
                s_slots[j].target.epoch == targets[i].epoch)
            {
                slot = &s_slots[j];
                index = j;
                break;
            }
        if (!slot)
            for (j = 0; j < ANCHOR_FREEZE_VISUAL_MAX; ++j)
                if (!s_slots[j].active && !s_slots[j].shard_frames &&
                    !s_slots[j].shattered)
                {
                    slot = &s_slots[j];
                    index = j;
                    break;
                }
        if (!slot)
            for (j = 0; j < ANCHOR_FREEZE_VISUAL_MAX; ++j)
                if (s_slots[j].shard_frames)
                {
                    slot = &s_slots[j];
                    index = j;
                    hide(slot);
                    break;
                }
        if (!slot || !ensure_slot(slot))
            continue;
        slot->target = targets[i];
        slot->room = D_800C7AB2;
        slot->shard_frames = 0;
        slot->active = slot->seen = 1;
        draw(slot, index);
    }
    for (i = 0; i < ANCHOR_FREEZE_VISUAL_MAX; ++i)
        if (s_slots[i].active && !s_slots[i].seen)
        {
            FreezeVisualSlot *slot = &s_slots[i];
            /* The model can be briefly unbound while the peer remains frozen.
             * Only a genuine thaw (or room/identity loss) removes the cube. */
            if (slot->room != D_800C7AB2 || !target_frozen(&slot->target))
                hide(slot);
        }
}
