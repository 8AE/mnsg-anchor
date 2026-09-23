/* Host-side logic tests for the alternative Ebisumaru skin toggle.
 *
 * The module is native-coupled in the MIPS build, so the test compiles it with
 * ANCHOR_PLAYER_SKIN_HOST_TEST and supplies the handful of native states and
 * predicates it reads. The pure mesh graft is tested separately. */

#include <stdio.h>
#include <string.h>

#define ANCHOR_PLAYER_SKIN_HOST_TEST
#include "../src/player/alternative_ebisumaru/anchor_player_skin.c"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 1; \
} } while (0)

/* Resident alternative-file size the skin module sees through the registry stub. */
#define TEST_ALTERNATIVE_SIZE ANCHOR_ALTERNATIVE_RESOURCE_BYTES

/* Native state the skin module reads. */
unsigned char *D_8015C5C8_15D1C8;
unsigned char D_800C7AE2;
unsigned char D_800C7AE3;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;

static int dialog_paused;
static int save_loaded = 1;
static int impact_root_live;
static int alternative_load_calls;
static unsigned int s_alternative_broad_base = 0x80180000u;
static unsigned int s_alternative_action_base = 0x81100000u;

static unsigned char input_system[0x3B080];
static unsigned char player_object[0x90];
static unsigned char player_task[0x100];

static void put_u32(unsigned char *p, unsigned int value)
{
    memcpy(p, &value, sizeof(value));
}

static void put_u16(unsigned char *p, unsigned short value)
{
    memcpy(p, &value, sizeof(value));
}

static void put_float(unsigned char *p, float value)
{
    memcpy(p, &value, sizeof(value));
}

static unsigned int get_u32(const unsigned char *p)
{
    unsigned int value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static unsigned short get_u16(const unsigned char *p)
{
    unsigned short value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static float get_float(const unsigned char *p)
{
    float value;
    memcpy(&value, p, sizeof(value));
    return value;
}

int anchor_dialog_world_paused(void) { return dialog_paused; }
int item_sync_save_is_loaded(void) { return save_loaded; }
int anchor_impact_native_root_live(void) { return impact_root_live; }

void *func_80013B14_14714(unsigned int file_id)
{
    (void)file_id;
    ++alternative_load_calls;
    return (void *)(unsigned long)0x80101000u;
}

void *func_800141C4_14DC4(unsigned int file_id)
{
    if (file_id != 0x4D9u)
        return (void *)(unsigned long)0xffffffffu;
    return (void *)(unsigned long)0x80100000u;
}

/* The skin module derives the alternative file size from the resident registry; the
 * host supplies a fixed bound. */
unsigned int resident_resource_size(const unsigned char *base)
{
    (void)base;
    return TEST_ALTERNATIVE_SIZE;
}

/* The skin module reserves the low render-data pool at stage load and reads
 * the shared copy base each frame. The native builder is exercised by the
 * pure helper below; here the copy is a fixed valid pointer. */
int anchor_player_models_reserve_alternative_pool(void)
{
    return 1;
}

unsigned int anchor_player_models_alternative_broad_base(void)
{
    return s_alternative_broad_base;
}

unsigned int anchor_player_models_alternative_action_base(int action,
                                                    unsigned int model_ptr)
{
    return action == 0 && model_ptr == 0x12345678u ?
           s_alternative_action_base : 0;
}

static void set_pressed(unsigned short pressed)
{
    *(unsigned short *)(input_system + 0x3B07C) = pressed;
}

static void reset_case(void)
{
    anchor_player_skin_reset();
    memset(input_system, 0, sizeof(input_system));
    memset(player_object, 0, sizeof(player_object));
    memset(player_task, 0, sizeof(player_task));
    put_u32(player_object + 0x2c, 0x12345678u);
    D_8015C5C8_15D1C8 = input_system;
    D_800C7AE2 = 0;
    D_800C7AE3 = 0;
    dialog_paused = 0;
    save_loaded = 1;
    impact_root_live = 0;
    alternative_load_calls = 0;
    s_alternative_broad_base = 0x80180000u;
    s_alternative_action_base = 0x81100000u;
    anchor_player_skin_test_set_player(player_task, player_object);
    anchor_player_skin_test_set_character(1); /* Ebisumaru */
}

/* Activate the toggle with one L press and clear the edge again. */
static void enable_skin(void)
{
    set_pressed(0x20);
    anchor_player_skin_update();
    set_pressed(0);
}

static int test_edge_toggle_pure(void)
{
    CHECK(anchor_player_skin_edge_toggle(0, 0, 1) == 0);
    CHECK(anchor_player_skin_edge_toggle(0, 0x20, 1) == 1);
    CHECK(anchor_player_skin_edge_toggle(1, 0x20, 1) == 0);
    CHECK(anchor_player_skin_edge_toggle(1, 0x00, 1) == 1);
    /* A gated frame never flips even with the edge present. */
    CHECK(anchor_player_skin_edge_toggle(0, 0x20, 0) == 0);
    CHECK(anchor_player_skin_edge_toggle(1, 0x20, 0) == 1);
    /* Other bits do not toggle. */
    CHECK(anchor_player_skin_edge_toggle(0, 0x10, 1) == 0);
    CHECK(anchor_player_skin_edge_toggle(0, 0xFFDF, 1) == 0);
    return 0;
}

static int test_active_only_for_ebisumaru(void)
{
    CHECK(!anchor_player_skin_toggle_active(0, 1));
    CHECK(anchor_player_skin_toggle_active(1, 1));
    CHECK(!anchor_player_skin_toggle_active(1, 0));
    CHECK(!anchor_player_skin_toggle_active(1, 2));
    CHECK(!anchor_player_skin_toggle_active(1, 3));
    return 0;
}

static int test_update_toggles_once_per_press(void)
{
    reset_case();
    set_pressed(0);
    anchor_player_skin_update();
    CHECK(!anchor_player_skin_active());
    CHECK(anchor_player_skin_appearance_bit() == 0);

    /* Rising edge: on and the appearance bit is exposed. */
    set_pressed(0x20);
    anchor_player_skin_update();
    CHECK(anchor_player_skin_active());
    CHECK(anchor_player_skin_appearance_bit() ==
          ANCHOR_APPEARANCE_ALTERNATIVE_EBISUMARU);

    /* The pressed edge is one frame only; held/zero does not flip. */
    set_pressed(0);
    anchor_player_skin_update();
    CHECK(anchor_player_skin_active());
    anchor_player_skin_update();
    CHECK(anchor_player_skin_active());

    /* A second press flips it off. */
    set_pressed(0x20);
    anchor_player_skin_update();
    CHECK(!anchor_player_skin_active());
    CHECK(anchor_player_skin_appearance_bit() == 0);

    /* A fresh press flips it back on. */
    set_pressed(0);
    anchor_player_skin_update();
    set_pressed(0x20);
    anchor_player_skin_update();
    CHECK(anchor_player_skin_active());
    return 0;
}

static int test_character_gate(void)
{
    static const int other_characters[] = {0, 2, 3};
    unsigned int i;

    reset_case();
    /* L has no effect on any of the other three playable characters. */
    for (i = 0; i < sizeof(other_characters) / sizeof(other_characters[0]); ++i)
    {
        anchor_player_skin_test_set_character(other_characters[i]);
        set_pressed(0x20);
        anchor_player_skin_update();
        CHECK(!anchor_player_skin_active());
        CHECK(anchor_player_skin_appearance_bit() == 0);
        CHECK(!s_toggle_on);
        set_pressed(0);
    }
    anchor_player_skin_test_set_character(1);
    CHECK(!anchor_player_skin_active());

    enable_skin();
    CHECK(anchor_player_skin_active());

    /* Switching away clears the latch, including for a return to Ebisumaru. */
    anchor_player_skin_test_set_character(0);
    anchor_player_skin_update();
    CHECK(!anchor_player_skin_active());
    CHECK(!s_toggle_on);
    anchor_player_skin_test_set_character(1);
    CHECK(!anchor_player_skin_active());
    CHECK(anchor_player_skin_appearance_bit() == 0);

    /* A fresh Ebisumaru-only press activates it again. */
    enable_skin();
    CHECK(anchor_player_skin_active());

    /* No local player => not active. */
    anchor_player_skin_test_set_player(0, 0);
    CHECK(!anchor_player_skin_active());

    /* The appearance bit is meaningful only for Ebisumaru even when on. */
    anchor_player_skin_test_set_player(player_task, player_object);
    anchor_player_skin_test_set_character(2);
    CHECK(anchor_player_skin_appearance_bit() == 0);
    return 0;
}

static int test_toggle_gating(void)
{
    struct GateCase { int dialog; int lock2; int lock3; int save; int impact; }
        cases[] = {
            {1, 0, 0, 1, 0},
            {0, 1, 0, 1, 0},
            {0, 0, 1, 1, 0},
            {0, 0, 0, 0, 0},
            {0, 0, 0, 1, 1},
        };
    unsigned int i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        reset_case();
        dialog_paused = cases[i].dialog;
        D_800C7AE2 = (unsigned char)cases[i].lock2;
        D_800C7AE3 = (unsigned char)cases[i].lock3;
        save_loaded = cases[i].save;
        impact_root_live = cases[i].impact;
        set_pressed(0x20);
        anchor_player_skin_update();
        CHECK(!anchor_player_skin_active());
        /* Clearing the gate lets the same held edge through. */
        dialog_paused = 0;
        D_800C7AE2 = 0;
        D_800C7AE3 = 0;
        save_loaded = 1;
        impact_root_live = 0;
        anchor_player_skin_update();
        CHECK(anchor_player_skin_active());
    }
    return 0;
}

static int test_suspend_retains_latch(void)
{
    reset_case();
    set_pressed(0x20);
    anchor_player_skin_update();
    set_pressed(0);
    CHECK(anchor_player_skin_active());

    /* Entering a suspended environment hides the override and clears the
     * published bit while the raw toggle latch stays on. */
    D_800C7AE3 = 1;
    CHECK(!anchor_player_skin_active());
    CHECK(anchor_player_skin_appearance_bit() == 0);

    /* Input is not accepted while suspended, so the latch cannot flip. */
    set_pressed(0x20);
    anchor_player_skin_update();
    set_pressed(0);
    CHECK(!anchor_player_skin_active());

    /* Resuming normal gameplay restores the override from the retained latch. */
    D_800C7AE3 = 0;
    CHECK(anchor_player_skin_active());

    /* A press now flips it off. */
    set_pressed(0x20);
    anchor_player_skin_update();
    CHECK(!anchor_player_skin_active());
    return 0;
}

static int test_load_resources_stages_asset(void)
{
    reset_case();
    anchor_player_skin_load_resources();
    CHECK(alternative_load_calls == 1);
    CHECK(anchor_player_skin_alternative_ready());
    CHECK(anchor_player_skin_alternative_base() == 0x80100000u);
    CHECK(anchor_player_skin_alternative_size() == TEST_ALTERNATIVE_SIZE);
    return 0;
}

static int test_remote_rebind_guard_includes_alternative(void)
{
    /* Identical bound state must not rebind. */
    CHECK(!anchor_player_model_remote_rebind_required(
        1, 5, 0, 0, 1, 5, 0, 0));
    CHECK(!anchor_player_model_remote_rebind_required(
        1, 5, 0, 1, 1, 5, 0, 1));
    /* Clothed -> alternative and alternative -> clothed both rebind. */
    CHECK(anchor_player_model_remote_rebind_required(
        1, 5, 0, 0, 1, 5, 0, 1));
    CHECK(anchor_player_model_remote_rebind_required(
        1, 5, 0, 1, 1, 5, 0, 0));
    /* The pre-existing guard fields still rebind. */
    CHECK(anchor_player_model_remote_rebind_required(
        1, 5, 0, 0, 0, 5, 0, 0));
    CHECK(anchor_player_model_remote_rebind_required(
        1, 5, 0, 0, 1, 6, 0, 0));
    CHECK(anchor_player_model_remote_rebind_required(
        1, 5, 0, 0, 1, 5, 1, 0));
    return 0;
}

static int test_alternative_render_data_offsets(void)
{
    CHECK(ANCHOR_ALTERNATIVE_MESH_OFFSET == 0x18000u);
    CHECK(ANCHOR_ALTERNATIVE_RENDER_DATA_SIZE == 0x28000u);
    CHECK(ANCHOR_ALTERNATIVE_RESOURCE_BYTES == 0x9d70u);
    CHECK(ANCHOR_ALTERNATIVE_MESH_OFFSET + ANCHOR_ALTERNATIVE_RESOURCE_BYTES <=
          ANCHOR_ALTERNATIVE_RENDER_DATA_SIZE);
    return 0;
}

/* Fill every object field the mesh graft must leave intact with a distinct
 * value and return the original +0x40. */
static void put_native_fields(unsigned int segment9)
{
    put_u32(player_object + 0x2c, 0x12345678u);
    put_u32(player_object + 0x30, 0xc01fc680u);
    put_u16(player_object + 0x34, 0x0012u);
    put_u32(player_object + 0x38, 0x80012345u);
    put_u16(player_object + 0x3c, 0x0034u);
    put_u32(player_object + 0x40, segment9);
    put_u16(player_object + 0x44, 0x0056u);
    put_u32(player_object + 0x48, 0x800aaaaau);
    put_u32(player_object + 0x50, 0x800bbbbbu);
    put_u32(player_object + 0x58, 0x800cccccu);
    put_u32(player_object + 0x60, 0x800dddddu);
    put_u32(player_object + 0x70, 0x0000abcdu);
    put_float(player_object + 0x28, 12.5f);
}

static int native_fields_intact(unsigned int segment8, unsigned int segment9)
{
    return get_u32(player_object + 0x2c) == 0x12345678u &&
           get_u32(player_object + 0x30) == 0xc01fc680u &&
           get_u16(player_object + 0x34) == 0x0012u &&
           get_u32(player_object + 0x38) == segment8 &&
           get_u16(player_object + 0x3c) == 0x0034u &&
           get_u32(player_object + 0x40) == segment9 &&
           get_u16(player_object + 0x44) == 0x0056u &&
           get_u32(player_object + 0x48) == 0x800aaaaau &&
           get_u32(player_object + 0x50) == 0x800bbbbbu &&
           get_u32(player_object + 0x58) == 0x800cccccu &&
           get_u32(player_object + 0x60) == 0x800dddddu &&
           get_u32(player_object + 0x70) == 0x0000abcdu &&
           get_float(player_object + 0x28) == 12.5f;
}

static int test_local_apply_binds_mesh_and_restores(void)
{
    unsigned int clothed = 0x80099999u;

    reset_case();
    put_native_fields(clothed);

    /* Inactive: nothing is touched. */
    anchor_player_skin_apply_local();
    CHECK(native_fields_intact(0x80012345u, clothed));

    /* The playable model/frame stay native; the action tree and broad base
     * switch together to the private mesh graft. */
    enable_skin();
    CHECK(anchor_player_skin_active());
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == s_alternative_broad_base);
    CHECK(native_fields_intact(s_alternative_action_base, s_alternative_broad_base));

    /* A later frame re-asserts +0x40 without touching the native frame
     * advance (+0x28), model pointer (+0x2c), context (+0x30) or any field. */
    put_u32(player_object + 0x40, 0x80011111u);
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == s_alternative_broad_base);
    CHECK(native_fields_intact(s_alternative_action_base, s_alternative_broad_base));

    /* Disable: the latest native binding comes back. */
    set_pressed(0x20);
    anchor_player_skin_update();
    set_pressed(0);
    CHECK(!anchor_player_skin_active());
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == 0x80011111u);
    CHECK(native_fields_intact(0x80012345u, 0x80011111u));
    return 0;
}

static int test_copy_unavailable_falls_back_to_clothed(void)
{
    unsigned int clothed = 0x80099999u;

    reset_case();
    put_native_fields(clothed);
    s_alternative_broad_base = 0;

    enable_skin();
    CHECK(anchor_player_skin_active());
    anchor_player_skin_apply_local();
    /* No copy: +0x40 stays native and no capture is taken. */
    CHECK(get_u32(player_object + 0x40) == clothed);
    CHECK(native_fields_intact(0x80012345u, clothed));
    CHECK(!s_have_capture);
    CHECK(anchor_player_skin_appearance_bit() == 0);
    return 0;
}

static int test_character_swap_restores_and_clears(void)
{
    unsigned int clothed = 0x80099999u;

    reset_case();
    put_native_fields(clothed);
    enable_skin();
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == s_alternative_broad_base);

    /* The cycler calls this before changing the native character id. */
    anchor_player_skin_clear_for_character_change();
    CHECK(native_fields_intact(0x80012345u, clothed));
    CHECK(!s_have_capture);
    CHECK(!s_toggle_on);
    anchor_player_skin_test_set_character(0);
    anchor_player_skin_apply_local();
    CHECK(native_fields_intact(0x80012345u, clothed));
    anchor_player_skin_test_set_character(1);
    CHECK(!anchor_player_skin_active());
    return 0;
}

static int test_detected_character_swap_restores_old_model(void)
{
    unsigned int clothed = 0x80099999u;

    reset_case();
    put_native_fields(clothed);
    enable_skin();
    anchor_player_skin_apply_local();
    CHECK(native_fields_intact(s_alternative_action_base, s_alternative_broad_base));

    /* A native path changed the character id while the display object still
     * has Ebisumaru's model. The frame update must clear the skin and restore
     * the two bases before that object can be rendered as another character. */
    anchor_player_skin_test_set_character(2);
    anchor_player_skin_update();
    CHECK(native_fields_intact(0x80012345u, clothed));
    CHECK(!s_have_capture);
    CHECK(!s_toggle_on);
    anchor_player_skin_test_set_character(1);
    CHECK(!anchor_player_skin_active());
    return 0;
}

static int test_swap_does_not_overwrite_new_native_model(void)
{
    reset_case();
    put_native_fields(0x80099999u);
    enable_skin();
    anchor_player_skin_apply_local();

    /* Native has already rebound another character on the same object. */
    put_u16(player_object + 0x3c, 0x0099u);
    put_u32(player_object + 0x38, 0x80077777u);
    put_u32(player_object + 0x40, 0x80088888u);
    anchor_player_skin_test_set_character(3);
    anchor_player_skin_update();
    CHECK(get_u32(player_object + 0x38) == 0x80077777u);
    CHECK(get_u32(player_object + 0x40) == 0x80088888u);
    CHECK(!s_have_capture);
    CHECK(!s_toggle_on);
    return 0;
}

static int test_restore_dropped_after_native_reinit(void)
{
    unsigned int clothed = 0x80099999u;
    unsigned int native = 0x80011111u;

    reset_case();
    put_native_fields(clothed);
    enable_skin();
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == s_alternative_broad_base);

    /* Native reinitialized the record before the disable frame; the mesh
     * graft stamp is gone, so the stale broad base is not written back. */
    put_u32(player_object + 0x40, native);
    set_pressed(0x20);
    anchor_player_skin_update();
    set_pressed(0);
    CHECK(!anchor_player_skin_active());
    anchor_player_skin_apply_local();
    CHECK(get_u32(player_object + 0x40) == native);
    return 0;
}

int main(void)
{
    if (test_edge_toggle_pure() ||
        test_active_only_for_ebisumaru() ||
        test_update_toggles_once_per_press() ||
        test_character_gate() ||
        test_toggle_gating() ||
        test_suspend_retains_latch() ||
        test_load_resources_stages_asset() ||
        test_remote_rebind_guard_includes_alternative() ||
        test_alternative_render_data_offsets() ||
        test_local_apply_binds_mesh_and_restores() ||
        test_copy_unavailable_falls_back_to_clothed() ||
        test_character_swap_restores_and_clears() ||
        test_detected_character_swap_restores_old_model() ||
        test_swap_does_not_overwrite_new_native_model() ||
        test_restore_dropped_after_native_reinit())
        return 1;
    puts("alternative Ebisumaru skin toggle logic tests passed");
    return 0;
}
