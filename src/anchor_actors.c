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

#define ANCHOR_REMOTE_MAX 25
#define POSITION_SEND_FRAMES 6
#define POSITION_KEEPALIVE_FRAMES 120
#define POSITION_MIN_DELTA_SQ 36
#define ANIMATION_SEND_DELTA_100 20
#define ANIMATION_RESTART_DELTA_100 50
#define LOBBY_REFRESH_FRAMES 1
#define REMOTE_SNAP_DISTANCE_SQ 250000.0f
#define REMOTE_SMOOTHING_FACTOR 0.35f
#define REMOTE_VELOCITY_LEAD_SECONDS 0.08f

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
    int x;
    int y;
    int z;
    int has_pos;
    int ch;
    int vx;
    int vy;
    int vz;
    int seq;
    int action;
    int anim_frame_100;
    int anim_frame_count_100;
    int rot_x;
    int rot_y;
    int rot_z;
    int appearance_flags;
    int same_team;
    char name[32];
} RemotePlayer;

typedef struct RemoteSmoothing
{
    int cid;
    int room;
    int active;
    float x;
    float y;
    float z;
    int seq;
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
static int s_last_sent_appearance_flags = -1;
static unsigned int s_last_sent_room = 0xffffffffu;
static int s_lobby_refresh_timer;

static const char *const s_char_names[4] = {
    "Goemon", "Ebisumaru", "Sasuke", "Yae"};

static const char *sfind(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle)
        return hay;

    for (; *hay; ++hay)
    {
        const char *h = hay;
        const char *n = needle;

        while (*n && *h == *n)
        {
            ++h;
            ++n;
        }
        if (!*n)
            return hay;
    }
    return 0;
}

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
    const char *p = sfind(obj, key);
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
    const char *p = sfind(obj, key);
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

        p = sfind(p, "{");
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
        s_remote_players[count].action = parse_int_after(p, "\"a\"", -1);
        s_remote_players[count].anim_frame_100 = parse_int_after(p, "\"af\"", 0);
        s_remote_players[count].anim_frame_count_100 = parse_int_after(p, "\"al\"", 0);
        s_remote_players[count].rot_x = parse_int_after(p, "\"rx\"", 0);
        s_remote_players[count].rot_y = parse_int_after(p, "\"ry\"", 0);
        s_remote_players[count].rot_z = parse_int_after(p, "\"rz\"", 0);
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
        p = sfind(p, "}");
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
    s_remote_smoothing[free_index].room = -1;
    s_remote_smoothing[free_index].x = 0.0f;
    s_remote_smoothing[free_index].y = 0.0f;
    s_remote_smoothing[free_index].z = 0.0f;
    s_remote_smoothing[free_index].seq = 0;
    return &s_remote_smoothing[free_index];
}

static void clear_remote_smoothing(void)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        s_remote_smoothing[i].active = 0;
}

static void smooth_remote_player(const RemotePlayer *remote, RemotePlayer *out)
{
    RemoteSmoothing *smooth = find_remote_smoothing(remote->cid, 1);
    float target_x;
    float target_y;
    float target_z;
    float dx;
    float dy;
    float dz;
    float dist_sq;

    *out = *remote;
    if (!smooth)
        return;
    target_x = (float)remote->x + (float)remote->vx * REMOTE_VELOCITY_LEAD_SECONDS;
    target_y = (float)remote->y + (float)remote->vy * REMOTE_VELOCITY_LEAD_SECONDS;
    target_z = (float)remote->z + (float)remote->vz * REMOTE_VELOCITY_LEAD_SECONDS;
    dx = target_x - smooth->x;
    dy = target_y - smooth->y;
    dz = target_z - smooth->z;
    dist_sq = dx * dx + dy * dy + dz * dz;
    if (smooth->room != remote->room || smooth->seq == 0 ||
        dist_sq > REMOTE_SNAP_DISTANCE_SQ)
    {
        smooth->x = target_x;
        smooth->y = target_y;
        smooth->z = target_z;
    }
    else
    {
        smooth->x += dx * REMOTE_SMOOTHING_FACTOR;
        smooth->y += dy * REMOTE_SMOOTHING_FACTOR;
        smooth->z += dz * REMOTE_SMOOTHING_FACTOR;
    }
    smooth->room = remote->room;
    smooth->seq = remote->seq ? remote->seq : smooth->seq + 1;
    out->x = (int)smooth->x;
    out->y = (int)smooth->y;
    out->z = (int)smooth->z;
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
    int anim_delta;
    int frame_restarted;
    int should_send_position = 0;
    int should_send_animation;
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
        return;

    x = (int)local_obj->x;
    y = (int)local_obj->y;
    z = (int)local_obj->z;
    /* Read the stable player task/model fields after the game frame so peers
     * receive the action boundary, animation phase, and final rotations. */
    action = (int)read_u8_at(D_801FC604_5B8514, 0xcc);
    frame_100 = (int)(read_float_at(local_obj, 0x28) * 100.0f);
    /* Use the engine's model resolver because flagged model references cannot
     * be safely dereferenced as ordinary pointers by the mod. */
    frame_count = func_8001B5AC_1C1AC(local_obj);
    frame_count_100 = frame_count > 0.0f ? (int)(frame_count * 100.0f) : 0;
    rot_x = (int)read_s16_at(local_obj, 0x14);
    rot_y = (int)read_s16_at(local_obj, 0x16);
    rot_z = (int)read_s16_at(local_obj, 0x18);
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
        return;

    if (anchor_set_position_anim(x, y, z, action, frame_100,
                                 frame_count_100, rot_x, rot_y, rot_z,
                                 appearance_flags))
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

    for (remote_index = 0;
         remote_index < s_remote_count && slot_index < ANCHOR_REMOTE_MAX;
         ++remote_index)
    {
        RemotePlayer *remote = &s_remote_players[remote_index];
        RemotePlayer smoothed_remote;

        if (!remote->has_pos || remote->room != (int)D_800C7AB2)
            continue;

        smooth_remote_player(remote, &smoothed_remote);
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
