/**
 * @file anchor_actors.c
 * @brief Anchor remote-player publishing and cutscene-model rendering.
 *
 * Standalone remote models use only model/resource paths created directly by
 * the new-file opening cutscene. No playable-player constructor, actor-manager
 * record, or player-character resource table is used here.
 */

#include "modding.h"
#include "anchor.h"
#include "anchor_nameplates.h"

#define ANCHOR_REMOTE_MAX 25
#define POSITION_SEND_FRAMES 2
#define POSITION_KEEPALIVE_FRAMES 30
#define POSITION_MIN_DELTA_SQ 36
#define ANIMATION_SEND_DELTA_100 20
#define ANIMATION_RESTART_DELTA_100 50
#define LOBBY_REFRESH_FRAMES 1
#define REMOTE_PACKET_FRAME_INTERVAL 2.0f
#define REMOTE_SNAP_DISTANCE_SQ 250000.0f
#define REMOTE_SMOOTHING_FACTOR 0.35f
#define REMOTE_VELOCITY_LEAD_SECONDS 0.04f
#define REMOTE_YAW_SPEED_THRESHOLD_SQ 64

#define CHARACTER_EBISUMARU 1
#define CUTSCENE_EBISUMARU_FILE_ID 0x4D9
#define CUTSCENE_EBISUMARU_MODEL_PTR 0x68000B0Cu
#define CUTSCENE_EBISUMARU_ANIM_CONTEXT 0xC006D898u
#define CUTSCENE_MODEL_SCALE 0.1f

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

typedef struct CutsceneModelSpec
{
    unsigned int model_ptr;
    unsigned int animation_context;
    unsigned short file_id;
    short base_rot_x;
    short base_rot_y;
    short base_rot_z;
    float default_frame_step;
} CutsceneModelSpec;

typedef struct RemoteCutsceneSlot
{
    int active;
    int cid;
    int seen;
    int ch;
    int last_seq;
    int last_action;
    int last_remote_frame_100;
    int last_remote_frame_count_100;
    short fallback_yaw;
    float frame;
    float frame_step;
    float frame_count;
    void *task;
    void *model;
} RemoteCutsceneSlot;

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

/* Allocate a child task and install its update callback. This is the stable
 * engine wrapper used by both opening-cutscene character creators. */
extern void *func_80034E08_35A08(void *parent_task,
                                  void (*update)(void *, void *),
                                  unsigned short flags);

/* Allocate and initialize one model/display record on a task. The opening
 * cutscene passes its model pointer, animation context, transform, scale, and
 * resource file id through this stable engine function. */
extern void *func_8000DBF0_E7F0(void *task,
                                 unsigned int model_ptr,
                                 unsigned int animation_context,
                                 float x, float y, float z,
                                 short rot_x, short rot_y, short rot_z,
                                 float scale_x, float scale_y, float scale_z,
                                 unsigned short file_id,
                                 unsigned short secondary_file_id);

/* Delete a task and its attached records. The file_18 opening director uses
 * this exact helper when it removes Goemon and Ebisumaru at state 10's end. */
extern void func_80034EF8_35AF8(void *task);

/* Load one resource file into the game's wave/resource registry. The hook
 * below calls it immediately after normal stage resources load, never from
 * the per-frame remote update path. */
extern void *func_80013B14_14714(unsigned int file_id);

/* Return the registered data pointer for a loaded resource file, or -1 when
 * absent. This verifies both cutscene resources before model construction. */
extern void *func_800141C4_14DC4(unsigned int file_id);

/* Resolve the active model pointer and return its animation frame count.
 * Local counts are transmitted for normalized phase mapping; remote counts
 * bound the selected cutscene model's frame field. */
extern float func_8001B5AC_1C1AC(void *model);

/* Current selected-character id. Ghidra shows the player action/character
 * routines reading and writing this stable global alongside player task +0x60. */
#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static RemoteSmoothing s_remote_smoothing[ANCHOR_REMOTE_MAX];
static RemoteCutsceneSlot s_cutscene_slots[ANCHOR_REMOTE_MAX];
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
static unsigned int s_last_sent_room = 0xffffffffu;
static int s_lobby_refresh_timer;
static int s_cutscene_ebisumaru_resource_ready;
static void *s_cutscene_owner_task;

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

static void write_u8_at(void *obj, unsigned int offset, unsigned char value)
{
    *(unsigned char *)((unsigned char *)obj + offset) = value;
}

static void write_u16_at(void *obj, unsigned int offset, unsigned short value)
{
    *(unsigned short *)((unsigned char *)obj + offset) = value;
}

static void write_float_at(void *obj, unsigned int offset, float value)
{
    *(float *)((unsigned char *)obj + offset) = value;
}

static unsigned char read_u8_at(const void *obj, unsigned int offset)
{
    return *(const unsigned char *)((const unsigned char *)obj + offset);
}

static short read_s16_at(const void *obj, unsigned int offset)
{
    return *(const short *)((const unsigned char *)obj + offset);
}

static float read_float_at(const void *obj, unsigned int offset)
{
    return *(const float *)((const unsigned char *)obj + offset);
}

static int is_rdram_pointer(const void *ptr)
{
    unsigned int addr = (unsigned int)(unsigned long)ptr;
    unsigned int phys = addr & 0x1fffffffu;

    return addr != 0 && phys < 0x00800000u;
}

static int resource_is_loaded(unsigned int file_id)
{
    /* Use the engine registry lookup so model creation is attempted only after
     * the return hook has made the file available to flagged model pointers. */
    void *resource = func_800141C4_14DC4(file_id);

    return resource != 0 && resource != (void *)(unsigned long)0xffffffffu;
}

/* Gameplay uses a different overlay than file_18, so its Ebisumaru creator
 * cannot be called directly. This return hook runs after the gameplay stage's
 * normal resource list and appends the verified standalone cutscene file. */
RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_load_remote_cutscene_resources(void)
{
    /* Use the normal resource loader here because func_8000DBF0 resolves its
     * flagged model pointers through the same registry immediately afterward. */
    func_80013B14_14714(CUTSCENE_EBISUMARU_FILE_ID);
    s_cutscene_ebisumaru_resource_ready =
        resource_is_loaded(CUTSCENE_EBISUMARU_FILE_ID);
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
    /* Read only the selected-character id to choose the matching cutscene
     * model on peers; no playable model resource or constructor is reused. */
    char_idx = *CURRENT_CHAR_PTR & 3;
    anchor_set_character(s_char_names[char_idx]);
    if (s_state_send_timer > 0)
        s_state_send_timer--;
    if (s_position_keepalive_timer > 0)
        s_position_keepalive_timer--;
    /* Validate the external player task before reading its action field; room
     * transitions temporarily clear or replace this engine-owned pointer. */
    if (!is_rdram_pointer(local_obj) ||
        !is_rdram_pointer(D_801FC604_5B8514) ||
        s_state_send_timer > 0)
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
            (dx * dx + dy * dy + dz * dz) >= POSITION_MIN_DELTA_SQ ||
            s_position_keepalive_timer <= 0;
    }
    anim_delta = frame_100 - s_last_sent_frame_100;
    if (anim_delta < 0)
        anim_delta = -anim_delta;
    frame_restarted =
        action == s_last_sent_action &&
        frame_100 + ANIMATION_RESTART_DELTA_100 < s_last_sent_frame_100;
    should_send_animation =
        s_state_send_timer <= 0 ||
        action != s_last_sent_action ||
        frame_restarted ||
        anim_delta >= ANIMATION_SEND_DELTA_100 ||
        frame_count_100 != s_last_sent_frame_count_100 ||
        rot_x != s_last_sent_rot_x ||
        rot_y != s_last_sent_rot_y ||
        rot_z != s_last_sent_rot_z;
    if (!should_send_position && !should_send_animation)
        return;

    if (anchor_set_position_anim(x, y, z, action, frame_100,
                                 frame_count_100, rot_x, rot_y, rot_z))
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

static int get_cutscene_model_spec(int ch, CutsceneModelSpec *spec)
{
    if (!spec)
        return 0;
    if (ch == CHARACTER_EBISUMARU)
    {
        spec->model_ptr = CUTSCENE_EBISUMARU_MODEL_PTR;
        spec->animation_context = CUTSCENE_EBISUMARU_ANIM_CONTEXT;
        spec->file_id = CUTSCENE_EBISUMARU_FILE_ID;
        spec->base_rot_x = 0;
        spec->base_rot_y = 0;
        spec->base_rot_z = 0;
        spec->default_frame_step = 0.3f;
        return 1;
    }
    return 0;
}

static RemoteCutsceneSlot *find_cutscene_slot_by_cid(int cid)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        if (s_cutscene_slots[i].active && s_cutscene_slots[i].cid == cid)
            return &s_cutscene_slots[i];
    return 0;
}

static RemoteCutsceneSlot *find_cutscene_slot_by_task(void *task)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        if (s_cutscene_slots[i].active && s_cutscene_slots[i].task == task)
            return &s_cutscene_slots[i];
    return 0;
}

static void clear_cutscene_slot(RemoteCutsceneSlot *slot, int delete_task)
{
    if (!slot)
        return;
    /* Use the opening cutscene's own task cleanup helper when the task tree is
     * still live so the attached model record returns to the engine pool. */
    if (delete_task && slot->task)
        func_80034EF8_35AF8(slot->task);
    slot->active = 0;
    slot->cid = 0;
    slot->seen = 0;
    slot->ch = -1;
    slot->last_seq = 0;
    slot->last_action = -2;
    slot->last_remote_frame_100 = 0;
    slot->last_remote_frame_count_100 = 0;
    slot->fallback_yaw = 0;
    slot->frame = 0.0f;
    slot->frame_step = 0.0f;
    slot->frame_count = 0.0f;
    slot->task = 0;
    slot->model = 0;
}

static void reset_cutscene_slots(int delete_tasks)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        clear_cutscene_slot(&s_cutscene_slots[i], delete_tasks);
}

static RemoteCutsceneSlot *allocate_cutscene_slot(int cid, int ch)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        if (!s_cutscene_slots[i].active)
        {
            s_cutscene_slots[i].active = 1;
            s_cutscene_slots[i].cid = cid;
            s_cutscene_slots[i].ch = ch;
            s_cutscene_slots[i].last_action = -2;
            return &s_cutscene_slots[i];
        }
    }
    return 0;
}

static void wrap_cutscene_frame(RemoteCutsceneSlot *slot)
{
    float period;

    if (!slot || slot->frame_count <= 1.0f)
    {
        if (slot)
            slot->frame = 0.0f;
        return;
    }
    period = slot->frame_count;
    while (slot->frame >= period)
        slot->frame -= period;
    while (slot->frame < 0.0f)
        slot->frame += period;
}

static void remote_cutscene_model_update(void *task, void *model)
{
    RemoteCutsceneSlot *slot = find_cutscene_slot_by_task(task);

    if (!slot || model != slot->model)
        return;
    slot->frame += slot->frame_step;
    wrap_cutscene_frame(slot);
    write_float_at(model, 0x28, slot->frame);
}

static int create_cutscene_model(RemoteCutsceneSlot *slot,
                                 const RemotePlayer *remote)
{
    CutsceneModelSpec spec;

    if (!slot || !remote || !s_cutscene_ebisumaru_resource_ready ||
        !is_rdram_pointer(s_cutscene_owner_task) ||
        !get_cutscene_model_spec(remote->ch, &spec) ||
        !resource_is_loaded(spec.file_id))
        return 0;

    /* Use the stable task helper to reproduce the first step of the file_18
     * Ebisumaru creator without calling overlay-local code at reused VRAM. */
    slot->task = func_80034E08_35A08(s_cutscene_owner_task,
                                     remote_cutscene_model_update, 0);
    if (!slot->task)
        return 0;
    /* Use the shared model allocator with the exact decompiled cutscene model,
     * context, base transform, scale, and resource id for this character. */
    slot->model = func_8000DBF0_E7F0(
        slot->task, spec.model_ptr, spec.animation_context,
        (float)remote->x, (float)remote->y, (float)remote->z,
        spec.base_rot_x, spec.base_rot_y, spec.base_rot_z,
        CUTSCENE_MODEL_SCALE, CUTSCENE_MODEL_SCALE, CUTSCENE_MODEL_SCALE,
        spec.file_id, 0);
    if (!slot->model)
    {
        /* Use normal task cleanup because the model allocator failed after the
         * child task was already linked into the live owner tree. */
        func_80034EF8_35AF8(slot->task);
        slot->task = 0;
        return 0;
    }
    /* Resolve the cutscene model's own clip length so received playable-player
     * phase is mapped into this different, fixed cutscene animation. */
    slot->frame_count = func_8001B5AC_1C1AC(slot->model);
    slot->frame_step = spec.default_frame_step;
    write_u8_at(slot->model, 0x65, 0);
    return 1;
}

static short yaw_from_velocity(int vx, int vz, short fallback)
{
    int avx;
    int avz;

    if (vx * vx + vz * vz < REMOTE_YAW_SPEED_THRESHOLD_SQ)
        return fallback;
    avx = vx < 0 ? -vx : vx;
    avz = vz < 0 ? -vz : vz;
    if (avx > avz * 2)
        return vx >= 0 ? 0x4000 : (short)0xc000;
    if (avz > avx * 2)
        return vz >= 0 ? (short)0x8000 : 0;
    if (vx >= 0 && vz >= 0)
        return 0x6000;
    if (vx < 0 && vz >= 0)
        return (short)0xa000;
    if (vx < 0 && vz < 0)
        return (short)0xe000;
    return 0x2000;
}

static float map_remote_animation_frame(const RemotePlayer *remote,
                                        float remote_model_frame_count)
{
    int source_frame;
    int source_period;
    float destination_period;

    if (!remote || remote_model_frame_count <= 1.0f)
        return 0.0f;
    destination_period = remote_model_frame_count;
    source_period = remote->anim_frame_count_100;
    if (source_period <= 100)
    {
        source_frame = remote->anim_frame_100;
        if (source_frame < 0)
            source_frame = 0;
        return (float)source_frame / 100.0f;
    }
    source_frame = remote->anim_frame_100;
    while (source_frame < 0)
        source_frame += source_period;
    while (source_frame >= source_period)
        source_frame -= source_period;
    return ((float)source_frame / (float)source_period) * destination_period;
}

static void sync_cutscene_animation(RemoteCutsceneSlot *slot,
                                    const RemotePlayer *remote)
{
    int delta;
    int period;
    float destination_period;

    if (!slot || !remote || !slot->model || remote->seq == slot->last_seq)
        return;
    slot->frame = map_remote_animation_frame(remote, slot->frame_count);
    slot->frame_step = 0.0f;
    if (slot->last_seq != 0 && remote->action == slot->last_action &&
        remote->anim_frame_count_100 > 100 &&
        remote->anim_frame_count_100 == slot->last_remote_frame_count_100 &&
        slot->frame_count > 1.0f)
    {
        period = remote->anim_frame_count_100;
        delta = remote->anim_frame_100 - slot->last_remote_frame_100;
        if (delta < -(period / 2))
            delta += period;
        else if (delta > period / 2)
            delta -= period;
        destination_period = slot->frame_count;
        slot->frame_step =
            ((float)delta / (float)period) * destination_period /
            REMOTE_PACKET_FRAME_INTERVAL;
    }
    slot->last_seq = remote->seq;
    slot->last_action = remote->action;
    slot->last_remote_frame_100 = remote->anim_frame_100;
    slot->last_remote_frame_count_100 = remote->anim_frame_count_100;
    wrap_cutscene_frame(slot);
    write_float_at(slot->model, 0x28, slot->frame);
}

static void update_cutscene_transform(RemoteCutsceneSlot *slot,
                                      const RemotePlayer *remote)
{
    CutsceneModelSpec spec;
    short rot_x;
    short rot_y;
    short rot_z;

    if (!slot || !slot->model || !remote ||
        !get_cutscene_model_spec(slot->ch, &spec))
        return;
    if (remote->action >= 0)
    {
        rot_x = (short)(spec.base_rot_x + (short)remote->rot_x);
        rot_y = (short)(spec.base_rot_y + (short)remote->rot_y);
        rot_z = (short)(spec.base_rot_z + (short)remote->rot_z);
        slot->fallback_yaw = (short)remote->rot_y;
    }
    else
    {
        slot->fallback_yaw = yaw_from_velocity(remote->vx, remote->vz,
                                                slot->fallback_yaw);
        rot_x = spec.base_rot_x;
        rot_y = (short)(spec.base_rot_y + slot->fallback_yaw);
        rot_z = spec.base_rot_z;
    }
    write_float_at(slot->model, 0x08, (float)remote->x);
    write_float_at(slot->model, 0x0c, (float)remote->y);
    write_float_at(slot->model, 0x10, (float)remote->z);
    write_u16_at(slot->model, 0x14, (unsigned short)rot_x);
    write_u16_at(slot->model, 0x16, (unsigned short)rot_y);
    write_u16_at(slot->model, 0x18, (unsigned short)rot_z);
    write_float_at(slot->model, 0x1c, CUTSCENE_MODEL_SCALE);
    write_float_at(slot->model, 0x20, CUTSCENE_MODEL_SCALE);
    write_float_at(slot->model, 0x24, CUTSCENE_MODEL_SCALE);
    write_u8_at(slot->model, 0x65, 0);
}

static int ensure_remote_cutscene_model(const RemotePlayer *remote)
{
    RemoteCutsceneSlot *slot;
    CutsceneModelSpec spec;

    if (!remote || !get_cutscene_model_spec(remote->ch, &spec))
        return 0;
    slot = find_cutscene_slot_by_cid(remote->cid);
    if (slot && slot->ch != remote->ch)
    {
        clear_cutscene_slot(slot, 1);
        slot = 0;
    }
    if (!slot)
        slot = allocate_cutscene_slot(remote->cid, remote->ch);
    if (!slot)
        return 0;
    slot->seen = 1;
    if ((!slot->task || !slot->model) && !create_cutscene_model(slot, remote))
    {
        clear_cutscene_slot(slot, 0);
        return 0;
    }
    sync_cutscene_animation(slot, remote);
    update_cutscene_transform(slot, remote);
    return 1;
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
    /* Use the live engine player task as the owner of the remote child tasks;
     * this makes normal room teardown own their lifetime. */
    void *owner_task = D_801FC604_5B8514;
    int remote_index;
    int slot_index = 0;
    int visible_nameplates = 0;
    int i;

    if (!is_rdram_pointer(local_obj) || !is_rdram_pointer(owner_task))
    {
        reset_cutscene_slots(0);
        s_cutscene_owner_task = 0;
        for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
            anchor_nameplates_hide_slot(i);
        anchor_nameplates_set_context_visible(0);
        return;
    }
    if (s_cutscene_owner_task != owner_task)
    {
        reset_cutscene_slots(0);
        s_cutscene_owner_task = owner_task;
    }
    if (!anchor_is_connected())
    {
        s_remote_count = 0;
        clear_remote_smoothing();
        reset_cutscene_slots(1);
        for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
            anchor_nameplates_hide_slot(i);
        anchor_nameplates_set_context_visible(0);
        return;
    }
    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        s_cutscene_slots[i].seen = 0;

    for (remote_index = 0;
         remote_index < s_remote_count && slot_index < ANCHOR_REMOTE_MAX;
         ++remote_index)
    {
        RemotePlayer *remote = &s_remote_players[remote_index];
        RemotePlayer smoothed_remote;
        RemoteCutsceneSlot *old_slot;

        /* Compare against the engine room global so off-room model tasks are
         * destroyed rather than merely hidden. */
        if (!remote->has_pos || remote->room != (int)D_800C7AB2)
            continue;
        smooth_remote_player(remote, &smoothed_remote);
        if (remote->ch == CHARACTER_EBISUMARU)
        {
            ensure_remote_cutscene_model(&smoothed_remote);
        }
        else
        {
            old_slot = find_cutscene_slot_by_cid(remote->cid);
            if (old_slot)
                clear_cutscene_slot(old_slot, 1);
        }
        visible_nameplates +=
            render_remote_nameplate(slot_index, &smoothed_remote, local_obj);
        slot_index++;
    }
    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
        if (s_cutscene_slots[i].active && !s_cutscene_slots[i].seen)
            clear_cutscene_slot(&s_cutscene_slots[i], 1);
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
