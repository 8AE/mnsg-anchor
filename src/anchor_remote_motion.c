#include "anchor_remote_motion.h"

#define REMOTE_GAME_TICKS_PER_SECOND 30.0f
#define REMOTE_NOMINAL_PACKET_FRAMES 6
#define REMOTE_MIN_PACKET_FRAMES 1
#define REMOTE_MAX_SAMPLE_FRAMES 30
#define REMOTE_STALE_RESET_FRAMES 30
#define REMOTE_STALE_RESET_MS 1000
#define REMOTE_POSITION_CORRECTION_FRAMES 4
#define REMOTE_ROTATION_CORRECTION_FRAMES 3
#define REMOTE_MIN_PROJECTION_LEAD_FRAMES (-REMOTE_NOMINAL_PACKET_FRAMES)
#define REMOTE_NATIVE_GRAVITY_PER_FRAME (-0.6666667f)
#define REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME (-8.0f)
#define REMOTE_VERTICAL_ACCEL_LIMIT_PER_FRAME 2.0f
#define REMOTE_TELEPORT_DISTANCE_SQ 250000.0f
#define REMOTE_ANGLE_PERIOD 65536.0f
#define REMOTE_ANGLE_HALF_PERIOD 32768.0f
#define REMOTE_ANGULAR_EDGE_PER_FRAME 32.0f
#define REMOTE_POSITION_CORRECTION_RATIO 0.5f
#define REMOTE_POSITION_CORRECTION_MIN 0.25f
#define REMOTE_ROTATION_CORRECTION_MAX 4096.0f
#define REMOTE_CORRECTION_EPSILON 0.0001f
#define REMOTE_AIRBORNE_ACTION_FIRST 0x17
#define REMOTE_AIRBORNE_ACTION_LAST 0x1a

static float wrap_angle(float angle)
{
    while (angle >= REMOTE_ANGLE_HALF_PERIOD)
        angle -= REMOTE_ANGLE_PERIOD;
    while (angle < -REMOTE_ANGLE_HALF_PERIOD)
        angle += REMOTE_ANGLE_PERIOD;
    return angle;
}

static float angle_delta(float target, float current)
{
    return wrap_angle(target - current);
}

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static float abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

static float min_float(float first, float second)
{
    return first < second ? first : second;
}

static int action_is_airborne(int action)
{
    return action >= REMOTE_AIRBORNE_ACTION_FIRST &&
           action <= REMOTE_AIRBORNE_ACTION_LAST;
}

int anchor_remote_motion_axis_has_edge(int previous_velocity,
                                       int current_velocity,
                                       int minimum_velocity)
{
    int previous_moving;
    int current_moving;
    int enter_velocity;
    int exit_velocity;

    if (minimum_velocity < 1)
        minimum_velocity = 1;
    enter_velocity = minimum_velocity + minimum_velocity / 4;
    exit_velocity = minimum_velocity - minimum_velocity / 4;
    if (enter_velocity < 1)
        enter_velocity = 1;
    if (exit_velocity < 1)
        exit_velocity = 1;
    previous_moving = previous_velocity >= exit_velocity ||
                      previous_velocity <= -exit_velocity;
    current_moving = current_velocity >=
                         (previous_moving ? exit_velocity : enter_velocity) ||
                     current_velocity <=
                         -(previous_moving ? exit_velocity : enter_velocity);
    if (previous_moving != current_moving)
        return 1;
    return previous_moving && current_moving &&
           ((previous_velocity < 0) != (current_velocity < 0));
}

int anchor_remote_motion_angle_delta_s16(int target, int current)
{
    int delta = target - current;

    while (delta > 32767)
        delta -= 65536;
    while (delta < -32768)
        delta += 65536;
    return delta;
}

static int angle_to_s16(float angle)
{
    int value = (int)wrap_angle(angle);

    if (value > 32767)
        value = 32767;
    if (value < -32768)
        value = -32768;
    return value;
}

static int clamp_sample_frames(int frames)
{
    if (frames < REMOTE_MIN_PACKET_FRAMES)
        return REMOTE_MIN_PACKET_FRAMES;
    if (frames > REMOTE_MAX_SAMPLE_FRAMES)
        return REMOTE_MAX_SAMPLE_FRAMES;
    return frames;
}

static int forward_sequence_delta(int previous, int current)
{
    if (previous <= 0 || current <= 0)
        return 0;
    if (current > previous)
        return current - previous;
    if (previous > 0x70000000 && current < 0x10000000)
        return (int)((0x80000000u - (unsigned int)previous) +
                     (unsigned int)current);
    return 0;
}

static int timestamp_delta_ms(int previous, int current)
{
    if (previous <= 0 || current <= 0)
        return 0;
    if (current >= previous)
        return current - previous;
    if (previous > 0x70000000 && current < 0x10000000)
        return (int)((0x80000000u - (unsigned int)previous) +
                     (unsigned int)current);
    return 0;
}

/* The Python bridge exposes only the newest coalesced state. Sender time and
 * sequence deltas keep a receiver hitch or skipped state from changing the
 * expected packet cadence or the displacement used by reset guards. */
static void derive_sample_timing(const AnchorRemoteMotionState *state,
                                 const AnchorRemoteMotionSample *sample,
                                 int receiver_elapsed_frames,
                                 int *sample_elapsed_frames)
{
    int seq_delta = forward_sequence_delta(state->seq, sample->seq);
    int dt_ms = timestamp_delta_ms(state->timestamp_ms,
                                   sample->timestamp_ms);

    *sample_elapsed_frames = clamp_sample_frames(receiver_elapsed_frames);
    if (dt_ms > 0 && dt_ms <= REMOTE_STALE_RESET_MS)
    {
        int total_frames = (dt_ms * 30 + 500) / 1000;

        total_frames = clamp_sample_frames(total_frames);
        *sample_elapsed_frames = total_frames;
        return;
    }
    if (dt_ms > REMOTE_STALE_RESET_MS)
    {
        *sample_elapsed_frames = REMOTE_MAX_SAMPLE_FRAMES;
        return;
    }
    if (seq_delta > 1)
    {
        int inferred_frames;

        if (seq_delta > REMOTE_MAX_SAMPLE_FRAMES)
            seq_delta = REMOTE_MAX_SAMPLE_FRAMES;
        inferred_frames = state->expected_interval_frames * seq_delta;
        *sample_elapsed_frames = clamp_sample_frames(inferred_frames);
    }
}

static int sequence_went_back(int previous, int current)
{
    if (previous <= 0 || current <= 0 || current >= previous)
        return 0;

    return !(previous > 0x70000000 && current < 0x10000000);
}

static int legacy_sample_changed(const AnchorRemoteMotionState *state,
                                 const AnchorRemoteMotionSample *sample)
{
    return state->raw_position.x != sample->position.x ||
           state->raw_position.y != sample->position.y ||
           state->raw_position.z != sample->position.z ||
           state->raw_velocity_per_frame.x !=
               sample->velocity.x / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_velocity_per_frame.y !=
               sample->velocity.y / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_velocity_per_frame.z !=
               sample->velocity.z / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_angular_velocity_per_frame.x !=
               sample->angular_velocity.x / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_angular_velocity_per_frame.y !=
               sample->angular_velocity.y / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_angular_velocity_per_frame.z !=
               sample->angular_velocity.z / REMOTE_GAME_TICKS_PER_SECOND ||
           state->raw_action != sample->action ||
           (int)state->raw_rotation.x != sample->rot_x ||
           (int)state->raw_rotation.y != sample->rot_y ||
           (int)state->raw_rotation.z != sample->rot_z;
}

static int sample_is_new(const AnchorRemoteMotionState *state,
                         const AnchorRemoteMotionSample *sample)
{
    if (!state->initialized || state->room != sample->room)
        return 1;
    if (sample->seq != 0)
    {
        if (sample->seq != state->seq)
            return 1;
        return sample->timestamp_ms > 0 &&
               sample->timestamp_ms != state->timestamp_ms;
    }
    if (sample->timestamp_ms > 0 &&
        sample->timestamp_ms != state->timestamp_ms)
        return 1;
    return legacy_sample_changed(state, sample);
}

static void write_output(const AnchorRemoteMotionState *state,
                         AnchorRemoteMotionOutput *output)
{
    output->position = state->position;
    output->rot_x = angle_to_s16(state->rotation.x);
    output->rot_y = angle_to_s16(state->rotation.y);
    output->rot_z = angle_to_s16(state->rotation.z);
    output->consumed_sample = 0;
}

static int prediction_limit(const AnchorRemoteMotionState *state)
{
    return state->expected_interval_frames;
}

static float bounded_position_correction(float error,
                                         float velocity,
                                         int correction_frames);

static int position_correction_pending(
    const AnchorRemoteMotionState *state)
{
    return abs_float(state->correction_debt.x) >
               REMOTE_CORRECTION_EPSILON ||
           abs_float(state->correction_debt.y) >
               REMOTE_CORRECTION_EPSILON ||
           abs_float(state->correction_debt.z) >
               REMOTE_CORRECTION_EPSILON;
}

static void clear_position_correction(AnchorRemoteMotionState *state)
{
    state->correction_per_frame.x = 0.0f;
    state->correction_per_frame.y = 0.0f;
    state->correction_per_frame.z = 0.0f;
    state->correction_debt.x = 0.0f;
    state->correction_debt.y = 0.0f;
    state->correction_debt.z = 0.0f;
    state->correction_frames = 0;
}

static void schedule_position_correction(
    AnchorRemoteMotionState *state,
    int correction_frames)
{
    if (!position_correction_pending(state))
    {
        clear_position_correction(state);
        return;
    }
    if (correction_frames < 1)
        correction_frames = 1;
    state->correction_frames = correction_frames;
    state->correction_per_frame.x = bounded_position_correction(
        state->correction_debt.x, state->velocity_per_frame.x,
        correction_frames);
    state->correction_per_frame.y = bounded_position_correction(
        state->correction_debt.y, state->velocity_per_frame.y,
        correction_frames);
    state->correction_per_frame.z = bounded_position_correction(
        state->correction_debt.z, state->velocity_per_frame.z,
        correction_frames);
}

static void apply_pending_corrections(AnchorRemoteMotionState *state,
                                      int advance_position)
{
    if (advance_position && state->correction_frames > 0)
    {
        state->position.x += state->correction_per_frame.x;
        state->position.y += state->correction_per_frame.y;
        state->position.z += state->correction_per_frame.z;
        state->correction_debt.x -= state->correction_per_frame.x;
        state->correction_debt.y -= state->correction_per_frame.y;
        state->correction_debt.z -= state->correction_per_frame.z;
        state->correction_frames--;
        if (abs_float(state->correction_debt.x) <=
            REMOTE_CORRECTION_EPSILON)
            state->correction_debt.x = 0.0f;
        if (abs_float(state->correction_debt.y) <=
            REMOTE_CORRECTION_EPSILON)
            state->correction_debt.y = 0.0f;
        if (abs_float(state->correction_debt.z) <=
            REMOTE_CORRECTION_EPSILON)
            state->correction_debt.z = 0.0f;
        if (!position_correction_pending(state))
            clear_position_correction(state);
        else if (state->correction_frames == 0)
            schedule_position_correction(
                state, REMOTE_POSITION_CORRECTION_FRAMES);
    }
    if (state->rotation_correction_frames > 0)
    {
        state->rotation.x = wrap_angle(
            state->rotation.x + state->rotation_correction_per_frame.x);
        state->rotation.y = wrap_angle(
            state->rotation.y + state->rotation_correction_per_frame.y);
        state->rotation.z = wrap_angle(
            state->rotation.z + state->rotation_correction_per_frame.z);
        state->rotation_correction_frames--;
    }
}

/* Carry the latest resolved displacement, advancing Y by the bounded
 * endpoint-derived acceleration first. Prediction is limited to one expected
 * packet interval; scheduled correction may still finish after it holds. */
static void advance_prediction(AnchorRemoteMotionState *state)
{
    int advanced = 0;

    if (state->prediction_frames < prediction_limit(state))
    {
        state->velocity_per_frame.y +=
            state->vertical_acceleration_per_frame;
        if (state->use_native_gravity_limit &&
            state->velocity_per_frame.y <
                REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME)
        {
            state->velocity_per_frame.y =
                REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME;
        }
        state->position.x += state->velocity_per_frame.x;
        state->position.y += state->velocity_per_frame.y;
        state->position.z += state->velocity_per_frame.z;
        state->rotation.x = wrap_angle(
            state->rotation.x + state->angular_velocity_per_frame.x);
        state->rotation.y = wrap_angle(
            state->rotation.y + state->angular_velocity_per_frame.y);
        state->rotation.z = wrap_angle(
            state->rotation.z + state->angular_velocity_per_frame.z);
        state->prediction_frames++;
        advanced = 1;
    }
    /* Once prediction has reached its packet horizon, hold both root motion
     * and opposing residual debt. Continuing correction by itself could make
     * a stale remote walk backward even though its last endpoint was forward. */
    apply_pending_corrections(state, advanced);
}

static void store_raw_sample(AnchorRemoteMotionState *state,
                             const AnchorRemoteMotionSample *sample)
{
    state->raw_position = sample->position;
    state->raw_velocity_per_frame.x =
        sample->velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_velocity_per_frame.y =
        sample->velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_velocity_per_frame.z =
        sample->velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_rotation.x = wrap_angle((float)sample->rot_x);
    state->raw_rotation.y = wrap_angle((float)sample->rot_y);
    state->raw_rotation.z = wrap_angle((float)sample->rot_z);
    state->raw_angular_velocity_per_frame.x =
        sample->angular_velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_angular_velocity_per_frame.y =
        sample->angular_velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_angular_velocity_per_frame.z =
        sample->angular_velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
    state->raw_action = sample->action;
    state->room = sample->room;
    state->seq = sample->seq;
    state->timestamp_ms = sample->timestamp_ms;
}

static void snap_to_sample(AnchorRemoteMotionState *state,
                           const AnchorRemoteMotionSample *sample,
                           int keep_sample_velocity,
                           int keep_sample_angular_velocity)
{
    state->position = sample->position;
    if (keep_sample_velocity)
    {
        state->velocity_per_frame.x =
            sample->velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
        state->velocity_per_frame.y =
            sample->velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
        state->velocity_per_frame.z =
            sample->velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
    }
    else
    {
        state->velocity_per_frame.x = 0.0f;
        state->velocity_per_frame.y = 0.0f;
        state->velocity_per_frame.z = 0.0f;
    }
    if (keep_sample_angular_velocity)
    {
        state->angular_velocity_per_frame.x =
            sample->angular_velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
        state->angular_velocity_per_frame.y =
            sample->angular_velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
        state->angular_velocity_per_frame.z =
            sample->angular_velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
        if (abs_float(state->angular_velocity_per_frame.x) <
            REMOTE_ANGULAR_EDGE_PER_FRAME)
            state->angular_velocity_per_frame.x = 0.0f;
        if (abs_float(state->angular_velocity_per_frame.y) <
            REMOTE_ANGULAR_EDGE_PER_FRAME)
            state->angular_velocity_per_frame.y = 0.0f;
        if (abs_float(state->angular_velocity_per_frame.z) <
            REMOTE_ANGULAR_EDGE_PER_FRAME)
            state->angular_velocity_per_frame.z = 0.0f;
    }
    else
    {
        state->angular_velocity_per_frame.x = 0.0f;
        state->angular_velocity_per_frame.y = 0.0f;
        state->angular_velocity_per_frame.z = 0.0f;
    }
    state->rotation.x = wrap_angle((float)sample->rot_x);
    state->rotation.y = wrap_angle((float)sample->rot_y);
    state->rotation.z = wrap_angle((float)sample->rot_z);
    state->vertical_acceleration_per_frame =
        keep_sample_velocity && action_is_airborne(sample->action) &&
                state->velocity_per_frame.y != 0.0f &&
                state->velocity_per_frame.y >
                    REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME
            ? REMOTE_NATIVE_GRAVITY_PER_FRAME
            : 0.0f;
    state->use_native_gravity_limit =
        state->vertical_acceleration_per_frame ==
        REMOTE_NATIVE_GRAVITY_PER_FRAME;
    clear_position_correction(state);
    state->rotation_correction_per_frame.x = 0.0f;
    state->rotation_correction_per_frame.y = 0.0f;
    state->rotation_correction_per_frame.z = 0.0f;
    state->frames_since_sample = 0;
    state->projection_lead_frames = 0;
    state->prediction_frames = 0;
    state->rotation_correction_frames = 0;
    state->expected_interval_frames = REMOTE_NOMINAL_PACKET_FRAMES;
    state->initialized = 1;
    store_raw_sample(state, sample);
}

static int sample_is_teleport(const AnchorRemoteMotionState *state,
                              const AnchorRemoteMotionSample *sample,
                              int elapsed_frames)
{
    float frames = (float)clamp_sample_frames(elapsed_frames);
    float expected_x = state->raw_position.x +
                       state->raw_velocity_per_frame.x * frames;
    float expected_y = state->raw_position.y +
                       state->raw_velocity_per_frame.y * frames;
    float expected_z = state->raw_position.z +
                       state->raw_velocity_per_frame.z * frames;
    float dx = sample->position.x - expected_x;
    float dy = sample->position.y - expected_y;
    float dz = sample->position.z - expected_z;

    return dx * dx + dy * dy + dz * dz > REMOTE_TELEPORT_DISTANCE_SQ;
}

/* Bound rotation prediction by both the endpoint derivative and the
 * shortest-arc packet average. Native model yaw can settle with a noisy final
 * one-frame delta; carrying that delta for a full sparse interval is what made
 * the remote alternate front/back while the source turned monotonically. */
static float derive_angular_velocity(float previous_rotation,
                                     float current_rotation,
                                     float previous_velocity,
                                     float endpoint_velocity,
                                     int elapsed_frames)
{
    float average_velocity =
        angle_delta(current_rotation, previous_rotation) /
        (float)clamp_sample_frames(elapsed_frames);
    float magnitude;

    if (abs_float(endpoint_velocity) < REMOTE_ANGULAR_EDGE_PER_FRAME)
    {
        /* A high-rate turn settling below the deadzone is a stop. When both
         * endpoints are already slow, retain a coherent packet-average turn
         * instead of producing a four-tick turn/two-tick hold staircase. */
        if (abs_float(previous_velocity) >= REMOTE_ANGULAR_EDGE_PER_FRAME ||
            average_velocity == 0.0f)
            return 0.0f;
        if (endpoint_velocity == 0.0f)
        {
            /* Signed-angle endpoints are integer-quantized. A coherent slow
             * turn commonly reports alternating zero/nonzero one-tick
             * derivatives; the packet-average still proves that it moved. */
            if (previous_velocity == 0.0f ||
                ((average_velocity < 0.0f) !=
                 (previous_velocity < 0.0f)))
                return 0.0f;
            magnitude = min_float(abs_float(previous_velocity),
                                  abs_float(average_velocity));
            return average_velocity < 0.0f ? -magnitude : magnitude;
        }
        if ((average_velocity < 0.0f) != (endpoint_velocity < 0.0f))
            return 0.0f;
        magnitude = min_float(abs_float(endpoint_velocity),
                              abs_float(average_velocity));
        return endpoint_velocity < 0.0f ? -magnitude : magnitude;
    }
    if (abs_float(previous_velocity) < REMOTE_ANGULAR_EDGE_PER_FRAME)
        return endpoint_velocity;
    if ((endpoint_velocity < 0.0f) != (previous_velocity < 0.0f))
    {
        magnitude = min_float(abs_float(endpoint_velocity),
                              abs_float(previous_velocity));
        return endpoint_velocity < 0.0f ? -magnitude : magnitude;
    }
    if (average_velocity == 0.0f ||
        ((average_velocity < 0.0f) != (endpoint_velocity < 0.0f)))
        return 0.0f;
    magnitude = min_float(abs_float(endpoint_velocity),
                          abs_float(average_velocity));
    return endpoint_velocity < 0.0f ? -magnitude : magnitude;
}

static float bounded_position_correction(float error,
                                         float velocity,
                                         int correction_frames)
{
    float correction = error / (float)correction_frames;
    float limit = abs_float(velocity) *
                  REMOTE_POSITION_CORRECTION_RATIO;

    if (limit < REMOTE_POSITION_CORRECTION_MIN)
        limit = REMOTE_POSITION_CORRECTION_MIN;
    correction = clamp_float(correction, -limit, limit);
    /* Never let authority repayment reverse an axis whose endpoint continues
     * in the same direction. At worst it slows that axis to half speed while
     * the authoritative trajectory catches up. */
    if (velocity > 0.0f &&
        correction < -velocity * REMOTE_POSITION_CORRECTION_RATIO)
        correction = -velocity * REMOTE_POSITION_CORRECTION_RATIO;
    if (velocity < 0.0f &&
        correction > -velocity * REMOTE_POSITION_CORRECTION_RATIO)
        correction = -velocity * REMOTE_POSITION_CORRECTION_RATIO;
    return correction;
}

static float bounded_rotation_correction(float error,
                                         float velocity,
                                         int correction_frames)
{
    float correction = error / (float)correction_frames;
    float limit = abs_float(velocity);

    if (limit > 0.0f)
    {
        /* Opposing debt may hold a continuing turn for a tick, but it cannot
         * reverse it; same-direction debt can at most double its rate. */
        return clamp_float(correction, -limit, limit);
    }
    return clamp_float(correction,
                       -REMOTE_ROTATION_CORRECTION_MAX,
                       REMOTE_ROTATION_CORRECTION_MAX);
}

static void begin_prediction_correction(
    AnchorRemoteMotionState *state,
    const AnchorRemoteMotionSample *sample,
    int sample_elapsed_frames,
    int receiver_lag_frames)
{
    AnchorRemoteMotionVec3 new_velocity;
    AnchorRemoteMotionVec3 new_angular_velocity;
    AnchorRemoteMotionVec3 projected_position;
    AnchorRemoteMotionVec3 projected_rotation;
    AnchorRemoteMotionVec3 position_error;
    AnchorRemoteMotionVec3 rotation_error;
    float derived_vertical_acceleration;
    float projected_vertical_velocity;
    int action_changed;
    int airborne_action_continues;
    int entered_airborne;
    int position_x_stop;
    int position_z_stop;
    int vertical_stop;
    int vertical_discontinuity;
    int projection_index;
    int correction_frames = REMOTE_POSITION_CORRECTION_FRAMES;
    int rotation_frames = REMOTE_ROTATION_CORRECTION_FRAMES;

    if (correction_frames > state->expected_interval_frames)
        correction_frames = state->expected_interval_frames;
    if (rotation_frames > state->expected_interval_frames)
        rotation_frames = state->expected_interval_frames;

    new_velocity.x =
        sample->velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
    new_velocity.y =
        sample->velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
    new_velocity.z =
        sample->velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
    new_angular_velocity.x = derive_angular_velocity(
        state->raw_rotation.x, (float)sample->rot_x,
        state->raw_angular_velocity_per_frame.x,
        sample->angular_velocity.x / REMOTE_GAME_TICKS_PER_SECOND,
        sample_elapsed_frames);
    new_angular_velocity.y = derive_angular_velocity(
        state->raw_rotation.y, (float)sample->rot_y,
        state->raw_angular_velocity_per_frame.y,
        sample->angular_velocity.y / REMOTE_GAME_TICKS_PER_SECOND,
        sample_elapsed_frames);
    new_angular_velocity.z = derive_angular_velocity(
        state->raw_rotation.z, (float)sample->rot_z,
        state->raw_angular_velocity_per_frame.z,
        sample->angular_velocity.z / REMOTE_GAME_TICKS_PER_SECOND,
        sample_elapsed_frames);
    position_x_stop = new_velocity.x == 0.0f &&
                      state->raw_velocity_per_frame.x != 0.0f;
    position_z_stop = new_velocity.z == 0.0f &&
                      state->raw_velocity_per_frame.z != 0.0f;
    /* Action edges are already delivered immediately. Pin their vertical root
     * to the same authority as the new animation. A negative-to-zero endpoint
     * velocity is a collision-clamped landing even if the action byte has not
     * changed yet, so it receives the same treatment. */
    action_changed = sample->action != state->raw_action;
    airborne_action_continues =
        action_changed && action_is_airborne(sample->action) &&
        action_is_airborne(state->raw_action);
    entered_airborne =
        action_changed && action_is_airborne(sample->action) &&
        !action_is_airborne(state->raw_action);
    vertical_stop =
        new_velocity.y == 0.0f &&
        state->raw_velocity_per_frame.y != 0.0f &&
        (state->raw_velocity_per_frame.y < 0.0f ||
         !action_is_airborne(sample->action));
    vertical_discontinuity = action_changed || vertical_stop;
    derived_vertical_acceleration = clamp_float(
        (new_velocity.y - state->raw_velocity_per_frame.y) /
            (float)clamp_sample_frames(sample_elapsed_frames),
        -REMOTE_VERTICAL_ACCEL_LIMIT_PER_FRAME,
        REMOTE_VERTICAL_ACCEL_LIMIT_PER_FRAME);
    if (airborne_action_continues)
    {
        /* Gravity persists through 0x17 -> 0x18 -> 0x19 -> 0x1a. Keep a
         * learned value across the discrete pose edge; if this is the first
         * usable pair, initialize it from the two authoritative endpoints. */
        if (state->vertical_acceleration_per_frame >= 0.0f)
        {
            state->vertical_acceleration_per_frame =
                derived_vertical_acceleration;
            state->use_native_gravity_limit = 0;
        }
    }
    else if (vertical_discontinuity)
    {
        /* Work +0x48 is initialized from 0xBF2AAAAA, or -2/3 per game
         * tick. This also covers a zero-velocity ledge entry. Subsequent
         * endpoint samples replace it with observed acceleration. */
        state->vertical_acceleration_per_frame =
            entered_airborne
                ? REMOTE_NATIVE_GRAVITY_PER_FRAME
                : 0.0f;
        state->use_native_gravity_limit = entered_airborne;
    }
    else
    {
        state->vertical_acceleration_per_frame =
            derived_vertical_acceleration;
    }
    state->use_native_gravity_limit =
        action_is_airborne(sample->action) &&
        state->vertical_acceleration_per_frame < 0.0f &&
        new_velocity.y >= REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME;
    if (action_is_airborne(sample->action) &&
        new_velocity.y == REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME &&
        state->vertical_acceleration_per_frame < 0.0f)
    {
        /* An exact -8 endpoint is the native +0xA4 terminal clamp. Do not
         * turn the packet-pair average that approached it into continued
         * downward acceleration during the next interval. */
        state->vertical_acceleration_per_frame = 0.0f;
        state->use_native_gravity_limit = 0;
    }

    if (receiver_lag_frames < REMOTE_MIN_PROJECTION_LEAD_FRAMES)
        receiver_lag_frames = REMOTE_MIN_PROJECTION_LEAD_FRAMES;
    if (receiver_lag_frames > state->expected_interval_frames)
        receiver_lag_frames = state->expected_interval_frames;
    state->projection_lead_frames = receiver_lag_frames;
    projected_position = sample->position;
    projected_rotation.x = wrap_angle((float)sample->rot_x);
    projected_rotation.y = wrap_angle((float)sample->rot_y);
    projected_rotation.z = wrap_angle((float)sample->rot_z);
    projected_vertical_velocity = new_velocity.y;
    for (projection_index = 0;
         projection_index < receiver_lag_frames;
         ++projection_index)
    {
        projected_vertical_velocity +=
            state->vertical_acceleration_per_frame;
        if (state->use_native_gravity_limit &&
            projected_vertical_velocity <
                REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME)
        {
            projected_vertical_velocity =
                REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME;
        }
        projected_position.x += new_velocity.x;
        projected_position.y += projected_vertical_velocity;
        projected_position.z += new_velocity.z;
        projected_rotation.x = wrap_angle(
            projected_rotation.x + new_angular_velocity.x);
        projected_rotation.y = wrap_angle(
            projected_rotation.y + new_angular_velocity.y);
        projected_rotation.z = wrap_angle(
            projected_rotation.z + new_angular_velocity.z);
    }
    for (projection_index = 0;
         projection_index > receiver_lag_frames;
         --projection_index)
    {
        /* Invert the native semi-implicit vertical step: P(n) = P(n-1) +
         * V(n), then V(n-1) = V(n) - A. A one-tick-early packet is retained
         * as signed phase instead of becoming a visible catch-up pulse. */
        projected_position.x -= new_velocity.x;
        projected_position.y -= projected_vertical_velocity;
        projected_position.z -= new_velocity.z;
        projected_vertical_velocity -=
            state->vertical_acceleration_per_frame;
        projected_rotation.x = wrap_angle(
            projected_rotation.x - new_angular_velocity.x);
        projected_rotation.y = wrap_angle(
            projected_rotation.y - new_angular_velocity.y);
        projected_rotation.z = wrap_angle(
            projected_rotation.z - new_angular_velocity.z);
    }

    if (position_x_stop)
        state->position.x = projected_position.x;
    if (position_z_stop)
        state->position.z = projected_position.z;
    if (vertical_discontinuity)
        state->position.y = projected_position.y;
    position_error.x = projected_position.x - state->position.x;
    position_error.y = projected_position.y - state->position.y;
    position_error.z = projected_position.z - state->position.z;
    state->correction_debt = position_error;

    rotation_error.x = angle_delta(projected_rotation.x,
                                   state->rotation.x);
    rotation_error.y = angle_delta(projected_rotation.y,
                                   state->rotation.y);
    rotation_error.z = angle_delta(projected_rotation.z,
                                   state->rotation.z);
    state->rotation_correction_per_frame.x = bounded_rotation_correction(
        rotation_error.x, new_angular_velocity.x, rotation_frames);
    state->rotation_correction_per_frame.y = bounded_rotation_correction(
        rotation_error.y, new_angular_velocity.y, rotation_frames);
    state->rotation_correction_per_frame.z = bounded_rotation_correction(
        rotation_error.z, new_angular_velocity.z, rotation_frames);
    /* Pay the first residual step on the packet tick. The remaining two
     * equal shortest-arc steps cannot cross the moving projected target. */
    state->rotation.x = wrap_angle(
        state->rotation.x + state->rotation_correction_per_frame.x);
    state->rotation.y = wrap_angle(
        state->rotation.y + state->rotation_correction_per_frame.y);
    state->rotation.z = wrap_angle(
        state->rotation.z + state->rotation_correction_per_frame.z);
    state->rotation_correction_frames = rotation_frames - 1;
    state->velocity_per_frame = new_velocity;
    state->velocity_per_frame.y = projected_vertical_velocity;
    state->angular_velocity_per_frame = new_angular_velocity;
    schedule_position_correction(state, correction_frames);
    state->frames_since_sample = 0;
    state->prediction_frames = 0;
    store_raw_sample(state, sample);
}

void anchor_remote_motion_reset(AnchorRemoteMotionState *state)
{
    unsigned char *bytes = (unsigned char *)state;
    unsigned int i;

    if (!state)
        return;
    for (i = 0; i < (unsigned int)sizeof(*state); ++i)
        bytes[i] = 0;
    state->expected_interval_frames = REMOTE_NOMINAL_PACKET_FRAMES;
}

int anchor_remote_motion_step(AnchorRemoteMotionState *state,
                              const AnchorRemoteMotionSample *sample,
                              AnchorRemoteMotionOutput *output)
{
    int was_initialized;
    int is_new;
    int elapsed_frames;
    int sample_elapsed_frames;
    int sender_elapsed_ms;
    int receiver_lag_frames;
    int seq_delta;
    int should_snap = 0;
    int keep_velocity_on_snap = 0;

    if (!state || !sample || !output)
        return 0;
    was_initialized = state->initialized;
    is_new = sample_is_new(state, sample);
    if (state->initialized)
        state->frames_since_sample++;
    if (!is_new)
    {
        advance_prediction(state);
        write_output(state, output);
        return 0;
    }

    elapsed_frames = state->frames_since_sample;
    seq_delta = forward_sequence_delta(state->seq, sample->seq);
    sender_elapsed_ms = timestamp_delta_ms(state->timestamp_ms,
                                           sample->timestamp_ms);
    derive_sample_timing(state, sample, elapsed_frames,
                         &sample_elapsed_frames);
    /* Arrival phase is cumulative. Once a sample is observed one receiver
     * tick late, later 6/6 intervals remain one tick ahead of their raw
     * endpoints until an early interval pays that lead back. Recomputing only
     * the current interval would create a slowdown pulse on the next packet. */
    receiver_lag_frames = state->projection_lead_frames +
                          elapsed_frames - sample_elapsed_frames;
    if (!state->initialized || state->room != sample->room ||
        sequence_went_back(state->seq, sample->seq) ||
        elapsed_frames > REMOTE_STALE_RESET_FRAMES ||
        sender_elapsed_ms > REMOTE_STALE_RESET_MS)
    {
        should_snap = 1;
    }
    else if (sample_is_teleport(state, sample, sample_elapsed_frames))
    {
        should_snap = 1;
    }
    else if (seq_delta > 1)
    {
        /* Only the newest state survives Python-side coalescing. Catching up
         * several missing packet intervals in a short correction window creates
         * a much larger burst than one honest rebase. Keep the new endpoint
         * velocity so ordinary prediction resumes immediately afterward. */
        should_snap = 1;
        keep_velocity_on_snap = 1;
    }

    if (should_snap)
    {
        /* The first sample may carry useful endpoint velocity. Room changes,
         * teleports, stale gaps, and sequence resets must hold at the new
         * authority until a coherent follow-up sample arrives. */
        snap_to_sample(state, sample,
                       !was_initialized || keep_velocity_on_snap,
                       !was_initialized);
    }
    else
    {
        /* Follow the native command-before-position order by installing the
         * newest resolved endpoint displacement for this game tick, then
         * paying one already-scheduled position correction step. New position
         * debt starts next frame; rotation discards stale debt and starts its
         * new three-step shortest-arc correction immediately. */
        state->rotation_correction_frames = 0;
        state->rotation_correction_per_frame.x = 0.0f;
        state->rotation_correction_per_frame.y = 0.0f;
        state->rotation_correction_per_frame.z = 0.0f;
        if (receiver_lag_frames < 0)
        {
            /* This receiver tick precedes the sender endpoint. Advance it at
             * the previously carried rate, then install the back-projected
             * endpoint rate in begin_prediction_correction for the next tick. */
            state->velocity_per_frame.y +=
                state->vertical_acceleration_per_frame;
            if (state->use_native_gravity_limit &&
                state->velocity_per_frame.y <
                    REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME)
            {
                state->velocity_per_frame.y =
                    REMOTE_NATIVE_TERMINAL_FALL_PER_FRAME;
            }
        }
        else
        {
            state->velocity_per_frame.x =
                sample->velocity.x / REMOTE_GAME_TICKS_PER_SECOND;
            state->velocity_per_frame.y =
                sample->velocity.y / REMOTE_GAME_TICKS_PER_SECOND;
            state->velocity_per_frame.z =
                sample->velocity.z / REMOTE_GAME_TICKS_PER_SECOND;
            state->angular_velocity_per_frame.x = derive_angular_velocity(
                state->raw_rotation.x, (float)sample->rot_x,
                state->raw_angular_velocity_per_frame.x,
                sample->angular_velocity.x / REMOTE_GAME_TICKS_PER_SECOND,
                sample_elapsed_frames);
            state->angular_velocity_per_frame.y = derive_angular_velocity(
                state->raw_rotation.y, (float)sample->rot_y,
                state->raw_angular_velocity_per_frame.y,
                sample->angular_velocity.y / REMOTE_GAME_TICKS_PER_SECOND,
                sample_elapsed_frames);
            state->angular_velocity_per_frame.z = derive_angular_velocity(
                state->raw_rotation.z, (float)sample->rot_z,
                state->raw_angular_velocity_per_frame.z,
                sample->angular_velocity.z / REMOTE_GAME_TICKS_PER_SECOND,
                sample_elapsed_frames);
            /* Retained authority debt was bounded against the previous
             * endpoint rate. Rebound it before this packet tick so a sharp
             * slowdown or reversal cannot let that stale step overpower the
             * newly authoritative direction. */
            if (state->correction_frames > 0)
                schedule_position_correction(
                    state, state->correction_frames);
        }
        state->position.x += state->velocity_per_frame.x;
        state->position.y += state->velocity_per_frame.y;
        state->position.z += state->velocity_per_frame.z;
        state->rotation.x = wrap_angle(
            state->rotation.x + state->angular_velocity_per_frame.x);
        state->rotation.y = wrap_angle(
            state->rotation.y + state->angular_velocity_per_frame.y);
        state->rotation.z = wrap_angle(
            state->rotation.z + state->angular_velocity_per_frame.z);
        apply_pending_corrections(state, 1);
        begin_prediction_correction(state, sample, sample_elapsed_frames,
                                    receiver_lag_frames);
    }
    write_output(state, output);
    output->consumed_sample = 1;
    return should_snap;
}
