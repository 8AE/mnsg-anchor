/**
 * @file anchor_actors.c
 * @brief Anchor remote-player state publishing and particle presence markers.
 *
 * Remote characters are represented as short-lived effect bursts instead of
 * player models. This avoids the fragile player constructor and lets the
 * engine's own effect task update/draw path handle the visual work.
 */

#include "modding.h"
#include "recompui.h"
#include "anchor.h"

#define ANCHOR_REMOTE_MAX 8
#define STATE_SEND_FRAMES 6
#define LOBBY_REFRESH_FRAMES 3
#define PARTICLE_COUNT 3

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
    char name[32];
} RemotePlayer;

extern unsigned short D_800C7AB2;

extern void *D_801FC604_5B8514;
extern PlayerObject *D_801FC60C_5B851C;
extern Vec3f D_8020D1C0_5C90D0;
extern float D_8020D1D0_5C90E0;

extern void *func_80034E08_35A08(void *task_list, void (*update)(void *, void *), unsigned short flags);
extern void *func_80035EEC_36AEC(void *task, short kind, unsigned int count);
extern void func_801EE4AC_5AA3BC(void *task, void *parent_task, unsigned char effect_type);
extern void func_801EE750_5AA660(void *particle, void *config, int type, unsigned int flags);
extern void func_801EF684_5AB594(void *particle, void *state);
extern void func_801DC554_598464(int set_prim, void *dl, void *texture, int prim_r, unsigned int prim_g, unsigned int prim_b, int env_r, unsigned int env_g, unsigned int env_b, unsigned int alpha);

extern unsigned char D_80204DA0_5C0CB0[];
extern unsigned char D_802049C0_5C08D0[];

#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static int s_remote_count;
static int s_state_send_timer;
static int s_lobby_refresh_timer;
static void *s_marker_tasks[ANCHOR_REMOTE_MAX];
static void *s_marker_particles[ANCHOR_REMOTE_MAX];
static void *s_marker_owner_task;
static RecompuiContext s_nameplate_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_nameplate_labels[ANCHOR_REMOTE_MAX];
static int s_nameplate_initialized;
static int s_nameplate_ctx_visible;

static const char *const s_char_names[4] = {
    "Goemon", "Ebisumaru", "Sasuke", "Yae"};

static const RecompuiColor NAMEPLATE_TEXT = {255, 255, 255, 235};
static const RecompuiColor NAMEPLATE_BG = {0, 0, 0, 150};

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

static void nameplates_ensure_init(void)
{
    int i;
    RecompuiResource root;

    if (s_nameplate_initialized)
        return;
    s_nameplate_initialized = 1;

    s_nameplate_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_nameplate_ctx, 0);
    recompui_set_context_captures_mouse(s_nameplate_ctx, 0);
    recompui_open_context(s_nameplate_ctx);

    root = recompui_context_root(s_nameplate_ctx);
    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        s_nameplate_labels[i] = recompui_create_label(s_nameplate_ctx, root, "", LABELSTYLE_ANNOTATION);
        recompui_set_position(s_nameplate_labels[i], POSITION_ABSOLUTE);
        recompui_set_width(s_nameplate_labels[i], 180.0f, UNIT_DP);
        recompui_set_margin_left(s_nameplate_labels[i], -90.0f, UNIT_DP);
        recompui_set_text_align(s_nameplate_labels[i], TEXT_ALIGN_CENTER);
        recompui_set_font_size(s_nameplate_labels[i], 20.0f, UNIT_DP);
        recompui_set_font_weight(s_nameplate_labels[i], 700);
        recompui_set_color(s_nameplate_labels[i], &NAMEPLATE_TEXT);
        recompui_set_background_color(s_nameplate_labels[i], &NAMEPLATE_BG);
        recompui_set_border_radius(s_nameplate_labels[i], 4.0f, UNIT_DP);
        recompui_set_padding(s_nameplate_labels[i], 3.0f, UNIT_DP);
        recompui_set_display(s_nameplate_labels[i], DISPLAY_NONE);
    }

    recompui_close_context(s_nameplate_ctx);
}

static float clamp_dp(float value, float lo, float hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static void hide_nameplate_slot(int slot_index)
{
    nameplates_ensure_init();
    if (s_nameplate_labels[slot_index] != RECOMPUI_NULL_RESOURCE)
    {
        recompui_open_context(s_nameplate_ctx);
        recompui_set_display(s_nameplate_labels[slot_index], DISPLAY_NONE);
        recompui_close_context(s_nameplate_ctx);
    }
}

static void set_nameplates_context_visible(int visible)
{
    nameplates_ensure_init();
    if (visible)
    {
        if (!s_nameplate_ctx_visible)
        {
            recompui_show_context(s_nameplate_ctx);
            s_nameplate_ctx_visible = 1;
        }
    }
    else if (s_nameplate_ctx_visible)
    {
        recompui_hide_context(s_nameplate_ctx);
        s_nameplate_ctx_visible = 0;
    }
}

static void update_nameplate_slot(int slot_index, const RemotePlayer *remote, const PlayerObject *local_obj)
{
    const float half_width = 960.0f;
    const float half_height = 540.0f;
    const float focal_x = 900.0f;
    const float focal_y = 620.0f;
    const float label_height = 55.0f;
    float eye_x;
    float eye_y;
    float eye_z;
    float radius;
    float forward_x;
    float forward_z;
    float right_x;
    float right_z;
    float rel_x;
    float rel_y;
    float rel_z;
    float depth;
    float side;
    float screen_x;
    float screen_y;

    nameplates_ensure_init();
    if (!remote || !local_obj || s_nameplate_labels[slot_index] == RECOMPUI_NULL_RESOURCE)
    {
        hide_nameplate_slot(slot_index);
        return;
    }

    radius = D_8020D1D0_5C90E0;
    if (radius < 1.0f)
        radius = 1.0f;

    eye_x = local_obj->x + D_8020D1C0_5C90D0.x;
    eye_y = local_obj->y + D_8020D1C0_5C90D0.y;
    eye_z = local_obj->z + D_8020D1C0_5C90D0.z;

    forward_x = -D_8020D1C0_5C90D0.x / radius;
    forward_z = -D_8020D1C0_5C90D0.z / radius;
    right_x = -forward_z;
    right_z = forward_x;

    rel_x = (float)remote->x - eye_x;
    rel_y = ((float)remote->y + label_height) - eye_y;
    rel_z = (float)remote->z - eye_z;
    depth = rel_x * forward_x + rel_z * forward_z;

    if (depth < 20.0f)
    {
        hide_nameplate_slot(slot_index);
        return;
    }

    side = rel_x * right_x + rel_z * right_z;
    screen_x = clamp_dp(half_width + (side / depth) * focal_x, 80.0f, 1840.0f);
    screen_y = clamp_dp(half_height - (rel_y / depth) * focal_y, 60.0f, 980.0f);

    recompui_open_context(s_nameplate_ctx);
    recompui_set_text(s_nameplate_labels[slot_index], remote->name);
    recompui_set_left(s_nameplate_labels[slot_index], screen_x, UNIT_DP);
    recompui_set_top(s_nameplate_labels[slot_index], screen_y, UNIT_DP);
    recompui_set_display(s_nameplate_labels[slot_index], DISPLAY_BLOCK);
    recompui_close_context(s_nameplate_ctx);
}

static void publish_local_state(PlayerObject *local_obj)
{
    unsigned int char_idx;

    if (s_state_send_timer > 0)
    {
        s_state_send_timer--;
        return;
    }
    s_state_send_timer = STATE_SEND_FRAMES;

    anchor_set_local_room((unsigned int)D_800C7AB2);
    if (local_obj)
        anchor_set_position((int)local_obj->x, (int)local_obj->y, (int)local_obj->z);

    char_idx = *CURRENT_CHAR_PTR & 3;
    anchor_set_character(s_char_names[char_idx]);
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
    static const unsigned char colors[4][3] = {
        {255, 224, 64},
        {64, 192, 255},
        {255, 96, 192},
        {128, 255, 128}};
    unsigned char *task_bytes = (unsigned char *)effect_task;
    unsigned char *particle_bytes = (unsigned char *)particle;
    void *state = task_bytes + 0xa0 + particle_index * 8;
    const unsigned char *color = colors[slot_index & 3];

    func_801EE750_5AA660(particle, D_80204DA0_5C0CB0, 2, ptr_low32(particle_bytes + 0x80));
    func_801EF684_5AB594(particle, state);

    write_u8_at(effect_task, 0x9c + particle_index, 0xe8);
    set_particle_visual(particle, 0xe8, 0.18f, color);
}

static void remote_sparkle_update(void *effect_task, void *first_particle)
{
    static const unsigned char colors[4][3] = {
        {255, 224, 64},
        {64, 192, 255},
        {255, 96, 192},
        {128, 255, 128}};
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
    slot_color = read_u8_at(effect_task, 0x62) & 3;
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
        write_u8_at(s_marker_tasks[slot_index], 0x62, (unsigned char)(slot_index & 3));
        move_particle_chain(s_marker_particles[slot_index], remote);
        return 1;
    }

    effect_task = func_80034E08_35A08(D_801FC604_5B8514, remote_sparkle_update, 0);
    if (!effect_task)
        return 0;

    func_801EE4AC_5AA3BC(effect_task, D_801FC604_5B8514, 0);
    write_u8_at(effect_task, 0x60, 0);
    write_u8_at(effect_task, 0x61, 1);
    write_u8_at(effect_task, 0x62, (unsigned char)(slot_index & 3));
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

static void update_remote_particle_markers(PlayerObject *local_obj)
{
    int remote_index;
    int slot_index = 0;
    int hide_index;

    if (s_marker_owner_task != D_801FC604_5B8514)
        clear_marker_slots_for_new_owner();

    if (!anchor_is_connected())
    {
        s_remote_count = 0;
        for (hide_index = 0; hide_index < ANCHOR_REMOTE_MAX; ++hide_index)
        {
            set_marker_slot_visible(hide_index, 0);
            hide_nameplate_slot(hide_index);
        }
        set_nameplates_context_visible(0);
        return;
    }

    for (remote_index = 0;
         remote_index < s_remote_count && slot_index < ANCHOR_REMOTE_MAX;
         ++remote_index)
    {
        RemotePlayer *remote = &s_remote_players[remote_index];

        if (!remote->has_pos || remote->room != (int)D_800C7AB2)
            continue;

        ensure_remote_particle_marker(remote, slot_index);
        update_nameplate_slot(slot_index, remote, local_obj);
        slot_index++;
    }

    for (hide_index = slot_index; hide_index < ANCHOR_REMOTE_MAX; ++hide_index)
    {
        set_marker_slot_visible(hide_index, 0);
        hide_nameplate_slot(hide_index);
    }

    set_nameplates_context_visible(slot_index > 0);
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update_particle_markers(void)
{
    PlayerObject *local_obj = D_801FC60C_5B851C;

    publish_local_state(local_obj);
    refresh_lobby();
    update_remote_particle_markers(local_obj);
}
