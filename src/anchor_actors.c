/**
 * @file anchor_actors.c
 * @brief Anchor remote-player state publishing and particle presence markers.
 *
 * Remote characters are represented as short-lived effect bursts instead of
 * player models. This avoids the fragile player constructor and lets the
 * engine's own effect task update/draw path handle the visual work.
 */

#include "modding.h"
#include "anchor.h"
#include "anchor_nameplates.h"

#define ANCHOR_REMOTE_MAX 25
#define POSITION_SEND_FRAMES 4
#define POSITION_KEEPALIVE_FRAMES 30
#define POSITION_MIN_DELTA_SQ 36
#define LOBBY_REFRESH_FRAMES 3
#define PARTICLE_COUNT 3
#define REMOTE_SNAP_DISTANCE_SQ 250000.0f
#define REMOTE_SMOOTHING_FACTOR 0.35f
#define REMOTE_VELOCITY_LEAD_SECONDS 0.04f

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

/* Game room/scene id. Actor manager code reads this while deciding which
 * room actors to spawn, so the mod uses it as the local room filter for
 * remote player markers and nameplates. */
extern unsigned short D_800C7AB2;

/* Current actor/task owner. Ghidra xrefs show this pointer is used by the
 * engine's actor and effect update code as the active parent task. Marker
 * tasks must be recreated when it changes because their parent link becomes
 * stale across scene transitions. */
extern void *D_801FC604_5B8514;

/* Current player background/world object. The first fields match CLS_BG_W:
 * bytes 0x08, 0x0c, and 0x10 are world x/y/z floats. */
extern PlayerObject *D_801FC60C_5B851C;

/* Camera offset from the player and its horizontal radius. Camera routines
 * update these each frame; the nameplate projection uses them to derive a
 * cheap forward/right basis without calling the game's renderer. */
extern Vec3f D_8020D1C0_5C90D0;
extern float D_8020D1D0_5C90E0;

/* Allocate and insert an engine task under `task_list`. Decompilation shows
 * this wraps func_80034B58 (take a task from the free list, set callback and
 * flags) and func_80034D24 (reorder by priority/depth). */
extern void *func_80034E08_35A08(void *task_list, void (*update)(void *, void *), unsigned short flags);

/* Allocate `count` particle/effect records of `kind` and append them to
 * task+0x18/task+0x1c's linked list. Returns the first record, or NULL if the
 * global pool for that kind does not have enough free entries. */
extern void *func_80035EEC_36AEC(void *task, short kind, unsigned int count);

/* Initialize the common effect-task header from its parent task. The game
 * copies parent state at +0x5c, stores the parent at +0x84, clears effect
 * state bytes 0x60..0x68, and records the effect type at +0x64. */
extern void func_801EE4AC_5AA3BC(void *task, void *parent_task, unsigned char effect_type);

/* Initialize one particle's draw/config data. The decompiled body resets the
 * particle, then writes particle+0x30 with a render-mode entry selected by
 * `type` ORed with `flags`; this mod passes particle+0x80 as the display-list
 * address through `flags`. */
extern void func_801EE750_5AA660(void *particle, void *config, int type, unsigned int flags);

/* Initialize the small per-particle state block used by the effect updater.
 * The game writes a default state vtable/config pointer, zeroes the state
 * byte, stores -1 in the state halfword, then calls the common reset helper. */
extern void func_801EF684_5AB594(void *particle, void *state);

/* Build a tiny display list:
 *   gSPDisplayList(texture), optional gDPSetPrimColor(...),
 *   gDPSetEnvColor(...), gSPEndDisplayList().
 * The helper writes the display list back to cache before returning it. */
extern void func_801DC554_598464(int set_prim, void *dl, void *texture, int prim_r, unsigned int prim_g, unsigned int prim_b, int env_r, unsigned int env_g, unsigned int env_b, unsigned int alpha);

/* Stock particle assets used by the engine. D_80204DA0 is the sparkle/effect
 * config passed into particle initialization; D_802049C0 is the texture
 * display list chained into the generated marker display list. */
extern unsigned char D_80204DA0_5C0CB0[];
extern unsigned char D_802049C0_5C08D0[];

#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static RemoteSmoothing s_remote_smoothing[ANCHOR_REMOTE_MAX];
static int s_remote_count;
static int s_state_send_timer;
static int s_position_keepalive_timer;
static int s_have_sent_position;
static int s_last_sent_x;
static int s_last_sent_y;
static int s_last_sent_z;
static unsigned int s_last_sent_char = 0xffffffffu;
static unsigned int s_last_sent_room = 0xffffffffu;
static int s_lobby_refresh_timer;
static void *s_marker_tasks[ANCHOR_REMOTE_MAX];
static void *s_marker_particles[ANCHOR_REMOTE_MAX];
static void *s_marker_owner_task;

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
    unsigned char *p = (unsigned char *)obj;

    *(unsigned char *)(p + offset) = value;
}

static void write_float_at(void *obj, unsigned int offset, float value)
{
    unsigned char *p = (unsigned char *)obj;

    *(float *)(p + offset) = value;
}

static void *read_ptr_at(void *obj, unsigned int offset)
{
    unsigned char *p = (unsigned char *)obj;

    return *(void **)(p + offset);
}

static unsigned int ptr_low32(void *ptr)
{
    return (unsigned int)(unsigned long)ptr;
}

static unsigned char read_u8_at(void *obj, unsigned int offset)
{
    unsigned char *p = (unsigned char *)obj;

    return *(unsigned char *)(p + offset);
}

static void set_particle_visual(void *particle, unsigned char alpha, float scale, const unsigned char *color)
{
    write_float_at(particle, 0x1c, scale);
    write_float_at(particle, 0x20, scale);
    write_float_at(particle, 0x24, scale);

    func_801DC554_598464(0, (unsigned char *)particle + 0x80, D_802049C0_5C08D0,
                          0, 0, 0,
                          color[0], color[1], color[2], alpha);
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
        p = sfind(p, "{");
        if (!p)
            break;

        s_remote_players[count].cid = parse_int_after(p, "\"cid\"", 0);
        s_remote_players[count].room = parse_int_after(p, "\"room\"", -1);
        s_remote_players[count].x = parse_int_after(p, "\"x\"", 0);
        s_remote_players[count].y = parse_int_after(p, "\"y\"", 0);
        s_remote_players[count].z = parse_int_after(p, "\"z\"", 0);
        s_remote_players[count].has_pos = parse_int_after(p, "\"hp\"", 0);
        s_remote_players[count].ch = parse_int_after(p, "\"ch\"", 0) & 3;
        s_remote_players[count].vx = parse_int_after(p, "\"vx\"", 0);
        s_remote_players[count].vy = parse_int_after(p, "\"vy\"", 0);
        s_remote_players[count].vz = parse_int_after(p, "\"vz\"", 0);
        s_remote_players[count].seq = parse_int_after(p, "\"s\"", 0);
        s_remote_players[count].same_team = parse_int_after(p, "\"tm\"", 1);
        parse_string_after(p, "\"n\"", s_remote_players[count].name, (int)sizeof(s_remote_players[count].name));
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

    if (smooth->room != remote->room || smooth->seq == 0 || dist_sq > REMOTE_SNAP_DISTANCE_SQ)
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
    int should_send_position = 0;

    if (!anchor_is_connected())
    {
        s_state_send_timer = 0;
        s_position_keepalive_timer = 0;
        s_have_sent_position = 0;
        s_last_sent_char = 0xffffffffu;
        s_last_sent_room = 0xffffffffu;
        return;
    }

    room = (unsigned int)D_800C7AB2;
    if (room != s_last_sent_room)
    {
        s_last_sent_room = room;
        s_have_sent_position = 0;
        s_position_keepalive_timer = 0;
    }
    anchor_set_local_room(room);

    char_idx = *CURRENT_CHAR_PTR & 3;
    anchor_set_character(s_char_names[char_idx]);
    s_last_sent_char = char_idx;

    if (s_state_send_timer > 0)
        s_state_send_timer--;
    if (s_position_keepalive_timer > 0)
        s_position_keepalive_timer--;

    if (!local_obj || s_state_send_timer > 0)
        return;

    x = (int)local_obj->x;
    y = (int)local_obj->y;
    z = (int)local_obj->z;

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

    if (!should_send_position)
        return;

    if (anchor_set_position(x, y, z))
    {
        s_have_sent_position = 1;
        s_last_sent_x = x;
        s_last_sent_y = y;
        s_last_sent_z = z;
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

static void move_particle_chain(void *first_particle, const RemotePlayer *remote)
{
    static const float x_offsets[PARTICLE_COUNT] = {0.0f, 5.0f, -5.0f};
    static const float y_offsets[PARTICLE_COUNT] = {10.0f, 16.0f, 22.0f};
    static const float z_offsets[PARTICLE_COUNT] = {0.0f, -3.0f, 3.0f};
    void *particle = first_particle;
    int i;

    for (i = 0; particle && i < PARTICLE_COUNT; ++i)
    {
        write_float_at(particle, 0x08, (float)remote->x + x_offsets[i]);
        write_float_at(particle, 0x0c, (float)remote->y + y_offsets[i]);
        write_float_at(particle, 0x10, (float)remote->z + z_offsets[i]);
        particle = read_ptr_at(particle, 0x00);
    }
}

static void init_remote_particle(void *effect_task, void *particle, const RemotePlayer *remote, int particle_index, int slot_index)
{
    static const unsigned char colors[5][3] = {
        {255, 224, 64},
        {64, 192, 255},
        {255, 96, 192},
        {128, 255, 128},
        {255, 48, 48}};
    unsigned char *task_bytes = (unsigned char *)effect_task;
    unsigned char *particle_bytes = (unsigned char *)particle;
    void *state = task_bytes + 0xa0 + particle_index * 8;
    const unsigned char *color = colors[remote && !remote->same_team ? 4 : (slot_index & 3)];

    func_801EE750_5AA660(particle, D_80204DA0_5C0CB0, 2, ptr_low32(particle_bytes + 0x80));
    func_801EF684_5AB594(particle, state);

    write_u8_at(effect_task, 0x9c + particle_index, 0xe8);
    set_particle_visual(particle, 0xe8, 0.18f, color);
}

static void remote_sparkle_update(void *effect_task, void *first_particle)
{
    static const unsigned char colors[5][3] = {
        {255, 224, 64},
        {64, 192, 255},
        {255, 96, 192},
        {128, 255, 128},
        {255, 48, 48}};
    void *particle = first_particle;
    unsigned char phase;
    unsigned char slot_color;
    int i;

    if (!first_particle)
    {
        write_u8_at(effect_task, 0x65, 1);
        return;
    }

    phase = read_u8_at(effect_task, 0x60) + 1;
    slot_color = read_u8_at(effect_task, 0x62);
    if (slot_color > 4)
        slot_color = slot_color & 3;
    write_u8_at(effect_task, 0x60, phase);

    for (i = 0; particle && i < PARTICLE_COUNT; ++i)
    {
        unsigned char alpha = 0;
        float scale;

        if (read_u8_at(effect_task, 0x61))
        {
            unsigned char pulse = (phase + (unsigned char)(i * 9)) & 0x1f;

            if (pulse > 0x10)
                pulse = 0x20 - pulse;
            alpha = 0x78 + pulse * 7;
        }

        write_u8_at(effect_task, 0x9c + i, alpha);
        scale = 0.07f + ((float)alpha * (1.0f / 255.0f)) * 0.09f;
        set_particle_visual(particle, alpha, scale, colors[slot_color]);

        particle = read_ptr_at(particle, 0x00);
    }
}

static void init_remote_particle_chain(void *effect_task, void *first_particle, const RemotePlayer *remote, int slot_index)
{
    void *particle = first_particle;
    int i;

    for (i = 0; particle && i < PARTICLE_COUNT; ++i)
    {
        init_remote_particle(effect_task, particle, remote, i, slot_index);
        particle = read_ptr_at(particle, 0x00);
    }

    move_particle_chain(first_particle, remote);
}

static void clear_marker_slots_for_new_owner(void)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        s_marker_tasks[i] = 0;
        s_marker_particles[i] = 0;
    }
    s_marker_owner_task = D_801FC604_5B8514;
}

static void set_marker_slot_visible(int slot_index, int visible)
{
    void *effect_task = s_marker_tasks[slot_index];
    void *particle = s_marker_particles[slot_index];
    int i;

    if (!effect_task)
        return;

    write_u8_at(effect_task, 0x61, visible ? 1 : 0);
    if (visible || !particle)
        return;

    for (i = 0; particle && i < PARTICLE_COUNT; ++i)
    {
        static const unsigned char white[3] = {255, 255, 255};

        write_u8_at(effect_task, 0x9c + i, 0);
        set_particle_visual(particle, 0, 0.01f, white);
        particle = read_ptr_at(particle, 0x00);
    }
}

static int ensure_remote_particle_marker(const RemotePlayer *remote, int slot_index)
{
    void *effect_task;
    void *particles;

    if (!remote || !D_801FC604_5B8514)
        return 0;

    if (s_marker_owner_task != D_801FC604_5B8514)
        clear_marker_slots_for_new_owner();

    if (s_marker_tasks[slot_index] && s_marker_particles[slot_index])
    {
        write_u8_at(s_marker_tasks[slot_index], 0x61, 1);
        write_u8_at(s_marker_tasks[slot_index], 0x62, (unsigned char)(remote->same_team ? (slot_index & 3) : 4));
        move_particle_chain(s_marker_particles[slot_index], remote);
        return 1;
    }

    effect_task = func_80034E08_35A08(D_801FC604_5B8514, remote_sparkle_update, 0);
    if (!effect_task)
        return 0;

    func_801EE4AC_5AA3BC(effect_task, D_801FC604_5B8514, 0);
    write_u8_at(effect_task, 0x60, 0);
    write_u8_at(effect_task, 0x61, 1);
    write_u8_at(effect_task, 0x62, (unsigned char)(remote->same_team ? (slot_index & 3) : 4));
    particles = func_80035EEC_36AEC(effect_task, 2, PARTICLE_COUNT);
    if (!particles)
    {
        write_u8_at(effect_task, 0x65, 1);
        return 0;
    }

    s_marker_tasks[slot_index] = effect_task;
    s_marker_particles[slot_index] = particles;
    init_remote_particle_chain(effect_task, particles, remote, slot_index);
    return 1;
}

static int render_remote_nameplate(int slot_index, const RemotePlayer *remote, const PlayerObject *local_obj)
{
    AnchorNameplateCamera camera;
    AnchorNameplatePlayer nameplate_remote;

    if (!local_obj || !remote)
    {
        anchor_nameplates_hide_slot(slot_index);
        return 0;
    }

    camera.player_x = local_obj->x;
    camera.player_y = local_obj->y;
    camera.player_z = local_obj->z;
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

static void update_remote_particle_markers(PlayerObject *local_obj)
{
    int remote_index;
    int slot_index = 0;
    int visible_nameplates = 0;
    int hide_index;

    if (s_marker_owner_task != D_801FC604_5B8514)
        clear_marker_slots_for_new_owner();

    if (!anchor_is_connected())
    {
        s_remote_count = 0;
        clear_remote_smoothing();
        for (hide_index = 0; hide_index < ANCHOR_REMOTE_MAX; ++hide_index)
        {
            set_marker_slot_visible(hide_index, 0);
            anchor_nameplates_hide_slot(hide_index);
        }
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
        ensure_remote_particle_marker(&smoothed_remote, slot_index);
        visible_nameplates += render_remote_nameplate(slot_index, &smoothed_remote, local_obj);
        slot_index++;
    }

    for (hide_index = slot_index; hide_index < ANCHOR_REMOTE_MAX; ++hide_index)
    {
        set_marker_slot_visible(hide_index, 0);
        anchor_nameplates_hide_slot(hide_index);
    }

    anchor_nameplates_set_context_visible(visible_nameplates > 0);
}

/* Frame-end hook on the main gameplay/update tick. Anchor publishes the local
 * player state, refreshes remote lobby positions, and updates visible marker
 * particles/nameplates after the game has advanced the current frame. */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update_particle_markers(void)
{
    PlayerObject *local_obj = D_801FC60C_5B851C;

    publish_local_state(local_obj);
    refresh_lobby();
    update_remote_particle_markers(local_obj);
}
