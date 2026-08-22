#include "anchor_remote_animation.h"

#define ANCHOR_REMOTE_ANIMATION_EPSILON 0.0001f
#define ANCHOR_REMOTE_ANIMATION_MAX_CONTINUOUS_STEP_100 400

static float abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static float wrap_frame(float frame, float frame_count)
{
    if (frame_count <= 0.0f)
        return 0.0f;
    while (frame >= frame_count)
        frame -= frame_count;
    while (frame < 0.0f)
        frame += frame_count;
    return frame;
}

static float frame_delta(float target, float current, float frame_count)
{
    float delta = target - current;

    if (frame_count > 0.0f)
    {
        float half = frame_count * 0.5f;

        while (delta > half)
            delta -= frame_count;
        while (delta < -half)
            delta += frame_count;
    }
    return delta;
}

static float source_to_target_scale(const AnchorRemoteAnimationInput *input)
{
    if (input->source_frame_count > 0.0f &&
        input->target_frame_count > 0.0f)
    {
        return input->target_frame_count / input->source_frame_count;
    }
    return 1.0f;
}

static void remap_frame_count(AnchorRemoteAnimationState *state,
                              float target_frame_count)
{
    float scale;

    if (state->frame_count == target_frame_count)
        return;
    if (state->frame_count > 0.0f && target_frame_count > 0.0f)
    {
        scale = target_frame_count / state->frame_count;
        state->frame = wrap_frame(state->frame * scale,
                                  target_frame_count);
        state->endpoint_step *= scale;
        state->native_step *= scale;
        state->playback_step *= scale;
        state->correction_debt *= scale;
    }
    else
    {
        state->frame = 0.0f;
        state->correction_debt = 0.0f;
    }
    state->frame_count = target_frame_count;
}

static float apply_pending_correction(AnchorRemoteAnimationState *state)
{
    float correction = clamp_float(
        state->correction_debt,
        -ANCHOR_REMOTE_ANIMATION_MAX_CORRECTION_STEP,
        ANCHOR_REMOTE_ANIMATION_MAX_CORRECTION_STEP);

    state->correction_debt -= correction;
    if (abs_float(state->correction_debt) <
        ANCHOR_REMOTE_ANIMATION_EPSILON)
    {
        state->correction_debt = 0.0f;
    }
    return correction;
}

void anchor_remote_animation_reset(AnchorRemoteAnimationState *state)
{
    unsigned char *bytes = (unsigned char *)state;
    unsigned int i;

    if (!state)
        return;
    for (i = 0; i < (unsigned int)sizeof(*state); ++i)
        bytes[i] = 0;
}

int anchor_remote_animation_step_is_continuous(int frame_restarted,
                                               int endpoint_step_valid,
                                               int endpoint_step_100)
{
    if (!endpoint_step_valid)
        return 0;
    if (!frame_restarted)
        return 1;

    /* The publisher's endpoint step is measured every game tick and already
     * accounts for a normal forward loop wrap. Keep that cadence across the
     * wrap. A negative or implausibly large delta instead indicates that the
     * same action's phase was explicitly reset. */
    return endpoint_step_100 >= 0 &&
           endpoint_step_100 <=
               ANCHOR_REMOTE_ANIMATION_MAX_CONTINUOUS_STEP_100;
}

void anchor_remote_animation_step(AnchorRemoteAnimationState *state,
                                  const AnchorRemoteAnimationInput *input,
                                  AnchorRemoteAnimationOutput *output)
{
    int action_changed;
    int consumed_sample;
    float rate_scale;
    float mapped_target;
    float projected_target;
    float phase_error;
    float correction_step = 0.0f;
    float previous_playback_step;
    float advance_step;
    int snapped = 0;

    if (!state || !input || !output)
        return;

    action_changed = state->initialized && input->action != state->action;
    consumed_sample = !state->initialized || action_changed ||
                      input->new_sample || input->seq != state->seq;

    remap_frame_count(state, input->target_frame_count);
    rate_scale = source_to_target_scale(input);
    mapped_target = input->target_frame * rate_scale;
    previous_playback_step = state->playback_step;

    if (consumed_sample)
    {
        state->native_step = input->native_step;
        state->endpoint_step_valid = input->endpoint_step_valid != 0;
        state->endpoint_step = input->endpoint_step * rate_scale;
        state->playback_step = state->endpoint_step_valid
                                   ? state->endpoint_step
                                   : state->native_step;
    }

    projected_target = wrap_frame(
        mapped_target + state->playback_step *
                            (float)input->root_phase_lead_frames,
        state->frame_count);

    if (!state->initialized || action_changed)
    {
        state->frame = projected_target;
        state->correction_debt = 0.0f;
        snapped = 1;
    }
    else
    {
        /* A negative root lead means this receiver tick is still before the
         * new sender endpoint. Finish it at the previously carried rate, then
         * retain the new endpoint rate for the following tick. */
        advance_step = consumed_sample && input->root_phase_lead_frames < 0
                           ? previous_playback_step
                           : state->playback_step;
        state->frame = wrap_frame(state->frame + advance_step,
                                  state->frame_count);
        correction_step = apply_pending_correction(state);
        state->frame = wrap_frame(state->frame + correction_step,
                                  state->frame_count);

        if (consumed_sample)
        {
            phase_error = frame_delta(projected_target, state->frame,
                                      state->frame_count);
            if (abs_float(phase_error) >
                ANCHOR_REMOTE_ANIMATION_SNAP_THRESHOLD)
            {
                state->frame = projected_target;
                state->correction_debt = 0.0f;
                snapped = 1;
            }
            else
            {
                state->correction_debt = phase_error;
            }
        }
    }

    if (consumed_sample)
    {
        state->action = input->action;
        state->seq = input->seq;
    }
    state->initialized = 1;

    output->frame = state->frame;
    output->playback_step = state->playback_step;
    output->correction_step = correction_step;
    output->correction_debt = state->correction_debt;
    output->consumed_sample = consumed_sample;
    output->snapped = snapped;
}
