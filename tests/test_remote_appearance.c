#include "anchor_remote_appearance.h"
#include "anchor_player_models.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The object is surrounded by independent remote-slot state. The real helper
 * must change only native object +0x64 bit0, even on a hidden flicker frame. */
typedef struct TestRemote
{
    int collision_ready;
    AnchorCollisionBody collision_body;
    unsigned char object[0x90];
    int animation_sequence;
    int nameplate_visible;
} TestRemote;

static void expect_only_render_bit_changed(TestRemote *remote,
                                           const TestRemote *before,
                                           unsigned char hidden)
{
    TestRemote expected = *before;
    expected.object[0x64] =
        (unsigned char)((before->object[0x64] & 0xfeu) | hidden);
    assert(memcmp(remote, &expected, sizeof(expected)) == 0);
}

int main(void)
{
    TestRemote remote;
    TestRemote before;
    unsigned int frame;
    const unsigned int model = 0x60012340;
    const float scale = 0.025f;
    const float animation_frame = 17.5f;
    const int gold_and_mini = ANCHOR_APPEARANCE_SUDDEN_IMPACT |
                              ANCHOR_APPEARANCE_MINI_EBISUMARU;

    memset(&remote, 0x5a, sizeof(remote));
    remote.collision_ready = 1;
    remote.collision_body = (AnchorCollisionBody){{10, 20, 30}, 1.625f, 4};
    memcpy(remote.object + 0x2c, &model, sizeof(model));
    memcpy(remote.object + 0x1c, &scale, sizeof(scale));
    memcpy(remote.object + 0x20, &scale, sizeof(scale));
    memcpy(remote.object + 0x24, &scale, sizeof(scale));
    memcpy(remote.object + 0x28, &animation_frame, sizeof(animation_frame));
    remote.animation_sequence = 52;
    remote.nameplate_visible = 1;
    before = remote;

    for (frame = 0; frame < 8; ++frame)
    {
        anchor_remote_appearance_apply_hurt(remote.object,
            gold_and_mini | ANCHOR_APPEARANCE_HURT_RECOVERY,
            (unsigned short)frame);
        expect_only_render_bit_changed(&remote, &before, frame & 1u);
        assert(remote.collision_ready == 1 && remote.nameplate_visible == 1);
    }
    /* A paused frame stays in one phase; the 16-bit native counter wraps. */
    anchor_remote_appearance_apply_hurt(remote.object,
        ANCHOR_APPEARANCE_HURT_RECOVERY, 0xffff);
    expect_only_render_bit_changed(&remote, &before, 1);
    anchor_remote_appearance_apply_hurt(remote.object,
        ANCHOR_APPEARANCE_HURT_RECOVERY, 0xffff);
    expect_only_render_bit_changed(&remote, &before, 1);
    anchor_remote_appearance_apply_hurt(remote.object,
        ANCHOR_APPEARANCE_HURT_RECOVERY, 0);
    expect_only_render_bit_changed(&remote, &before, 0);

    /* Recovery end and a fresh/legacy sample clear stale blink state on both
     * counter phases. Appearance bits for gold and Mini remain independent. */
    anchor_remote_appearance_apply_hurt(remote.object,
        ANCHOR_APPEARANCE_HURT_RECOVERY, 1);
    anchor_remote_appearance_apply_hurt(remote.object, gold_and_mini, 1);
    expect_only_render_bit_changed(&remote, &before, 0);
    anchor_remote_appearance_apply_hurt(remote.object, 0, 0);
    expect_only_render_bit_changed(&remote, &before, 0);
    anchor_remote_appearance_apply_hurt(0, ANCHOR_APPEARANCE_HURT_RECOVERY, 1);

    puts("remote appearance flicker and collision preservation tests passed");
    return 0;
}
