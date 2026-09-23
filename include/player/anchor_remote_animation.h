#ifndef ANCHOR_REMOTE_ANIMATION_H
#define ANCHOR_REMOTE_ANIMATION_H

#define ANCHOR_REMOTE_ANIMATION_MAX_CORRECTION_STEP 0.5f
#define ANCHOR_REMOTE_ANIMATION_SNAP_THRESHOLD 3.0f

/* One authoritative animation sample. Frame and endpoint_step are expressed
 * in source-clip frames; native_step is already expressed in target-clip
 * frames. The helper maps source phase/rate onto target_frame_count before it
 * advances the receiver clock. */
typedef struct AnchorRemoteAnimationInput
{
    int action;
    int seq;
    int new_sample;
    float source_frame_count;
    float target_frame_count;
    float target_frame;
    int endpoint_step_valid;
    float endpoint_step;
    float native_step;
    int root_phase_lead_frames;
} AnchorRemoteAnimationInput;

/* Pure visual clock state. endpoint_step and native_step are retained in
 * target-clip frames per 30 Hz game tick so ticks without a packet do not
 * depend on repeated input fields. */
typedef struct AnchorRemoteAnimationState
{
    int initialized;
    int action;
    int seq;
    int endpoint_step_valid;
    float frame;
    float frame_count;
    float endpoint_step;
    float native_step;
    float playback_step;
    float correction_debt;
} AnchorRemoteAnimationState;

typedef struct AnchorRemoteAnimationOutput
{
    float frame;
    float playback_step;
    float correction_step;
    float correction_debt;
    int consumed_sample;
    int snapped;
} AnchorRemoteAnimationOutput;

void anchor_remote_animation_reset(AnchorRemoteAnimationState *state);

/* Preserve an ordinary forward loop wrap while rejecting a same-action frame
 * reset whose one-tick delta cannot be a native playback rate. */
int anchor_remote_animation_step_is_continuous(int frame_restarted,
                                               int endpoint_step_valid,
                                               int endpoint_step_100);

/* Advance one 30 Hz receiver tick. A changed nonzero sequence is sufficient
 * to identify a sample; new_sample supports legacy/repeated sequence values.
 * First authority and action changes snap to the phase-projected target.
 * Same-action phase error is repaid monotonically at no more than half a clip
 * frame per tick, unless its shortest-loop distance exceeds three frames. */
void anchor_remote_animation_step(AnchorRemoteAnimationState *state,
                                  const AnchorRemoteAnimationInput *input,
                                  AnchorRemoteAnimationOutput *output);

#endif
