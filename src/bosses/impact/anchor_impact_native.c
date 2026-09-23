#include "bosses/impact/anchor_impact_native.h"
#include "bosses/impact/anchor_impact_boss.h"
#include "bosses/impact/anchor_impact_smoothing.h"
#include "bosses/impact/anchor_impact_visuals.h"
#include "bosses/impact/anchor_impact_stage.h"
#include "bosses/anchor_boss_invite_world.h"
#include "player/anchor_remote_model_pool.h"

#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
#include "platform/modding.h"
#include "platform/recomputils.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

/* Dedicated file_13 Impact-battle overlay (USA). The shared battle-state
 * pointer is D_8020EED0_63A2B0; the encounter selector is system+0x3ADF4.
 * All four bosses (1 Kashiwagi, 2 Taisamba 2, 3 Balberra, 4 D'Etoile) reuse it. */
extern unsigned short D_800C7AB2;
extern unsigned char *D_8015C5C8_15D1C8;
extern void *D_8020EED0_63A2B0;
extern void *D_8016DAB4_16E6B4;
extern void func_801D2EE4_5FE2C4(void *object, const void *clip);


#define IMPACT_STATE_HP 0x60
#define IMPACT_STATE_AMMO 0x64
#define IMPACT_STATE_MECH_HP 0x68
#define IMPACT_STATE_PAUSE 0x2C0
#define IMPACT_STATE_CLOCK 0x2C8
#define IMPACT_STATE_ROOT 0x1D8
#define IMPACT_STATE_MODEL 0x1E0

#define IMPACT_ENCOUNTER_OFFSET 0x3ADF4u
#define IMPACT_CALLBACK_DISABLED 0x00800000ul
/* file_13 is linked at 0x801CB460 (size 0x43A70). An overlay AI callback is
 * accepted only from this executable range, never a caller-supplied pointer. */
#define IMPACT_CODE_BASE 0x801CB460u
#define IMPACT_CODE_END 0x8020EED0u
#define IMPACT_REMOVE_PENDING 0x00000002u

static void *s_state;
static void *s_task;
static void *s_object;
static unsigned int s_encounter;
static unsigned int s_stage;
static unsigned int s_id;
/* Monotonic fallback encounter visit for modes (title-menu boss rush) that
 * never pass through the ordinary room loader and so report visit 0. */
static unsigned int s_visit_counter;
static int s_bound;
static int s_active;
static int s_owner;
static int s_paused;
static int s_pending_valid;
static AnchorImpactNativeSnapshot s_pending;
static AnchorImpactNativeSnapshot s_view;
static int s_view_valid;
static struct MechPresentation {
    AnchorImpactSmoothPose smooth;
    void *object;
    unsigned int model, file, saved[11];
    int restore;
} s_mech_view[ANCHOR_IMPACT_MECH_OBJECTS];
static void restore_mech_presentation(int reset);
static const unsigned short s_mech_offsets[ANCHOR_IMPACT_MECH_OBJECTS] = {
    0x1C, 0x20, 0x28, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C
};

#ifndef ANCHOR_IMPACT_READ_U8
#define ANCHOR_IMPACT_READ_U8(p, o) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U8(p, o, v) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)) = (unsigned char)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_U16
#define ANCHOR_IMPACT_READ_U16(p, o) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U16(p, o, v) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)) = (unsigned short)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_U32
#define ANCHOR_IMPACT_READ_U32(p, o) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U32(p, o, v) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)) = (unsigned int)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_I32
#define ANCHOR_IMPACT_READ_I32(p, o) \
    (*(volatile signed int *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_I32(p, o, v) \
    (*(volatile signed int *)((unsigned char *)(p) + (o)) = (signed int)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_F32
#define ANCHOR_IMPACT_READ_F32(p, o) \
    (*(volatile float *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_F32(p, o, v) \
    (*(volatile float *)((unsigned char *)(p) + (o)) = (float)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_PTR
#define ANCHOR_IMPACT_READ_PTR(p, o) \
    (*(void *volatile *)((unsigned char *)(p) + (o)))
#endif
#ifndef ANCHOR_IMPACT_AI
#define ANCHOR_IMPACT_AI(p) \
    (*(unsigned long volatile *)((unsigned char *)(p) + 0x0C))
#define ANCHOR_IMPACT_SET_AI(p, v) \
    (*(unsigned long volatile *)((unsigned char *)(p) + 0x0C) = \
         (unsigned long)(v))
#endif
#ifndef ANCHOR_IMPACT_POST_DISABLED
#define ANCHOR_IMPACT_POST_DISABLED(p) \
    ((ANCHOR_IMPACT_READ_U32((p), 0x10) & (unsigned int)IMPACT_CALLBACK_DISABLED) != 0u)
#endif

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_IMPACT_NATIVE_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           ((address >= 0x80001000u && address < 0x80800000u) ||
            anchor_remote_model_pool_contains(pointer));
#endif
}

static unsigned int float_bits(float value)
{
    union { float f; unsigned int u; } bits;
    bits.f = value;
    return bits.u;
}

static float bits_float(unsigned int value)
{
    union { float f; unsigned int u; } bits;
    bits.u = value;
    return bits.f;
}

/* Bit-pattern finiteness; float compares are unreliable under -ffast-math. */
static int finite_word(unsigned int value)
{
    return (value & 0x7f800000u) != 0x7f800000u;
}

static unsigned int current_encounter(void)
{
    if (!D_8015C5C8_15D1C8)
        return 0;
    return ANCHOR_IMPACT_READ_U16(D_8015C5C8_15D1C8, IMPACT_ENCOUNTER_OFFSET);
}

/* A callback catalog alone does not provide the boss-specific graph and
 * lifecycle handling needed for synchronization. Only complete profiles can
 * activate the shared mech; later bosses keep their native gameplay. */
static unsigned int encounter_task_id(unsigned int encounter)
{
    const AnchorImpactBossProfile *profile = anchor_impact_boss_profile(encounter);
    return profile ? profile->task_id : 0;
}

static int bound_live(void)
{
    void *state;
    unsigned int id;
    if (!s_bound || !s_task || !pointer_valid(s_task))
        return 0;
    state = D_8020EED0_63A2B0;
    if (!pointer_valid(state))
        return 0;
    /* The battle stage can change within one Impact sequence (intro cutscene
     * -> minigame -> boss), so always follow the current stage rather than a
     * value captured at bind time. */
    if (!anchor_impact_stage_matches(D_800C7AB2, s_encounter))
        return 0;
    if (current_encounter() != s_encounter)
        return 0;
    if (ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_ROOT) != s_task)
        return 0;
    id = ANCHOR_IMPACT_READ_U16(s_task, 0x5C);
    if (!id || id != encounter_task_id(s_encounter))
        return 0;
    if (ANCHOR_IMPACT_READ_U32(s_task, 0x68) & IMPACT_REMOVE_PENDING)
        return 0;
    /* Always re-resolve the model object. It is recreated across phases and a
     * freed-but-in-range stale pointer must never receive transform writes. */
    s_object = ANCHOR_IMPACT_READ_PTR(s_task, 0x18);
    if (!pointer_valid(s_object))
        s_object = ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_MODEL);
    s_state = state;
    s_id = id;
    s_stage = D_800C7AB2;
    return pointer_valid(s_object);
}

int anchor_impact_native_root_live(void)
{
    return bound_live();
}

int anchor_impact_native_ready(void)
{
    return s_active && bound_live();
}

int anchor_impact_native_is_owner(void)
{
    return s_active && s_owner && !s_paused && bound_live();
}

int anchor_impact_native_snapshot_ready(void)
{
    return bound_live();
}

unsigned int anchor_impact_native_visit(void)
{
    /* Read live so a bind that races the Impact-sequence start cannot pin a
     * stale visit. The sequence visit is stable for the whole sequence. A
     * mode that never reports one (boss rush) falls back to a per-boss value,
     * which keeps the coordinator's required positive visit and re-elects on
     * each new boss. */
    unsigned int visit = anchor_boss_invite_world_visit();
    return visit ? visit : s_visit_counter;
}

unsigned int anchor_impact_native_encounter(void)
{
    return s_encounter;
}

unsigned int anchor_impact_native_stage(void)
{
    return s_stage;
}

void anchor_impact_native_reset(void)
{
    restore_mech_presentation(1);
    s_state = 0;
    s_task = 0;
    s_object = 0;
    s_encounter = 0;
    s_stage = 0;
    s_id = 0;
    s_bound = 0;
    s_active = 0;
    s_owner = 0;
    s_paused = 0;
    s_pending_valid = 0;
    s_view_valid = 0;
}

void anchor_impact_native_set_role(int active, int owner, int paused)
{
    if (!active || owner || paused) restore_mech_presentation(1);
    if (!active)
    {
        s_pending_valid = 0;
        s_view_valid = 0;
    }
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
}

/* Bind the boss root from its own file_13 initializer entry. The initializer
 * body has not run yet, so the task ID, model object and battle-state root
 * pointer are all resolved lazily by bound_live() on later frames. The four
 * initializers are boss-specific, so the expected encounter is passed in. */
int anchor_impact_native_bind(void *task, unsigned int encounter)
{
    if (!pointer_valid(task) || encounter < 1u || encounter > 4u)
        return 0;
    anchor_impact_native_reset();
    if (!encounter_task_id(encounter))
    {
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
        recomp_printf("[Impact] native-only encounter=%u stage=%u (no sync profile)\n",
                      encounter, (unsigned int)D_800C7AB2);
#endif
        return 0;
    }
    s_visit_counter = s_visit_counter >= 0x7fffffffu ? 1u : s_visit_counter + 1u;
    s_task = task;
    s_encounter = encounter;
    s_stage = D_800C7AB2;
    s_bound = 1;
    anchor_impact_catalog_init();
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    recomp_printf("[Impact] bind encounter=%u stage=%u task=%u\n",
                  encounter, (unsigned int)D_800C7AB2,
                  (unsigned int)(unsigned long)task);
#endif
    return 1;
}


static int snapshot_valid(const AnchorImpactNativeSnapshot *snapshot)
{
    const unsigned int *r;
    unsigned int i, j;
    if (!snapshot)
        return 0;
    r = snapshot->root;
    if (snapshot->encounter != s_encounter || snapshot->encounter < 1u ||
        snapshot->encounter > 4u ||
        snapshot->stage != (unsigned int)D_800C7AB2 ||
        !anchor_impact_stage_valid(snapshot->stage))
        return 0;
    /* Validate the complete checkpoint before any native writes. Hidden or
     * zero-scale intro models remain valid; NaN/Inf and unknown IDs do not. */
    if (r[IMP_BOSS_HP] > 1000000u || r[IMP_MECH_HP] > 1000000u ||
        r[IMP_AMMO] > 1000000u)
        return 0;
    if (!finite_word(r[IMP_POS_X]) || !finite_word(r[IMP_POS_Y]) ||
        !finite_word(r[IMP_POS_Z]) || !finite_word(r[IMP_SCALE_X]) ||
        !finite_word(r[IMP_SCALE_Y]) || !finite_word(r[IMP_SCALE_Z]) ||
        !finite_word(r[IMP_FRAME]))
        return 0;
    if (r[IMP_ROT_X] > 65535u || r[IMP_ROT_Y] > 65535u || r[IMP_ROT_Z] > 65535u)
        return 0;
    if ((r[IMP_PHASE] && !anchor_impact_phase_callback(s_encounter, r[IMP_PHASE])) ||
        (r[IMP_CLIP] && !anchor_impact_clip_data(s_encounter, r[IMP_CLIP])) ||
        r[IMP_COLLIDER_X] > 65535u || r[IMP_COLLIDER_Y] > 65535u ||
        r[IMP_COLLIDER_Z] > 65535u || r[IMP_MODEL_FLAGS] > 1u ||
        r[IMP_MODE] > 15u || r[IMP_PAUSE] > 255u ||
        r[IMP_MECH_MASK] >= (1u << ANCHOR_IMPACT_MECH_OBJECTS))
        return 0;
    for (i = 0; i < 3; ++i)
        if (!finite_word(r[IMP_PRIVATE + i])) return 0;
    for (i = 0; i < ANCHOR_IMPACT_MECH_OBJECTS; ++i)
    {
        const unsigned int *p = &r[IMP_MECH_POSES + i * ANCHOR_IMPACT_POSE_WORDS];
        if (!(r[IMP_MECH_MASK] & (1u << i))) continue;
        for (j = 0; j < 10; ++j)
            if (j >= 3 && j < 6 ? p[j] > 65535u : !finite_word(p[j])) return 0;
        if (p[10] > 1u) return 0;
    }
    if (r[IMP_AUX_KIND] > 2 ||
        (r[IMP_AUX_KIND] == 2 && (s_encounter < 3 ||
         (r[IMP_AUX_DATA+3]&255u) || (r[IMP_AUX_DATA+4]&255u))) || r[IMP_AUX_DATA+5]) return 0;
    return anchor_impact_boss_validate(s_encounter, r);
}

static void capture_pose(const void *object, unsigned int *out)
{
    unsigned int i;
    for (i = 0; i < 3; ++i)
    {
        out[i] = ANCHOR_IMPACT_READ_U32(object, 0x08 + i * 4);
        out[3 + i] = ANCHOR_IMPACT_READ_U16(object, 0x14 + i * 2);
        out[6 + i] = ANCHOR_IMPACT_READ_U32(object, 0x1C + i * 4);
    }
    out[9] = ANCHOR_IMPACT_READ_U32(object, 0x28);
    out[10] = ANCHOR_IMPACT_READ_U8(object, 0x64) & 1u;
}

static void apply_pose(void *object, const unsigned int *in)
{
    unsigned int i;
    for (i = 0; i < 3; ++i)
    {
        ANCHOR_IMPACT_WRITE_U32(object, 0x08 + i * 4, in[i]);
        ANCHOR_IMPACT_WRITE_U16(object, 0x14 + i * 2, in[3 + i]);
        ANCHOR_IMPACT_WRITE_U32(object, 0x1C + i * 4, in[6 + i]);
    }
    ANCHOR_IMPACT_WRITE_U32(object, 0x28, in[9]);
    ANCHOR_IMPACT_WRITE_U8(object, 0x64,
        (ANCHOR_IMPACT_READ_U8(object, 0x64) & ~1u) | in[10]);
}

/* Restore the local native pose before AI/collision work; interpolated poses
 * are display output and must never feed back into gameplay or promotion. */
static void restore_mech_presentation(int reset)
{
    unsigned int i;
    int live = s_bound && bound_live();
    for (i = 0; i < ANCHOR_IMPACT_MECH_OBJECTS; ++i) {
        struct MechPresentation *v = &s_mech_view[i];
        if (v->restore && live && pointer_valid(v->object) &&
            ANCHOR_IMPACT_READ_PTR(s_state, s_mech_offsets[i]) == v->object &&
            ANCHOR_IMPACT_READ_U32(v->object, 0x2C) == v->model &&
            ANCHOR_IMPACT_READ_U16(v->object, 0x34) == v->file)
            apply_pose(v->object, v->saved);
        v->restore = 0;
        if (reset) { v->object = 0; v->smooth.valid = 0; }
    }
}

int anchor_impact_native_capture(AnchorImpactNativeSnapshot *snapshot)
{
    unsigned int *r;
    unsigned int i;
    if (!snapshot || !bound_live())
        return 0;
    r = snapshot->root;
    for (i = 0; i < ANCHOR_IMPACT_ROOT_WORDS; ++i) r[i] = 0;
    r[IMP_BOSS_HP] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_HP);
    r[IMP_AMMO] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_AMMO);
    r[IMP_MECH_HP] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_MECH_HP);
    r[IMP_PAUSE] = ANCHOR_IMPACT_READ_U8(s_state, IMPACT_STATE_PAUSE);
    r[IMP_CLOCK] = ANCHOR_IMPACT_READ_U32(s_state, IMPACT_STATE_CLOCK);
    r[IMP_POS_X] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x08));
    r[IMP_POS_Y] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x0C));
    r[IMP_POS_Z] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x10));
    r[IMP_ROT_X] = ANCHOR_IMPACT_READ_U16(s_object, 0x14);
    r[IMP_ROT_Y] = ANCHOR_IMPACT_READ_U16(s_object, 0x16);
    r[IMP_ROT_Z] = ANCHOR_IMPACT_READ_U16(s_object, 0x18);
    r[IMP_SCALE_X] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x1C));
    r[IMP_SCALE_Y] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x20));
    r[IMP_SCALE_Z] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x24));
    r[IMP_FRAME] = float_bits(ANCHOR_IMPACT_READ_F32(s_object, 0x28));
    r[IMP_PHASE] = anchor_impact_phase_id(s_encounter,
        (unsigned int)ANCHOR_IMPACT_AI(s_task));
    r[IMP_CLIP] = anchor_impact_clip_id(s_encounter,
        ANCHOR_IMPACT_READ_U32(s_object, 0x2C),
        ANCHOR_IMPACT_READ_U16(s_object, 0x34));
    r[IMP_FLAGS] = ANCHOR_IMPACT_READ_U32(s_task, 0x64);
    r[IMP_COLLIDER_X] = ANCHOR_IMPACT_READ_U16(s_task, 0x3C);
    r[IMP_COLLIDER_Y] = ANCHOR_IMPACT_READ_U16(s_task, 0x3E);
    r[IMP_COLLIDER_Z] = ANCHOR_IMPACT_READ_U16(s_task, 0x40);
    r[IMP_MODEL_FLAGS] = ANCHOR_IMPACT_READ_U8(s_object, 0x64) & 1u;
    r[IMP_MODE] = ANCHOR_IMPACT_READ_U8(s_object, 0x05);
    for (i = 0; i < ANCHOR_IMPACT_PRIVATE_WORDS; ++i)
        if (anchor_impact_private_used(s_encounter, i))
            r[IMP_PRIVATE + i] = ANCHOR_IMPACT_READ_U32(s_task,
                anchor_impact_private_offset(i));
    for (i = 0; i < ANCHOR_IMPACT_MECH_OBJECTS; ++i)
    {
        const void *object = ANCHOR_IMPACT_READ_PTR(s_state, s_mech_offsets[i]);
        if (!pointer_valid(object)) continue;
        const struct MechPresentation *v = &s_mech_view[i];
        unsigned int *pose = &r[IMP_MECH_POSES + i * ANCHOR_IMPACT_POSE_WORDS];
        if (v->restore && v->object == object &&
            ANCHOR_IMPACT_READ_U32(object, 0x2C) == v->model &&
            ANCHOR_IMPACT_READ_U16(object, 0x34) == v->file) {
            unsigned int j;
            for (j = 0; j < ANCHOR_IMPACT_POSE_WORDS; ++j) pose[j] = v->saved[j];
        } else capture_pose(object, pose);
        r[IMP_MECH_MASK] |= 1u << i;
    }
    anchor_impact_boss_capture(s_encounter, s_state, s_task, r);
    snapshot->encounter = s_encounter;
    snapshot->stage = s_stage;
    return snapshot_valid(snapshot);
}

/* Native simulation adopts phase/clip/motion at checkpoint boundaries. The
 * presentation pass also restores this pose after the follower's update. */
static void apply_transform(const AnchorImpactNativeSnapshot *snapshot)
{
    const unsigned int *r = snapshot->root;
    if (!pointer_valid(s_object))
        return;
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x08, bits_float(r[IMP_POS_X]));
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x0C, bits_float(r[IMP_POS_Y]));
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x10, bits_float(r[IMP_POS_Z]));
    ANCHOR_IMPACT_WRITE_U16(s_object, 0x14, (unsigned short)r[IMP_ROT_X]);
    ANCHOR_IMPACT_WRITE_U16(s_object, 0x16, (unsigned short)r[IMP_ROT_Y]);
    ANCHOR_IMPACT_WRITE_U16(s_object, 0x18, (unsigned short)r[IMP_ROT_Z]);
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x1C, bits_float(r[IMP_SCALE_X]));
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x20, bits_float(r[IMP_SCALE_Y]));
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x24, bits_float(r[IMP_SCALE_Z]));
    ANCHOR_IMPACT_WRITE_F32(s_object, 0x28, bits_float(r[IMP_FRAME]));
    ANCHOR_IMPACT_WRITE_U8(s_object, 0x64,
        (ANCHOR_IMPACT_READ_U8(s_object, 0x64) & ~1u) | r[IMP_MODEL_FLAGS]);
    ANCHOR_IMPACT_WRITE_U8(s_object, 0x05, r[IMP_MODE]);
}

static void apply_root(const AnchorImpactNativeSnapshot *snapshot)
{
    const unsigned int *r = snapshot->root;
    const void *clip;
    unsigned int i, callback;
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_HP, (int)r[IMP_BOSS_HP]);
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_AMMO, (int)r[IMP_AMMO]);
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_MECH_HP,
                           (int)r[IMP_MECH_HP]);
    anchor_impact_boss_apply_lifecycle(s_encounter, s_state, r);
    /* Let each native introduction finish creating its task graph. An early
     * phase jump could otherwise enter an action before its child exists. */
    if (r[IMP_PAUSE] || ANCHOR_IMPACT_READ_U8(s_state, IMPACT_STATE_PAUSE))
        return;
    anchor_impact_boss_apply(s_encounter, s_state, s_task, r);
    ANCHOR_IMPACT_WRITE_U32(s_state, IMPACT_STATE_CLOCK, r[IMP_CLOCK]);
    clip = anchor_impact_clip_data(s_encounter, r[IMP_CLIP]);
    if (clip) func_801D2EE4_5FE2C4(s_object, clip);
    callback = anchor_impact_phase_callback(s_encounter, r[IMP_PHASE]);
    if (callback)
    {
        ANCHOR_IMPACT_SET_AI(s_task, callback |
            ((unsigned int)ANCHOR_IMPACT_AI(s_task) & IMPACT_CALLBACK_DISABLED));
        for (i = 0; i < ANCHOR_IMPACT_PRIVATE_WORDS; ++i)
            if (anchor_impact_private_used(s_encounter, i))
                ANCHOR_IMPACT_WRITE_U32(s_task, anchor_impact_private_offset(i),
                                       r[IMP_PRIVATE + i]);
        ANCHOR_IMPACT_WRITE_U32(s_task, 0x64, r[IMP_FLAGS]);
        ANCHOR_IMPACT_WRITE_U16(s_task, 0x3C, r[IMP_COLLIDER_X]);
        ANCHOR_IMPACT_WRITE_U16(s_task, 0x3E, r[IMP_COLLIDER_Y]);
        ANCHOR_IMPACT_WRITE_U16(s_task, 0x40, r[IMP_COLLIDER_Z]);
    }
    apply_transform(snapshot);
}

static void apply_shared_view(const AnchorImpactNativeSnapshot *snapshot)
{
    unsigned int i;
    const unsigned int *r = snapshot->root;
    if (r[IMP_PAUSE] || ANCHOR_IMPACT_READ_U8(s_state, IMPACT_STATE_PAUSE)) return;
    apply_transform(snapshot);
    for (i = 0; i < ANCHOR_IMPACT_MECH_OBJECTS; ++i)
    {
        void *object = ANCHOR_IMPACT_READ_PTR(s_state, s_mech_offsets[i]);
        struct MechPresentation *v = &s_mech_view[i];
        if (pointer_valid(object) && (r[IMP_MECH_MASK] & (1u << i))) {
            unsigned int model = ANCHOR_IMPACT_READ_U32(object, 0x2C);
            unsigned int file = ANCHOR_IMPACT_READ_U16(object, 0x34);
            if (v->object != object || v->model != model || v->file != file)
                v->smooth.valid = 0;
            v->object = object; v->model = model; v->file = file;
            if (!v->restore) capture_pose(object, v->saved);
            v->restore = 1;
            impact_pose_offer(&v->smooth, &r[IMP_MECH_POSES + i * ANCHOR_IMPACT_POSE_WORDS]);
            impact_pose_step(&v->smooth);
            apply_pose(object, v->smooth.current);
        } else v->smooth.valid = 0;
    }
    /* FUN_801CC10C derives this client's camera from its own aim. */
}

int anchor_impact_native_apply(const AnchorImpactNativeSnapshot *snapshot)
{
    if (!snapshot_valid(snapshot) || !bound_live())
        return 0;
    s_pending = *snapshot;
    s_pending_valid = 1;
    return 1;
}

/* The battle-state root pointer is authoritative. This late bind runs every
 * frame, so a missing/incorrect initializer-hook argument cannot leave the
 * encounter unbound, and a new boss re-binds when its root replaces the old. */
void anchor_impact_native_tick(void)
{
    void *state = D_8020EED0_63A2B0;
    void *root;
    unsigned int encounter;
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    static unsigned int s_diag;
#endif
    encounter = current_encounter();
    if (!pointer_valid(state) || !anchor_impact_stage_matches(D_800C7AB2, encounter) ||
        !encounter_task_id(encounter))
    {
        if (s_bound) anchor_impact_native_reset();
        return;
    }
    root = ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_ROOT);
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    if ((++s_diag % 240u) == 0u)
        recomp_printf("[Impact] diag stage=%u sel=%u root=%u id=%u hp=%d mhp=%d ammo=%d bound=%d live=%d\n",
                      (unsigned int)D_800C7AB2, encounter,
                      (unsigned int)(unsigned long)root,
                      pointer_valid(root)
                          ? (unsigned int)ANCHOR_IMPACT_READ_U16(root, 0x5C) : 0u,
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_HP),
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_MECH_HP),
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_AMMO),
                      s_bound, s_bound ? bound_live() : 0);
#endif
    if (!pointer_valid(root) || encounter < 1u || encounter > 4u)
        return;
    if (!s_bound || s_task != root || s_encounter != encounter ||
        s_stage != D_800C7AB2)
    {
        (void)anchor_impact_native_bind(root, encounter);
    }
}

int anchor_impact_native_world_paused(void)
{
    if (!anchor_impact_native_ready())
        return 0;
    /* Deliberately exclude s_paused: it is a coordinator role flag, not a
     * native pause. Feeding it back here would latch the encounter paused
     * forever during election (s_paused includes !role), which silently
     * disables hit delivery and checkpoint adoption. */
    return (ANCHOR_IMPACT_AI(s_task) & IMPACT_CALLBACK_DISABLED) != 0;
}

RECOMP_HOOK("func_80034734_35334")
void anchor_impact_native_scheduler_begin(void)
{
    anchor_impact_visuals_begin_frame();
    restore_mech_presentation(0);
    if (!s_active)
        return;
    if (s_pending_valid)
    {
        /* A queued checkpoint can outlive a stage change between the frame
         * bridge and this scheduler entry. Revalidate before any writes. */
        if (bound_live() && snapshot_valid(&s_pending))
        {
            apply_root(&s_pending);
            s_view = s_pending;
            s_view_valid = 1;
        }
        s_pending_valid = 0;
    }
    /* Keep the native graph alive through introductions and transitions.
     * Combat callbacks use the allowlisted phase and native scalar state
     * adopted above; no foreign pointers enter the scheduler. */
}

RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_impact_native_scheduler_end(void)
{
    if (s_active && !s_owner && s_view_valid && !s_paused && bound_live() &&
        s_view.stage == (unsigned int)D_800C7AB2 && s_view.encounter == s_encounter)
        apply_shared_view(&s_view);
}
