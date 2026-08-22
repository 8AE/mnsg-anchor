#ifndef ANCHOR_REMOTE_MOTION_H
#define ANCHOR_REMOTE_MOTION_H

typedef struct AnchorRemoteMotionVec3
{
    float x;
    float y;
    float z;
} AnchorRemoteMotionVec3;

typedef struct AnchorRemoteMotionSample
{
    int room;
    int seq;
    /* Sender monotonic milliseconds, masked to a positive 31-bit value. */
    int timestamp_ms;
    AnchorRemoteMotionVec3 position;
    /* Post-collision world units per second at the sample endpoint. */
    AnchorRemoteMotionVec3 velocity;
    /* Signed 16-bit angle units per second at the sample endpoint. */
    AnchorRemoteMotionVec3 angular_velocity;
    int action;
    int rot_x;
    int rot_y;
    int rot_z;
} AnchorRemoteMotionSample;

typedef struct AnchorRemoteMotionOutput
{
    AnchorRemoteMotionVec3 position;
    int rot_x;
    int rot_y;
    int rot_z;
    int consumed_sample;
} AnchorRemoteMotionOutput;

/* Per-peer visual-only motion state. The receiver carries the sender's latest
 * post-collision displacement every game tick and pays packet residuals in
 * a few fixed steps. It does not delay the transform by a packet or run a
 * second gameplay/collision simulation. */
typedef struct AnchorRemoteMotionState
{
    int initialized;
    int room;
    int seq;
    int timestamp_ms;
    int frames_since_sample;
    int expected_interval_frames;
    int projection_lead_frames;
    int prediction_frames;
    int correction_frames;
    int rotation_correction_frames;

    AnchorRemoteMotionVec3 position;
    AnchorRemoteMotionVec3 velocity_per_frame;
    AnchorRemoteMotionVec3 correction_per_frame;
    AnchorRemoteMotionVec3 correction_debt;
    float vertical_acceleration_per_frame;
    int use_native_gravity_limit;
    AnchorRemoteMotionVec3 rotation;
    AnchorRemoteMotionVec3 angular_velocity_per_frame;
    AnchorRemoteMotionVec3 rotation_correction_per_frame;

    AnchorRemoteMotionVec3 raw_position;
    AnchorRemoteMotionVec3 raw_velocity_per_frame;
    AnchorRemoteMotionVec3 raw_rotation;
    AnchorRemoteMotionVec3 raw_angular_velocity_per_frame;
    int raw_action;
} AnchorRemoteMotionState;

void anchor_remote_motion_reset(AnchorRemoteMotionState *state);

/* Shared sender/receiver helpers. A sparse edge is a start, stop, or strong
 * sign reversal around a 25-percent hysteresis band derived from the supplied
 * quantization threshold. */
int anchor_remote_motion_axis_has_edge(int previous_velocity,
                                       int current_velocity,
                                       int minimum_velocity);
int anchor_remote_motion_angle_delta_s16(int target, int current);

/* Advance one 30 Hz game tick and consume a new authoritative sample when
 * its sequence or legacy payload changes. Returns nonzero when the transform
 * was deliberately snapped because prediction would be invalid. */
int anchor_remote_motion_step(AnchorRemoteMotionState *state,
                              const AnchorRemoteMotionSample *sample,
                              AnchorRemoteMotionOutput *output);

#endif
