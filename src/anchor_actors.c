/**
 * @file anchor_actors.c
 * @brief Anchor remote publishing and cutscene-style model rendering.
 *
 * Remote Goemon/Ebisumaru/Sasuke/Yae models use standalone task/display
 * objects like the game's character cutscenes. Shared model/action data stays
 * immutable; appearance substitutions use slot-private render data. No
 * playable constructor, player actor, controls, or behavior callback is used.
 */

#include "modding.h"
#include "anchor.h"
#include "anchor_nameplates.h"
#include "anchor_player_models.h"
#include "anchor_remote_animation.h"
#include "anchor_remote_motion.h"
#include "utils/string_utils.h"

#define ANCHOR_REMOTE_MAX 25
#define POSITION_SEND_FRAMES 6
#define POSITION_KEEPALIVE_FRAMES 120
#define POSITION_MIN_DELTA_SQ 36
#define ANIMATION_SEND_DELTA_100 20
#define ANIMATION_RESTART_DELTA_100 50
#define LOBBY_REFRESH_FRAMES 0
#define LOCAL_GAME_TICKS_PER_SECOND 30.0f
#define MOTION_EDGE_LINEAR_MIN_SPEED 30
/* 32 signed-angle units/tick is about 0.18 degrees/tick. Ignore the common
 * one-unit quantization wobble while still publishing a visible turn edge. */
#define MOTION_EDGE_ANGULAR_MIN_SPEED 960

#define CHARACTER_GOEMON 0
#define CHARACTER_EBISUMARU 1
#define CHARACTER_SASUKE 2
#define CHARACTER_YAE 3
#define CHARACTER_COUNT 4

typedef struct PlayerObject
{
    unsigned char header[8];
    float x;
    float y;
    float z;
} PlayerObject;

typedef struct Vec3f
{
    float x;
    float y;
    float z;
} Vec3f;

typedef struct RemotePlayer
{
    int cid;
    int room;
    float x;
    float y;
    float z;
    int has_pos;
    int ch;
    int vx;
    int vy;
    int vz;
    int seq;
    int timestamp_ms;
    int action;
    int anim_frame_100;
    int anim_frame_count_100;
    int anim_step_100;
    int has_anim_step;
    int rot_x;
    int rot_y;
    int rot_z;
    int rot_vx;
    int rot_vy;
    int rot_vz;
    int appearance_flags;
    int motion_phase_frames;
    int new_motion_sample;
    int same_team;
    char name[32];
} RemotePlayer;

typedef struct RemoteSmoothing
{
    int cid;
    int active;
    int seen;
    AnchorRemoteMotionState motion;
} RemoteSmoothing;

/* Current room id. The remote renderer uses the same room boundary as the
 * game so it never leaves cutscene-model tasks visible across room changes. */
extern unsigned short D_800C7AB2;

/* Ghidra shows this as the live player task. The task stores the current
 * action at +0xCC and is used as the parent for remote cutscene-model tasks. */
extern void *D_801FC604_5B8514;

/* Ghidra shows player task +0x18 being copied here. This model/display object
 * contains position, rotation, current animation frame, and model resources. */
extern PlayerObject *D_801FC60C_5B851C;

/* Camera offset and radius maintained by the game. They are read only to
 * project the existing remote-player nameplates. */
extern Vec3f D_8020D1C0_5C90D0;
extern float D_8020D1D0_5C90E0;

/* Resolve the active model pointer and return its animation frame count.
 * Local counts are transmitted for normalized phase mapping; remote counts
 * bound the selected cutscene model's frame field. */
extern float func_8001B5AC_1C1AC(void *model);

/* Current selected-character id. Ghidra shows the player action/character
 * routines reading and writing this stable global alongside player task +0x60. */
#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static RemoteSmoothing s_remote_smoothing[ANCHOR_REMOTE_MAX];
static AnchorPlayerModelRemote s_remote_models[ANCHOR_REMOTE_MAX];
static int s_remote_count;
static int s_state_send_timer;
static int s_position_keepalive_timer;
static int s_have_sent_position;
static int s_last_sent_x;
static int s_last_sent_y;
static int s_last_sent_z;
static int s_last_sent_action = -2;
static int s_last_sent_frame_100;
static int s_last_sent_frame_count_100;
static int s_last_sent_rot_x;
static int s_last_sent_rot_y;
static int s_last_sent_rot_z;
static int s_last_sent_velocity_x;
static int s_last_sent_velocity_y;
static int s_last_sent_velocity_z;
static int s_last_sent_angular_velocity_x;
static int s_last_sent_angular_velocity_y;
static int s_last_sent_angular_velocity_z;
static int s_last_sent_appearance_flags = -1;
static unsigned int s_last_sent_room = 0xffffffffu;
static int s_have_previous_frame_position;
static float s_previous_frame_x;
static float s_previous_frame_y;
static float s_previous_frame_z;
static int s_have_previous_frame_rotation;
static int s_previous_frame_rot_x;
static int s_previous_frame_rot_y;
static int s_previous_frame_rot_z;
static int s_have_previous_frame_animation;
static int s_previous_frame_action;
static float s_previous_frame_anim_frame;
static float s_previous_frame_anim_count;
static const PlayerObject *s_previous_frame_object;
static const void *s_previous_frame_task;
static int s_lobby_refresh_timer;

static const char *const s_char_names[4] = {
    "Goemon", "Ebisumaru", "Sasuke", "Yae"};

static unsigned char read_u8_at(const void *obj, unsigned int offset)
{
    return *(const unsigned char *)((const unsigned char *)obj + offset);
}

static short read_s16_at(const void *obj, unsigned int offset)
{
    return *(const short *)((const unsigned char *)obj + offset);
}

static unsigned short read_u16_at(const void *obj, unsigned int offset)
{
    return *(const unsigned short *)((const unsigned char *)obj + offset);
}

static float read_float_at(const void *obj, unsigned int offset)
{
    return *(const float *)((const unsigned char *)obj + offset);
}

static int is_rdram_pointer(const void *ptr)
{
    unsigned int addr = (unsigned int)(unsigned long)ptr;
    unsigned int phys = addr & 0x1fffffffu;

    /* The engine uses 0x80000000 as an invalid-link sentinel during task
     * teardown; it must not pass the same test as a live RDRAM object. */
    return phys >= 0x00001000u && phys < 0x00800000u;
}

static int round_float_to_int(float value)
{
    if (value < 0.0f)
        return (int)(value - 0.5f);
    return (int)(value + 0.5f);
}

static void reset_frame_motion_baseline(void)
{
    s_have_previous_frame_position = 0;
    s_have_previous_frame_rotation = 0;
    s_have_previous_frame_animation = 0;
    s_previous_frame_object = 0;
    s_previous_frame_task = 0;
}

/* Gameplay uses a different overlay than file_18. This return hook runs after
 * normal stage resources and stages the immutable render files for all four
 * characters required by the standalone remote model tasks. */
RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_load_remote_cutscene_resources(void)
{
    /* Use the dedicated stage-load path so broad decompression and whole-file
     * action DMA never occur inside the per-frame remote update hook. */
    anchor_player_models_load_resources();
}

static int parse_int_after(const char *obj, const char *key, int fallback)
{
    const char *p = mnsg_string_find(obj, key);
    int sign = 1;
    int value = 0;
    int saw_digit = 0;

    if (!p)
        return fallback;
    while (*p && *p != ':')
        ++p;
    if (*p == ':')
        ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (*p == '-')
    {
        sign = -1;
        ++p;
    }
    while (*p >= '0' && *p <= '9')
    {
        saw_digit = 1;
        value = value * 10 + (*p - '0');
        ++p;
    }
    return saw_digit ? value * sign : fallback;
}

static void parse_string_after(const char *obj, const char *key, char *out, int out_size)
{
    const char *p = mnsg_string_find(obj, key);
    int n = 0;

    if (out_size <= 0)
        return;
    out[0] = '\0';
    if (!p)
        return;
    while (*p && *p != ':')
        ++p;
    if (*p == ':')
        ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (*p != '"')
        return;
    ++p;
    while (*p && *p != '"' && n < out_size - 1)
    {
        if (*p == '\\' && *(p + 1))
            ++p;
        out[n++] = *p++;
    }
    out[n] = '\0';
}

static int parse_lobby_positions(const char *json)
{
    const char *p = json;
    int count = 0;

    while (p && *p && count < ANCHOR_REMOTE_MAX)
    {
        int ch;

        p = mnsg_string_find(p, "{");
        if (!p)
            break;
        s_remote_players[count].cid = parse_int_after(p, "\"cid\"", 0);
        s_remote_players[count].room = parse_int_after(p, "\"room\"", -1);
        s_remote_players[count].x = parse_int_after(p, "\"x\"", 0);
        s_remote_players[count].y = parse_int_after(p, "\"y\"", 0);
        s_remote_players[count].z = parse_int_after(p, "\"z\"", 0);
        s_remote_players[count].has_pos = parse_int_after(p, "\"hp\"", 0);
        ch = parse_int_after(p, "\"ch\"", -1);
        s_remote_players[count].ch = (ch >= 0 && ch < 4) ? ch : -1;
        s_remote_players[count].vx = parse_int_after(p, "\"vx\"", 0);
        s_remote_players[count].vy = parse_int_after(p, "\"vy\"", 0);
        s_remote_players[count].vz = parse_int_after(p, "\"vz\"", 0);
        s_remote_players[count].seq = parse_int_after(p, "\"s\"", 0);
        s_remote_players[count].timestamp_ms =
            parse_int_after(p, "\"t\"", 0);
        s_remote_players[count].action = parse_int_after(p, "\"a\"", -1);
        s_remote_players[count].anim_frame_100 = parse_int_after(p, "\"af\"", 0);
        s_remote_players[count].anim_frame_count_100 = parse_int_after(p, "\"al\"", 0);
        s_remote_players[count].anim_step_100 = parse_int_after(p, "\"as\"", 0);
        s_remote_players[count].has_anim_step = parse_int_after(p, "\"ah\"", 0);
        s_remote_players[count].rot_x = parse_int_after(p, "\"rx\"", 0);
        s_remote_players[count].rot_y = parse_int_after(p, "\"ry\"", 0);
        s_remote_players[count].rot_z = parse_int_after(p, "\"rz\"", 0);
        s_remote_players[count].rot_vx = parse_int_after(p, "\"rvx\"", 0);
        s_remote_players[count].rot_vy = parse_int_after(p, "\"rvy\"", 0);
        s_remote_players[count].rot_vz = parse_int_after(p, "\"rvz\"", 0);
        s_remote_players[count].appearance_flags =
            parse_int_after(p, "\"ap\"", 0);
        s_remote_players[count].same_team = parse_int_after(p, "\"tm\"", 1);
        parse_string_after(p, "\"n\"", s_remote_players[count].name,
                           (int)sizeof(s_remote_players[count].name));
        if (!s_remote_players[count].name[0])
        {
            s_remote_players[count].name[0] = 'P';
            s_remote_players[count].name[1] = '\0';
        }
        if (s_remote_players[count].cid > 0)
            count++;
        p = mnsg_string_find(p, "}");
        if (p)
            ++p;
    }
    return count;
}

static RemoteSmoothing *find_remote_smoothing(int cid, int create)
{
    int i;
    int free_index = -1;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        if (s_remote_smoothing[i].active && s_remote_smoothing[i].cid == cid)
            return &s_remote_smoothing[i];
        if (!s_remote_smoothing[i].active && free_index < 0)
            free_index = i;
    }
    if (!create || free_index < 0)
        return 0;
    s_remote_smoothing[free_index].cid = cid;
    s_remote_smoothing[free_index].active = 1;
    s_remote_smoothing[free_index].seen = 0;
    anchor_remote_motion_reset(&s_remote_smoothing[free_index].motion);
    return &s_remote_smoothing[free_index];
}

static void clear_remote_smoothing(void)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        s_remote_smoothing[i].active = 0;
}

static void begin_remote_smoothing_frame(void)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        s_remote_smoothing[i].seen = 0;
}

static void end_remote_smoothing_frame(void)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        if (s_remote_smoothing[i].active && !s_remote_smoothing[i].seen)
            s_remote_smoothing[i].active = 0;
    }
}

static void drop_remote_smoothing(int cid)
{
    RemoteSmoothing *smooth = find_remote_smoothing(cid, 0);

    if (smooth)
        smooth->active = 0;
}

static void smooth_remote_player(const RemotePlayer *remote, RemotePlayer *out)
{
    RemoteSmoothing *smooth = find_remote_smoothing(remote->cid, 1);
    AnchorRemoteMotionSample sample;
    AnchorRemoteMotionOutput motion;

    *out = *remote;
    if (!smooth)
        return;
    smooth->seen = 1;
    sample.room = remote->room;
    sample.seq = remote->seq;
    sample.timestamp_ms = remote->timestamp_ms;
    sample.position.x = remote->x;
    sample.position.y = remote->y;
    sample.position.z = remote->z;
    sample.velocity.x = (float)remote->vx;
    sample.velocity.y = (float)remote->vy;
    sample.velocity.z = (float)remote->vz;
    sample.action = remote->action;
    sample.rot_x = remote->rot_x;
    sample.rot_y = remote->rot_y;
    sample.rot_z = remote->rot_z;
    sample.angular_velocity.x = (float)remote->rot_vx;
    sample.angular_velocity.y = (float)remote->rot_vy;
    sample.angular_velocity.z = (float)remote->rot_vz;
    anchor_remote_motion_step(&smooth->motion, &sample, &motion);
    out->x = motion.position.x;
    out->y = motion.position.y;
    out->z = motion.position.z;
    out->rot_x = motion.rot_x;
    out->rot_y = motion.rot_y;
    out->rot_z = motion.rot_z;
    out->motion_phase_frames = smooth->motion.projection_lead_frames;
    out->new_motion_sample = motion.consumed_sample;
}

static void reset_last_sent_state(void)
{
    s_state_send_timer = 0;
    s_position_keepalive_timer = 0;
    s_have_sent_position = 0;
    s_last_sent_action = -2;
    s_last_sent_frame_100 = 0;
    s_last_sent_frame_count_100 = 0;
    s_last_sent_appearance_flags = -1;
    s_last_sent_room = 0xffffffffu;
    reset_frame_motion_baseline();
}

static void publish_local_state(PlayerObject *local_obj)
{
    unsigned int char_idx;
    unsigned int room;
    int x;
    int y;
    int z;
    int dx;
    int dy;
    int dz;
    int action;
    int frame_100;
    int frame_count_100;
    int rot_x;
    int rot_y;
    int rot_z;
    int appearance_flags;
    int velocity_x = 0;
    int velocity_y = 0;
    int velocity_z = 0;
    int angular_velocity_x = 0;
    int angular_velocity_y = 0;
    int angular_velocity_z = 0;
    int animation_step_100 = 0;
    int has_animation_step = 0;
    int anim_delta;
    int frame_restarted;
    int should_send_position = 0;
    int should_send_animation;
    int force_motion_edge = 0;
    float current_x;
    float current_y;
    float current_z;
    float frame_count;

    if (!anchor_is_connected())
    {
        reset_last_sent_state();
        return;
    }
    /* Read the engine's room id so the network visibility boundary stays
     * identical to the room in which the local player task exists. */
    room = (unsigned int)D_800C7AB2;
    if (room != s_last_sent_room)
    {
        s_last_sent_room = room;
        s_have_sent_position = 0;
        s_position_keepalive_timer = 0;
        s_last_sent_action = -2;
        reset_frame_motion_baseline();
    }
    anchor_set_local_room(room);
    /* Read only the selected-character id so peers select the matching
     * immutable render assets; no playable constructor or behavior is run. */
    char_idx = *CURRENT_CHAR_PTR & 3;
    anchor_set_character(s_char_names[char_idx]);
    if (s_state_send_timer > 0)
        s_state_send_timer--;
    if (s_position_keepalive_timer > 0)
        s_position_keepalive_timer--;
    /* Validate the external player task before reading its action field; room
     * transitions temporarily clear or replace this engine-owned pointer.
     * Sample every frame so action changes and animation restarts can bypass
     * the normal network cadence without increasing steady-state traffic. */
    if (!is_rdram_pointer(local_obj) ||
        !is_rdram_pointer(D_801FC604_5B8514))
    {
        reset_frame_motion_baseline();
        return;
    }

    if (local_obj != s_previous_frame_object ||
        D_801FC604_5B8514 != s_previous_frame_task)
    {
        /* A same-room respawn or character task replacement can swap valid
         * pointers without an invalid frame in between. Never finite-
         * difference the new model against the previous owner's transform. */
        reset_frame_motion_baseline();
        s_previous_frame_object = local_obj;
        s_previous_frame_task = D_801FC604_5B8514;
        /* The new authority may be stationary at the old coordinates and
         * otherwise evade every cadence predicate. Publish its zero endpoint
         * immediately so peers cannot keep carrying the previous owner. */
        force_motion_edge = 1;
    }

    current_x = local_obj->x;
    current_y = local_obj->y;
    current_z = local_obj->z;
    x = (int)current_x;
    y = (int)current_y;
    z = (int)current_z;
    /* Capture the final post-collision displacement of this exact game frame.
     * Packet-to-packet averages hide acceleration, jump apices, landings, and
     * abrupt stops; the receiver carries this latest resolved displacement
     * across only the missing game ticks. */
    if (s_have_previous_frame_position)
    {
        velocity_x = round_float_to_int(
            (current_x - s_previous_frame_x) *
            LOCAL_GAME_TICKS_PER_SECOND);
        velocity_y = round_float_to_int(
            (current_y - s_previous_frame_y) *
            LOCAL_GAME_TICKS_PER_SECOND);
        velocity_z = round_float_to_int(
            (current_z - s_previous_frame_z) *
            LOCAL_GAME_TICKS_PER_SECOND);
    }
    s_previous_frame_x = current_x;
    s_previous_frame_y = current_y;
    s_previous_frame_z = current_z;
    s_have_previous_frame_position = 1;
    /* Read the stable player task/model fields after the game frame so peers
     * receive the action boundary, animation phase, and final rotations. */
    action = (int)read_u8_at(D_801FC604_5B8514, 0xcc);
    frame_100 = (int)(read_float_at(local_obj, 0x28) * 100.0f);
    /* Use the engine's model resolver because flagged model references cannot
     * be safely dereferenced as ordinary pointers by the mod. */
    frame_count = func_8001B5AC_1C1AC(local_obj);
    frame_count_100 = frame_count > 0.0f ? (int)(frame_count * 100.0f) : 0;
    if (s_have_previous_frame_animation &&
        action == s_previous_frame_action &&
        frame_count > 0.0f &&
        frame_count == s_previous_frame_anim_count)
    {
        float animation_delta =
            read_float_at(local_obj, 0x28) - s_previous_frame_anim_frame;
        float half_frame_count = frame_count * 0.5f;

        while (animation_delta > half_frame_count)
            animation_delta -= frame_count;
        while (animation_delta < -half_frame_count)
            animation_delta += frame_count;
        animation_step_100 = round_float_to_int(animation_delta * 100.0f);
        has_animation_step = 1;
    }
    s_previous_frame_action = action;
    s_previous_frame_anim_frame = read_float_at(local_obj, 0x28);
    s_previous_frame_anim_count = frame_count;
    s_have_previous_frame_animation = 1;
    rot_x = (int)read_s16_at(local_obj, 0x14);
    rot_y = (int)read_s16_at(local_obj, 0x16);
    rot_z = (int)read_s16_at(local_obj, 0x18);
    if (s_have_previous_frame_rotation)
    {
        angular_velocity_x =
            anchor_remote_motion_angle_delta_s16(
                rot_x, s_previous_frame_rot_x) *
            (int)LOCAL_GAME_TICKS_PER_SECOND;
        angular_velocity_y =
            anchor_remote_motion_angle_delta_s16(
                rot_y, s_previous_frame_rot_y) *
            (int)LOCAL_GAME_TICKS_PER_SECOND;
        angular_velocity_z =
            anchor_remote_motion_angle_delta_s16(
                rot_z, s_previous_frame_rot_z) *
            (int)LOCAL_GAME_TICKS_PER_SECOND;
    }
    s_previous_frame_rot_x = rot_x;
    s_previous_frame_rot_y = rot_y;
    s_previous_frame_rot_z = rot_z;
    s_have_previous_frame_rotation = 1;
    if (s_have_sent_position)
    {
        force_motion_edge = force_motion_edge ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_velocity_x, velocity_x,
                MOTION_EDGE_LINEAR_MIN_SPEED) ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_velocity_y, velocity_y,
                MOTION_EDGE_LINEAR_MIN_SPEED) ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_velocity_z, velocity_z,
                MOTION_EDGE_LINEAR_MIN_SPEED) ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_angular_velocity_x, angular_velocity_x,
                MOTION_EDGE_ANGULAR_MIN_SPEED) ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_angular_velocity_y, angular_velocity_y,
                MOTION_EDGE_ANGULAR_MIN_SPEED) ||
            anchor_remote_motion_axis_has_edge(
                s_last_sent_angular_velocity_z, angular_velocity_z,
                MOTION_EDGE_ANGULAR_MIN_SPEED);
    }
    /* Pack remote appearance state into one byte-sized bitmap. FUN_801F5314
     * sets work +0x84 when Sudden Impact actually turns gold. Ebisumaru's
     * work +0x86 marker remains nonzero from the shrink action until the grow
     * action completes; peers derive the live scale locally from that bit and
     * the already-synchronized action frame. */
    appearance_flags = 0;
    {
        void *player_work =
            *(void **)((unsigned char *)D_801FC604_5B8514 + 0x5c);

        if (is_rdram_pointer(player_work))
        {
            if (char_idx == CHARACTER_GOEMON &&
                read_u8_at(player_work, 0x84) == 1)
                appearance_flags |= ANCHOR_APPEARANCE_SUDDEN_IMPACT;
            if (char_idx == CHARACTER_EBISUMARU &&
                read_u16_at(player_work, 0x86) != 0)
                appearance_flags |= ANCHOR_APPEARANCE_MINI_EBISUMARU;
        }
    }

    if (!s_have_sent_position)
    {
        should_send_position = 1;
    }
    else
    {
        dx = x - s_last_sent_x;
        dy = y - s_last_sent_y;
        dz = z - s_last_sent_z;
        should_send_position =
            s_state_send_timer <= 0 &&
            ((dx * dx + dy * dy + dz * dz) >= POSITION_MIN_DELTA_SQ ||
             s_position_keepalive_timer <= 0);
    }
    anim_delta = frame_100 - s_last_sent_frame_100;
    if (anim_delta < 0)
        anim_delta = -anim_delta;
    frame_restarted =
        action == s_last_sent_action &&
        frame_100 + ANIMATION_RESTART_DELTA_100 < s_last_sent_frame_100;
    has_animation_step = anchor_remote_animation_step_is_continuous(
        frame_restarted, has_animation_step, animation_step_100);
    should_send_animation =
        action != s_last_sent_action ||
        appearance_flags != s_last_sent_appearance_flags ||
        frame_restarted ||
        (s_state_send_timer <= 0 &&
         (anim_delta >= ANIMATION_SEND_DELTA_100 ||
          frame_count_100 != s_last_sent_frame_count_100 ||
          rot_x != s_last_sent_rot_x ||
          rot_y != s_last_sent_rot_y ||
          rot_z != s_last_sent_rot_z));
    if (!should_send_position && !should_send_animation)
    {
        if (!force_motion_edge)
            return;
        should_send_position = 1;
    }

    if (anchor_set_position_anim(x, y, z, action, frame_100,
                                 frame_count_100, rot_x, rot_y, rot_z,
                                 appearance_flags, velocity_x, velocity_y,
                                 velocity_z, angular_velocity_x,
                                 angular_velocity_y, angular_velocity_z,
                                 force_motion_edge, animation_step_100,
                                 has_animation_step))
    {
        s_have_sent_position = 1;
        s_last_sent_x = x;
        s_last_sent_y = y;
        s_last_sent_z = z;
        s_last_sent_action = action;
        s_last_sent_frame_100 = frame_100;
        s_last_sent_frame_count_100 = frame_count_100;
        s_last_sent_rot_x = rot_x;
        s_last_sent_rot_y = rot_y;
        s_last_sent_rot_z = rot_z;
        s_last_sent_velocity_x = velocity_x;
        s_last_sent_velocity_y = velocity_y;
        s_last_sent_velocity_z = velocity_z;
        s_last_sent_angular_velocity_x = angular_velocity_x;
        s_last_sent_angular_velocity_y = angular_velocity_y;
        s_last_sent_angular_velocity_z = angular_velocity_z;
        s_last_sent_appearance_flags = appearance_flags;
        s_state_send_timer = POSITION_SEND_FRAMES;
        s_position_keepalive_timer = POSITION_KEEPALIVE_FRAMES;
    }
}

static void refresh_lobby(void)
{
    char *json;

    if (s_lobby_refresh_timer > 0)
    {
        s_lobby_refresh_timer--;
        return;
    }
    s_lobby_refresh_timer = LOBBY_REFRESH_FRAMES;
    json = anchor_get_lobby_positions_json();
    s_remote_count = parse_lobby_positions(json ? json : "[]");
    if (json)
        recomp_free(json);
}

static int render_remote_nameplate(int slot_index, const RemotePlayer *remote,
                                   const PlayerObject *local_obj)
{
    AnchorNameplateCamera camera;
    AnchorNameplatePlayer nameplate_remote;

    if (!is_rdram_pointer(local_obj) || !remote)
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }
    camera.player_x = local_obj->x;
    camera.player_y = local_obj->y;
    camera.player_z = local_obj->z;
    /* Read the engine-maintained camera orbit only for nameplate projection so
     * the cutscene model renderer does not create a second camera state. */
    camera.camera_x = D_8020D1C0_5C90D0.x;
    camera.camera_y = D_8020D1C0_5C90D0.y;
    camera.camera_z = D_8020D1C0_5C90D0.z;
    camera.camera_radius = D_8020D1D0_5C90E0;
    nameplate_remote.x = remote->x;
    nameplate_remote.y = remote->y;
    nameplate_remote.z = remote->z;
    nameplate_remote.ch = remote->ch;
    nameplate_remote.same_team = remote->same_team;
    nameplate_remote.name = remote->name;
    return anchor_nameplates_render_slot(slot_index, &nameplate_remote, &camera);
}

static void update_remote_cutscene_models(PlayerObject *local_obj)
{
    /* Use the live player task only as the owner of independent remote render
     * children, matching the opening cutscene's task hierarchy. */
    void *owner_task = D_801FC604_5B8514;
    int remote_index;
    int model_count = 0;
    int slot_index = 0;
    int visible_nameplates = 0;
    int i;

    if (!is_rdram_pointer(local_obj) || !is_rdram_pointer(owner_task))
    {
        /* The previous owner may already have been torn down; clear mod-side
         * handles without dereferencing or deleting that stale external task. */
        anchor_player_models_reset();
        clear_remote_smoothing();
        for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
            anchor_nameplates_hide_slot(i);
        anchor_nameplates_set_context_visible(0);
        return;
    }
    if (!anchor_is_connected())
    {
        s_remote_count = 0;
        clear_remote_smoothing();
        /* An empty update hides and retires all remote objects. Their reusable
         * child tasks remain owned by the player task until engine teardown. */
        anchor_player_models_update(0, 0, owner_task);
        for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
            anchor_nameplates_hide_slot(i);
        anchor_nameplates_set_context_visible(0);
        return;
    }

    begin_remote_smoothing_frame();

    for (remote_index = 0;
         remote_index < s_remote_count && slot_index < ANCHOR_REMOTE_MAX;
         ++remote_index)
    {
        RemotePlayer *remote = &s_remote_players[remote_index];
        RemotePlayer smoothed_remote;

        if (!remote->has_pos)
        {
            drop_remote_smoothing(remote->cid);
            continue;
        }

        /* Ingest every peer before applying the local-room visibility filter.
         * This lets room changes reset interpolation even while the peer is
         * invisible, so returning to a room cannot reuse an old trajectory. */
        smooth_remote_player(remote, &smoothed_remote);
        if (remote->room != (int)D_800C7AB2)
            continue;
        if (remote->ch >= CHARACTER_GOEMON &&
            remote->ch < CHARACTER_COUNT &&
            model_count < ANCHOR_REMOTE_MAX)
        {
            AnchorPlayerModelRemote *model = &s_remote_models[model_count++];

            model->cid = smoothed_remote.cid;
            model->ch = smoothed_remote.ch;
            model->x = smoothed_remote.x;
            model->y = smoothed_remote.y;
            model->z = smoothed_remote.z;
            model->vx = smoothed_remote.vx;
            model->vy = smoothed_remote.vy;
            model->vz = smoothed_remote.vz;
            model->seq = smoothed_remote.seq;
            model->action = smoothed_remote.action;
            model->anim_frame_100 = smoothed_remote.anim_frame_100;
            model->anim_frame_count_100 =
                smoothed_remote.anim_frame_count_100;
            model->anim_step_100 = smoothed_remote.anim_step_100;
            model->has_anim_step = smoothed_remote.has_anim_step;
            model->motion_phase_frames =
                smoothed_remote.motion_phase_frames;
            model->new_motion_sample =
                smoothed_remote.new_motion_sample;
            model->rot_x = smoothed_remote.rot_x;
            model->rot_y = smoothed_remote.rot_y;
            model->rot_z = smoothed_remote.rot_z;
            model->appearance_flags = smoothed_remote.appearance_flags;
            model->same_team = smoothed_remote.same_team;
        }

        visible_nameplates +=
            render_remote_nameplate(slot_index, &smoothed_remote, local_obj);
        slot_index++;
    }
    end_remote_smoothing_frame();

    /* Bind the sender's character/action records to plain cutscene-style
     * objects. This call never runs playable constructors or behavior code. */
    anchor_player_models_update(s_remote_models, model_count,
                                owner_task);
    for (i = slot_index; i < ANCHOR_REMOTE_MAX; ++i)
        anchor_nameplates_hide_slot(i);
    anchor_nameplates_set_context_visible(visible_nameplates > 0);
}

/* Run after the normal game frame so published and displayed state both use
 * the final player/model transform selected by the game's action callback. */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update_cutscene_models(void)
{
    /* Read the engine's final player display object after the frame return;
     * this is the authoritative transform and source animation phase. */
    PlayerObject *local_obj = D_801FC60C_5B851C;

    publish_local_state(local_obj);
    refresh_lobby();
    update_remote_cutscene_models(local_obj);
}
