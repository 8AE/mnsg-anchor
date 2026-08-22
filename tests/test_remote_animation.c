#include "anchor_remote_animation.h"

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
    return abs_float(actual - expected) < 0.001f;
}

static float phase_delta(float target, float current, float frame_count)
{
    float delta = target - current;
    float half = frame_count * 0.5f;

    while (delta > half)
        delta -= frame_count;
    while (delta < -half)
        delta += frame_count;
    return delta;
}

static AnchorRemoteAnimationInput sample_at(
    int action, int seq, int new_sample, float frame, float frame_count,
    int endpoint_step_valid, float endpoint_step, float native_step,
    int root_phase_lead_frames)
{
    AnchorRemoteAnimationInput input;

    input.action = action;
    input.seq = seq;
    input.new_sample = new_sample;
    input.source_frame_count = frame_count;
    input.target_frame_count = frame_count;
    input.target_frame = frame;
    input.endpoint_step_valid = endpoint_step_valid;
    input.endpoint_step = endpoint_step;
    input.native_step = native_step;
    input.root_phase_lead_frames = root_phase_lead_frames;
    return input;
}

static int test_nominal_six_tick_rate_stays_one(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 16.0f, 1, 1.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.snapped);
    for (tick = 1; tick < 6; ++tick)
    {
        input.new_sample = 0;
        anchor_remote_animation_step(&state, &input, &output);
        CHECK(near_float(output.frame, (float)tick));
    }
    input.seq = 2;
    input.target_frame = 6.0f;
    /* A changed sequence is sufficient even without the explicit flag. */
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.consumed_sample);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 6.0f));
    CHECK(near_float(output.playback_step, 1.0f));
    return 0;
}

static int test_one_tick_edge_does_not_divide_rate_by_six(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 1.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 1.0f));
    CHECK(near_float(output.playback_step, 1.0f));
    input.new_sample = 0;
    /* Endpoint rate is retained; non-sample fields cannot pause the clock. */
    input.endpoint_step = 0.0f;
    input.native_step = 0.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 2.0f));
    CHECK(near_float(output.playback_step, 1.0f));
    return 0;
}

static int test_repeated_early_edges_remain_monotonic(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);
    float previous = 0.0f;
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    for (tick = 1; tick <= 12; ++tick)
    {
        input.seq++;
        input.target_frame = (float)(tick % 20);
        anchor_remote_animation_step(&state, &input, &output);
        CHECK(near_float(phase_delta(output.frame, previous, 20.0f),
                         1.0f));
        CHECK(!output.snapped);
        previous = output.frame;
    }
    return 0;
}

static int test_short_loop_does_not_alias_endpoint_rate_backward(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 8.0f, 1, 1.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    for (tick = 1; tick < 6; ++tick)
        anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 6.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 6.0f));
    CHECK(near_float(output.playback_step, 1.0f));
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 7.0f));
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 0.0f));
    return 0;
}

static int test_positive_root_lead_projects_target_phase(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    for (tick = 1; tick < 6; ++tick)
        anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 6.0f;
    input.root_phase_lead_frames = 1;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 6.0f));
    CHECK(near_float(output.correction_debt, 1.0f));
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.correction_step, 0.5f));
    CHECK(near_float(output.frame, 7.5f));
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 9.0f));
    CHECK(near_float(output.correction_debt, 0.0f));
    return 0;
}

static int test_small_phase_error_settles_monotonically_and_bounded(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 32.0f, 1, 1.0f, 1.0f, 0);
    float expected[] = {3.5f, 5.0f, 6.5f, 8.0f};
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 1.0f));
    input.seq = 2;
    input.target_frame = 4.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 2.0f));
    CHECK(near_float(output.correction_debt, 2.0f));
    input.new_sample = 0;
    for (tick = 0; tick < 4; ++tick)
    {
        anchor_remote_animation_step(&state, &input, &output);
        CHECK(near_float(output.correction_step, 0.5f));
        CHECK(near_float(output.frame, expected[tick]));
    }
    CHECK(near_float(output.correction_debt, 0.0f));
    return 0;
}

static int test_valid_zero_endpoint_rate_remains_paused(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 5.0f, 20.0f, 1, 0.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.playback_step, 0.0f));
    input.new_sample = 0;
    for (tick = 0; tick < 6; ++tick)
    {
        anchor_remote_animation_step(&state, &input, &output);
        CHECK(near_float(output.frame, 5.0f));
    }
    input.new_sample = 1;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 5.0f));
    CHECK(near_float(output.playback_step, 0.0f));
    return 0;
}

static int test_endpoint_rate_change_tracks_native_stride_without_snap(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    for (tick = 1; tick < 6; ++tick)
        anchor_remote_animation_step(&state, &input, &output);

    input.seq = 2;
    input.target_frame = 5.5f;
    input.endpoint_step = 0.5f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 5.5f));
    CHECK(near_float(output.playback_step, 0.5f));
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 6.0f));

    input.seq = 3;
    input.target_frame = 6.0f;
    input.endpoint_step = 0.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 6.0f));
    CHECK(near_float(output.playback_step, 0.0f));
    return 0;
}

static int test_early_rate_change_finishes_tick_at_previous_stride(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    for (tick = 1; tick <= 4; ++tick)
        anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 4.0f));

    input.seq = 2;
    input.target_frame = 5.5f;
    input.endpoint_step = 0.5f;
    input.root_phase_lead_frames = -1;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 5.0f));
    CHECK(near_float(output.playback_step, 0.5f));
    CHECK(near_float(output.correction_debt, 0.0f));
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 5.5f));
    return 0;
}

static int test_action_change_and_reset_snap_and_clear_debt(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 4.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.correction_debt, 2.0f));

    input.action = 2;
    input.seq = 3;
    input.new_sample = 0;
    input.target_frame = 7.0f;
    input.endpoint_step = 0.5f;
    input.native_step = 0.5f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.snapped);
    CHECK(near_float(output.frame, 7.0f));
    CHECK(near_float(output.correction_debt, 0.0f));
    CHECK(near_float(output.playback_step, 0.5f));

    anchor_remote_animation_reset(&state);
    input.new_sample = 1;
    input.target_frame = 2.0f;
    input.root_phase_lead_frames = 1;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.snapped);
    CHECK(near_float(output.frame, 2.5f));
    return 0;
}

static int test_missing_endpoint_rate_uses_and_retains_native_fallback(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 0, 0.0f, 0.75f, 0);

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.playback_step, 0.75f));
    input.new_sample = 0;
    /* Non-sample rate fields are deliberately ignored. */
    input.native_step = 0.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 0.75f));
    CHECK(near_float(output.playback_step, 0.75f));
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 1.5f));
    input.seq = 2;
    input.target_frame = 2.25f;
    input.native_step = 0.75f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 2.25f));
    CHECK(near_float(output.playback_step, 0.75f));
    return 0;
}

static int test_source_target_mapping_signed_lead_and_wrap(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 9.0f, 10.0f, 1, 1.0f, 2.0f, -1);

    input.target_frame_count = 20.0f;
    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.snapped);
    CHECK(near_float(output.playback_step, 2.0f));
    CHECK(near_float(output.frame, 16.0f));

    input.root_phase_lead_frames = 0;
    input.new_sample = 0;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 18.0f));
    input.seq = 2;
    input.target_frame = 0.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 0.0f));
    return 0;
}

static int test_same_action_snaps_only_above_discontinuity_threshold(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 0.0f, 20.0f, 1, 1.0f, 1.0f, 0);

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 4.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.frame, 1.0f));
    CHECK(near_float(output.correction_debt, 3.0f));

    anchor_remote_animation_reset(&state);
    input.seq = 1;
    input.new_sample = 1;
    input.target_frame = 0.0f;
    anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 4.01f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(output.snapped);
    CHECK(near_float(output.frame, 4.01f));
    CHECK(near_float(output.correction_debt, 0.0f));
    return 0;
}

static int test_negative_correction_is_bounded_across_loop_wrap(void)
{
    AnchorRemoteAnimationState state;
    AnchorRemoteAnimationOutput output;
    AnchorRemoteAnimationInput input = sample_at(
        1, 1, 1, 1.0f, 20.0f, 1, 0.0f, 1.0f, 0);
    float expected[] = {0.5f, 0.0f, 19.5f, 19.0f};
    int tick;

    anchor_remote_animation_reset(&state);
    anchor_remote_animation_step(&state, &input, &output);
    input.seq = 2;
    input.target_frame = 19.0f;
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(!output.snapped);
    CHECK(near_float(output.correction_debt, -2.0f));
    input.new_sample = 0;
    for (tick = 0; tick < 4; ++tick)
    {
        anchor_remote_animation_step(&state, &input, &output);
        CHECK(near_float(output.correction_step, -0.5f));
        CHECK(near_float(output.frame, expected[tick]));
    }
    CHECK(near_float(output.correction_debt, 0.0f));
    anchor_remote_animation_step(&state, &input, &output);
    CHECK(near_float(output.frame, 19.0f));
    return 0;
}

static int test_restart_classification_preserves_natural_loop_rate(void)
{
    CHECK(anchor_remote_animation_step_is_continuous(0, 1, -100));
    CHECK(anchor_remote_animation_step_is_continuous(1, 1, 100));
    CHECK(anchor_remote_animation_step_is_continuous(1, 1, 0));
    CHECK(!anchor_remote_animation_step_is_continuous(1, 1, -100));
    CHECK(!anchor_remote_animation_step_is_continuous(1, 1, 401));
    CHECK(!anchor_remote_animation_step_is_continuous(1, 0, 100));
    return 0;
}

int main(void)
{
    int result;

    result = test_nominal_six_tick_rate_stays_one();
    if (result)
        return result;
    result = test_one_tick_edge_does_not_divide_rate_by_six();
    if (result)
        return result;
    result = test_repeated_early_edges_remain_monotonic();
    if (result)
        return result;
    result = test_short_loop_does_not_alias_endpoint_rate_backward();
    if (result)
        return result;
    result = test_positive_root_lead_projects_target_phase();
    if (result)
        return result;
    result = test_small_phase_error_settles_monotonically_and_bounded();
    if (result)
        return result;
    result = test_valid_zero_endpoint_rate_remains_paused();
    if (result)
        return result;
    result = test_endpoint_rate_change_tracks_native_stride_without_snap();
    if (result)
        return result;
    result = test_early_rate_change_finishes_tick_at_previous_stride();
    if (result)
        return result;
    result = test_action_change_and_reset_snap_and_clear_debt();
    if (result)
        return result;
    result = test_missing_endpoint_rate_uses_and_retains_native_fallback();
    if (result)
        return result;
    result = test_source_target_mapping_signed_lead_and_wrap();
    if (result)
        return result;
    result = test_same_action_snaps_only_above_discontinuity_threshold();
    if (result)
        return result;
    result = test_negative_correction_is_bounded_across_loop_wrap();
    if (result)
        return result;
    result = test_restart_classification_preserves_natural_loop_rate();
    if (result)
        return result;
    return 0;
}
