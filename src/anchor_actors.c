/**
 * @file anchor_actors.c
 * @brief Same-room Anchor player phantoms for MNSG: Recompiled.
 *
 * The randomizer actor-manager hook clones the current room's actor data,
 * inserts extra actor instances before the terminator, and redirects overlay
 * lookup so the room loader sees the expanded list.  For multiplayer phantoms
 * we can keep the same room-scoped behavior without guessing a fragile actor
 * definition ID: clone the local player's render object into stable slots and
 * link only same-room remote players into the render list each frame.
 */

#include "modding.h"
#include "anchor.h"

typedef struct AnchorCls
{
    struct AnchorCls *next;
    unsigned char kind;
    unsigned char pri;
    unsigned short s_pri;
} AnchorCls;

typedef struct AnchorWave
{
    unsigned short wave_no;
    int data;
} AnchorWave;

typedef struct CLS_BG_W
{
    AnchorCls header;
    float tx;
    float ty;
    float tz;
    short rx;
    short ry;
    short rz;
    unsigned char pad_1a[2];
    float sx;
    float sy;
    float sz;
    float time;
    unsigned int AModel;
    unsigned int init_glist;
    AnchorWave SegWave[6];
    unsigned char unk_64;
    char unk_65;
    unsigned char unk_66;
    unsigned char unk_67;
    int unk_68;
    int unk_6c;
    float unk_70;
    struct CLS_BG_W *left;
    struct CLS_BG_W *right;
    unsigned char unk_7c[2];
    short unk_7e;
    void *unk_80;
    unsigned short unk_84;
    unsigned char unk_86;
    unsigned char unk_87;
    float unk_88;
    float unk_8c;
    float unk_90;
    struct CLS_BG_W *prev;
} CLS_BG_W;

/* Current scene/room ID. */
extern unsigned short D_800C7AB2;

/* Local player CLS_BG_W*. */
extern CLS_BG_W *D_801FC60C_5B851C;

/* Character index written by the character cycler: 0..3. */
#define CURRENT_CHAR_PTR ((volatile unsigned int *)0x8015C5DC)

#define ANCHOR_REMOTE_MAX 8
#define ANCHOR_STATE_SEND_FRAMES 6
#define ANCHOR_LOBBY_REFRESH_FRAMES 3

typedef struct AnchorRemotePlayer
{
    int cid;
    int room;
    int x;
    int y;
    int z;
    int has_pos;
    int character;
} AnchorRemotePlayer;

typedef struct AnchorPhantom
{
    CLS_BG_W bg;
    int active;
    int cid;
} AnchorPhantom;

static AnchorPhantom s_phantoms[ANCHOR_REMOTE_MAX];
static CLS_BG_W s_character_templates[4];
static unsigned char s_have_character_template[4];
static AnchorRemotePlayer s_remote_players[ANCHOR_REMOTE_MAX];
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

static int parse_int_after(const char *obj, const char *key, int fallback)
{
    const char *p = sfind(obj, key);
    int sign = 1;
    int value = 0;
    int saw_digit = 0;

    if (!p)
        return fallback;

    p += 1;
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
        s_remote_players[count].character = parse_int_after(p, "\"ch\"", 0) & 3;
        count++;

        p = sfind(p, "}");
        if (p)
            ++p;
    }

    return count;
}

static int is_phantom_node(AnchorCls *node)
{
    int i;

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        if (node == &s_phantoms[i].bg.header)
            return 1;
    }

    return 0;
}

static void unlink_phantoms_from(CLS_BG_W *anchor)
{
    AnchorCls *prev;
    int guard = 0;

    if (!anchor)
        return;

    prev = &anchor->header;
    while (prev->next && guard++ < 256)
    {
        if (is_phantom_node(prev->next))
        {
            prev->next = prev->next->next;
            continue;
        }
        prev = prev->next;
    }
}

static void clear_phantoms(CLS_BG_W *anchor)
{
    int i;

    unlink_phantoms_from(anchor);
    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        s_phantoms[i].active = 0;
        s_phantoms[i].cid = 0;
        s_phantoms[i].bg.header.next = 0;
    }
}

static void remember_local_character_template(CLS_BG_W *local_bg)
{
    unsigned int char_idx;

    if (!local_bg)
        return;

    char_idx = *CURRENT_CHAR_PTR & 3;
    s_character_templates[char_idx] = *local_bg;
    s_have_character_template[char_idx] = 1;
}

static void publish_local_state(CLS_BG_W *local_bg)
{
    unsigned int char_idx;

    if (s_state_send_timer > 0)
    {
        s_state_send_timer--;
        return;
    }
    s_state_send_timer = ANCHOR_STATE_SEND_FRAMES;

    anchor_set_local_room((unsigned int)D_800C7AB2);
    if (local_bg)
    {
        anchor_set_position((int)local_bg->tx, (int)local_bg->ty, (int)local_bg->tz);
    }

    char_idx = *CURRENT_CHAR_PTR & 3;
    anchor_set_character(s_char_names[char_idx]);
}

static void refresh_remote_players(void)
{
    char *json;

    if (s_lobby_refresh_timer > 0)
    {
        s_lobby_refresh_timer--;
        return;
    }
    s_lobby_refresh_timer = ANCHOR_LOBBY_REFRESH_FRAMES;

    json = anchor_get_lobby_positions_json();
    s_remote_count = parse_lobby_positions(json ? json : "[]");
    if (json)
        recomp_free(json);
}

static void build_phantoms(CLS_BG_W *local_bg)
{
    AnchorCls *chain;
    int i;
    int slot = 0;

    if (!local_bg)
    {
        clear_phantoms(0);
        return;
    }

    unlink_phantoms_from(local_bg);

    for (i = 0; i < ANCHOR_REMOTE_MAX; ++i)
    {
        s_phantoms[i].active = 0;
        s_phantoms[i].cid = 0;
        s_phantoms[i].bg.header.next = 0;
    }

    for (i = 0; i < s_remote_count && slot < ANCHOR_REMOTE_MAX; ++i)
    {
        AnchorRemotePlayer *remote = &s_remote_players[i];
        CLS_BG_W *src = local_bg;
        CLS_BG_W *dst;

        if (!remote->has_pos || remote->room != (int)D_800C7AB2)
            continue;

        if (remote->character >= 0 && remote->character < 4 &&
            s_have_character_template[remote->character])
        {
            src = &s_character_templates[remote->character];
        }

        dst = &s_phantoms[slot].bg;
        *dst = *src;
        dst->tx = (float)remote->x;
        dst->ty = (float)remote->y;
        dst->tz = (float)remote->z;
        dst->left = 0;
        dst->right = 0;
        dst->prev = local_bg;
        dst->header.next = 0;

        s_phantoms[slot].active = 1;
        s_phantoms[slot].cid = remote->cid;
        slot++;
    }

    chain = local_bg->header.next;
    for (i = slot - 1; i >= 0; --i)
    {
        s_phantoms[i].bg.header.next = chain;
        chain = &s_phantoms[i].bg.header;
    }
    local_bg->header.next = chain;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update(void)
{
    CLS_BG_W *local_bg = D_801FC60C_5B851C;

    if (!anchor_is_connected())
    {
        clear_phantoms(local_bg);
        s_remote_count = 0;
        return;
    }

    remember_local_character_template(local_bg);
    publish_local_state(local_bg);
    refresh_remote_players();
    build_phantoms(local_bg);
}
