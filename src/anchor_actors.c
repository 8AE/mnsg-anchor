/**
 * @file anchor_actors.c
 * @brief Anchor remote-player display via the game's unused Player 2 slots.
 *
 * The actor-injection and direct render-list approaches can both crash RT64
 * when an overlay/display-list pointer is not exactly what the renderer expects.
 * This module instead mirrors a same-room remote player into the dormant
 * Player 2 object globals discovered in Ghidra, using engine-allocated
 * child objects from the real player task.
 */

#include "modding.h"
#include "anchor.h"

#define PLAYER_OBJECT_SIZE 0x98
#define PLAYER_PAYLOAD_OFFSET 0x14
#define PLAYER_PAYLOAD_END 0x74
#define ANCHOR_REMOTE_MAX 8
#define STATE_SEND_FRAMES 6
#define LOBBY_REFRESH_FRAMES 3

typedef struct PlayerObject
{
    unsigned char header[8];
    float x;
    float y;
    float z;
    unsigned char rest[PLAYER_OBJECT_SIZE - 0x14];
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
extern void *D_801FC608; /* player 2 task */
extern PlayerObject *D_801FC60C_5B851C;
extern PlayerObject *D_801FC610; /* player 2 object */
extern void *D_801FC614_5B8524;
extern void *D_801FC61C_5B852C;

extern int func_80035EEC_36AEC(int task, short kind, unsigned int count);

#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)
#define PLAYER2_PARENT_PTR (*(void **)0x801FC618)
#define PLAYER2_SHADOW_PTR (*(void **)0x801FC620)

static PlayerObject *s_player2_object;
static PlayerObject *s_player2_parent;
static PlayerObject *s_player2_shadow;
static void *s_player2_alloc_task;
static PlayerObject s_character_templates[4];
static unsigned char s_have_character_template[4];
static RemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
static int s_remote_count;
static int s_state_send_timer;
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

static void copy_bytes(void *dst, const void *src, unsigned int size)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    unsigned int i;

    for (i = 0; i < size; ++i)
        d[i] = s[i];
}

static void copy_player_payload(PlayerObject *dst, const PlayerObject *src)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    copy_bytes(d + PLAYER_PAYLOAD_OFFSET,
               s + PLAYER_PAYLOAD_OFFSET,
               PLAYER_PAYLOAD_END - PLAYER_PAYLOAD_OFFSET);
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

static void remember_character_template(PlayerObject *local_obj)
{
    unsigned int char_idx;

    if (!local_obj)
        return;

    char_idx = *CURRENT_CHAR_PTR & 3;
    copy_bytes(&s_character_templates[char_idx], local_obj, PLAYER_OBJECT_SIZE);
    s_have_character_template[char_idx] = 1;
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

static RemotePlayer *first_same_room_remote(void)
{
    int i;

    for (i = 0; i < s_remote_count; ++i)
    {
        if (s_remote_players[i].has_pos &&
            s_remote_players[i].room == (int)D_800C7AB2)
        {
            return &s_remote_players[i];
        }
    }

    return 0;
}

static void clear_player2_slot(void)
{
    D_801FC608 = 0;
    D_801FC610 = 0;
    PLAYER2_PARENT_PTR = 0;
    PLAYER2_SHADOW_PTR = 0;
}

static int ensure_player2_objects(void)
{
    if (!D_801FC604_5B8514)
        return 0;

    if (s_player2_alloc_task != D_801FC604_5B8514)
    {
        s_player2_object = 0;
        s_player2_parent = 0;
        s_player2_shadow = 0;
        s_player2_alloc_task = D_801FC604_5B8514;
    }

    if (!s_player2_object)
        s_player2_object = (PlayerObject *)func_80035EEC_36AEC((int)D_801FC604_5B8514, 2, 1);
    if (!s_player2_parent)
        s_player2_parent = (PlayerObject *)func_80035EEC_36AEC((int)D_801FC604_5B8514, 2, 1);
    if (!s_player2_shadow)
        s_player2_shadow = (PlayerObject *)func_80035EEC_36AEC((int)D_801FC604_5B8514, 2, 1);

    return s_player2_object != 0;
}

static void mirror_remote_into_player2(PlayerObject *local_obj, RemotePlayer *remote)
{
    PlayerObject *src = local_obj;

    if (!local_obj || !remote || !ensure_player2_objects())
    {
        clear_player2_slot();
        return;
    }

    if (remote->ch >= 0 && remote->ch < 4 && s_have_character_template[remote->ch])
        src = &s_character_templates[remote->ch];

    copy_player_payload(s_player2_object, src);
    s_player2_object->x = (float)remote->x;
    s_player2_object->y = (float)remote->y;
    s_player2_object->z = (float)remote->z;

    if (D_801FC614_5B8524 && s_player2_parent)
    {
        copy_player_payload(s_player2_parent, (const PlayerObject *)D_801FC614_5B8524);
        s_player2_parent->x = s_player2_object->x;
        s_player2_parent->y = s_player2_object->y;
        s_player2_parent->z = s_player2_object->z;
    }
    if (D_801FC61C_5B852C && s_player2_shadow)
    {
        copy_player_payload(s_player2_shadow, (const PlayerObject *)D_801FC61C_5B852C);
        s_player2_shadow->x = s_player2_object->x;
        s_player2_shadow->y = s_player2_object->y;
        s_player2_shadow->z = s_player2_object->z;
    }

    D_801FC608 = D_801FC604_5B8514;
    D_801FC610 = s_player2_object;
    PLAYER2_PARENT_PTR = (D_801FC614_5B8524 && s_player2_parent) ? s_player2_parent : 0;
    PLAYER2_SHADOW_PTR = (D_801FC61C_5B852C && s_player2_shadow) ? s_player2_shadow : 0;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update_player2_slot(void)
{
    PlayerObject *local_obj = D_801FC60C_5B851C;
    RemotePlayer *remote;

    if (!anchor_is_connected())
    {
        clear_player2_slot();
        s_remote_count = 0;
        return;
    }

    remember_character_template(local_obj);
    publish_local_state(local_obj);
    refresh_lobby();

    remote = first_same_room_remote();
    mirror_remote_into_player2(local_obj, remote);
}
