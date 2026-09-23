/**
 * @file anchor_player_skin.c
 * @brief Local alternative Ebisumaru skin toggle and shared asset staging.
 *
 * The opening cutscene renders a standalone Ebisumaru from file 0x4D9 using
 * model pointer 0x68000B0C (segment 8 offset 0xB0C) and animation context
 * 0xC006D898. This module registers that file during the stage-load return
 * hook, toggles a local skin state from the unused N64 L button edge, and
 * binds a private copy of the active playable action tree whose body display
 * references point to the opening mesh and its own texture pages. The action
 * header, joints and animation stream remain the full playable set.
 * It also exposes the appearance bitmap bit other clients use to do the same
 * for the remote model. See docs/alternative-ebisumaru-skin.md for the full recipe.
 *
 * No playable constructor, action callback, or player manager routine is
 * called here. The native late player update still advances object +0x28 and
 * wraps it through func_8001B5AC against the standard action's clip length.
 */

#include "player/alternative_ebisumaru/anchor_player_skin.h"
#include "player/anchor_player_models.h"
#include "core/anchor_dialog.h"
#include "progression/item_sync.h"
#include "bosses/impact/anchor_impact_native.h"

#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
#define SKIN_LOG(...) ((void)0)
#else
#include "platform/recomputils.h"
#define SKIN_LOG(...) recomp_printf(__VA_ARGS__)
#endif

/* Opening-cutscene alternative Ebisumaru asset (docs/opening-cutscene-...). */
#define ALTERNATIVE_EBISUMARU_FILE 0x4D9u

/* Selected-character id; bit 1..2 hold the character index. */
#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)
#define CHARACTER_EBISUMARU 1

/* Raw one-frame controller pressed edge, pad 0, in the native system state.
 * src/bosses/impact/anchor_impact_players.c reads the same +0x3B07C field with a 0x18
 * stride per pad. The unused N64 L button is 0x0020. */
#define INPUT_PRESSED_OFFSET 0x3B07Cu
#define INPUT_L_MASK 0x0020u

/* The engine stores scene-registry bases with a cache tag in bit 30. */
#define RENDER_CACHE_TAG_MASK 0xbfffffffu

extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned char D_800C7AE2;
extern unsigned char D_800C7AE3;
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;

extern void *func_80013B14_14714(unsigned int file_id);
extern void *func_800141C4_14DC4(unsigned int file_id);

static int s_toggle_on;
static unsigned int s_alternative_base;
static unsigned int s_alternative_size;

/* Restore capture for the two segment bases redirected by the mesh graft.
 * Everything else on the object belongs to the native player. The capture is
 * only valid while the captured task/object identity still matches the live
 * local player; otherwise it is dropped without writing. */
static int s_have_capture;
static void *s_capture_task;
static void *s_capture_object;
static int s_capture_char;                 /* character index at capture */
static unsigned short s_capture_broad_file; /* +0x3c, native model identity */
static unsigned int s_saved_segment8;      /* +0x38 */
static unsigned int s_saved_segment9;      /* +0x40 */
static unsigned int s_applied_segment8;

static int skin_pointer_valid(const void *pointer)
{
#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    unsigned int phys = address & 0x1fffffffu;

    /* Exclude the engine's 0x80000000 invalid-link sentinel as well as null. */
    return phys >= 0x00001000u && phys < 0x00800000u;
#endif
}

/* Pure logic seams, also used directly by the host test. */
int anchor_player_skin_toggle_active(int toggle_on, int char_index)
{
    return toggle_on && char_index == CHARACTER_EBISUMARU;
}

int anchor_player_skin_edge_toggle(int toggle_on, int pressed, int gate)
{
    if (gate && (pressed & INPUT_L_MASK))
        return !toggle_on;
    return toggle_on;
}

static int resolve_alternative_base(void)
{
    void *resource = func_800141C4_14DC4(ALTERNATIVE_EBISUMARU_FILE);
    unsigned int address;

    if (!resource || resource == (void *)(unsigned long)0xffffffffu)
        return 0;
    address = (unsigned int)(unsigned long)resource & RENDER_CACHE_TAG_MASK;
    if (address < 0x80001000u || address >= 0x80800000u)
        return 0;
    s_alternative_base = address;
    return 1;
}

/* Cache the resident alternative-file size and reserve the low-RDRAM mesh pool.
 * The cached size is what the builder uses to validate all texture pages:
 * advancing the scene sentinel for the pool would otherwise let
 * resident_resource_size() bound the alternative file by the pool instead of the
 * file end. */
static void prepare_alternative_asset(void)
{
    s_alternative_size = resident_resource_size(
        (const unsigned char *)(unsigned long)s_alternative_base);
    /* An unknown bound cannot safely include the four decoded texture pages. */
    if (s_alternative_size < ANCHOR_ALTERNATIVE_RESOURCE_BYTES)
    {
        s_alternative_size = 0;
        SKIN_LOG("[player_skin] alternative resource is incomplete\n");
        return;
    }
    if (!anchor_player_models_reserve_alternative_pool())
        SKIN_LOG("[player_skin] alternative render-data pool unavailable; clothed fallback\n");
}

/* The stage-load hook stages the file. The per-frame path is lookup-only: it
 * may run in title/menu states before any stage load, so it must never invoke
 * the scene resource loader. */
static void ensure_alternative_base(void)
{
    if (s_alternative_base)
        return;
    if (resolve_alternative_base())
    {
        prepare_alternative_asset();
        SKIN_LOG("[player_skin] alternative file %x ready at %x (size %x)\n",
                 ALTERNATIVE_EBISUMARU_FILE, s_alternative_base, s_alternative_size);
    }
}

void anchor_player_skin_load_resources(void)
{
    /* A new stage rebuilds the scene registry and can reuse the old local
     * display-object address, so invalidate the cached base/size and any
     * restore capture. The toggle itself persists across the stage change. */
    s_alternative_base = 0;
    s_alternative_size = 0;
    s_have_capture = 0;
    s_capture_task = 0;
    s_capture_object = 0;
    s_capture_char = 0;
    s_capture_broad_file = 0;
    s_saved_segment8 = 0;
    s_saved_segment9 = 0;
    s_applied_segment8 = 0;
    (void)func_80013B14_14714(ALTERNATIVE_EBISUMARU_FILE);
    if (resolve_alternative_base())
    {
        prepare_alternative_asset();
        SKIN_LOG("[player_skin] alternative file %x ready at %x (size %x)\n",
                 ALTERNATIVE_EBISUMARU_FILE, s_alternative_base, s_alternative_size);
    }
    else
        SKIN_LOG("[player_skin] file %x not resident yet; retrying per frame\n",
                 ALTERNATIVE_EBISUMARU_FILE);
}

void anchor_player_skin_reset(void)
{
    s_toggle_on = 0;
    s_alternative_base = 0;
    s_alternative_size = 0;
    s_have_capture = 0;
    s_capture_task = 0;
    s_capture_object = 0;
    s_capture_char = 0;
    s_capture_broad_file = 0;
    s_saved_segment8 = 0;
    s_saved_segment9 = 0;
    s_applied_segment8 = 0;
}

#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
static int s_test_char = CHARACTER_EBISUMARU;

void anchor_player_skin_test_set_character(int char_index)
{
    s_test_char = char_index;
}

void anchor_player_skin_test_set_player(void *task, void *object)
{
    D_801FC604_5B8514 = task;
    D_801FC60C_5B851C = object;
}
#endif

static int local_character(void)
{
#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
    return s_test_char;
#else
    return (int)(*CURRENT_CHAR_PTR & 3u);
#endif
}

static int local_player_valid(void)
{
    void *task = D_801FC604_5B8514;
    void *object = D_801FC60C_5B851C;
#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
    return task != 0 && object != 0;
#else
    if (!skin_pointer_valid(task) || !skin_pointer_valid(object))
        return 0;
    /* Require the task to still own this exact primary display object. */
    return *(void **)((unsigned char *)task + 0x18) == object;
#endif
}

static int read_pressed_edge(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;

    if (!skin_pointer_valid(system))
        return 0;
    return (int)*(unsigned short *)(system + INPUT_PRESSED_OFFSET);
}

/* Normal-gameplay environment shared by input acceptance and the override
 * itself. Scripted/cutscene/dialog/Impact states suspend the override while
 * the raw toggle latch stays as it is. */
static int environment_ok(void)
{
    if (!local_player_valid())
        return 0;
    if (anchor_dialog_world_paused())
        return 0;
    /* D_800C7AE2/D_800C7AE3 are the native control/sequence locks. Do not use
     * the derived D_800C7DB0 block; anchor_dialog clears it. */
    if (D_800C7AE2 != 0 || D_800C7AE3 != 0)
        return 0;
    if (!item_sync_save_is_loaded())
        return 0;
    if (anchor_impact_native_root_live())
        return 0;
    return 1;
}

static int toggle_allowed(void)
{
    return local_character() == CHARACTER_EBISUMARU && environment_ok();
}

void anchor_player_skin_update(void)
{
    ensure_alternative_base();
    if (local_character() != CHARACTER_EBISUMARU)
    {
        /* Also cover native/scripted swaps that bypass the cycler patch. */
        anchor_player_skin_clear_for_character_change();
        return;
    }
    s_toggle_on = anchor_player_skin_edge_toggle(
        s_toggle_on, read_pressed_edge(), toggle_allowed());
}

int anchor_player_skin_active(void)
{
    return anchor_player_skin_toggle_active(s_toggle_on, local_character()) &&
           environment_ok();
}

int anchor_player_skin_appearance_bit(void)
{
    unsigned char *task = D_801FC604_5B8514;
    unsigned char *object = D_801FC60C_5B851C;

    if (!anchor_player_skin_active() || !anchor_player_skin_alternative_ready() ||
        !anchor_player_models_alternative_broad_base())
        return 0;
    if (!anchor_player_models_alternative_action_base((int)task[0xcc],
            *(unsigned int *)(object + 0x2c)))
        return 0;
    return ANCHOR_APPEARANCE_ALTERNATIVE_EBISUMARU;
}

int anchor_player_skin_alternative_ready(void)
{
    return s_alternative_base != 0 && s_alternative_size >= ANCHOR_ALTERNATIVE_RESOURCE_BYTES;
}

unsigned int anchor_player_skin_alternative_base(void)
{
    return s_alternative_base;
}

unsigned int anchor_player_skin_alternative_size(void)
{
    /* The scene sentinel moves past the pool, so a later registry walk would
     * no longer bound the file correctly. */
    return s_alternative_size;
}

static void capture_local_clothed(void *task, void *object)
{
    unsigned char *bytes = (unsigned char *)object;

    s_saved_segment8 = *(unsigned int *)(bytes + 0x38);
    s_saved_segment9 = *(unsigned int *)(bytes + 0x40);
    s_capture_task = task;
    s_capture_object = object;
    s_capture_char = local_character();
    s_capture_broad_file = *(unsigned short *)(bytes + 0x3c);
    s_have_capture = 1;
}

/* Restore either segment base only if it still carries our binding. Native
 * action changes and character swaps may have replaced either one already. */
static void restore_local_capture(void *task, void *object, unsigned char *bytes)
{
    unsigned int alternative_broad;

    if (!s_have_capture)
        return;
    alternative_broad = anchor_player_models_alternative_broad_base();
    if (s_capture_task == task && s_capture_object == object &&
        skin_pointer_valid(task) && skin_pointer_valid(object) &&
        (s_capture_char == local_character() ||
         *(unsigned short *)(bytes + 0x3c) == s_capture_broad_file))
    {
        if (s_applied_segment8 &&
            *(unsigned int *)(bytes + 0x38) == s_applied_segment8)
            *(unsigned int *)(bytes + 0x38) = s_saved_segment8;
        if (alternative_broad && *(unsigned int *)(bytes + 0x40) == alternative_broad)
            *(unsigned int *)(bytes + 0x40) = s_saved_segment9;
    }
    s_have_capture = 0;
    s_capture_task = 0;
    s_capture_object = 0;
    s_capture_char = 0;
    s_capture_broad_file = 0;
    s_applied_segment8 = 0;
}

void anchor_player_skin_clear_for_character_change(void)
{
    void *task = D_801FC604_5B8514;
    void *object = D_801FC60C_5B851C;

    s_toggle_on = 0;
    restore_local_capture(task, object, (unsigned char *)object);
}

void anchor_player_skin_apply_local(void)
{
    void *task = D_801FC604_5B8514;
    void *object = D_801FC60C_5B851C;
    unsigned char *bytes = (unsigned char *)object;
    unsigned int alternative_broad;
    unsigned int alternative_action;
    int action;

    if (!anchor_player_skin_active() ||
        !anchor_player_skin_alternative_ready() ||
        !skin_pointer_valid(object) ||
        !skin_pointer_valid(task))
    {
        restore_local_capture(task, object, bytes);
        return;
    }

    /* The opening mesh, its textures, and the private playable action must
     * all be ready. Otherwise leave the native clothed binding in place. */
    alternative_broad = anchor_player_models_alternative_broad_base();
    if (!alternative_broad)
    {
        restore_local_capture(task, object, bytes);
        return;
    }
    action = (int)*((unsigned char *)task + 0xcc);
    alternative_action = anchor_player_models_alternative_action_base(
        action, *(unsigned int *)(bytes + 0x2c));
    if (!alternative_action)
    {
        restore_local_capture(task, object, bytes);
        return;
    }

    /* Capture the native segment bases before the first override. */
    if (!s_have_capture || s_capture_task != task ||
        s_capture_object != object)
    {
        capture_local_clothed(task, object);
    }
    else
    {
        /* Native action changes can rebind either segment between frames. */
        if (*(unsigned int *)(bytes + 0x38) != s_applied_segment8)
            s_saved_segment8 = *(unsigned int *)(bytes + 0x38);
        if (*(unsigned int *)(bytes + 0x40) != alternative_broad)
            s_saved_segment9 = *(unsigned int *)(bytes + 0x40);
    }

    /* Preserve the native model pointer, frame, context, segment file IDs and
     * aux fields. The private broad copy keeps cue data at its native offsets. */
    *(unsigned int *)(bytes + 0x38) = alternative_action;
    *(unsigned int *)(bytes + 0x40) = alternative_broad;
    s_applied_segment8 = alternative_action;
}
