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

#define ANCHOR_REMOTE_MAX 8
#define STATE_SEND_FRAMES 6
#define LOBBY_REFRESH_FRAMES 3
#define PARTICLE_SPAWN_FRAMES 45
#define PARTICLE_COUNT 3

typedef struct PlayerObject
{
    unsigned char header[8];
    float x;
    float y;
    float z;
} PlayerObject;

typedef struct RemotePlayer
{
    int cid;
    int room;
    int x;
    int y;
    int z;
    int has_pos;
    int ch;
} RemotePlayer;

extern unsigned short D_800C7AB2;

extern void *D_801FC604_5B8514;
extern PlayerObject *D_801FC60C_5B851C;

extern void *func_80034E08_35A08(void *task_list, void (*update)(void *, void *), unsigned short flags);
extern void *func_80035EEC_36AEC(void *task, short kind, unsigned int count);
extern void func_801EE4AC_5AA3BC(void *task, void *parent_task, unsigned char effect_type);
extern void func_801EE750_5AA660(void *particle, void *config, int type, unsigned int flags);
extern void func_801EF684_5AB594(void *particle, void *state);
extern void func_801DC554_598464(int set_prim, void *dl, void *texture, int prim_r, unsigned int prim_g, unsigned int prim_b, int env_r, unsigned int env_g, unsigned int env_b, unsigned int alpha);

extern unsigned char D_80204BF8_5C0B08[];
extern unsigned char D_802049C0_5C08D0[];

#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static int s_remote_count;
static int s_state_send_timer;
static int s_lobby_refresh_timer;
static int s_particle_timers[ANCHOR_REMOTE_MAX];

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
        if (s_remote_players[count].cid > 0)
            count++;

        p = sfind(p, "}");
        if (p)
            ++p;
    }

    return count;
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

    func_801EE750_5AA660(particle, D_80204BF8_5C0B08, 2, ptr_low32(particle_bytes + 0x80));
    func_801EF684_5AB594(particle, state);

    write_float_at(particle, 0x1c, 0.18f);
    write_float_at(particle, 0x20, 0.18f);
    write_float_at(particle, 0x24, 0.18f);
    write_u8_at(effect_task, 0x9c + particle_index, 0xe8);

    func_801DC554_598464(0, particle_bytes + 0x80, D_802049C0_5C08D0,
                          0, 0, 0,
                          color[0], color[1], color[2], 0xff);
}

static void remote_sparkle_update(void *effect_task, void *first_particle)
{
    void *particle = first_particle;
    int done_count = 0;
    int i;

    if (read_u8_at(effect_task, 0x60) >= 8 || !first_particle)
    {
        write_u8_at(effect_task, 0x65, 1);
        return;
    }
    write_u8_at(effect_task, 0x60, read_u8_at(effect_task, 0x60) + 1);

    for (i = 0; particle && i < PARTICLE_COUNT; ++i)
    {
        unsigned char alpha = read_u8_at(effect_task, 0x9c + i);
        unsigned char next_alpha;
        float scale;

        if (alpha <= 0x28)
        {
            next_alpha = 0;
            done_count++;
        }
        else
        {
            next_alpha = alpha - 0x28;
        }

        write_u8_at(effect_task, 0x9c + i, next_alpha);
        scale = 0.04f + ((float)next_alpha * (1.0f / 255.0f)) * 0.18f;
        write_float_at(particle, 0x1c, scale);
        write_float_at(particle, 0x20, scale);
        write_float_at(particle, 0x24, scale);

        func_801DC554_598464(0, (unsigned char *)particle + 0x80, D_802049C0_5C08D0,
                              0, 0, 0,
                              255, 255, 255, next_alpha);

        particle = read_ptr_at(particle, 0x00);
    }

    if (done_count == PARTICLE_COUNT)
        write_u8_at(effect_task, 0x65, 1);
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

static void spawn_remote_particle_marker(const RemotePlayer *remote, int slot_index)
{
    void *effect_task;
    void *particles;

    if (!remote || !D_801FC604_5B8514)
        return;

    effect_task = func_80034E08_35A08(D_801FC604_5B8514, remote_sparkle_update, 0);
    if (!effect_task)
        return;

    func_801EE4AC_5AA3BC(effect_task, D_801FC604_5B8514, 0);
    write_u8_at(effect_task, 0x60, 0);
    particles = func_80035EEC_36AEC(effect_task, 2, PARTICLE_COUNT);
    if (!particles)
    {
        write_u8_at(effect_task, 0x65, 1);
        return;
    }

    init_remote_particle_chain(effect_task, particles, remote, slot_index);
}

static void update_remote_particle_markers(void)
{
    int remote_index;
    int slot_index = 0;

    if (!anchor_is_connected())
    {
        s_remote_count = 0;
        return;
    }

    for (remote_index = 0;
         remote_index < s_remote_count && slot_index < ANCHOR_REMOTE_MAX;
         ++remote_index)
    {
        RemotePlayer *remote = &s_remote_players[remote_index];

        if (!remote->has_pos || remote->room != (int)D_800C7AB2)
            continue;

        if (s_particle_timers[slot_index] > 0)
            s_particle_timers[slot_index]--;
        else
        {
            spawn_remote_particle_marker(remote, slot_index);
            s_particle_timers[slot_index] = PARTICLE_SPAWN_FRAMES;
        }

        slot_index++;
    }
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update_particle_markers(void)
{
    PlayerObject *local_obj = D_801FC60C_5B851C;

    publish_local_state(local_obj);
    refresh_lobby();
    update_remote_particle_markers();
}
