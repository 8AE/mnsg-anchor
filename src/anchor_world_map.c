/**
 * @file anchor_world_map.c
 * @brief Online-player face markers for the native Japan map.
 *
 * file_17's stock map builds one face render record under its map task. This
 * module attaches one more engine-owned record per online remote. The parent
 * task therefore retains normal ownership and tears every marker down with
 * the map; no persistent playable actor or custom renderer is introduced.
 */

#ifndef ANCHOR_WORLD_MAP_HOST_TEST
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#endif

#include "anchor_world_map.h"
#include "utils/array_utils.h"
#include "utils/json_utils.h"

#define WORLD_MAP_FACE_GOEMON 0x48000040u
#define WORLD_MAP_FACE_STRIDE 0x000000E0u
#define WORLD_MAP_FACE_ANIMATION 0xA0216AE0u
#define WORLD_MAP_FACE_FILE 0x04D1
#define WORLD_MAP_FACE_LAYER 9
#define WORLD_MAP_FACE_SCALE 0.2f
#define WORLD_MAP_GROUP_SPACING 0.9f
#define WORLD_MAP_REFRESH_UPDATES 21
#define WORLD_MAP_LOCAL_FACE_OFFSET 0x5C
#define WORLD_MAP_OBJECT_LAYER_OFFSET 0x05
#define WORLD_MAP_OBJECT_X_OFFSET 0x08
#define WORLD_MAP_OBJECT_Y_OFFSET 0x0C
#define WORLD_MAP_OBJECT_Z_OFFSET 0x10
#define WORLD_MAP_OBJECT_MODEL_OFFSET 0x2C
#define WORLD_MAP_OBJECT_VISIBILITY_OFFSET 0x64

typedef struct AnchorWorldMapPosition
{
    float x;
    float y;
    float z;
} AnchorWorldMapPosition;

typedef struct AnchorWorldMapRemote
{
    int cid;
    int room;
    int world_x_100;
    int world_z_100;
    int character;
    AnchorWorldMapPosition base;
    void *object;
    int active;
} AnchorWorldMapRemote;

static AnchorWorldMapRemote *s_map_remotes;
static int s_map_remote_capacity;
static int s_map_remote_count;
static AnchorWorldMapRemote *s_map_roster;
static int s_map_roster_capacity;
static int s_map_roster_count;
static void *s_active_map_task;
static void *s_updating_map_task;
static int s_map_creation_pending;
static int s_map_refresh_updates;
static AnchorWorldMapPosition s_local_map_position;

static int parse_s32_after(const char *object, const char *key, int fallback)
{
    int value;

    if (!mnsg_json_get_s32(object, key, &value))
        return fallback;
    return value;
}

/* The Python getter owns ordering (ascending client ID) and filters offline
 * and self records. Temporarily terminate each object so a missing field can
 * never leak in from the following player. */
static int parse_world_map_remotes(char *json)
{
    char *cursor = json;
    char *object;
    char *end;
    int required = 0;
    int count = 0;

    while ((object = mnsg_json_next_object(&cursor, &end)) != 0)
    {
        if (required == 0x7fffffff)
            return -1;
        ++required;
    }
    if (!mnsg_array_reserve((void **)&s_map_roster,
                            &s_map_roster_capacity, required,
                            sizeof(*s_map_roster)))
        return -1;

    cursor = json;
    while ((object = mnsg_json_next_object(&cursor, &end)) != 0)
    {
        int cid;
        int room;
        int has_position;
        int world_x_100;
        int world_z_100;
        int has_world_x;
        int has_world_z;
        int character;
        char saved = *end;

        *end = '\0';
        cid = parse_s32_after(object, "cid", 0);
        room = parse_s32_after(object, "mr", -1);
        has_position = parse_s32_after(object, "mhp", 0);
        has_world_x = mnsg_json_get_s32(object, "mx", &world_x_100);
        has_world_z = mnsg_json_get_s32(object, "mz", &world_z_100);
        character = parse_s32_after(object, "ch", -1);

        /* 0x226 is the map overlay, not a point on the map. New clients send
         * a durable gameplay snapshot; legacy clients already on the map are
         * skipped instead of being placed at F734's unrelated default slot. */
        if (cid > 0 && room >= 0 && room <= 0xffff &&
            room != ANCHOR_WORLD_MAP_ROOM && has_position == 1 &&
            has_world_x && has_world_z)
        {
            AnchorWorldMapRemote *remote = &s_map_roster[count++];

            remote->cid = cid;
            remote->room = room;
            remote->world_x_100 = world_x_100;
            remote->world_z_100 = world_z_100;
            remote->character =
                character >= 0 && character < 4 ? character : 0;
            remote->base.x = 0.0f;
            remote->base.y = 0.0f;
            remote->base.z = 0.0f;
            remote->object = 0;
            remote->active = 1;
        }
        *end = saved;
    }
    return count;
}

static int map_positions_equal(const AnchorWorldMapPosition *left,
                               const AnchorWorldMapPosition *right)
{
    return left->x == right->x && left->y == right->y &&
           left->z == right->z;
}

/* Ordinal zero owns the canonical point. Later players alternate right/left,
 * leaving the stock local face centered when it belongs to this group. */
static float world_map_group_offset(int ordinal)
{
    int distance;

    if (ordinal <= 0)
        return 0.0f;
    distance = (ordinal + 1) / 2;
    if ((ordinal & 1) == 0)
        distance = -distance;
    return (float)distance * WORLD_MAP_GROUP_SPACING;
}

static int world_map_group_ordinal(int index)
{
    int ordinal = map_positions_equal(&s_map_roster[index].base,
                                      &s_local_map_position) ? 1 : 0;
    int previous;

    for (previous = 0; previous < index; ++previous)
    {
        if (map_positions_equal(&s_map_roster[index].base,
                                &s_map_roster[previous].base))
            ++ordinal;
    }
    return ordinal;
}

static void begin_world_map_remote_refresh(void)
{
    int index;

    for (index = 0; index < s_map_remote_count; ++index)
        s_map_remotes[index].active = 0;
}

/* Native display records stay owned by the map task. Preserve a matching
 * client's record, or recycle an inactive slot before growing the finite
 * kind-2 pool; never destroy a record while the task may still render it. */
static AnchorWorldMapRemote *claim_world_map_remote(int cid)
{
    AnchorWorldMapRemote *remote;
    void *object;
    int index;

    for (index = 0; index < s_map_remote_count; ++index)
    {
        if (s_map_remotes[index].cid == cid)
        {
            s_map_remotes[index].active = 1;
            return &s_map_remotes[index];
        }
    }
    for (index = 0; index < s_map_remote_count; ++index)
    {
        if (!s_map_remotes[index].active)
        {
            remote = &s_map_remotes[index];
            object = remote->object;
            remote->cid = cid;
            remote->room = -1;
            remote->world_x_100 = 0;
            remote->world_z_100 = 0;
            remote->character = 0;
            remote->base.x = 0.0f;
            remote->base.y = 0.0f;
            remote->base.z = 0.0f;
            remote->object = object;
            remote->active = 1;
            return remote;
        }
    }
    if (!mnsg_array_reserve((void **)&s_map_remotes,
                            &s_map_remote_capacity, s_map_remote_count + 1,
                            sizeof(*s_map_remotes)))
        return 0;
    remote = &s_map_remotes[s_map_remote_count++];
    remote->cid = cid;
    remote->room = -1;
    remote->world_x_100 = 0;
    remote->world_z_100 = 0;
    remote->character = 0;
    remote->base.x = 0.0f;
    remote->base.y = 0.0f;
    remote->base.z = 0.0f;
    remote->object = 0;
    remote->active = 1;
    return remote;
}

#ifndef ANCHOR_WORLD_MAP_HOST_TEST

static void reset_active_world_map(void)
{
    s_active_map_task = 0;
    s_updating_map_task = 0;
    s_map_remote_count = 0;
    s_map_creation_pending = 0;
    s_map_refresh_updates = 0;
}

/* Exact file_17 map mapper. It selects one of 49 canonical map-space triples
 * from the pre-map room plus world X/Z (including its room-specific splits). */
extern void func_8020F734_66E6E4(unsigned int room, float world_x,
                                 float world_z, float *map_x, float *map_y,
                                 float *map_z);

/* Generic engine kind-2 display allocator used by the stock local face. */
extern void *func_8000DBF0_E7F0(void *task, unsigned int model,
                                unsigned int animation,
                                float x, float y, float z,
                                short rot_x, short rot_y, short rot_z,
                                float scale_x, float scale_y, float scale_z,
                                short file_8, short file_9);

static void set_object_visibility(void *object, int hidden)
{
    unsigned char *bytes = (unsigned char *)object;

    bytes[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] =
        (unsigned char)((bytes[WORLD_MAP_OBJECT_VISIBILITY_OFFSET] & ~1u) |
                        (hidden & 1));
}

static void update_world_map_object(AnchorWorldMapRemote *remote,
                                    void *map_task,
                                    const AnchorWorldMapPosition *position)
{
    unsigned char *bytes;
    unsigned int face = WORLD_MAP_FACE_GOEMON +
        (unsigned int)remote->character * WORLD_MAP_FACE_STRIDE;

    if (!remote->object)
    {
        remote->object = func_8000DBF0_E7F0(
            map_task, face, WORLD_MAP_FACE_ANIMATION,
            position->x, position->y, position->z,
            (short)0x8000, (short)0x8000, (short)0x8000,
            WORLD_MAP_FACE_SCALE, WORLD_MAP_FACE_SCALE,
            WORLD_MAP_FACE_SCALE, WORLD_MAP_FACE_FILE, 0);
        if (!remote->object)
            return;
    }
    bytes = (unsigned char *)remote->object;
    *(float *)(bytes + WORLD_MAP_OBJECT_X_OFFSET) = position->x;
    *(float *)(bytes + WORLD_MAP_OBJECT_Y_OFFSET) = position->y;
    *(float *)(bytes + WORLD_MAP_OBJECT_Z_OFFSET) = position->z;
    *(unsigned int *)(bytes + WORLD_MAP_OBJECT_MODEL_OFFSET) = face;
    bytes[WORLD_MAP_OBJECT_LAYER_OFFSET] = WORLD_MAP_FACE_LAYER;
}

static void refresh_world_map_remotes(void *map_task)
{
    char *json = anchor_get_lobby_positions_json();
    int parsed;
    int index;

    if (!json)
        return;
    parsed = parse_world_map_remotes(json);
    recomp_free(json);
    if (parsed < 0)
        return;
    s_map_roster_count = parsed;
    begin_world_map_remote_refresh();

    for (index = 0; index < s_map_roster_count; ++index)
    {
        AnchorWorldMapRemote *incoming = &s_map_roster[index];
        AnchorWorldMapRemote *remote;
        AnchorWorldMapPosition position;
        int ordinal;

        func_8020F734_66E6E4((unsigned int)incoming->room,
                             (float)incoming->world_x_100 / 100.0f,
                             (float)incoming->world_z_100 / 100.0f,
                             &incoming->base.x, &incoming->base.y,
                             &incoming->base.z);
        position = incoming->base;
        ordinal = world_map_group_ordinal(index);
        position.x += world_map_group_offset(ordinal);
        remote = claim_world_map_remote(incoming->cid);
        if (!remote)
            continue;
        remote->room = incoming->room;
        remote->world_x_100 = incoming->world_x_100;
        remote->world_z_100 = incoming->world_z_100;
        remote->character = incoming->character;
        remote->base = incoming->base;
        update_world_map_object(remote, map_task, &position);
    }
}

/* F170 receives exactly the room/X/Z triple used for the stock local face.
 * Capture it before the current room changes to the 0x226 map overlay, and
 * publish the same edge for peers that open their maps simultaneously. */
RECOMP_HOOK("func_8020F170_66E120")
void anchor_world_map_begin(void *owner, int mode, int destination,
                            int character, unsigned int source_room,
                            float world_x, float world_z)
{
    (void)owner;
    (void)mode;
    (void)destination;
    (void)character;

    reset_active_world_map();
    s_map_creation_pending = 1;
    func_8020F734_66E6E4(source_room, world_x, world_z,
                         &s_local_map_position.x, &s_local_map_position.y,
                         &s_local_map_position.z);
    anchor_set_world_map_location(source_room, world_x, world_z);
}

/* E930 has one other caller: F3B8's fixed special-map sequence. Clear the
 * previous generation before it can reuse the same task-pool address; only a
 * subsequent F170 user-map entry is allowed to arm multiplayer markers. */
RECOMP_HOOK("func_8020F3B8_66E368")
void anchor_world_map_special_begin(void *owner, int mode)
{
    (void)owner;
    (void)mode;
    reset_active_world_map();
}

/* E630 is the persistent update callback for the exact map task returned by
 * E930. Its first call is late enough that native resource setup is complete;
 * render records attached here inherit that task's normal cleanup. */
RECOMP_HOOK("func_8020E630_66D5E0")
void anchor_world_map_update_begin(void *map_task, void *object)
{
    (void)object;
    s_updating_map_task = map_task;
    if (s_map_creation_pending)
    {
        s_map_creation_pending = 0;
        s_active_map_task = map_task;
        s_map_refresh_updates = 0;
        refresh_world_map_remotes(map_task);
    }
    else if (map_task == s_active_map_task &&
             ++s_map_refresh_updates >= WORLD_MAP_REFRESH_UPDATES)
    {
        s_map_refresh_updates = 0;
        refresh_world_map_remotes(map_task);
    }
}

/* Stock E630 toggles the local face and destination pins every 21 updates.
 * Mirror only that visibility bit after the native update so every player
 * face shares the stock face phase without altering other object flags. */
RECOMP_HOOK_RETURN("func_8020E630_66D5E0")
void anchor_world_map_update_end(void)
{
    void *map_task = s_updating_map_task;
    void *local_face;
    int hidden;
    int index;

    s_updating_map_task = 0;
    if (!map_task || map_task != s_active_map_task)
        return;
    local_face = *(void **)((unsigned char *)map_task +
                            WORLD_MAP_LOCAL_FACE_OFFSET);
    if (!local_face)
        return;
    hidden = ((unsigned char *)local_face)
        [WORLD_MAP_OBJECT_VISIBILITY_OFFSET] & 1;
    for (index = 0; index < s_map_remote_count; ++index)
    {
        if (s_map_remotes[index].object)
            set_object_visibility(s_map_remotes[index].object,
                                  hidden || !s_map_remotes[index].active);
    }
}

#endif /* !ANCHOR_WORLD_MAP_HOST_TEST */
