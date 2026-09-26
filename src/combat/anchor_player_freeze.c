#include "combat/anchor_player_freeze.h"
#include "combat/anchor_player_cube.h"
#include "combat/anchor_player_damage.h"
#include "combat/anchor_player_freeze_visual.h"
#include "core/anchor_dialog.h"
#include "player/anchor_player_models.h"
#include "progression/item_sync.h"
#ifndef ANCHOR_PLAYER_FREEZE_HOST_TEST
#include "core/anchor.h"
#include "platform/modding.h"
#include "platform/recomputils.h"
#else
extern int anchor_is_connected(void);
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned short D_800C7AB2;
extern unsigned char D_800C7AE0;
extern unsigned char D_800C7AE2;
extern unsigned char D_800C7AE3;
extern unsigned char D_800C7DB0_C89B0[0x60];
extern void func_801CF3A0_58B2B0(void *player);
extern void anchor_player_cube_victim_breakout(void);
extern int anchor_player_cube_victim_push_pose(float *x, float *y, float *z);

#define ICE_BREAKOUT_PRESSES 12
#define ICE_MASH_BUTTONS 0xc000u
#define ICE_RAW_HELD_OFFSET 0x3b07au
#define ICE_RAW_PRESSED_OFFSET 0x3b07cu
#define ICE_CONTROL_BIT 2u

static void *s_player;
static void *s_object;
static unsigned short s_room;
static int s_epoch;
static int s_pending;
static int s_mash_presses;
static unsigned short s_last_mash_held;
static int s_move_bit;
static int s_pose_ready;
static unsigned char s_character;
static unsigned char s_action;
static unsigned int s_model;
static float s_frame;
#define ICE_DRAW_STACK 8
static struct
{
    void *object;
    float original_frame;
} s_draw_stack[ICE_DRAW_STACK];
static unsigned int s_draw_depth;

static int rdram_pointer(const void *pointer)
{
#ifdef ANCHOR_PLAYER_FREEZE_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return address >= 0x1000u && address < 0x800000u;
#endif
}

static unsigned char read_byte(const void *pointer, unsigned int offset)
{
    return *(const volatile unsigned char *)((const unsigned char *)pointer + offset);
}

static int display_type_two(const void *pointer)
{
#ifdef ANCHOR_PLAYER_FREEZE_HOST_TEST
    /* Host pointers occupy +0..+7, while N64 +4 is the display type. */
    return pointer != 0;
#else
    return read_byte(pointer, 4) == 2;
#endif
}

static unsigned int read_word(const void *pointer, unsigned int offset)
{
    return *(const volatile unsigned int *)((const unsigned char *)pointer + offset);
}

static unsigned short raw_buttons(unsigned int offset)
{
    if (!rdram_pointer(D_8015C5C8_15D1C8))
        return 0;
    return *(const volatile unsigned short *)(D_8015C5C8_15D1C8 + offset);
}

static float read_frame(const void *pointer)
{
    return *(const volatile float *)((const unsigned char *)pointer + 0x28);
}

static void write_frame(void *pointer, float frame)
{
    *(volatile float *)((unsigned char *)pointer + 0x28) = frame;
}

static void place_frozen_body(float x, float y, float z)
{
    void *middle, *follower;
    if (!rdram_pointer(s_player) || !rdram_pointer(s_object) ||
        *(void **)((unsigned char *)s_player + 0x18) != s_object ||
        !display_type_two(s_object))
        return;
    middle = *(void **)s_object;
    if (!rdram_pointer(middle) || !display_type_two(middle))
        return;
    follower = *(void **)middle;
    if (!rdram_pointer(follower) || !display_type_two(follower) ||
        read_word(follower, 0x2c) != s_model + 1u)
        return;
    *(float *)((unsigned char *)s_object + 8) = x;
    *(float *)((unsigned char *)s_object + 0xc) = y;
    *(float *)((unsigned char *)s_object + 0x10) = z;
    *(float *)((unsigned char *)middle + 8) = x;
    *(float *)((unsigned char *)middle + 0xc) = y;
    *(float *)((unsigned char *)middle + 0x10) = z;
    /* Native 801CD084 normally projects the follower after moving primary
     * and middle. Frozen late movement skips that path; invoke only its
     * ground/display projection on this guarded real player chain. */
    func_801CF3A0_58B2B0(s_player);
}

static void latch_pose(void)
{
    s_action = read_byte(s_player, 0xcc);
    s_model = read_word(s_object, 0x2c);
    s_frame = read_frame(s_object);
    s_pose_ready = 1;
}

int anchor_player_freeze_control_scoped(void)
{
    return s_move_bit;
}

static void clear_ice(void)
{
    anchor_player_cube_victim_thaw();
    s_player = s_object = 0;
    s_pending = s_mash_presses = 0;
    s_last_mash_held = 0;
    s_pose_ready = 0;
}

static int ice_live(void)
{
    int current_epoch;
    if (!s_player)
        return 0;
    current_epoch = anchor_player_models_get_epoch();
    if (s_player != D_801FC604_5B8514 ||
        s_object != D_801FC60C_5B851C || s_room != D_800C7AB2 ||
        s_epoch != current_epoch ||
        s_character != read_byte(s_player, 0x60) ||
        !anchor_is_connected() || !item_sync_save_is_loaded() ||
        item_sync_local_player_health() == 0 ||
        D_800C7AE2 || D_800C7AE3 || anchor_dialog_busy() ||
        (D_800C7AE0 & (s_move_bit ? 1u : 3u)))
    {
#if DEBUG_BUTTON_ENABLED
        recomp_printf("[player_ice] cleared early epoch=%d/%d control=%u/%u/%u hp=%u\n",
                      s_epoch, current_epoch, (unsigned int)D_800C7AE0,
                      (unsigned int)D_800C7AE2, (unsigned int)D_800C7AE3,
                      item_sync_local_player_health());
#endif
        clear_ice();
        return 0;
    }
    return 1;
}

int anchor_player_freeze_active(void)
{
    return ice_live() && !s_pending;
}

void anchor_player_freeze_thaw_on_cube_impact(void)
{
    if (ice_live() && !s_pending && anchor_player_cube_victim_moving())
    {
        float x, y, z;
        if (anchor_player_cube_victim_pose(&x, &y, &z))
            place_frozen_body(x, y, z);
        clear_ice();
    }
}

int anchor_player_freeze_visual_pose(int *action, float *frame)
{
    if (!ice_live() || s_pending || !s_pose_ready)
        return 0;
    if (s_action != read_byte(s_player, 0xcc) ||
        s_model != read_word(s_object, 0x2c))
        latch_pose();
    if (action)
        *action = (int)s_action;
    if (frame)
        *frame = s_frame;
    return 1;
}

int anchor_player_freeze_apply_hit(float x, float y, float z, int hit_kind)
{
    int accepted;
    if (hit_kind != 0 && hit_kind != 1)
        return 0;
    accepted = anchor_player_damage_apply(x, y, z);
#if DEBUG_BUTTON_ENABLED
    if (hit_kind == 1)
        recomp_printf("[player_ice] hit accepted=%d hp=%u\n", accepted,
                      item_sync_local_player_health());
#endif
    if (accepted && hit_kind == 1 && item_sync_local_player_health() > 0)
    {
        s_player = D_801FC604_5B8514;
        s_object = D_801FC60C_5B851C;
        s_room = D_800C7AB2;
        s_epoch = anchor_player_models_get_epoch();
        s_pending = 1;
        s_mash_presses = 0;
        s_last_mash_held = raw_buttons(ICE_RAW_HELD_OFFSET) & ICE_MASH_BUTTONS;
        s_pose_ready = 0;
        s_character = read_byte(s_player, 0x60);
    }
    return accepted;
}

/* The native controller collector leaves a derived first-player record at
 * +0x3B0F0. The raw input and other controllers remain untouched. */
RECOMP_HOOK_RETURN("func_80004AF8_56F8")
void anchor_player_freeze_input(void)
{
    int offset;
    unsigned short held, pressed, fresh;
    if (!ice_live())
        return;
    if (!s_pending)
        for (offset = 2; offset < 0x18; ++offset)
            ((volatile unsigned char *)D_800C7DB0_C89B0)[offset] = 0;
    held = raw_buttons(ICE_RAW_HELD_OFFSET) & ICE_MASH_BUTTONS;
    pressed = raw_buttons(ICE_RAW_PRESSED_OFFSET) & ICE_MASH_BUTTONS;
    fresh = pressed & held & (unsigned short)~s_last_mash_held;
    s_last_mash_held = held;
    if (fresh & 0x8000u)
        ++s_mash_presses;
    if (fresh & 0x4000u)
        ++s_mash_presses;
    if (s_mash_presses >= ICE_BREAKOUT_PRESSES)
    {
        /* This input sample still belongs to the trapped player. Do not
         * expose its final A/B press to the ordinary action interpreter. */
        for (offset = 2; offset < 0x18; ++offset)
            ((volatile unsigned char *)D_800C7DB0_C89B0)[offset] = 0;
        (void)anchor_player_freeze_visual_shatter(0, 0, s_epoch);
        anchor_player_cube_victim_breakout();
        clear_ice();
    }
}

RECOMP_HOOK_RETURN("func_801CB824_587734")
void anchor_player_freeze_after_update(void)
{
    /* Leave native damage/action intake intact. The received hit enters at
     * this callback's start, so activation waits until the ordinary update
     * has processed its hurt reaction. */
    if (!ice_live())
        return;
    if (s_pending)
    {
        s_pending = 0;
        latch_pose();
    }
    else if (anchor_player_cube_victim_moving())
    {
        float x, y, z;
        if (anchor_player_cube_victim_pose(&x, &y, &z) &&
            rdram_pointer(s_object))
            place_frozen_body(x, y, z);
    }
    else
    {
        float x, y, z;
        if (anchor_player_cube_victim_push_pose(&x, &y, &z))
        {
            if (rdram_pointer(s_object))
                place_frozen_body(x, y, z);
        }
        else if (s_action != read_byte(s_player, 0xcc) ||
                 s_model != read_word(s_object, 0x2c))
            latch_pose();
    }
}

/* FUN_801D9C54 mirrors the primary pose into the display object reached by
 * two +0 links. Keep the native gameplay clock intact and substitute the
 * latched frame only while either owned object is submitted to the renderer. */
RECOMP_HOOK("func_80016C44_17844")
void anchor_player_freeze_before_draw(void *object)
{
    void *middle;
    void *follower;
    unsigned int depth = s_draw_depth++;

    if (depth >= ICE_DRAW_STACK)
        return;
    s_draw_stack[depth].object = 0;
    if (!anchor_player_freeze_visual_pose(0, 0) ||
        !rdram_pointer(s_player) || !rdram_pointer(s_object) ||
        !rdram_pointer(object) ||
        *(void **)((unsigned char *)s_player + 0x18) != s_object ||
        !display_type_two(s_object) || !display_type_two(object))
        return;
    if (object != s_object)
    {
        middle = *(void **)s_object;
        if (!rdram_pointer(middle) || !display_type_two(middle))
            return;
        follower = *(void **)middle;
        if (!rdram_pointer(follower) || !display_type_two(follower) ||
            object != follower ||
            read_word(follower, 0x2c) != s_model + 1u)
            return;
    }
    s_draw_stack[depth].object = object;
    s_draw_stack[depth].original_frame = read_frame(object);
    write_frame(object, s_frame);
}

RECOMP_HOOK_RETURN("func_80016C44_17844")
void anchor_player_freeze_after_draw(void)
{
    unsigned int depth;
    if (!s_draw_depth)
        return;
    depth = --s_draw_depth;
    if (depth < ICE_DRAW_STACK && s_draw_stack[depth].object)
    {
        write_frame(s_draw_stack[depth].object,
                    s_draw_stack[depth].original_frame);
        s_draw_stack[depth].object = 0;
    }
}

/* Match the existing modal-control pattern only for late movement and
 * collision; the bit is relinquished as soon as that callback returns. */
RECOMP_HOOK("func_801CBAF8_587A08")
void anchor_player_freeze_before_movement(void *player)
{
    s_move_bit = 0;
    if (player == s_player && ice_live() && !s_pending &&
        !(D_800C7AE0 & ICE_CONTROL_BIT))
    {
        D_800C7AE0 |= ICE_CONTROL_BIT;
        s_move_bit = 1;
    }
}

RECOMP_HOOK_RETURN("func_801CBAF8_587A08")
void anchor_player_freeze_after_movement(void)
{
    if (s_move_bit)
        D_800C7AE0 &= (unsigned char)~ICE_CONTROL_BIT;
    s_move_bit = 0;
}
