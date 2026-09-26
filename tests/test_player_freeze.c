#define ANCHOR_PLAYER_FREEZE_HOST_TEST
#include "../src/combat/anchor_player_freeze.c"
#include "../src/combat/anchor_scripted_state.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
unsigned char D_800C7AE0;
unsigned char D_800C7AE2;
unsigned char D_800C7AE3;
unsigned char D_800C7DB0_C89B0[0x60];
static int s_mock_epoch, s_connected, s_loaded, s_dialog, s_damage_calls;
static int s_last_scripted;
static unsigned int s_health;
static unsigned long long s_player_storage[0x100 / 8];
static unsigned long long s_object_storage[0x80 / 8];
static unsigned long long s_middle_storage[0x80 / 8];
static unsigned long long s_follower_storage[0x80 / 8];
void anchor_player_cube_victim_thaw(void) {}
int anchor_player_cube_victim_moving(void) { return 0; }
int anchor_player_cube_victim_pose(float *x, float *y, float *z)
{
    (void)x; (void)y; (void)z;
    return 0;
}
void func_801CF3A0_58B2B0(void *player) { (void)player; }

static void set_frame(float frame)
{
    *(float *)((unsigned char *)s_object_storage + 0x28) = frame;
}

int anchor_player_damage_apply(float x, float y, float z)
{
    assert(x == 1 && y == 2 && z == 3);
    ++s_damage_calls;
    if (s_health)
        --s_health;
    return 1;
}
int anchor_is_connected(void) { return s_connected; }
int item_sync_save_is_loaded(void) { return s_loaded; }
unsigned int item_sync_local_player_health(void) { return s_health; }
int anchor_player_models_get_epoch(void)
{
    int scripted = anchor_remote_collision_is_scripted_for_epoch();
    if (scripted != s_last_scripted)
    {
        ++s_mock_epoch;
        s_last_scripted = scripted;
    }
    return s_mock_epoch;
}
int anchor_dialog_busy(void) { return s_dialog; }

static void setup(void)
{
    clear_ice();
    s_move_bit = 0;
    memset(s_player_storage, 0, sizeof(s_player_storage));
    memset(s_object_storage, 0, sizeof(s_object_storage));
    memset(s_middle_storage, 0, sizeof(s_middle_storage));
    memset(s_follower_storage, 0, sizeof(s_follower_storage));
    D_801FC604_5B8514 = s_player_storage;
    D_801FC60C_5B851C = s_object_storage;
    *(void **)((unsigned char *)s_player_storage + 0x18) = s_object_storage;
    ((unsigned char *)s_player_storage)[0x60] = 2;
    ((unsigned char *)s_player_storage)[0xcc] = 7;
    *(unsigned int *)((unsigned char *)s_object_storage + 0x2c) = 0x68001000u;
    *(unsigned int *)((unsigned char *)s_follower_storage + 0x2c) = 0x68001001u;
    *(void **)s_object_storage = s_middle_storage;
    *(void **)s_middle_storage = s_follower_storage;
    set_frame(3.0f);
    D_800C7AB2 = 10;
    D_800C7AE0 = D_800C7AE2 = D_800C7AE3 = 0;
    s_mock_epoch = s_connected = s_loaded = 1;
    s_last_scripted = 0;
    s_health = 10;
    s_dialog = s_damage_calls = 0;
    memset(D_800C7DB0_C89B0, 0x5a, sizeof(D_800C7DB0_C89B0));
}

static void test_damage_then_scoped_freeze(void)
{
    int i;
    assert(ICE_FRAMES == 125);
    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    assert(s_damage_calls == 1 && s_health == 9);
    assert(!anchor_player_freeze_active());
    anchor_player_freeze_after_update();
    assert(anchor_player_freeze_active());
    anchor_player_freeze_input();
    assert(D_800C7DB0_C89B0[0] == 0x5a);
    assert(D_800C7DB0_C89B0[1] == 0x5a);
    for (i = 2; i < 0x18; ++i)
        assert(D_800C7DB0_C89B0[i] == 0);
    assert(D_800C7DB0_C89B0[0x18] == 0x5a);
    anchor_player_freeze_before_movement(D_801FC604_5B8514);
    assert(D_800C7AE0 == 2 && anchor_player_freeze_active());
    assert(anchor_remote_collision_is_scripted());
    assert(anchor_player_models_get_epoch() == s_epoch);
    anchor_player_freeze_after_movement();
    assert(D_800C7AE0 == 0);
    assert(anchor_player_models_get_epoch() == s_epoch);
    for (i = 0; i < ICE_FRAMES - 1; ++i)
    {
        anchor_player_freeze_after_update();
        assert(anchor_player_freeze_active());
    }
    anchor_player_freeze_after_update();
    assert(!anchor_player_freeze_active());
    anchor_player_freeze_before_movement(D_801FC604_5B8514);
    assert(D_800C7AE0 == 0);
}

static void test_lifecycle_and_ordinary_hits(void)
{
    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 0));
    assert(!anchor_player_freeze_active());
    assert(!anchor_player_freeze_apply_hit(1, 2, 3, 2));
    assert(s_damage_calls == 1);
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    anchor_player_freeze_after_update();
    ++s_mock_epoch;
    assert(!anchor_player_freeze_active());
    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    anchor_player_freeze_after_update();
    D_800C7AB2 = 11;
    assert(!anchor_player_freeze_active());
    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    anchor_player_freeze_after_update();
    s_connected = 0;
    assert(!anchor_player_freeze_active());
    setup();
    s_health = 1;
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    assert(!anchor_player_freeze_active()); /* Lethal hits still enter death. */
}

static void test_visual_pose_stays_fixed_without_rewinding_native_frame(void)
{
    int action;
    float frame;

    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    assert(!anchor_player_freeze_visual_pose(&action, &frame));
    anchor_player_freeze_after_update();
    assert(anchor_player_freeze_visual_pose(&action, &frame));
    assert(action == 7 && frame == 3.0f);
    set_frame(8.0f);
    anchor_player_freeze_after_update();
    assert(anchor_player_freeze_visual_pose(&action, &frame));
    assert(action == 7 && frame == 3.0f);
    assert(read_frame(s_object_storage) == 8.0f);
    ((unsigned char *)s_player_storage)[0xcc] = 8;
    *(unsigned int *)((unsigned char *)s_object_storage + 0x2c) = 0x68002000u;
    set_frame(0.0f);
    assert(anchor_player_freeze_visual_pose(&action, &frame));
    assert(action == 8 && frame == 0.0f);
    set_frame(4.0f);
    ((unsigned char *)s_player_storage)[0x60] = 3;
    assert(!anchor_player_freeze_visual_pose(&action, &frame));
    assert(!anchor_player_freeze_active());
}

static void test_render_override_restores_each_native_frame(void)
{
    float *primary_frame = (float *)((unsigned char *)s_object_storage + 0x28);
    float *follower_frame = (float *)((unsigned char *)s_follower_storage + 0x28);
    float *middle_frame = (float *)((unsigned char *)s_middle_storage + 0x28);

    setup();
    assert(anchor_player_freeze_apply_hit(1, 2, 3, 1));
    anchor_player_freeze_after_update();
    *primary_frame = 8.0f;
    *follower_frame = 9.0f;
    *middle_frame = 10.0f;
    anchor_player_freeze_before_draw(s_object_storage);
    assert(*primary_frame == 3.0f);
    anchor_player_freeze_before_draw(s_follower_storage);
    assert(*follower_frame == 3.0f);
    anchor_player_freeze_after_draw();
    assert(*follower_frame == 9.0f);
    anchor_player_freeze_after_draw();
    assert(*primary_frame == 8.0f);
    anchor_player_freeze_before_draw(s_middle_storage);
    assert(*middle_frame == 10.0f);
    anchor_player_freeze_after_draw();
    assert(*middle_frame == 10.0f);
    clear_ice();
    anchor_player_freeze_before_draw(s_object_storage);
    assert(*primary_frame == 8.0f);
    anchor_player_freeze_after_draw();
}

int main(void)
{
    test_damage_then_scoped_freeze();
    test_lifecycle_and_ordinary_hits();
    test_visual_pose_stays_fixed_without_rewinding_native_frame();
    test_render_override_restores_each_native_frame();
    puts("player ice damage and lifecycle tests passed");
    return 0;
}
