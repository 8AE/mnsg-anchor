/* Remote markers owned by the native dungeon-map and room-minimap tasks.
 * Native recipes: file_17 802155F4/80215198; file_0D 80215CA4/80216228.
 * No map state, visited-room flags or network messages are written here. */
#ifndef ANCHOR_DUNGEON_MAPS_HOST_TEST
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern void *recomp_alloc(unsigned long size);
extern void recomp_free(void *memory);
extern char *anchor_get_lobby_positions_json(void);
extern void *map_test_decode(unsigned int value);
extern unsigned int map_test_encode(const void *value);
#endif
#include "utils/array_utils.h"
#include "utils/json_utils.h"
#include "anchor_dungeon_maps.h"

#define MAP_REFRESH_UPDATES 6
#define MAP_COLORS 16
#define MAP_SHEET_PIXELS (64 * 32)
#define MAP_DOT_SIDE 8
#define MAP_DOT_PIXELS (MAP_DOT_SIDE * MAP_DOT_SIDE)
#define MAP_DOT_ARENA_BYTES (MAP_COLORS * MAP_DOT_PIXELS * 2u)
#define MAP_FLOOR_HEAD_SPACING 18
#define MAP_I32(p, o) (*(int *)((unsigned char *)(p) + (o)))
#define MAP_U16(p, o) (*(unsigned short *)((unsigned char *)(p) + (o)))
#define MAP_BYTE(p, o) (((unsigned char *)(p))[(o)])

extern unsigned char *D_8015C5C8_15D1C8;
extern void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int animation,
    float x, float y, float z, short rx, short ry, short rz,
    float sx, float sy, float sz, short file, short animation_file);
extern void *func_80035EEC_36AEC(void *task, short kind, unsigned int count);
extern void func_800115E0_121E0(void *object, int x, int y);
extern int func_800141C4_14DC4(unsigned int file);
extern int func_80014840_15440(int address, unsigned int file);

typedef struct MapSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} MapSceneResource;
extern MapSceneResource D_80167FC0_168BC0[48];

static void *map_cpu_pointer(unsigned int value)
{
#ifdef ANCHOR_DUNGEON_MAPS_HOST_TEST
    return map_test_decode(value);
#else
    return (void *)(unsigned long)value;
#endif
}

static unsigned int map_cpu_address(const void *value)
{
#ifdef ANCHOR_DUNGEON_MAPS_HOST_TEST
    return map_test_encode(value);
#else
    return (unsigned int)(unsigned long)value;
#endif
}

static void *map_pointer(const void *p, unsigned int offset)
{
    return map_cpu_pointer(*(const unsigned int *)((const unsigned char *)p + offset));
}

static void map_set_pointer(void *p, unsigned int offset, const void *value)
{
    *(unsigned int *)((unsigned char *)p + offset) = map_cpu_address(value);
}

typedef struct MapPeer
{
    int cid, room, map_room, x100, y100, z100, character;
} MapPeer;

typedef struct MapSlot
{
    void *object;
    int cid, color, active;
} MapSlot;

typedef struct MapMarkers
{
    void *owner;
    MapSlot *slots;
    int count, capacity, updates;
} MapMarkers;

static MapPeer *s_peers;
static int s_peer_capacity;
static MapMarkers s_floor_markers, s_mini_markers;
static void *s_mini_updating;
static int s_floor_pending, s_local_floor, s_floor_layer;

/* First match per room from D_80209A00_5C5910 through D_80209B48_5C5A58.
 * The stock dynamic-floor updater (801FB2D0_5B71E0) supplies the height rules.
 * Keep a scalar copy: file_0C's code/data need not stay loaded during file_17. */
static const unsigned char s_dungeon_rooms[][4] = {
#include "anchor_dungeon_rooms.inc"
};

static int dungeon_floor(int room, int y100, int *dungeon)
{
    unsigned int i;
    for (i = 0; i < sizeof(s_dungeon_rooms) / sizeof(s_dungeon_rooms[0]); ++i)
    {
        const unsigned char *row = s_dungeon_rooms[i];
        if (row[1] != room) continue;
        *dungeon = row[0];
        switch (row[3])
        {
            case 1: return y100 > -15000 ? 1 : 0;
            case 2: return y100 > -8000 ? 2 : 1;
            case 3: return y100 > 13000 ? 2 : 1;
            case 4: return y100 > 12000 ? 1 : 0;
            case 5: return y100 > 30500 ? 3 : 2;
            case 6: return y100 > 39200 ? 3 : 2;
            case 7: return y100 > -11500 ? 2 : (y100 > -37200 ? 1 : 0);
            case 8: return y100 > -12000 ? 2 : 1;
            case 9: return y100 > 10000 ? 3 : 2;
            default: return row[2];
        }
    }
    return -1;
}

static int parse_map_peers(char *json)
{
    char *cursor = json, *object, *end;
    int required = 0, count = 0;
    while (mnsg_json_next_object(&cursor, &end)) ++required;
    if (!mnsg_array_reserve((void **)&s_peers, &s_peer_capacity, required, sizeof(*s_peers)))
        return -1;
    cursor = json;
    while ((object = mnsg_json_next_object(&cursor, &end)))
    {
        MapPeer peer;
        int has_position;
        char saved = *end;
        *end = '\0';
        if (mnsg_json_get_s32(object, "cid", &peer.cid) && peer.cid > 0 &&
            mnsg_json_get_s32(object, "room", &peer.room) &&
            mnsg_json_get_s32(object, "mr", &peer.map_room) && peer.map_room >= 0 && peer.map_room < 0x10000 &&
            mnsg_json_get_s32(object, "mhp", &has_position) && has_position == 1 &&
            mnsg_json_get_s32(object, "mx", &peer.x100) &&
            mnsg_json_get_s32(object, "my", &peer.y100) &&
            mnsg_json_get_s32(object, "mz", &peer.z100) &&
            mnsg_json_get_s32(object, "ch", &peer.character) && peer.character >= 0 && peer.character < 4)
            s_peers[count++] = peer;
        *end = saved;
    }
    return count;
}

static int read_map_peers(void)
{
    char *json = anchor_get_lobby_positions_json();
    int count;
    if (!json) return 0; /* A lost connection must not leave visible ghosts. */
    count = parse_map_peers(json);
    recomp_free(json);
    return count < 0 ? 0 : count;
}

static void reset_markers(MapMarkers *markers, void *owner)
{
    /* Native task cleanup owns the old objects. Never dereference them here,
     * including when the task pool reuses exactly the same address. */
    markers->owner = owner;
    markers->count = 0;
    markers->updates = 0;
}

static void begin_markers(MapMarkers *markers, int peers)
{
    int i, j;
    for (i = 0; i < markers->count; ++i)
    {
        markers->slots[i].active = 0;
        /* Reserve all surviving identities before any new client can take a
         * slot, even if a new lower client ID precedes them in the roster. */
        for (j = 0; j < peers; ++j)
            if (markers->slots[i].cid == s_peers[j].cid)
                markers->slots[i].active = 1;
    }
}

static MapSlot *claim_marker(MapMarkers *markers, int cid)
{
    MapSlot *slot = 0;
    unsigned int used = 0;
    int i;
    for (i = 0; i < markers->count; ++i)
    {
        if (markers->slots[i].cid == cid) return &markers->slots[i];
        if (markers->slots[i].active) used |= 1u << markers->slots[i].color;
        else if (!slot) slot = &markers->slots[i];
    }
    if (!slot)
    {
        if (!mnsg_array_reserve((void **)&markers->slots, &markers->capacity,
                markers->count + 1, sizeof(*markers->slots))) return 0;
        slot = &markers->slots[markers->count++];
        slot->object = 0;
    }
    slot->cid = cid;
    slot->color = (unsigned int)cid % MAP_COLORS;
    for (i = 0; i < MAP_COLORS; ++i)
        if (!(used & (1u << ((slot->color + i) % MAP_COLORS))))
        {
            slot->color = (slot->color + i) % MAP_COLORS;
            break;
        }
    slot->active = 1;
    return slot;
}

static void refresh_floor_markers(void *task)
{
    int count = read_map_peers(), i, matching = 0;
    int floors = MAP_I32(task, 0x98), dungeon = MAP_I32(task, 0x90);
    int per_floor[4] = {0, 0, 0, 0};
    if (floors < 1 || floors > 4 || dungeon < 0 || dungeon >= 5) return;
    if (s_local_floor >= 0 && s_local_floor < floors) per_floor[s_local_floor] = 1;
    /* Compact to this dungeon first, so irrelevant peers cannot retain slots. */
    for (i = 0; i < count; ++i)
    {
        int peer_dungeon, floor = dungeon_floor(s_peers[i].map_room, s_peers[i].y100, &peer_dungeon);
        if (floor >= 0 && floor < floors && peer_dungeon == dungeon)
            s_peers[matching++] = s_peers[i];
    }
    begin_markers(&s_floor_markers, matching);
    for (i = 0; i < matching; ++i)
    {
        MapPeer *peer = &s_peers[i];
        MapSlot *slot = claim_marker(&s_floor_markers, peer->cid);
        int ignored, floor = dungeon_floor(peer->map_room, peer->y100, &ignored);
        int ordinal = per_floor[floor]++;
        unsigned int face = 0x48000040u + peer->character * 0xe0u;
        if (!slot) continue;
        if (!slot->object)
            slot->object = func_8000DBF0_E7F0(task, face, 0xA0216AE0u, 0, 0, 0,
                (short)0x8000, (short)0x8000, (short)0x8000, 3, 3, 3, 0x4d1, 0);
        if (!slot->object) continue;
        MAP_I32(slot->object, 0x2c) = face;
        MAP_BYTE(slot->object, 5) = s_floor_layer;
        func_800115E0_121E0(slot->object, 53 + ordinal * MAP_FLOOR_HEAD_SPACING,
            53 + (112 / (floors + 1)) * (floors - floor));
    }
    for (i = 0; i < s_floor_markers.count; ++i)
    {
        MapSlot *slot = &s_floor_markers.slots[i];
        if (slot->object) MAP_BYTE(slot->object, 0x64) =
            (MAP_BYTE(slot->object, 0x64) & ~1u) | (slot->active ? 0u : 1u);
    }
}

RECOMP_HOOK("func_802155F4_6745A4")
void anchor_dungeon_floor_begin(void *owner, int x, int y, int layer,
    unsigned int unused4, unsigned int unused5, unsigned int unused6,
    int character, int dungeon, int floor)
{
    reset_markers(&s_floor_markers, 0);
    s_floor_pending = 1;
    s_local_floor = floor;
    s_floor_layer = layer + 2;
}

RECOMP_HOOK("func_80215198_674148")
void anchor_dungeon_floor_update(void *task)
{
    if (s_floor_pending)
    {
        s_floor_pending = 0;
        s_floor_markers.owner = task;
    }
    if (s_floor_markers.owner != task) return;
    if (s_floor_markers.updates++ % MAP_REFRESH_UPDATES == 0)
        refresh_floor_markers(task);
    if (s_floor_markers.updates >= MAP_REFRESH_UPDATES) s_floor_markers.updates = 0;
}

/* Only the pixels are read by the RDP; banks/descriptors are CPU data. Mod
 * BSS lives above 0x81000000 and cannot supply native texture commands. */
static unsigned short (*s_dot_pixels)[MAP_DOT_PIXELS];
static unsigned char s_dot_banks[MAP_COLORS][16] __attribute__((aligned(8)));
static const unsigned short s_dot_descriptor[6] = {0xc000, 0x0808, 0, 0, 0, 0};
static int s_dot_ready;
static const unsigned char s_dot_colors[MAP_COLORS][3] = {
    {0,31,31}, {8,31,4}, {31,31,0}, {22,8,31},
    {31,17,0}, {4,16,31}, {31,6,24}, {31,31,31},
    {0,21,16}, {16,20,31}, {31,22,15}, {17,31,20},
    {22,13,31}, {24,27,8}, {12,31,26}, {31,18,27}
};

static int dot_arena_plan(unsigned int cursor, unsigned int *start, unsigned int *end)
{
    cursor &= 0xbfffffffu; /* Normalize the native C0-tagged cached alias. */
    if (cursor < 0x80001000u || cursor >= 0x80800000u) return 0;
    *start = (cursor + 15u) & ~15u;
    *end = *start + MAP_DOT_ARENA_BYTES;
    return *end <= 0x80800000u;
}

void anchor_dungeon_maps_load_resources(void)
{
    unsigned int start, end;
    int i;
    /* The scene loader has invalidated both its arena and old map tasks.
     * Reopening a minimap within this scene reuses this one reservation. */
    reset_markers(&s_mini_markers, 0);
    s_mini_updating = 0;
    s_dot_pixels = 0;
    s_dot_ready = 0;
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !dot_arena_plan(map_cpu_address(D_80167FC0_168BC0[i].data), &start, &end))
        return;
    /* Reserve only sixteen 8x8 crops (2 KiB), not sixteen full HUD sheets. */
    D_80167FC0_168BC0[i].data = map_cpu_pointer(end);
    s_dot_pixels = map_cpu_pointer(start);
}

static unsigned short recolor_dot(unsigned short pixel, int color)
{
    unsigned int r = (pixel >> 11) & 31, g = (pixel >> 6) & 31, b = (pixel >> 1) & 31;
    unsigned int intensity = r > g ? r : g;
    if (b > intensity) intensity = b;
    return ((intensity * s_dot_colors[color][0] / 31) << 11) |
        ((intensity * s_dot_colors[color][1] / 31) << 6) |
        ((intensity * s_dot_colors[color][2] / 31) << 1) | (pixel & 1);
}

static int prepare_dot_textures(void)
{
    const unsigned short *source;
    int color, pixel;
    if (s_dot_ready) return 1;
    if (!s_dot_pixels) return 0;
    /* The resolver intentionally faults on an unloaded file; guard first. */
    if (func_800141C4_14DC4(0x7f) == -1) return 0;
    source = map_cpu_pointer((unsigned int)func_80014840_15440(0x0a000000, 0x7f));
    if (!source) return 0;
    for (color = 0; color < MAP_COLORS; ++color)
    {
        for (pixel = 0; pixel < MAP_DOT_PIXELS; ++pixel)
            s_dot_pixels[color][pixel] = recolor_dot(
                source[(24 + pixel / MAP_DOT_SIDE) * 64 + 24 + pixel % MAP_DOT_SIDE], color);
        MAP_U16(s_dot_banks[color], 0) = MAP_DOT_SIDE;
        MAP_U16(s_dot_banks[color], 2) = MAP_DOT_SIDE;
        MAP_BYTE(s_dot_banks[color], 4) = 0x10; /* RGBA16; file 0 = direct pointer. */
        map_set_pointer(s_dot_banks[color], 8, s_dot_pixels[color]);
    }
    s_dot_ready = 1;
    return 1;
}

static int minimap_axis(float offset, int origin, int maximum)
{
    /* Truncate offsets before adding the native +13 origin correction.
     * Clamp before conversion for extreme network coordinates. */
    if (offset > 65536.0f) return maximum;
    if (offset < -65536.0f) return 8;
    origin = (short)origin + (int)offset + 13;
    return origin < 8 ? 8 : (origin > maximum ? maximum : origin);
}

static int project_minimap(int x100, int z100, unsigned int extent,
    int orientation, int origin_x, int origin_y, int square, int *x, int *y)
{
    float wx, wz, dx, dy, scale;
    if (!extent) return 0;
    scale = 128.0f / (float)extent;
    wx = ((float)x100 / 100.0f) * scale;
    wz = ((float)z100 / 100.0f) * scale;
    switch (orientation)
    {
        case 0: dx = -wx; dy = -wz; break;
        case 0x100: dx = wz; dy = -wx; break;
        case 0x200: dx = wx; dy = wz; break;
        default: dx = -wz; dy = wx; break;
    }
    *x = minimap_axis(dx, origin_x, 144);
    *y = minimap_axis(dy, origin_y, square ? 144 : 80);
    return 1;
}

static void refresh_minimap_markers(void *task, void *local)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    int count = read_map_peers(), matching = 0, i;
    int room = MAP_U16(system, 0x3adf2);
    for (i = 0; i < count; ++i)
        if (MAP_I32(system, 0x3b008) != 0 && s_peers[i].room == room && s_peers[i].map_room == room)
            s_peers[matching++] = s_peers[i];
    begin_markers(&s_mini_markers, matching);
    for (i = 0; i < matching; ++i)
    {
        MapPeer *peer = &s_peers[i];
        MapSlot *slot;
        int x, y, byte;
        if (!project_minimap(peer->x100, peer->z100, (unsigned int)MAP_I32(system, 0x3b008),
                MAP_U16(system, 0xcf88e), MAP_I32(system, 0x3b000), MAP_I32(system, 0x3b004),
                MAP_I32(task, 0xe4), &x, &y)) continue;
        slot = claim_marker(&s_mini_markers, peer->cid);
        if (!slot) continue;
        if (!slot->object) slot->object = func_80035EEC_36AEC(task, 1, 1);
        if (!slot->object) continue;
        /* Append after the background. Stock update follows local->next to
         * that background; copying the link would corrupt the native chain. */
        for (byte = 4; byte < 0x3c; ++byte)
            MAP_BYTE(slot->object, byte) = MAP_BYTE(local, byte);
        /* The private crop has an 8-pixel stride and zero UV origin. Native
         * sheet masks would wrap its coordinates across unrelated texels. */
        MAP_I32(slot->object, 8) = (MAP_I32(slot->object, 8) & ~((15u << 15) | (15u << 11))) |
            (3u << 15) | (3u << 11);
        MAP_U16(slot->object, 6) = 0x2f; /* Keep the local red marker in front. */
        map_set_pointer(slot->object, 0x14, s_dot_banks[slot->color]);
        map_set_pointer(slot->object, 0x34, s_dot_descriptor);
        map_set_pointer(slot->object, 0x38, 0);
        MAP_U16(slot->object, 0x1c) = x;
        MAP_U16(slot->object, 0x20) = y;
    }
}

RECOMP_HOOK("func_80215CA4_5D1174")
void anchor_minimap_update_begin(void *task, void *object)
{
    s_mini_updating = task;
    if (MAP_I32(task, 0xdc) == 0 || s_mini_markers.owner != task)
        reset_markers(&s_mini_markers, task);
}

RECOMP_HOOK_RETURN("func_80215CA4_5D1174")
void anchor_minimap_update_end(void)
{
    void *task = s_mini_updating, *local, *background;
    int state, alpha = 0, i;
    s_mini_updating = 0;
    if (!task || task != s_mini_markers.owner) return;
    state = MAP_I32(task, 0xdc);
    if (state < 1 || state > 3) return; /* Includes failed native initialization. */
    local = map_pointer(task, 0xe0);
    if (!local) return;
    background = map_pointer(local, 0);
    if (!background) return;
    if (state == 2 || state == 3)
    {
        alpha = MAP_BYTE(background, 0x10);
        if (prepare_dot_textures() && s_mini_markers.updates++ == 0)
            refresh_minimap_markers(task, local);
        if (s_mini_markers.updates >= MAP_REFRESH_UPDATES) s_mini_markers.updates = 0;
    }
    else s_mini_markers.updates = 0; /* Refresh immediately on the next opening. */
    for (i = 0; i < s_mini_markers.count; ++i)
    {
        MapSlot *slot = &s_mini_markers.slots[i];
        if (slot->object) MAP_BYTE(slot->object, 0x10) = slot->active ? alpha : 0;
    }
}
