#include "anchor_remote_motion.h"

#define CHECK(condition)       \
    do                         \
    {                          \
        if (!(condition))      \
            return __LINE__;   \
    } while (0)

static float abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

static int near_float(float actual, float expected)
{
    return abs_float(actual - expected) < 0.01f;
}

static AnchorRemoteMotionSample sample_at(
    int room, int seq, float x, float y, float z,
    float vx, float vy, float vz, int rot_y)
{
    AnchorRemoteMotionSample sample;

    sample.room = room;
    sample.seq = seq;
    sample.timestamp_ms = 0;
    sample.position.x = x;
    sample.position.y = y;
    sample.position.z = z;
    sample.velocity.x = vx;
    sample.velocity.y = vy;
    sample.velocity.z = vz;
    sample.angular_velocity.x = 0.0f;
    sample.angular_velocity.y = 0.0f;
    sample.angular_velocity.z = 0.0f;
    sample.action = 1;
    sample.rot_x = 0;
    sample.rot_y = rot_y;
    sample.rot_z = 0;
    return sample;
}

static int test_constant_motion_stays_in_phase(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        10, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    anchor_remote_motion_reset(&state);
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(near_float(output.position.x, 0.0f));
    for (frame = 1; frame <= 72; ++frame)
    {
        if (frame % 6 == 0)
        {
            sample.seq++;
            sample.position.x = (float)frame * 10.0f;
        }
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, (float)frame * 10.0f));
    }
    return 0;
}

static int test_new_velocity_controls_packet_frame(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.position.x = 4.0f;
    sample.velocity.x = 120.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 4.0f));
    return 0;
}

static int test_stop_is_immediate_and_exact(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 11; ++frame)
    {
        if (frame == 6)
        {
            sample.seq = 2;
            sample.position.x = 60.0f;
        }
        anchor_remote_motion_step(&state, &sample, &output);
    }
    CHECK(near_float(output.position.x, 110.0f));

    sample.seq = 3;
    sample.position.x = 110.0f;
    sample.velocity.x = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 110.0f));
    for (frame = 0; frame < 12; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, 110.0f));
    }
    return 0;
}

static int test_stop_clears_outstanding_horizontal_debt(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.x = 66.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.correction_frames == 4);

    sample.seq = 3;
    sample.velocity.x = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 66.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    for (i = 0; i < 8; ++i)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, 66.0f));
    }
    return 0;
}

static int test_one_axis_wall_stop_clears_only_that_axis(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 300.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.x = 66.0f;
    sample.position.z = 66.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.correction_frames == 4);

    sample.seq = 3;
    sample.position.x = 66.0f;
    sample.position.z = 74.0f;
    sample.velocity.x = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 66.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    CHECK(state.velocity_per_frame.z > 0.0f);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 66.0f));
    CHECK(output.position.z > 74.0f);
    return 0;
}

static int test_residual_is_paid_without_easing(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.position.x = 66.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 60.0f));
    CHECK(state.correction_frames == 4);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 71.5f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 83.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 94.5f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 106.0f));
    CHECK(state.correction_frames == 0);
    return 0;
}

static int test_retarget_does_not_apply_two_corrections(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.x = 66.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 60.0f));

    sample.seq = 3;
    sample.position.x = 74.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 71.5f));
    CHECK(state.correction_frames == 4);
    return 0;
}

static int test_large_negative_residual_cannot_reverse_forward_motion(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    float previous_x;
    float previous_debt;
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 50.0f));

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.x = 12.0f;
    sample.velocity.x = 90.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 53.0f));
    CHECK(near_float(state.correction_per_frame.x, -1.5f));
    previous_debt = -state.correction_debt.x;
    previous_x = output.position.x;
    for (frame = 0; frame < 5; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(output.position.x >= previous_x);
        CHECK(output.position.x - previous_x <= 1.501f);
        previous_x = output.position.x;
    }
    CHECK(state.correction_frames > 0);
    CHECK(-state.correction_debt.x < previous_debt);

    /* A coherent follow-up endpoint retargets the retained residual instead
     * of restoring full speed for a packet and recreating a sawtooth. */
    sample.seq = 3;
    sample.timestamp_ms = 1400;
    sample.position.x = 30.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.position.x >= previous_x);
    CHECK(output.position.x - previous_x <= 1.501f);
    CHECK(-state.correction_debt.x < previous_debt);
    return 0;
}

static int test_position_reversal_rebounds_retained_debt(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 0; frame < 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.position.x = 66.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 60.0f));
    CHECK(near_float(state.correction_per_frame.x, 1.5f));

    sample.seq = 3;
    sample.position.x = 65.0f;
    sample.velocity.x = -30.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.position.x < 60.0f);
    CHECK(near_float(output.position.x, 59.5f));
    CHECK(near_float(state.correction_per_frame.x, 0.5f));
    return 0;
}

static int test_vertical_acceleration_crosses_apex_once(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 180.0f, 0.0f, 0);
    float source_y = 0.0f;
    float source_vy = 6.0f;
    float apex_y = 0.0f;
    int frame;

    sample.action = 0x17;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 18; ++frame)
    {
        source_vy -= 0.6666667f;
        source_y += source_vy;
        if (frame % 6 == 0)
        {
            sample.seq++;
            sample.position.y = source_y;
            sample.velocity.y = source_vy * 30.0f;
        }
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.y, source_y));
        if (frame == 9)
            apex_y = output.position.y;
        if (frame > 9)
            CHECK(output.position.y < apex_y);
    }
    CHECK(near_float(state.vertical_acceleration_per_frame, -0.6666667f));
    return 0;
}

static int test_takeoff_uses_native_gravity_before_second_packet(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.action = 0x17;
    sample.velocity.y = 180.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 0.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, -0.6666667f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 5.333333f));
    return 0;
}

static int test_ledge_entry_and_first_descent_use_native_gravity(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.action = 0x17;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.vertical_acceleration_per_frame, -0.6666667f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, -0.6666667f));

    sample = sample_at(
        1, 1, 0.0f, 100.0f, 0.0f, 0.0f, -90.0f, 0.0f, 0);
    sample.action = 0x18;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.vertical_acceleration_per_frame, -0.6666667f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 96.333333f));
    return 0;
}

static int test_fallback_gravity_clamps_at_native_terminal_speed(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 100.0f, 0.0f, 0.0f, -210.0f, 0.0f, 0);

    sample.action = 0x18;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.velocity_per_frame.y, -7.6666667f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.velocity_per_frame.y, -8.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.velocity_per_frame.y, -8.0f));
    return 0;
}

static int test_authoritative_terminal_endpoint_stays_terminal(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 100.0f, 0.0f, 0.0f, -210.0f, 0.0f, 0);
    int frame;

    sample.action = 0x18;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.y = 52.333333f;
    sample.velocity.y = -240.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 52.333333f));
    CHECK(near_float(state.velocity_per_frame.y, -8.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, 0.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 44.333333f));
    CHECK(near_float(state.velocity_per_frame.y, -8.0f));
    return 0;
}

static int test_learned_gravity_clamps_at_native_terminal_speed(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 100.0f, 0.0f, 0.0f, -60.0f, 0.0f, 0);
    int frame;

    sample.action = 0x18;
    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame < 6; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.y = 74.0f;
    sample.velocity.y = -180.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 74.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, -0.6666667f));
    CHECK(state.use_native_gravity_limit);

    for (frame = 0; frame < 6; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(state.velocity_per_frame.y >= -8.0f);
    }
    CHECK(near_float(state.velocity_per_frame.y, -8.0f));
    CHECK(near_float(output.position.y, 28.0f));
    return 0;
}

static int test_sparse_edges_and_angle_wrap_helpers(void)
{
    CHECK(anchor_remote_motion_axis_has_edge(300, 0, 30));
    CHECK(anchor_remote_motion_axis_has_edge(0, -300, 30));
    CHECK(anchor_remote_motion_axis_has_edge(300, -300, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(300, 150, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(10, -10, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(29, 31, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(31, 29, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(0, 31, 30));
    CHECK(anchor_remote_motion_axis_has_edge(0, 38, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(38, 23, 30));
    CHECK(anchor_remote_motion_axis_has_edge(38, 22, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(30, 0, 960));
    CHECK(anchor_remote_motion_axis_has_edge(3000, 0, 960));
    CHECK(anchor_remote_motion_axis_has_edge(3000, 30, 960));
    CHECK(anchor_remote_motion_axis_has_edge(300, 10, 30));
    CHECK(!anchor_remote_motion_axis_has_edge(930, 960, 960));
    CHECK(!anchor_remote_motion_axis_has_edge(960, 930, 960));
    CHECK(anchor_remote_motion_angle_delta_s16(-32760, 32760) == 16);
    CHECK(anchor_remote_motion_angle_delta_s16(32760, -32760) == -16);
    return 0;
}

static int test_landing_clears_outstanding_vertical_debt(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 100.0f, 0.0f, 0.0f, -300.0f, 0.0f, 0);
    int i;

    sample.action = 0x1a;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.y = 45.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.correction_frames == 4);
    CHECK(output.position.y != sample.position.y);

    sample.seq = 3;
    sample.position.y = 0.0f;
    sample.velocity.y = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 0.0f));
    CHECK(near_float(state.velocity_per_frame.y, 0.0f));
    CHECK(near_float(state.correction_per_frame.y, 0.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, 0.0f));
    for (i = 0; i < 8; ++i)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.y, 0.0f));
    }
    return 0;
}

static int test_airborne_action_edge_keeps_gravity(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 360.0f, 0.0f, 0);
    int frame;

    sample.action = 0x17;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 11; ++frame)
    {
        if (frame == 6)
        {
            sample.seq = 2;
            sample.position.y = 51.0f;
            sample.velocity.y = 180.0f;
        }
        anchor_remote_motion_step(&state, &sample, &output);
    }
    CHECK(near_float(state.vertical_acceleration_per_frame, -1.0f));
    CHECK(near_float(output.position.y, 66.0f));

    sample.seq = 3;
    sample.position.y = 66.0f;
    sample.velocity.y = 0.0f;
    sample.action = 0x18;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 66.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, -1.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 65.0f));
    return 0;
}

static int test_non_airborne_vertical_stop_does_not_fall_back(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 150.0f, 0.0f, 0);
    int frame;

    sample.action = 1;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.position.y = 25.0f;
    sample.velocity.y = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 25.0f));
    CHECK(near_float(state.vertical_acceleration_per_frame, 0.0f));
    for (frame = 0; frame < 8; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.y, 25.0f));
    }
    return 0;
}

static int test_prediction_is_bounded_to_one_packet(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 30; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 60.0f));
    return 0;
}

static int test_float_position_is_preserved(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 12.25f, -3.5f, 7.75f, 0.0f, 0.0f, 0.0f, 0);

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.position.x == 12.25f);
    CHECK(output.position.y == -3.5f);
    CHECK(output.position.z == 7.75f);
    return 0;
}

static int test_room_and_teleport_snap_then_hold(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.position.x = 1000.0f;
    sample.velocity.x = 30000.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(near_float(output.position.x, 1000.0f));
    CHECK(near_float(state.velocity_per_frame.x, 0.0f));
    for (i = 0; i < 5; ++i)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, 1000.0f));
    }

    sample.room = 2;
    sample.seq = 3;
    sample.position.x = -250.0f;
    sample.velocity.x = 300.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(near_float(output.position.x, -250.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, -250.0f));
    return 0;
}

static int test_rotation_uses_shortest_arc_without_spin(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 32760);
    int i;

    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.rot_y = -32760;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 32765);
    for (i = 0; i < 2; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(abs_float((float)(output.rot_y + 32760)) <= 1.0f);
    for (i = 0; i < 10; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(abs_float((float)(output.rot_y + 32760)) <= 1.0f);
    return 0;
}

static int test_endpoint_angular_velocity_stays_in_phase_and_stops(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.angular_velocity.y = 3000.0f;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 11; ++frame)
    {
        if (frame == 6)
        {
            sample.seq = 2;
            sample.rot_y = 600;
        }
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(output.rot_y == frame * 100);
    }

    sample.seq = 3;
    sample.rot_y = 1100;
    sample.angular_velocity.y = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 1100);
    for (frame = 0; frame < 10; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(output.rot_y == 1100);
    }
    return 0;
}

static int test_angular_deceleration_never_overshoots_endpoint(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    sample.angular_velocity.y = 3000.0f;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 5; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(output.rot_y == frame * 100);
    }

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.rot_y = 600;
    sample.angular_velocity.y = 30.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 533);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 566);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(abs_float((float)(output.rot_y - 600)) <= 1.0f);
    for (frame = 0; frame < 8; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(abs_float((float)(output.rot_y - 600)) <= 1.0f);
    }
    return 0;
}

static int test_rotation_reversal_discards_stale_correction(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 500);

    anchor_remote_motion_reset(&state);
    state.initialized = 1;
    state.room = 1;
    state.seq = 1;
    state.timestamp_ms = 1000;
    state.rotation.y = 600.0f;
    state.raw_rotation.y = 600.0f;
    state.angular_velocity_per_frame.y = 100.0f;
    state.raw_angular_velocity_per_frame.y = 100.0f;
    state.raw_action = 1;
    state.rotation_correction_per_frame.y = 40.0f;
    state.rotation_correction_frames = 2;
    sample.timestamp_ms = 1033;
    sample.angular_velocity.y = -3000.0f;

    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 500);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 400);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 300);
    return 0;
}

static int test_rotation_residual_cannot_reverse_continuing_turn(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    sample.angular_velocity.y = 3000.0f;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 500);

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.rot_y = 120;
    sample.angular_velocity.y = 1200.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 500);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 500);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 500);
    CHECK(state.angular_velocity_per_frame.y > 0.0f);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y > 500);
    return 0;
}

static int test_sustained_slow_rotation_is_carried_between_packets(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    sample.timestamp_ms = 1000;
    sample.angular_velocity.y = 30.0f;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.rot_y = 6;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(state.angular_velocity_per_frame.y, 1.0f));
    CHECK(output.rot_y == 2);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 4);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 6);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 7);
    return 0;
}

static int test_quantized_zero_endpoint_keeps_coherent_slow_turn(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    sample.angular_velocity.y = 30.0f;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 0; frame < 5; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.rot_y = 3;
    sample.angular_velocity.y = 0.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.angular_velocity_per_frame.y > 0.0f);
    CHECK(near_float(state.angular_velocity_per_frame.y, 0.5f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y >= 2);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y >= 3);
    return 0;
}

static int test_sender_time_preserves_packet_cadence(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);

    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 4;
    sample.timestamp_ms = 1600;
    sample.position.x = 180.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(output.position.x, 180.0f));

    for (i = 0; i < 18; ++i)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 5;
    sample.timestamp_ms = 1800;
    sample.position.x = 240.0f;
    CHECK(!anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    return 0;
}

static int test_equal_sequence_new_timestamp_is_consumed(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 7, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.consumed_sample);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(!output.consumed_sample);
    sample.timestamp_ms = 1033;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.consumed_sample);

    sample.seq = 0;
    sample.timestamp_ms = 2000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.consumed_sample);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(!output.consumed_sample);
    sample.timestamp_ms = 2033;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.consumed_sample);
    return 0;
}

static int test_one_tick_late_packet_keeps_motion_phase(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 6; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, (float)frame * 10.0f));
    }
    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.x = 60.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 70.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 80.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 90.0f));
    for (frame = 0; frame < 3; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 120.0f));

    sample.seq = 3;
    sample.timestamp_ms = 1400;
    sample.position.x = 120.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 130.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    CHECK(state.projection_lead_frames == 1);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 140.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 150.0f));
    return 0;
}

static int test_one_tick_late_airborne_packet_keeps_velocity_phase(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 180.0f, 0.0f, 0);
    int frame;

    sample.action = 0x17;
    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 6; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 22.0f));

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.y = 22.0f;
    sample.velocity.y = 60.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 24.0f));
    CHECK(near_float(state.velocity_per_frame.y, 1.333333f));
    CHECK(near_float(state.correction_per_frame.y, -0.166667f));

    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 24.5f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.y, 24.333333f));
    return 0;
}

static int test_one_tick_early_packet_preserves_signed_phase(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 4; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.x = 60.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 50.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    CHECK(state.projection_lead_frames == -1);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 60.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 70.0f));
    return 0;
}

static int test_early_rate_change_finishes_tick_at_previous_speed(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 150.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 4; ++frame)
        anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 20.0f));

    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.x = 35.0f;
    sample.velocity.x = 300.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 25.0f));
    CHECK(near_float(state.correction_per_frame.x, 0.0f));
    CHECK(state.projection_lead_frames == -1);
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 35.0f));
    return 0;
}

static int test_consecutive_early_packets_accumulate_signed_phase(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    float previous_x = 0.0f;
    int interval;
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (interval = 1; interval <= 3; ++interval)
    {
        for (frame = 0; frame < 4; ++frame)
        {
            anchor_remote_motion_step(&state, &sample, &output);
            CHECK(near_float(output.position.x - previous_x, 10.0f));
            previous_x = output.position.x;
        }
        sample.seq++;
        sample.timestamp_ms += 200;
        sample.position.x = (float)interval * 60.0f;
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x - previous_x, 10.0f));
        previous_x = output.position.x;
        CHECK(state.projection_lead_frames == -interval);
    }
    return 0;
}

static int test_alternating_early_and_late_packets_keep_constant_speed(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    float previous_x = 0.0f;
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    for (frame = 1; frame <= 4; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x - previous_x, 10.0f));
        previous_x = output.position.x;
    }
    sample.seq = 2;
    sample.timestamp_ms = 1200;
    sample.position.x = 60.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x - previous_x, 10.0f));
    previous_x = output.position.x;
    CHECK(state.projection_lead_frames == -1);

    for (frame = 0; frame < 6; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x - previous_x, 10.0f));
        previous_x = output.position.x;
    }
    sample.seq = 3;
    sample.timestamp_ms = 1400;
    sample.position.x = 120.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x - previous_x, 10.0f));
    previous_x = output.position.x;
    CHECK(state.projection_lead_frames == 0);

    for (frame = 0; frame < 4; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x - previous_x, 10.0f));
        previous_x = output.position.x;
    }
    sample.seq = 4;
    sample.timestamp_ms = 1600;
    sample.position.x = 180.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x - previous_x, 10.0f));
    CHECK(state.projection_lead_frames == -1);
    return 0;
}

static int test_early_event_packet_keeps_six_tick_horizon(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int frame;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);
    anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 2;
    sample.timestamp_ms = 1100;
    sample.position.x = 30.0f;
    sample.action = 2;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(output.position.x, 30.0f));
    for (frame = 4; frame <= 8; ++frame)
    {
        anchor_remote_motion_step(&state, &sample, &output);
        CHECK(near_float(output.position.x, (float)frame * 10.0f));
    }

    sample.seq = 3;
    sample.timestamp_ms = 1300;
    sample.position.x = 90.0f;
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(output.position.x, 90.0f));
    return 0;
}

static int test_discontinuity_reseeds_nominal_cadence(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.0f, 0);
    int i;

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    state.expected_interval_frames = 15;

    sample.room = 2;
    sample.seq = 2;
    sample.timestamp_ms = 1100;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(state.velocity_per_frame.x, 0.0f));
    for (i = 0; i < 5; ++i)
        anchor_remote_motion_step(&state, &sample, &output);

    sample.seq = 3;
    sample.timestamp_ms = 1300;
    sample.position.x = 60.0f;
    CHECK(!anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    return 0;
}

static int test_sender_time_detects_paused_receiver(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 31;
    sample.timestamp_ms = 4001;
    sample.position.x = 25.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(state.velocity_per_frame.x, 0.0f));
    return 0;
}

static int test_coalesced_motion_rebases_without_catch_up_burst(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 0.0f, 0);

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 10;
    sample.timestamp_ms = 1900;
    sample.position.x = 900.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(state.expected_interval_frames == 6);
    CHECK(near_float(output.position.x, 900.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(near_float(output.position.x, 933.3333f));
    return 0;
}

static int test_coalesced_rotation_rebases_without_endpoint_extrapolation(void)
{
    AnchorRemoteMotionState state;
    AnchorRemoteMotionOutput output;
    AnchorRemoteMotionSample sample = sample_at(
        1, 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);

    sample.timestamp_ms = 1000;
    anchor_remote_motion_reset(&state);
    anchor_remote_motion_step(&state, &sample, &output);
    sample.seq = 10;
    sample.timestamp_ms = 1900;
    sample.rot_y = 900;
    sample.angular_velocity.y = 3000.0f;
    CHECK(anchor_remote_motion_step(&state, &sample, &output));
    CHECK(output.rot_y == 900);
    CHECK(near_float(state.angular_velocity_per_frame.y, 0.0f));
    anchor_remote_motion_step(&state, &sample, &output);
    CHECK(output.rot_y == 900);
    return 0;
}

int main(void)
{
    int result = test_constant_motion_stays_in_phase();

    if (result)
        return result;
    result = test_new_velocity_controls_packet_frame();
    if (result)
        return result;
    result = test_stop_is_immediate_and_exact();
    if (result)
        return result;
    result = test_stop_clears_outstanding_horizontal_debt();
    if (result)
        return result;
    result = test_one_axis_wall_stop_clears_only_that_axis();
    if (result)
        return result;
    result = test_residual_is_paid_without_easing();
    if (result)
        return result;
    result = test_retarget_does_not_apply_two_corrections();
    if (result)
        return result;
    result = test_large_negative_residual_cannot_reverse_forward_motion();
    if (result)
        return result;
    result = test_position_reversal_rebounds_retained_debt();
    if (result)
        return result;
    result = test_vertical_acceleration_crosses_apex_once();
    if (result)
        return result;
    result = test_takeoff_uses_native_gravity_before_second_packet();
    if (result)
        return result;
    result = test_ledge_entry_and_first_descent_use_native_gravity();
    if (result)
        return result;
    result = test_fallback_gravity_clamps_at_native_terminal_speed();
    if (result)
        return result;
    result = test_authoritative_terminal_endpoint_stays_terminal();
    if (result)
        return result;
    result = test_learned_gravity_clamps_at_native_terminal_speed();
    if (result)
        return result;
    result = test_sparse_edges_and_angle_wrap_helpers();
    if (result)
        return result;
    result = test_landing_clears_outstanding_vertical_debt();
    if (result)
        return result;
    result = test_airborne_action_edge_keeps_gravity();
    if (result)
        return result;
    result = test_non_airborne_vertical_stop_does_not_fall_back();
    if (result)
        return result;
    result = test_prediction_is_bounded_to_one_packet();
    if (result)
        return result;
    result = test_float_position_is_preserved();
    if (result)
        return result;
    result = test_room_and_teleport_snap_then_hold();
    if (result)
        return result;
    result = test_rotation_uses_shortest_arc_without_spin();
    if (result)
        return result;
    result = test_endpoint_angular_velocity_stays_in_phase_and_stops();
    if (result)
        return result;
    result = test_angular_deceleration_never_overshoots_endpoint();
    if (result)
        return result;
    result = test_rotation_reversal_discards_stale_correction();
    if (result)
        return result;
    result = test_rotation_residual_cannot_reverse_continuing_turn();
    if (result)
        return result;
    result = test_sustained_slow_rotation_is_carried_between_packets();
    if (result)
        return result;
    result = test_quantized_zero_endpoint_keeps_coherent_slow_turn();
    if (result)
        return result;
    result = test_sender_time_preserves_packet_cadence();
    if (result)
        return result;
    result = test_equal_sequence_new_timestamp_is_consumed();
    if (result)
        return result;
    result = test_one_tick_late_packet_keeps_motion_phase();
    if (result)
        return result;
    result = test_one_tick_late_airborne_packet_keeps_velocity_phase();
    if (result)
        return result;
    result = test_one_tick_early_packet_preserves_signed_phase();
    if (result)
        return result;
    result = test_early_rate_change_finishes_tick_at_previous_speed();
    if (result)
        return result;
    result = test_consecutive_early_packets_accumulate_signed_phase();
    if (result)
        return result;
    result = test_alternating_early_and_late_packets_keep_constant_speed();
    if (result)
        return result;
    result = test_early_event_packet_keeps_six_tick_horizon();
    if (result)
        return result;
    result = test_discontinuity_reseeds_nominal_cadence();
    if (result)
        return result;
    result = test_sender_time_detects_paused_receiver();
    if (result)
        return result;
    result = test_coalesced_motion_rebases_without_catch_up_burst();
    if (result)
        return result;
    return test_coalesced_rotation_rebases_without_endpoint_extrapolation();
}
