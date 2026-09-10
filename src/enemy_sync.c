/**
 * @file enemy_sync.c
 * @brief Compact, occupied-room synchronization for defeated regular enemies.
 *
 * Static room actors have a deterministic 0x14-byte ActorInstance descriptor.
 * A unified normal-plus-partition source index stays stable across clients even
 * when several enemies use the same entity or randomization replaces their
 * definitions.  A live death packet therefore needs only the raw room, that
 * one-byte index, and a small signature that rejects mismatched room layouts.
 *
 * Death state deliberately lasts only while the room remains occupied.  Each
 * occupant publishes a trimmed 256-bit bitmap in its replace-style Anchor
 * client state.  A player entering the room imports and merges those bitmaps
 * before the native spawn loop, then sends a small request as a fallback.  If
 * everybody leaves, no client advertises that room and vanilla respawning is
 * restored on the next visit.
 */

#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#include "anchor_player_models.h"
#include "enemy_sync.h"
#include "utils/json_utils.h"
#include "utils/string_utils.h"

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC60C_5B851C;
extern void func_8021664C_5D1B1C(void *task, unsigned int animation,
                                 float rate, unsigned int flags);
extern void func_802197D8_5D4CA8(void *task, int turn_step);

typedef struct
{
    short x;
    short y;
    short z;
} EnemyVec3s;

typedef struct
{
    EnemyVec3s position;
    EnemyVec3s rotation;
    void *definition;
    unsigned char is_spawned;
    unsigned char unk_11;
    unsigned char unk_12;
    unsigned char unk_13;
} EnemyActorInstance;

typedef struct
{
    unsigned short actor_id;
    unsigned short params;
    unsigned int data[3];
} EnemyActorDefinition;

typedef struct
{
    EnemyActorInstance *persistent_actor_instances;
    const char **actor_definition_names;
    EnemyActorInstance *actor_instances;
    void *actor_partitions;
    void *actor_partition_configuration;
    unsigned short actor_data_file_id;
    unsigned short unk_16;
    void (*load_stage_data_files)(void);
} EnemyStageActorMetadata;

typedef struct
{
    unsigned char unknown[0x14];
    unsigned short cells_x;
    unsigned short cells_y;
    unsigned short cells_z;
} EnemyActorPartitionConfig;

extern EnemyStageActorMetadata *D_80231300_5EC7D0[];
extern EnemyActorInstance *D_8015CDDC;
extern EnemyActorDefinition *D_8015CDE0;
extern signed int func_800141C4_14DC4(unsigned int file_id);
extern signed int func_80014840_15440(signed int pointer,
                                     unsigned int file_id);

#define ENEMY_MAX_INSTANCES 256
#define ENEMY_BITMAP_BYTES (ENEMY_MAX_INSTANCES / 8)
#define ENEMY_BITMAP_HEX_CHARS (ENEMY_BITMAP_BYTES * 2)
#define ENEMY_INSTANCE_SIZE 0x14
#define ENEMY_MAX_PARTITION_CELLS 4096
#define ENEMY_OUTGOING_CAPACITY 64
#define ENEMY_INVALID_ROOM 0xFFFFu
#define ENEMY_ROOM_METADATA_COUNT 800u

#define ACTOR_ENTITY_ID(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0x5C))
#define ACTOR_STATUS(actor) \
    (*(volatile unsigned int *)((char *)(actor) + 0x68))
#define ACTOR_INSTANCE(actor) \
    (*(EnemyActorInstance **)((char *)(actor) + 0x70))
#define ACTOR_HEALTH(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0x8D))
#define ACTOR_AI(actor) \
    (*(void *volatile *)((char *)(actor) + 0x0C))
#define ACTOR_OBJECT(actor) \
    (*(void *volatile *)((char *)(actor) + 0x18))
#define ACTOR_VEL_X(actor) (*(volatile float *)((char *)(actor) + 0x78))
#define ACTOR_VEL_Y(actor) (*(volatile float *)((char *)(actor) + 0x7C))
#define ACTOR_VEL_Z(actor) (*(volatile float *)((char *)(actor) + 0x80))
#define ACTOR_TARGET(actor) \
    (*(void *volatile *)((char *)(actor) + 0x84))
#define OBJECT_FLOAT(object, offset) \
    (*(volatile float *)((char *)(object) + (offset)))
#define OBJECT_HALF(object, offset) \
    (*(volatile unsigned short *)((char *)(object) + (offset)))

#define ACTOR_STATUS_REMOVE_PENDING 0x00000002u
#define ACTOR_STATUS_DAMAGE_PENDING 0x00040000u

#define PACKET_ENEMY_DEATH "MNSG_ED"
#define PACKET_ENEMY_REQUEST "MNSG_ER"
#define PACKET_ENEMY_STATE "MNSG_ES"

typedef struct
{
    unsigned short room;
    unsigned short signature;
    unsigned short entity_id;
    unsigned char instance_index;
} OutgoingEnemyDeath;

static unsigned short s_room = ENEMY_INVALID_ROOM;
static unsigned short s_signature;
static unsigned short s_instance_count;
static unsigned int s_roster_valid;
static unsigned int s_session_active;
static EnemyActorInstance *s_source_instances[ENEMY_MAX_INSTANCES];
static unsigned short s_entity_ids[ENEMY_MAX_INSTANCES];
static void *s_live_actors[ENEMY_MAX_INSTANCES];
static void *s_death_actors[ENEMY_MAX_INSTANCES];
static unsigned char s_dead_bitmap[ENEMY_BITMAP_BYTES];
static unsigned char s_remote_kill_pending[ENEMY_BITMAP_BYTES];

static OutgoingEnemyDeath s_outgoing[ENEMY_OUTGOING_CAPACITY];
static unsigned int s_outgoing_head;
static unsigned int s_outgoing_count;

static unsigned int s_publish_pending;
static unsigned int s_request_pending;
static unsigned int s_broadcast_pending;
static unsigned int s_response_client_id;
static unsigned int s_setup_yield_pending;

/* ── Authoritative live enemy simulation ────────────────────────────────
 * One elected occupant of a room runs the native enemy AI and streams each
 * live enemy's transform, animation and health.  Every other occupant holds
 * the enemy AI and applies the stream, so all players see and fight the same
 * enemy while the room stays occupied.  Enemy-owned attacks are not streamed
 * yet; see docs/enemy-sync.md. */
#define ENEMY_LIVE_MAX 40
#define ENEMY_LIVE_STRIDE 9
#define ENEMY_HIT_QUEUE 32
#define ENEMY_LIVE_FRAMES 3
#define ENEMY_TARGET_MAX 8
#define ENEMY_LIVE_PACKET "MNSG_ENEMY_LIVE"
#define ENEMY_HIT_PACKET "MNSG_ENEMY_HIT"
#define ENEMY_ROLE_NONE 0
#define ENEMY_ROLE_OWNER 1
#define ENEMY_ROLE_REPLICA 2
#define ENEMY_LIVE_INDEX 0
#define ENEMY_LIVE_ENTITY 1
#define ENEMY_LIVE_HP 2
#define ENEMY_LIVE_X 3
#define ENEMY_LIVE_Y 4
#define ENEMY_LIVE_Z 5
#define ENEMY_LIVE_YAW 6
#define ENEMY_LIVE_CLIP 7
#define ENEMY_LIVE_FRAME 8

typedef struct
{
    unsigned short index;
    unsigned short amount;
} OutgoingEnemyHit;

typedef void (*EnemyAiCallback)(void *, void *);

/* Native scheduler callback-disable tag (bit 23). */
#define ENEMY_CALLBACK_DISABLED 0x00800000ul

static int s_role = ENEMY_ROLE_NONE;
static unsigned int s_authority;
static unsigned int s_live_seq;
static unsigned int s_live_authority_seen;
static unsigned int s_live_timer;
static int s_live_dirty;
static unsigned int s_clip[ENEMY_MAX_INSTANCES];
static unsigned int s_applied_clip[ENEMY_MAX_INSTANCES];
static unsigned char s_applied_valid[ENEMY_MAX_INSTANCES];
static void *s_hold_ai[ENEMY_MAX_INSTANCES];
static unsigned char s_held[ENEMY_MAX_INSTANCES];

static int s_net_count;
static unsigned char s_net_have[ENEMY_MAX_INSTANCES];
static unsigned short s_net_index[ENEMY_LIVE_MAX];
static int s_net_entity[ENEMY_LIVE_MAX];
static int s_net_hp[ENEMY_LIVE_MAX];
static float s_net_x[ENEMY_LIVE_MAX];
static float s_net_y[ENEMY_LIVE_MAX];
static float s_net_z[ENEMY_LIVE_MAX];
static int s_net_yaw[ENEMY_LIVE_MAX];
static int s_net_clip[ENEMY_LIVE_MAX];
static float s_net_frame[ENEMY_LIVE_MAX];
static unsigned char s_net_fresh[ENEMY_LIVE_MAX];

static int s_damage_index;
static int s_damage_before;
static int s_armed_index = -1;

static void *s_target_swap_task;
static void *s_target_saved;
static void *s_owner_ai[ENEMY_MAX_INSTANCES];
/* Per-frame candidate players (local + remotes) for nearest-enemy targeting,
 * plus the target chosen for the enemy currently running its AI callback. */
static AnchorBossTarget s_player_targets[ENEMY_TARGET_MAX];
static int s_player_target_count;
static int s_target_has_remote;
static float s_active_target[8];
static int s_active_target_remote;
static OutgoingEnemyHit s_hits[ENEMY_HIT_QUEUE];
static unsigned int s_hit_head;
static unsigned int s_hit_count;

static int s_dbg_timer;
static unsigned int s_live_sent_ok;
static unsigned int s_live_recv_ok;
static unsigned int s_held_count;
static unsigned int s_applied_count;

static char s_send_buf[2048];

static void enemy_hold_noop(void *task, void *object)
{
    (void)task;
    (void)object;
}

static void enemy_target_remove(void);
static void enemy_live_unhold(void);

static void clear_bytes(unsigned char *bytes, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; ++i)
        bytes[i] = 0;
}

static void clear_words(unsigned short *words, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; ++i)
        words[i] = 0;
}

static void clear_pointers(void **pointers, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; ++i)
        pointers[i] = 0;
}

static int bitmap_test(const unsigned char *bitmap, unsigned int index)
{
    if (!bitmap || index >= ENEMY_MAX_INSTANCES)
        return 0;
    return (bitmap[index >> 3] & (1u << (index & 7u))) != 0;
}

static int bitmap_set(unsigned char *bitmap, unsigned int index)
{
    unsigned char mask;

    if (!bitmap || index >= ENEMY_MAX_INSTANCES)
        return 0;
    mask = (unsigned char)(1u << (index & 7u));
    if ((bitmap[index >> 3] & mask) != 0)
        return 0;
    bitmap[index >> 3] |= mask;
    return 1;
}

static void bitmap_clear(unsigned char *bitmap, unsigned int index)
{
    if (!bitmap || index >= ENEMY_MAX_INSTANCES)
        return;
    bitmap[index >> 3] &= (unsigned char)~(1u << (index & 7u));
}

static int is_regular_enemy(unsigned short entity_id)
{
    /* Curated ordinary combat actors.  Boss roots, scripted NPC fights,
     * hazards, destructibles, and spawners are intentionally absent even if
     * they happen to expose the common fields.
     *
     * Stored as a sorted table rather than a switch: LLVM lowers a dense
     * switch to a MIPS jump table whose computed branch the live recompiler
     * can mistake for an indirect function dispatch, crashing on unregistered
     * interior blocks.  See src/anchor_collision_actors.c for the same fix. */
    static const unsigned short ids[] = {
        0x0FA, /* Robot Bee */
        0x0FB, /* Pink Robot */
        0x0FC, /* Pink Robot */
        0x0FD, /* Green Robot */
        0x0FE, /* Green Robot */
        0x0FF, /* Big Swatting Robot */
        0x100, /* Blue Robot */
        0x102, /* Slinky Enemy */
        0x103, /* Scarecrow Bot */
        0x104, /* Can-can Legs */
        0x105, /* Flatfish */
        0x106, /* Spinning Kite */
        0x107, /* Bamboo Shooter */
        0x108, /* Jet Robot */
        0x109, /* Yellow Robot */
        0x10A, /* Red Swatting Robot */
        0x10B, /* Shrinking Robot */
        0x10C, /* Sword Robot */
        0x10F, /* One-scale Can-can Legs (randomizer variant) */
        0x110, /* Festival/Hagi Pink Robot */
        0x12C, /* Small Cannon */
        0x12D, /* Flying Dragon Head */
        0x12E, /* Seahorse */
        0x12F, /* Spiny Sea Urchin */
        0x130, /* Drum Robot */
        0x131, /* Triangle Robot */
        0x132, /* Bouncing Darumanyo enemy */
        0x133, /* Paper Ghost */
        0x136, /* Red Eye Fish */
        0x13A, /* Tank */
        0x13B, /* Kii-Awaji Spiny Sea Urchin */
        0x13C, /* Giant Enemy Crab */
        0x13D, /* Samurai */
        0x13E, /* Rose Robot */
        0x13F, /* Hammer Thrower */
        0x140, /* Fire Stalker */
        0x141, /* Ninja */
        0x144, /* Burrowing Robot */
        0x145, /* Kite */
        0x147, /* Bomber Bird */
        0x148, /* Water Snake */
        0x190, /* Flying Tile */
        0x1A6, /* Fox Mask */
    };
    unsigned int low = 0;
    unsigned int high = sizeof(ids) / sizeof(ids[0]);

    while (low < high)
    {
        unsigned int middle = low + (high - low) / 2u;
        if (ids[middle] < entity_id)
            low = middle + 1u;
        else
            high = middle;
    }
    return low < sizeof(ids) / sizeof(ids[0]) && ids[low] == entity_id;
}

static unsigned short signature_mix(unsigned short hash,
                                    unsigned short value)
{
    hash ^= (unsigned short)(value & 0xFFu);
    hash = (unsigned short)(hash * 257u + 17u);
    hash ^= (unsigned short)(value >> 8);
    hash = (unsigned short)(hash * 257u + 17u);
    return hash;
}

static void *resolve_actor_data_pointer(void *pointer,
                                        unsigned short file_id)
{
    signed int raw_pointer;
    signed int resolved;

    if (!pointer)
        return 0;
    raw_pointer = (signed int)pointer;

    /* func_80014840 deliberately writes through address -1 when a positive
     * segmented pointer's overlay file is not loaded.  Actor-manager setup can
     * run before that file becomes ready, so probe with the non-faulting lookup
     * first and let the native spawn callback retry us on a later frame. */
    if (raw_pointer > 0 &&
        func_800141C4_14DC4((unsigned int)file_id) == -1)
        return 0;

    resolved = func_80014840_15440(raw_pointer, (unsigned int)file_id);
    return (void *)resolved;
}

static EnemyActorDefinition *resolve_definition(void *definition,
                                                unsigned short file_id)
{
    return (EnemyActorDefinition *)resolve_actor_data_pointer(definition,
                                                               file_id);
}

static int instance_index(EnemyActorInstance *instance)
{
    unsigned int i;

    if (!instance || !s_roster_valid)
        return -1;
    for (i = 0; i < s_instance_count; ++i)
    {
        if (s_source_instances[i] == instance)
            return (int)i;
    }
    return -1;
}

static int find_live_actor(void *actor)
{
    unsigned int i;

    if (!actor)
        return -1;
    for (i = 0; i < s_instance_count; ++i)
    {
        if (s_live_actors[i] == actor)
            return (int)i;
    }
    return -1;
}

static void forget_actor_pointer(void *actor)
{
    unsigned int i;

    if (!actor)
        return;
    for (i = 0; i < s_instance_count; ++i)
    {
        if (s_live_actors[i] == actor)
            s_live_actors[i] = 0;
        if (s_death_actors[i] == actor)
            s_death_actors[i] = 0;
    }
}

static int actor_matches_index(void *actor, unsigned int index)
{
    if (!actor || index >= s_instance_count ||
        !s_entity_ids[index] || ACTOR_ENTITY_ID(actor) != s_entity_ids[index])
        return 0;
    return s_live_actors[index] == actor;
}

/* ── Live authority helpers ───────────────────────────────────────────── */

typedef struct
{
    char *p;
    char *end;
    int ok;
} EnemyText;

static void enemy_text_char(EnemyText *t, char c)
{
    if (!t->ok || t->p >= t->end)
    {
        t->ok = 0;
        return;
    }
    *t->p++ = c;
}

static void enemy_text(EnemyText *t, const char *text)
{
    while (text && *text)
        enemy_text_char(t, *text++);
}

static void enemy_text_int(EnemyText *t, int value)
{
    char digits[12];

    mnsg_string_write_s32(digits, value);
    enemy_text(t, digits);
}

static void enemy_live_unhold(void)
{
    unsigned int i;

    for (i = 0; i < ENEMY_MAX_INSTANCES; ++i)
    {
        if (!s_held[i])
            continue;
        if (s_live_actors[i] && ACTOR_AI(s_live_actors[i]) == enemy_hold_noop)
            ACTOR_AI(s_live_actors[i]) = s_hold_ai[i];
        s_held[i] = 0;
        s_hold_ai[i] = 0;
    }
}

static void enemy_live_clear_net(void)
{
    unsigned int i;

    enemy_target_remove();
    enemy_live_unhold();
    s_net_count = 0;
    s_live_seq = 0;
    s_live_authority_seen = 0;
    s_live_timer = 0;
    s_live_dirty = 0;
    s_hit_head = 0;
    s_hit_count = 0;
    s_damage_index = -1;
    s_target_swap_task = 0;
    s_player_target_count = 0;
    s_target_has_remote = 0;
    s_active_target_remote = 0;
    clear_bytes(s_net_have, ENEMY_MAX_INSTANCES);
    clear_bytes(s_applied_valid, ENEMY_MAX_INSTANCES);
    for (i = 0; i < ENEMY_MAX_INSTANCES; ++i)
    {
        s_clip[i] = 0;
        s_applied_clip[i] = 0;
    }
}

static int enemy_live_encode(void)
{
    EnemyText t;
    unsigned int i;
    int count = 0;

    t.p = s_send_buf;
    t.end = s_send_buf + sizeof(s_send_buf) - 1;
    t.ok = 1;
    /* Reserve the array by building the header first, then the records. */
    enemy_text(&t, "{\"r\":");
    enemy_text_int(&t, (int)s_room);
    enemy_text(&t, ",\"s\":");
    enemy_text_int(&t, (int)s_signature);
    enemy_text(&t, ",\"q\":");
    enemy_text_int(&t, (int)s_live_seq);
    enemy_text(&t, ",\"d\":[");
    for (i = 0; i < s_instance_count && i < ENEMY_MAX_INSTANCES; ++i)
    {
        void *actor = s_live_actors[i];
        void *object;

        if (!s_entity_ids[i] || !actor_matches_index(actor, i))
            continue;
        if (ACTOR_HEALTH(actor) == 0 ||
            (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING))
            continue;
        object = ACTOR_OBJECT(actor);
        if (!object)
            continue;
        if (count)
            enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)i);
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)s_entity_ids[i]);
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)ACTOR_HEALTH(actor));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)OBJECT_FLOAT(object, 0x08));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)OBJECT_FLOAT(object, 0x0C));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)OBJECT_FLOAT(object, 0x10));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)(OBJECT_HALF(object, 0x16) & 0x3FFu));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)(s_clip[i] & 0xFFFFu));
        enemy_text_char(&t, ',');
        enemy_text_int(&t, (int)(OBJECT_FLOAT(object, 0x28) * 100.0f));
        ++count;
        if (count >= ENEMY_LIVE_MAX)
            break;
    }
    enemy_text(&t, "]}");
    if (!t.ok)
        return 0;
    *t.p = '\0';
    return count;
}

/* Parse the flat integer array at "d" into out.  Returns the element count or
 * -1 when the array is missing or malformed. */
static int enemy_live_parse_array(const char *json, int *out, int capacity)
{
    const char *p = mnsg_json_find_value(json, "d");
    int count = 0;

    if (!p || *p != '[')
        return -1;
    ++p;
    for (;;)
    {
        int negative = 0;
        int value = 0;
        int digits = 0;

        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            ++p;
        if (*p == ']')
            break;
        if (*p == '-')
        {
            negative = 1;
            ++p;
        }
        while (*p >= '0' && *p <= '9')
        {
            if (value > 214748364)
                return -1;
            value = value * 10 + (*p - '0');
            ++digits;
            ++p;
        }
        if (!digits || count >= capacity)
            return -1;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            ++p;
        out[count++] = negative ? -value : value;
        if (*p == ',')
        {
            ++p;
            continue;
        }
        if (*p == ']')
            break;
        return -1;
    }
    if (*p != ']')
        return -1;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    /* Anchor re-serialises the payload and appends reserved root fields
     * (type/clientId/targetTeamId) after "d", so the array is not last. */
    return *p == '\0' || *p == ',' || *p == '}' ? count : -1;
}

static int enemy_live_accept(const char *json)
{
    int values[ENEMY_LIVE_MAX * ENEMY_LIVE_STRIDE];
    unsigned int seq;
    int count;
    int records;
    int i;

    if (!s_roster_valid)
        return 0;
    count = enemy_live_parse_array(json, values,
                                   (int)(ENEMY_LIVE_MAX * ENEMY_LIVE_STRIDE));
    if (count < 0 || (count % ENEMY_LIVE_STRIDE) != 0)
        return 0;
    records = count / ENEMY_LIVE_STRIDE;
    /* Order snapshots so a delayed older stream cannot overwrite a newer one. */
    if (!mnsg_json_get_u32(json, "q", &seq))
        return 0;
    if (seq <= s_live_seq && s_live_seq != 0)
        return 0;

    /* Validate every record before mutating any shared state so a malformed
     * snapshot can never leave a partially-applied enemy set. */
    for (i = 0; i < records; ++i)
    {
        const int *row = &values[i * ENEMY_LIVE_STRIDE];
        int index = row[ENEMY_LIVE_INDEX];
        int entity = row[ENEMY_LIVE_ENTITY];

        if (index < 0 || (unsigned int)index >= s_instance_count ||
            index >= ENEMY_MAX_INSTANCES || !s_entity_ids[index] ||
            s_entity_ids[index] != entity)
            return 0;
        if (row[ENEMY_LIVE_HP] < 0 || row[ENEMY_LIVE_HP] > 255 ||
            row[ENEMY_LIVE_YAW] < 0 || row[ENEMY_LIVE_YAW] > 1023 ||
            row[ENEMY_LIVE_CLIP] < 0 || row[ENEMY_LIVE_CLIP] > 0xFFFF)
            return 0;
        /* Positions are whole world units and the frame is fixed hundredths. */
        if (row[ENEMY_LIVE_X] < -200000 || row[ENEMY_LIVE_X] > 200000 ||
            row[ENEMY_LIVE_Y] < -200000 || row[ENEMY_LIVE_Y] > 200000 ||
            row[ENEMY_LIVE_Z] < -200000 || row[ENEMY_LIVE_Z] > 200000 ||
            row[ENEMY_LIVE_FRAME] < -100000 || row[ENEMY_LIVE_FRAME] > 100000)
            return 0;
    }

    clear_bytes(s_net_have, ENEMY_MAX_INSTANCES);
    s_net_count = records;
    s_live_seq = seq;
    for (i = 0; i < records; ++i)
    {
        const int *row = &values[i * ENEMY_LIVE_STRIDE];
        int index = row[ENEMY_LIVE_INDEX];

        s_net_index[i] = (unsigned short)index;
        s_net_entity[i] = row[ENEMY_LIVE_ENTITY];
        s_net_hp[i] = row[ENEMY_LIVE_HP];
        s_net_x[i] = (float)row[ENEMY_LIVE_X];
        s_net_y[i] = (float)row[ENEMY_LIVE_Y];
        s_net_z[i] = (float)row[ENEMY_LIVE_Z];
        s_net_yaw[i] = row[ENEMY_LIVE_YAW];
        s_net_clip[i] = row[ENEMY_LIVE_CLIP];
        s_net_frame[i] = (float)row[ENEMY_LIVE_FRAME] / 100.0f;
        s_net_fresh[i] = 1;
        s_net_have[index] = 1;
    }
    return 1;
}

static void enqueue_enemy_hit(unsigned int index, unsigned int amount)
{
    unsigned int tail;

    if (!s_session_active || index >= ENEMY_MAX_INSTANCES || !amount)
        return;
    if (s_hit_count >= ENEMY_HIT_QUEUE)
        return;
    tail = (s_hit_head + s_hit_count) % ENEMY_HIT_QUEUE;
    s_hits[tail].index = (unsigned short)index;
    s_hits[tail].amount = (unsigned short)(amount > 255u ? 255u : amount);
    ++s_hit_count;
}

/* The authority consumes a forwarded player hit.  Non-lethal damage writes the
 * shared health directly; a lethal amount arms the native zero-health path so
 * the real reaction, counters and cleanup run and the death broadcasts. */
static void apply_remote_enemy_hit(unsigned int index, unsigned int amount)
{
    void *actor = index < ENEMY_MAX_INSTANCES ? s_live_actors[index] : 0;
    unsigned int health;

    if (!actor_matches_index(actor, index) || !amount)
        return;
    health = ACTOR_HEALTH(actor);
    if (health == 0 || (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING))
        return;
    if (amount >= health)
        bitmap_set(s_remote_kill_pending, index);
    else
        ACTOR_HEALTH(actor) = (unsigned char)(health - amount);
}

static void enemy_live_hold_one(unsigned int index)
{
    void *actor;

    if (index >= ENEMY_MAX_INSTANCES || s_held[index])
        return;
    /* A dead or dying enemy must keep its own removal path; do not freeze it. */
    if (bitmap_test(s_dead_bitmap, index) ||
        bitmap_test(s_remote_kill_pending, index))
        return;
    actor = s_live_actors[index];
    if (!actor_matches_index(actor, index))
        return;
    s_hold_ai[index] = ACTOR_AI(actor);
    ACTOR_AI(actor) = enemy_hold_noop;
    s_held[index] = 1;
    /* Freeze native integration so the streamed transform is authoritative. */
    ACTOR_VEL_X(actor) = 0.0f;
    ACTOR_VEL_Y(actor) = 0.0f;
    ACTOR_VEL_Z(actor) = 0.0f;
}

static void enemy_sync_scheduler_hold(void)
{
    int i;

    s_armed_index = -1;
    s_held_count = 0;
    if (s_role != ENEMY_ROLE_REPLICA)
        return;
    for (i = 0; i < s_net_count; ++i)
    {
        enemy_live_hold_one(s_net_index[i]);
        if (s_held[s_net_index[i]])
            ++s_held_count;
    }
}

static void enemy_sync_scheduler_apply(void)
{
    int i;

    s_applied_count = 0;
    if (s_role != ENEMY_ROLE_REPLICA)
        return;
    for (i = 0; i < s_net_count; ++i)
    {
        unsigned int index = s_net_index[i];
        void *actor = index < ENEMY_MAX_INSTANCES ? s_live_actors[index] : 0;
        void *object;
        float x;
        float y;
        float z;
        unsigned short yaw;

        if (!actor_matches_index(actor, index) ||
            !s_net_have[index])
            continue;
        /* Never fight a death that is already in flight: a remote ED set the
         * dead bit and the native removal pipeline now owns this actor. */
        if (bitmap_test(s_dead_bitmap, index) ||
            bitmap_test(s_remote_kill_pending, index) ||
            ACTOR_HEALTH(actor) == 0 ||
            (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING))
            continue;
        object = ACTOR_OBJECT(actor);
        if (!object)
            continue;
        /* Bounded convergence toward the authority transform. */
        x = OBJECT_FLOAT(object, 0x08);
        y = OBJECT_FLOAT(object, 0x0C);
        z = OBJECT_FLOAT(object, 0x10);
        if (!s_applied_valid[index] || s_net_fresh[i])
        {
            if (!s_applied_valid[index])
            {
                x = s_net_x[i];
                y = s_net_y[i];
                z = s_net_z[i];
            }
            else
            {
                x += (s_net_x[i] - x) * 0.5f;
                y += (s_net_y[i] - y) * 0.5f;
                z += (s_net_z[i] - z) * 0.5f;
            }
        }
        else
        {
            x += (s_net_x[i] - x) * 0.35f;
            y += (s_net_y[i] - y) * 0.35f;
            z += (s_net_z[i] - z) * 0.35f;
        }
        OBJECT_FLOAT(object, 0x08) = x;
        OBJECT_FLOAT(object, 0x0C) = y;
        OBJECT_FLOAT(object, 0x10) = z;
        yaw = (unsigned short)s_net_yaw[i];
        OBJECT_HALF(object, 0x16) = yaw;
        if (s_net_fresh[i])
        {
            if (!s_applied_valid[index] ||
                s_applied_clip[index] != (unsigned int)s_net_clip[i])
            {
                func_8021664C_5D1B1C(actor, (unsigned int)s_net_clip[i],
                                     0.05f, 0);
                s_applied_clip[index] = (unsigned int)s_net_clip[i];
            }
            OBJECT_FLOAT(object, 0x28) = s_net_frame[i];
            s_net_fresh[i] = 0;
        }
        ACTOR_HEALTH(actor) = s_net_hp[i] > 0 ? (unsigned char)s_net_hp[i] : 1;
        s_applied_valid[index] = 1;
        ++s_applied_count;
    }
}

/* ── Authority player targeting ───────────────────────────────────────── */

/* Run a tracked enemy's real AI with the native player object temporarily
 * moved onto the chosen remote player, so enemies that pursue or attack "the
 * player" instead pursue that teammate.  The trampoline exists so this swap is
 * scoped to one enemy's callback and never touches the local player's own
 * update, rendering or camera work. */
static void enemy_target_trampoline(void *task, void *object)
{
    int index = find_live_actor(task);
    EnemyAiCallback real = index >= 0
        ? (EnemyAiCallback)((unsigned long)s_owner_ai[index] &
                            ~ENEMY_CALLBACK_DISABLED)
        : 0;
    void *player;
    void *enemy_object;
    int best = -1;
    float best_dist = 0.0f;
    int k;
    float saved_x;
    float saved_y;
    float saved_z;

    if (!real)
        return;

    /* Pick the player nearest this enemy from this frame's candidate list. */
    enemy_object = object ? object : ACTOR_OBJECT(task);
    if (enemy_object && s_player_target_count > 0)
    {
        float ex = OBJECT_FLOAT(enemy_object, 0x08);
        float ey = OBJECT_FLOAT(enemy_object, 0x0C);
        float ez = OBJECT_FLOAT(enemy_object, 0x10);

        for (k = 0; k < s_player_target_count; ++k)
        {
            float dx = s_player_targets[k].x - ex;
            float dy = s_player_targets[k].y - ey;
            float dz = s_player_targets[k].z - ez;
            float dist = dx * dx + dy * dy + dz * dz;

            if (best < 0 || dist < best_dist)
            {
                best = k;
                best_dist = dist;
            }
        }
    }
    if (best < 0)
    {
        real(task, object);
        return;
    }
    s_active_target[2] = s_player_targets[best].x;
    s_active_target[3] = s_player_targets[best].y;
    s_active_target[4] = s_player_targets[best].z;
    s_active_target_remote =
        s_player_targets[best].cid != (int)anchor_get_client_id();

    player = D_801FC60C_5B851C;
    if (!s_active_target_remote || !player)
    {
        real(task, object);
        return;
    }
    saved_x = OBJECT_FLOAT(player, 0x08);
    saved_y = OBJECT_FLOAT(player, 0x0C);
    saved_z = OBJECT_FLOAT(player, 0x10);
    OBJECT_FLOAT(player, 0x08) = s_active_target[2];
    OBJECT_FLOAT(player, 0x0C) = s_active_target[3];
    OBJECT_FLOAT(player, 0x10) = s_active_target[4];
    real(task, object);
    OBJECT_FLOAT(player, 0x08) = saved_x;
    OBJECT_FLOAT(player, 0x0C) = saved_y;
    OBJECT_FLOAT(player, 0x10) = saved_z;
}

static void enemy_target_install(void)
{
    int i;

    /* Without a remote candidate the relocation is a no-op; leave the native
     * AI untouched so single-player behavior is byte-for-byte stock. */
    if (s_role != ENEMY_ROLE_OWNER || !s_target_has_remote)
        return;
    for (i = 0; i < s_instance_count && i < ENEMY_MAX_INSTANCES; ++i)
    {
        void *actor = s_live_actors[i];
        void *callback;

        if (!s_entity_ids[i] || !actor_matches_index(actor, (unsigned int)i))
            continue;
        callback = ACTOR_AI(actor);
        /* Keep the wrapper around whatever callback the AI most recently set,
         * preserving the scheduler's disable tag so paused actors stay paused. */
        if (((unsigned long)callback & ~ENEMY_CALLBACK_DISABLED) ==
            ((unsigned long)enemy_target_trampoline & ~ENEMY_CALLBACK_DISABLED))
            continue;
        s_owner_ai[i] = callback;
        ACTOR_AI(actor) = (void *)((unsigned long)enemy_target_trampoline |
                                   ((unsigned long)callback &
                                    ENEMY_CALLBACK_DISABLED));
    }
}

static void enemy_target_remove(void)
{
    int i;

    for (i = 0; i < ENEMY_MAX_INSTANCES; ++i)
    {
        void *actor = s_live_actors[i];
        void *callback;

        if (!s_owner_ai[i] || !actor)
            continue;
        callback = ACTOR_AI(actor);
        /* Only restore if the AI did not advance its own state callback. */
        if (((unsigned long)callback & ~ENEMY_CALLBACK_DISABLED) ==
            ((unsigned long)enemy_target_trampoline & ~ENEMY_CALLBACK_DISABLED))
            ACTOR_AI(actor) = s_owner_ai[i];
        s_owner_ai[i] = 0;
    }
}

static void enemy_live_send(void)
{
    char *team_id;
    int records;

    if (!s_roster_valid || !anchor_is_connected() || anchor_is_disabled())
        return;
    records = enemy_live_encode();
    if (records <= 0)
        return;
    team_id = anchor_get_team_id();
    if (!team_id || !team_id[0])
    {
        if (team_id)
            recomp_free(team_id);
        return;
    }
    if (anchor_send_custom_packet(ENEMY_LIVE_PACKET, s_send_buf, team_id, 0, 0))
    {
        ++s_live_seq;
        ++s_live_sent_ok;
    }
    recomp_free(team_id);
}

static int enemy_live_send_hit(void)
{
    OutgoingEnemyHit *hit;
    MnsgJsonObjectWriter writer;
    char payload[96];
    unsigned int authority;
    int sent;

    if (!s_hit_count || s_role != ENEMY_ROLE_REPLICA)
        return 0;
    authority = s_authority;
    if (!authority || authority == anchor_get_client_id())
        return 0;
    hit = &s_hits[s_hit_head];
    mnsg_json_writer_begin(&writer, payload, (unsigned int)sizeof(payload));
    if (!mnsg_json_writer_add_s32(&writer, "r", (int)s_room) ||
        !mnsg_json_writer_add_s32(&writer, "s", (int)s_signature) ||
        !mnsg_json_writer_add_s32(&writer, "i", (int)hit->index) ||
        !mnsg_json_writer_add_s32(&writer, "a", (int)hit->amount) ||
        !mnsg_json_writer_finish(&writer))
        return 0;
    sent = anchor_send_custom_packet(ENEMY_HIT_PACKET, payload, "",
                                     authority, 0);
    if (sent)
    {
        s_hit_head = (s_hit_head + 1u) % ENEMY_HIT_QUEUE;
        --s_hit_count;
    }
    return sent;
}

static char hex_digit(unsigned int value)
{
    value &= 0xFu;
    return (char)(value < 10u ? ('0' + value) : ('a' + value - 10u));
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return (int)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F')
        return (int)(c - 'A') + 10;
    return -1;
}

static void bitmap_to_hex(const unsigned char *bitmap, char *out)
{
    signed int last = ENEMY_BITMAP_BYTES - 1;
    unsigned int i;

    while (last >= 0 && bitmap[last] == 0)
        --last;
    if (last < 0)
    {
        out[0] = '\0';
        return;
    }
    for (i = 0; i <= (unsigned int)last; ++i)
    {
        out[i * 2] = hex_digit(bitmap[i] >> 4);
        out[i * 2 + 1] = hex_digit(bitmap[i]);
    }
    out[(last + 1) * 2] = '\0';
}

static int bitmap_from_hex(const char *text, unsigned char *out)
{
    unsigned int length = 0;
    unsigned int i;

    if (!text || !out)
        return 0;
    while (text[length])
    {
        if (length >= ENEMY_BITMAP_HEX_CHARS ||
            hex_value(text[length]) < 0)
            return 0;
        ++length;
    }
    if ((length & 1u) != 0)
        return 0;

    clear_bytes(out, ENEMY_BITMAP_BYTES);
    for (i = 0; i < length / 2u; ++i)
    {
        int high = hex_value(text[i * 2]);
        int low = hex_value(text[i * 2 + 1]);
        out[i] = (unsigned char)((high << 4) | low);
    }
    return 1;
}

static void apply_snapshot_bitmap(const unsigned char *incoming)
{
    unsigned int i;
    int changed = 0;

    if (!incoming || !s_roster_valid)
        return;

    for (i = 0; i < s_instance_count; ++i)
    {
        void *actor;

        if (!s_entity_ids[i] || !bitmap_test(incoming, i) ||
            !bitmap_set(s_dead_bitmap, i))
            continue;
        changed = 1;

        /* Snapshot state describes a death that predates this entrant.  If
         * the actor already initialized while the reply was in flight, retire
         * it through the native remove-pending path without replaying drops. */
        actor = s_live_actors[i];
        if (actor_matches_index(actor, i) &&
            s_death_actors[i] != actor)
        {
            ACTOR_STATUS(actor) |= ACTOR_STATUS_REMOVE_PENDING;
            s_live_actors[i] = 0;
        }
    }
    if (changed)
        s_publish_pending = 1;
}

static void import_occupied_room_state(void)
{
    char *state_hex;
    unsigned char incoming[ENEMY_BITMAP_BYTES];

    if (!s_roster_valid || !anchor_is_connected() || anchor_is_disabled())
        return;

    state_hex = anchor_get_enemy_room_state((unsigned int)s_room,
                                            (unsigned int)s_signature);
    if (!state_hex)
        return;
    if (bitmap_from_hex(state_hex, incoming))
        apply_snapshot_bitmap(incoming);
    recomp_free(state_hex);
}

static void begin_room_network_state(void)
{
    char bitmap_hex[ENEMY_BITMAP_HEX_CHARS + 1];

    if (!s_roster_valid || !anchor_is_connected() || anchor_is_disabled() ||
        anchor_get_client_id() == 0)
        return;

    /* A room rebuild starts with an empty local bitmap, while a reconnect in
     * the same loaded room retains its native deaths.  Publishing that exact
     * state before importing peers preserves both cases. */
    bitmap_to_hex(s_dead_bitmap, bitmap_hex);
    if (!anchor_set_enemy_room_state((unsigned int)s_room,
                                     (unsigned int)s_signature, bitmap_hex))
        s_publish_pending = 1;
    if (bitmap_hex[0])
        s_broadcast_pending = 1;
    import_occupied_room_state();
    s_request_pending = 1;
    s_setup_yield_pending = 1;
}

static void clear_advertised_room_state(void)
{
    if (!anchor_is_connected() || anchor_is_disabled() ||
        anchor_get_client_id() == 0)
        return;

    /* Keep replace-style client metadata from carrying the last enemy room
     * through a room that has no supported roster.  Exact room+signature
     * matching then cannot resurrect a completed, now-empty occupancy epoch. */
    anchor_set_enemy_room_state((unsigned int)s_room, 0, "");
    s_setup_yield_pending = 1;
}

static void clear_room_state(void)
{
    s_room = ENEMY_INVALID_ROOM;
    s_signature = 0;
    s_instance_count = 0;
    s_roster_valid = 0;
    clear_pointers((void **)s_source_instances, ENEMY_MAX_INSTANCES);
    clear_words(s_entity_ids, ENEMY_MAX_INSTANCES);
    clear_pointers(s_live_actors, ENEMY_MAX_INSTANCES);
    clear_pointers(s_death_actors, ENEMY_MAX_INSTANCES);
    clear_bytes(s_dead_bitmap, ENEMY_BITMAP_BYTES);
    clear_bytes(s_remote_kill_pending, ENEMY_BITMAP_BYTES);
    s_publish_pending = 0;
    s_request_pending = 0;
    s_broadcast_pending = 0;
    s_response_client_id = 0;
    s_setup_yield_pending = 0;
}

static void record_roster_source(EnemyActorInstance *source,
                                 unsigned short file_id,
                                 unsigned int index,
                                 unsigned short *hash,
                                 unsigned int *enemy_count)
{
    EnemyActorDefinition *definition =
        resolve_definition(source->definition, file_id);
    unsigned short entity_id = definition ? definition->actor_id : 0;

    s_source_instances[index] = source;
    *hash = signature_mix(*hash, (unsigned short)index);
    *hash = signature_mix(*hash, entity_id);
    *hash = signature_mix(*hash, (unsigned short)source->position.x);
    *hash = signature_mix(*hash, (unsigned short)source->position.y);
    *hash = signature_mix(*hash, (unsigned short)source->position.z);
    if (is_regular_enemy(entity_id))
    {
        s_entity_ids[index] = entity_id;
        ++*enemy_count;
    }
}

static void build_room_roster(void)
{
    EnemyStageActorMetadata *metadata;
    EnemyActorInstance *instances;
    EnemyActorPartitionConfig *partition_config;
    void **partition_grid;
    EnemyActorInstance *grouped_sources[ENEMY_MAX_INSTANCES];
    unsigned short hash = 0x6D5Au;
    unsigned short previous_signature = s_signature;
    unsigned char previous_dead_bitmap[ENEMY_BITMAP_BYTES];
    unsigned int preserve_dead_bitmap =
        s_roster_valid && s_room == D_800C7AB2;
    unsigned int enemy_count = 0;
    unsigned int grouped_count = 0;
    unsigned int total_count = 0;
    unsigned int i;
    int terminated = 0;

    if (preserve_dead_bitmap)
    {
        for (i = 0; i < ENEMY_BITMAP_BYTES; ++i)
            previous_dead_bitmap[i] = s_dead_bitmap[i];
    }
    clear_room_state();
    s_room = D_800C7AB2;
    if ((unsigned int)s_room >= ENEMY_ROOM_METADATA_COUNT)
        goto invalid_roster;
    metadata = D_80231300_5EC7D0[s_room];
    if (!metadata || !metadata->actor_instances)
        goto invalid_roster;

    /* The native pointer resolver crashes intentionally if this wave is not
     * present.  func_8020D848 is retried while actor data is loading, so leave
     * an invalid roster and rebuild from its next invocation instead. */
    if (metadata->actor_data_file_id == 0 ||
        func_800141C4_14DC4(
            (unsigned int)metadata->actor_data_file_id) == -1)
        goto invalid_roster;

    instances = (EnemyActorInstance *)resolve_actor_data_pointer(
        metadata->actor_instances, metadata->actor_data_file_id);
    if (!instances)
        goto invalid_roster;

    for (i = 0; i < ENEMY_MAX_INSTANCES; ++i)
    {
        if (!instances[i].definition)
        {
            terminated = 1;
            break;
        }
        record_roster_source(&instances[i], metadata->actor_data_file_id,
                             total_count, &hash, &enemy_count);
        ++total_count;
    }
    if (!terminated)
        goto invalid_roster;

    partition_config = 0;
    partition_grid = 0;
    if (metadata->actor_partition_configuration)
        partition_config = (EnemyActorPartitionConfig *)
            resolve_actor_data_pointer(
                metadata->actor_partition_configuration,
                metadata->actor_data_file_id);
    if (metadata->actor_partitions)
        partition_grid = (void **)resolve_actor_data_pointer(
            metadata->actor_partitions, metadata->actor_data_file_id);

    if (partition_config && partition_grid &&
        partition_config->cells_x && partition_config->cells_y &&
        partition_config->cells_z)
    {
        unsigned int cells_xy;
        unsigned int cell_count;
        unsigned int cell;

        if ((unsigned int)partition_config->cells_x >
                ENEMY_MAX_PARTITION_CELLS /
                    (unsigned int)partition_config->cells_y)
            goto invalid_roster;
        cells_xy = (unsigned int)partition_config->cells_x *
                   (unsigned int)partition_config->cells_y;
        if (cells_xy > ENEMY_MAX_PARTITION_CELLS /
                           (unsigned int)partition_config->cells_z)
            goto invalid_roster;
        cell_count = cells_xy * (unsigned int)partition_config->cells_z;

        /* Enumerate the native partition grid instead of guessing through raw
         * bytes after the normal terminator.  Randomizer replacements may turn
         * definition pointers into 0x81xxxxxx allocations, while the source
         * descriptor addresses used here remain stable. */
        for (cell = 0; cell < cell_count; ++cell)
        {
            EnemyActorInstance *list;
            unsigned int entry;
            int list_terminated = 0;

            if (!partition_grid[cell])
                continue;
            list = (EnemyActorInstance *)resolve_actor_data_pointer(
                partition_grid[cell], metadata->actor_data_file_id);
            if (!list)
                continue;

            for (entry = 0; entry < ENEMY_MAX_INSTANCES; ++entry)
            {
                EnemyActorInstance *candidate = &list[entry];
                EnemyActorDefinition *definition;
                unsigned int existing;
                int duplicate = 0;

                if (!candidate->definition)
                {
                    list_terminated = 1;
                    break;
                }
                definition = resolve_definition(candidate->definition,
                                                metadata->actor_data_file_id);
                if (!definition)
                {
                    list_terminated = 1;
                    break;
                }
                for (existing = 0; existing < total_count; ++existing)
                {
                    if (s_source_instances[existing] == candidate)
                    {
                        duplicate = 1;
                        break;
                    }
                }
                for (existing = 0;
                     !duplicate && existing < grouped_count;
                     ++existing)
                {
                    if (grouped_sources[existing] == candidate)
                        duplicate = 1;
                }
                if (duplicate)
                    continue;
                if (total_count + grouped_count >= ENEMY_MAX_INSTANCES)
                    goto invalid_roster;
                grouped_sources[grouped_count++] = candidate;
            }
            if (!list_terminated)
                goto invalid_roster;
        }
    }

    /* Partition cells can refer to the same list.  Sorting the deduplicated
     * source addresses recreates their deterministic actor-data order and the
     * randomizer's normal-then-grouped overall index. */
    for (i = 1; i < grouped_count; ++i)
    {
        EnemyActorInstance *source = grouped_sources[i];
        unsigned int source_address = (unsigned int)source;
        unsigned int insert = i;

        while (insert > 0 &&
               (unsigned int)grouped_sources[insert - 1u] > source_address)
        {
            grouped_sources[insert] = grouped_sources[insert - 1u];
            --insert;
        }
        grouped_sources[insert] = source;
    }

    for (i = 0; i < grouped_count; ++i)
    {
        record_roster_source(grouped_sources[i], metadata->actor_data_file_id,
                             total_count, &hash, &enemy_count);
        ++total_count;
    }

    if (enemy_count == 0)
        goto invalid_roster;

    s_instance_count = (unsigned short)total_count;
    hash = signature_mix(hash, s_instance_count);
    hash = signature_mix(hash, (unsigned short)enemy_count);
    s_signature = hash ? hash : 1u;
    s_roster_valid = 1;

    /* A native actor-manager refresh can rebuild the same loaded room without
     * changing its numeric ID.  Preserve its monotonic occupied-room deaths
     * only when the complete roster signature still matches; live pointers
     * are intentionally reacquired by the new actor initializers. */
    if (preserve_dead_bitmap && previous_signature == s_signature)
    {
        for (i = 0; i < ENEMY_BITMAP_BYTES; ++i)
            s_dead_bitmap[i] = previous_dead_bitmap[i];
    }

    begin_room_network_state();

    recomp_printf("[EnemySync] Room 0x%X roster: %u actors, %u enemies, sig=0x%X.\n",
                  s_room, (unsigned int)s_instance_count, enemy_count,
                  s_signature);
    return;

invalid_roster:
    {
        unsigned short room = s_room;

        clear_room_state();
        s_room = room;
        clear_advertised_room_state();
    }
}

static void enqueue_local_death(unsigned int index)
{
    unsigned int tail;
    OutgoingEnemyDeath *event;

    if (s_outgoing_count >= ENEMY_OUTGOING_CAPACITY)
    {
        recomp_printf("[EnemySync] Outgoing death queue full; room=0x%X index=%u retained in bitmap.\n",
                      s_room, index);
        return;
    }

    tail = (s_outgoing_head + s_outgoing_count) %
           ENEMY_OUTGOING_CAPACITY;
    event = &s_outgoing[tail];
    event->room = s_room;
    event->signature = s_signature;
    event->entity_id = s_entity_ids[index];
    event->instance_index = (unsigned char)index;
    ++s_outgoing_count;
}

static char *team_id_or_null(void)
{
    char *team_id = anchor_get_team_id();

    if (!team_id || !team_id[0])
    {
        if (team_id)
            recomp_free(team_id);
        return 0;
    }
    return team_id;
}

static int send_death_event(void)
{
    OutgoingEnemyDeath *event;
    MnsgJsonObjectWriter writer;
    char payload[80];
    char *team_id;
    int sent;

    if (s_outgoing_count == 0)
        return 0;
    event = &s_outgoing[s_outgoing_head];

    mnsg_json_writer_begin(&writer, payload, (unsigned int)sizeof(payload));
    if (!mnsg_json_writer_add_s32(&writer, "r", event->room) ||
        !mnsg_json_writer_add_s32(&writer, "s", event->signature) ||
        !mnsg_json_writer_add_s32(&writer, "i", event->instance_index) ||
        !mnsg_json_writer_add_s32(&writer, "e", event->entity_id) ||
        !mnsg_json_writer_finish(&writer))
        return 0;

    team_id = team_id_or_null();
    if (!team_id)
        return 0;
    sent = anchor_send_custom_packet(PACKET_ENEMY_DEATH, payload,
                                     team_id, 0, 0);
    recomp_free(team_id);
    if (!sent)
        return 0;

    recomp_printf("[EnemySync] Sent death room=0x%X index=%u entity=0x%X.\n",
                  event->room, (unsigned int)event->instance_index,
                  event->entity_id);
    s_outgoing_head = (s_outgoing_head + 1u) %
                      ENEMY_OUTGOING_CAPACITY;
    --s_outgoing_count;
    return 1;
}

static int publish_room_state(void)
{
    char bitmap_hex[ENEMY_BITMAP_HEX_CHARS + 1];

    if (!s_roster_valid)
        return 0;
    bitmap_to_hex(s_dead_bitmap, bitmap_hex);
    if (!anchor_set_enemy_room_state((unsigned int)s_room,
                                     (unsigned int)s_signature,
                                     bitmap_hex))
        return 0;
    s_publish_pending = 0;
    return 1;
}

static int send_room_request(void)
{
    MnsgJsonObjectWriter writer;
    char payload[40];
    char *team_id;
    int sent;

    if (!s_roster_valid)
        return 0;
    mnsg_json_writer_begin(&writer, payload, (unsigned int)sizeof(payload));
    if (!mnsg_json_writer_add_s32(&writer, "r", s_room) ||
        !mnsg_json_writer_add_s32(&writer, "s", s_signature) ||
        !mnsg_json_writer_finish(&writer))
        return 0;

    team_id = team_id_or_null();
    if (!team_id)
        return 0;
    sent = anchor_send_custom_packet(PACKET_ENEMY_REQUEST, payload,
                                     team_id, 0, 0);
    recomp_free(team_id);
    if (sent)
        s_request_pending = 0;
    return sent;
}

static int send_room_response(void)
{
    MnsgJsonObjectWriter writer;
    char bitmap_hex[ENEMY_BITMAP_HEX_CHARS + 1];
    char payload[128];
    unsigned int requester = s_response_client_id;
    int sent;

    if (!requester || !s_roster_valid)
        return 0;
    bitmap_to_hex(s_dead_bitmap, bitmap_hex);
    mnsg_json_writer_begin(&writer, payload, (unsigned int)sizeof(payload));
    if (!mnsg_json_writer_add_s32(&writer, "r", s_room) ||
        !mnsg_json_writer_add_s32(&writer, "s", s_signature) ||
        !mnsg_json_writer_add_string(&writer, "b", bitmap_hex) ||
        !mnsg_json_writer_finish(&writer))
        return 0;

    sent = anchor_send_custom_packet(PACKET_ENEMY_STATE, payload,
                                     "", requester, 0);
    if (sent)
        s_response_client_id = 0;
    return sent;
}

static int broadcast_room_state(void)
{
    MnsgJsonObjectWriter writer;
    char bitmap_hex[ENEMY_BITMAP_HEX_CHARS + 1];
    char payload[128];
    char *team_id;
    int sent;

    if (!s_broadcast_pending || !s_roster_valid)
        return 0;
    bitmap_to_hex(s_dead_bitmap, bitmap_hex);
    if (!bitmap_hex[0])
    {
        s_broadcast_pending = 0;
        return 0;
    }
    mnsg_json_writer_begin(&writer, payload, (unsigned int)sizeof(payload));
    if (!mnsg_json_writer_add_s32(&writer, "r", s_room) ||
        !mnsg_json_writer_add_s32(&writer, "s", s_signature) ||
        !mnsg_json_writer_add_string(&writer, "b", bitmap_hex) ||
        !mnsg_json_writer_finish(&writer))
        return 0;

    team_id = team_id_or_null();
    if (!team_id)
        return 0;
    sent = anchor_send_custom_packet(PACKET_ENEMY_STATE, payload,
                                     team_id, 0, 0);
    recomp_free(team_id);
    if (sent)
        s_broadcast_pending = 0;
    return sent;
}

static unsigned int packet_sender(const char *json)
{
    unsigned int sender = 0;

    mnsg_json_get_u32(json, "clientId", &sender);
    return sender;
}

static int packet_is_remote(const char *json)
{
    unsigned int own_id = anchor_get_client_id();
    unsigned int sender = packet_sender(json);

    return own_id == 0 || sender == 0 || sender != own_id;
}

static int packet_matches_roster(const char *json,
                                 unsigned short *room,
                                 unsigned short *signature)
{
    return s_roster_valid &&
           mnsg_json_get_u16(json, "r", room) &&
           mnsg_json_get_u16(json, "s", signature) &&
           *room == s_room && *signature == s_signature;
}

int enemy_sync_handle_packet(const char *json)
{
    unsigned short room;
    unsigned short signature;

    if (!json)
        return 0;

    if (mnsg_json_string_equals(json, "type", PACKET_ENEMY_DEATH))
    {
        unsigned short index;
        unsigned short entity_id;

        if (!s_session_active || !packet_is_remote(json) ||
            !packet_matches_roster(json, &room, &signature) ||
            !mnsg_json_get_u16(json, "i", &index) ||
            !mnsg_json_get_u16(json, "e", &entity_id) ||
            index >= s_instance_count || index >= ENEMY_MAX_INSTANCES ||
            !s_entity_ids[index] || s_entity_ids[index] != entity_id)
            return 1;

        if (bitmap_set(s_dead_bitmap, index))
        {
            void *actor = s_live_actors[index];

            if (actor_matches_index(actor, index) &&
                ACTOR_HEALTH(actor) > 0 &&
                (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) == 0)
                bitmap_set(s_remote_kill_pending, index);

            s_publish_pending = 1;
            recomp_printf("[EnemySync] Accepted death room=0x%X index=%u entity=0x%X.\n",
                          room, (unsigned int)index, entity_id);
        }
        return 1;
    }

    if (mnsg_json_string_equals(json, "type", PACKET_ENEMY_REQUEST))
    {
        unsigned int requester = packet_sender(json);

        if (s_session_active && packet_is_remote(json) && requester &&
            packet_matches_roster(json, &room, &signature))
            s_response_client_id = requester;
        return 1;
    }

    if (mnsg_json_string_equals(json, "type", PACKET_ENEMY_STATE))
    {
        char bitmap_hex[ENEMY_BITMAP_HEX_CHARS + 1];
        unsigned char incoming[ENEMY_BITMAP_BYTES];

        if (s_session_active && packet_is_remote(json) &&
            packet_matches_roster(json, &room, &signature) &&
            mnsg_json_get_string(json, "b", bitmap_hex,
                                 (unsigned int)sizeof(bitmap_hex)) &&
            bitmap_from_hex(bitmap_hex, incoming))
            apply_snapshot_bitmap(incoming);
        return 1;
    }

    if (mnsg_json_string_equals(json, "type", ENEMY_LIVE_PACKET))
    {
        unsigned int sender = packet_sender(json);

        /* Only the elected room authority may drive the shared simulation. */
        if (s_session_active && s_role == ENEMY_ROLE_REPLICA &&
            packet_is_remote(json) && s_authority && sender == s_authority &&
            packet_matches_roster(json, &room, &signature))
        {
            /* A new authority restarts its sequence at zero; do not reject its
             * stream with the previous authority's ordering. */
            if (sender != s_live_authority_seen)
            {
                s_live_authority_seen = sender;
                s_live_seq = 0;
            }
            if (enemy_live_accept(json))
            {
                s_live_dirty = 0;
                ++s_live_recv_ok;
            }
        }
        return 1;
    }

    if (mnsg_json_string_equals(json, "type", ENEMY_HIT_PACKET))
    {
        unsigned short index;
        unsigned short amount;

        if (s_session_active && s_role == ENEMY_ROLE_OWNER &&
            packet_is_remote(json) &&
            packet_matches_roster(json, &room, &signature) &&
            mnsg_json_get_u16(json, "i", &index) &&
            mnsg_json_get_u16(json, "a", &amount))
            apply_remote_enemy_hit(index, amount);
        return 1;
    }

    return 0;
}

void enemy_sync_update(void)
{
    int connected = anchor_is_connected() && !anchor_is_disabled();
    int starting_session;

    /* A clientId of zero means the handshake has not completed.  Keep the
     * request armed until ALL_CLIENT_STATE assigns our real sender identity;
     * otherwise peers correctly reject the unaddressable request. */
    if (!connected || anchor_get_client_id() == 0)
    {
        s_session_active = 0;
        return;
    }

    starting_session = !s_session_active;
    s_session_active = 1;

    /* Elect the single room authority and swap roles when the lowest matching
     * occupant changes.  A role swap invalidates the whole live stream. */
    s_authority = anchor_get_enemy_authority();
    {
        int role = (s_authority == 0 || anchor_get_client_id() == 0)
                       ? ENEMY_ROLE_NONE
                       : (s_authority == anchor_get_client_id()
                              ? ENEMY_ROLE_OWNER : ENEMY_ROLE_REPLICA);
        if (role != s_role)
        {
            enemy_live_clear_net();
            s_role = role;
        }
    }

    /* The authority collects this frame's candidate players so each enemy can
     * pursue whichever one is nearest to it. */
    if (s_role == ENEMY_ROLE_OWNER)
    {
        int k;

        s_player_target_count = anchor_player_models_get_boss_targets(
            s_player_targets, ENEMY_TARGET_MAX);
        s_target_has_remote = 0;
        for (k = 0; k < s_player_target_count; ++k)
        {
            if (s_player_targets[k].cid != (int)anchor_get_client_id())
            {
                s_target_has_remote = 1;
                break;
            }
        }
    }
    else
    {
        s_player_target_count = 0;
        s_target_has_remote = 0;
    }

    if (++s_dbg_timer >= 60)
    {
        s_dbg_timer = 0;
        recomp_printf("[EnemySync] role=%d auth=%u self=%u roster=%d net=%d held=%u applied=%u tx=%u rx=%u tgts=%d\n",
                      s_role, s_authority, anchor_get_client_id(),
                      s_roster_valid ? 1 : 0, s_net_count,
                      s_held_count, s_applied_count,
                      s_live_sent_ok, s_live_recv_ok, s_player_target_count);
    }

    /* A room without supported enemies is still a completed roster scan.
     * Re-scan only on a real room change; the native spawn-stage hook rebuilds
     * naturally if stage data is replaced in the same numeric room. */
    if (s_room != D_800C7AB2)
    {
        /* Room IDs can change before their actor-data wave is ready.  Clear
         * the prior occupancy epoch here; the native spawn-stage hook below
         * performs the only roster build once that data is safe to resolve. */
        clear_room_state();
        enemy_live_clear_net();
        s_room = D_800C7AB2;
        clear_advertised_room_state();
    }
    else if (starting_session)
    {
        if (s_roster_valid)
            begin_room_network_state();
        else
            clear_advertised_room_state();
    }

    if (s_setup_yield_pending)
    {
        s_setup_yield_pending = 0;
        return;
    }

    /* One network operation per frame keeps Anchor pressure bounded.  Failed
     * operations retain their captured state and retry without blocking later
     * room transitions or converting deaths into an unbounded server queue. */
    if (s_outgoing_count != 0)
    {
        send_death_event();
        return;
    }
    if (s_broadcast_pending)
    {
        broadcast_room_state();
        return;
    }
    if (s_publish_pending)
    {
        publish_room_state();
        return;
    }
    if (s_request_pending)
    {
        send_room_request();
        return;
    }
    if (s_response_client_id)
    {
        send_room_response();
        return;
    }

    /* Replica: forward captured local damage to the authority. */
    if (s_role == ENEMY_ROLE_REPLICA && s_hit_count)
    {
        enemy_live_send_hit();
        return;
    }

    /* Authority: publish the live enemy set on a fixed low cadence. */
    if (s_role == ENEMY_ROLE_OWNER && s_roster_valid)
    {
        if (s_live_timer == 0)
        {
            enemy_live_send();
            s_live_timer = ENEMY_LIVE_FRAMES;
        }
        else
        {
            --s_live_timer;
        }
    }
}

void enemy_sync_reset(void)
{
    enemy_live_clear_net();
    clear_room_state();
    s_session_active = 0;
    s_role = ENEMY_ROLE_NONE;
    s_authority = 0;
    s_outgoing_head = 0;
    s_outgoing_count = 0;

    /* Full resets represent save unload, unlike a transient disconnect.  Clear
     * Python's replace-style metadata now so the next save-load update cannot
     * briefly re-advertise the prior file's room bitmap. */
    if (anchor_is_connected() && !anchor_is_disabled() &&
        anchor_get_client_id() != 0)
        anchor_set_enemy_room_state((unsigned int)D_800C7AB2, 0, "");
}

void enemy_sync_disconnect(void)
{
    enemy_live_clear_net();
    s_session_active = 0;
    s_role = ENEMY_ROLE_NONE;
    s_authority = 0;
    s_outgoing_head = 0;
    s_outgoing_count = 0;
    clear_bytes(s_remote_kill_pending, ENEMY_BITMAP_BYTES);
    s_publish_pending = 0;
    s_request_pending = 0;
    s_broadcast_pending = 0;
    s_response_client_id = 0;
    s_setup_yield_pending = 0;
}

/* This is the native actor-data resolution and initial-spawn stage.  Its PRE
 * hook is retried while the actor-data wave is unavailable, runs after the
 * randomizer's earlier setup hook, and still precedes every static actor init. */
RECOMP_HOOK("func_8020D848_5C8D18")
void enemy_sync_prepare_room_roster(void)
{
    EnemyStageActorMetadata *metadata;
    unsigned short room = D_800C7AB2;

    /* This callback may be attempted while room resources are still loading.
     * Do not clear/publish or touch segmented pointers until the actor wave is
     * present; the native scheduler will invoke this stage again. */
    if ((unsigned int)room >= ENEMY_ROOM_METADATA_COUNT)
        return;
    metadata = D_80231300_5EC7D0[room];
    if (!metadata || metadata->actor_data_file_id == 0 ||
        func_800141C4_14DC4(
            (unsigned int)metadata->actor_data_file_id) == -1)
        return;

    build_room_roster();
}

RECOMP_HOOK("func_80218A54_5D3F24")
void enemy_sync_register_static_enemy(void *actor,
                                      EnemyActorInstance *instance)
{
    int index;
    unsigned short entity_id;

    /* Actor-pool addresses are reused.  Forget stale ownership even when this
     * new spawn is dynamic or belongs to a non-enemy. */
    forget_actor_pointer(actor);

    if (!actor || !s_roster_valid || instance != D_8015CDDC ||
        !D_8015CDE0)
        return;
    index = instance_index(instance);
    if (index < 0 || !s_entity_ids[index])
        return;
    entity_id = D_8015CDE0->actor_id;
    if (entity_id != s_entity_ids[index] || !is_regular_enemy(entity_id))
        return;

    s_live_actors[index] = actor;
    s_death_actors[index] = 0;
    if (bitmap_test(s_dead_bitmap, (unsigned int)index))
    {
        ACTOR_STATUS(actor) |= ACTOR_STATUS_REMOVE_PENDING;
        s_live_actors[index] = 0;
        recomp_printf("[EnemySync] Suppressed dead spawn room=0x%X index=%u entity=0x%X.\n",
                      s_room, (unsigned int)index, entity_id);
    }
}

/* Apply a remote live kill at the last possible point before the game's common
 * damage pipeline.  Health 1 plus the native fast-hit bit produces the real
 * zero-health callback, animation, counters, effects, and actor cleanup. */
RECOMP_HOOK("func_80218E7C_5D434C")
void enemy_sync_arm_remote_lethal_hit(void *actor)
{
    int index = find_live_actor(actor);

    if (index < 0 || !bitmap_test(s_remote_kill_pending,
                                  (unsigned int)index))
        return;
    bitmap_clear(s_remote_kill_pending, (unsigned int)index);

    if (!actor_matches_index(actor, (unsigned int)index) ||
        ACTOR_HEALTH(actor) == 0 ||
        (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) != 0)
        return;

    ACTOR_HEALTH(actor) = 1;
    ACTOR_STATUS(actor) |= ACTOR_STATUS_DAMAGE_PENDING;
    /* The next 18350 call for this actor is our synthetic kill, not a local
     * attack; the replica damage bridge must not forward it back. */
    s_armed_index = index;
    recomp_printf("[EnemySync] Armed native remote death room=0x%X index=%u.\n",
                  s_room, (unsigned int)index);
}

/* func_80218548 is entered only from the common health-zero path.  It is the
 * authoritative local death edge; unloads and distance culls do not call it. */
RECOMP_HOOK("func_80218548_5D3A18")
void enemy_sync_observe_native_death(void *actor)
{
    int index;

    if (!s_roster_valid)
        return;
    index = find_live_actor(actor);
    if (index < 0 || !actor_matches_index(actor, (unsigned int)index))
        return;

    if (bitmap_set(s_dead_bitmap, (unsigned int)index))
    {
        if (s_session_active)
        {
            enqueue_local_death((unsigned int)index);
            s_publish_pending = 1;
        }
        recomp_printf("[EnemySync] Observed local death room=0x%X index=%u entity=0x%X.\n",
                      s_room, (unsigned int)index, s_entity_ids[index]);
    }

    /* A remote native death already set the bit before arming damage, so it
     * reaches this path without enqueuing an echo. */
    bitmap_clear(s_remote_kill_pending, (unsigned int)index);
    s_held[index] = 0;
    s_hold_ai[index] = 0;
    s_death_actors[index] = actor;
    s_live_actors[index] = 0;
}

/* Clear actor-pool ownership on native removal.  Proximity-spawned actors also
 * expose their source descriptor at +0x70, which recovers tracking when a
 * multiplayer session begins after the room itself was already initialized. */
RECOMP_HOOK("func_80218F30_5D4400")
void enemy_sync_track_common_finalizer(void *actor, void *unused)
{
    int index;
    EnemyActorInstance *instance;

    (void)unused;
    if (!actor || !s_roster_valid)
        return;

    index = find_live_actor(actor);
    if (index < 0)
    {
        instance = ACTOR_INSTANCE(actor);
        index = instance_index(instance);
    }

    if (index >= 0 && s_entity_ids[index] &&
        ACTOR_ENTITY_ID(actor) == s_entity_ids[index])
    {
        /* Grouped proximity spawning writes status and source fields after the
         * common initializer returns, so it can overwrite the PRE-hook's first
         * remove bit.  Re-arm it here.  A live ED instead keeps its pending bit
         * so the original finalizer reaches the real lethal-damage pipeline. */
        if (bitmap_test(s_dead_bitmap, (unsigned int)index) &&
            !bitmap_test(s_remote_kill_pending, (unsigned int)index) &&
            s_death_actors[index] != actor)
        {
            ACTOR_STATUS(actor) |= ACTOR_STATUS_REMOVE_PENDING;
        }
        else if (!bitmap_test(s_dead_bitmap, (unsigned int)index))
        {
            s_live_actors[index] = actor;
        }
    }

    if (index >= 0 &&
        (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) != 0)
    {
        if (s_live_actors[index] == actor)
            s_live_actors[index] = 0;
        if (s_death_actors[index] == actor)
            s_death_actors[index] = 0;
        s_held[index] = 0;
        s_hold_ai[index] = 0;
        bitmap_clear(s_remote_kill_pending, (unsigned int)index);
    }
}

/* ── Live authority hooks ─────────────────────────────────────────────── */

/* Replicas hold every streamed enemy's AI for the duration of the scheduler
 * pass so the authority transform is authoritative, then apply it afterwards. */
RECOMP_HOOK("func_80034734_35334")
void enemy_sync_scheduler_begin(void)
{
    enemy_sync_scheduler_hold();
    enemy_target_install();
}

RECOMP_HOOK_RETURN("func_80034734_35334")
void enemy_sync_scheduler_end(void)
{
    enemy_sync_scheduler_apply();
    enemy_target_remove();
}

/* Turn an enemy toward the chosen player by substituting the generic target
 * object at +0x84 only while an actor is actually calling the native turn
 * helper, so unrelated uses of that field are never overwritten. */
RECOMP_HOOK("func_802197D8_5D4CA8")
void enemy_sync_target_begin(void *task, int turn_step)
{
    (void)turn_step;
    s_target_swap_task = 0;
    if (s_role != ENEMY_ROLE_OWNER || !s_active_target_remote || !task)
        return;
    if (find_live_actor(task) < 0 || !ACTOR_TARGET(task))
        return;
    s_target_saved = ACTOR_TARGET(task);
    ACTOR_TARGET(task) = s_active_target;
    s_target_swap_task = task;
}

RECOMP_HOOK_RETURN("func_802197D8_5D4CA8")
void enemy_sync_target_end(void)
{
    if (s_target_swap_task)
    {
        ACTOR_TARGET(s_target_swap_task) = s_target_saved;
        s_target_swap_task = 0;
    }
}

/* Record the animation clip an authority-side enemy selects so replicas can
 * request the same clip instead of advancing their own. */
RECOMP_HOOK("func_8021664C_5D1B1C")
void enemy_sync_capture_clip(void *task, unsigned int animation,
                             float rate, unsigned int flags)
{
    int index;

    (void)rate;
    (void)flags;
    if (s_role != ENEMY_ROLE_OWNER || !task)
        return;
    index = find_live_actor(task);
    if (index >= 0 && actor_matches_index(task, (unsigned int)index))
        s_clip[index] = animation;
}

/* Capture the health a local attack removes from a replica's enemy and forward
 * that exact delta to the authority's shared pool. */
RECOMP_HOOK("func_80218350_5D3820")
void enemy_sync_damage_begin(void *actor)
{
    int index;

    s_damage_index = -1;
    if (s_role != ENEMY_ROLE_REPLICA || !actor)
        return;
    index = find_live_actor(actor);
    if (index < 0 || !actor_matches_index(actor, (unsigned int)index))
        return;
    if (index == s_armed_index)
    {
        /* This is the native kill we armed from a remote death, not a hit. */
        s_armed_index = -1;
        return;
    }
    s_damage_index = index;
    s_damage_before = ACTOR_HEALTH(actor);
}

RECOMP_HOOK_RETURN("func_80218350_5D3820")
void enemy_sync_damage_end(void)
{
    int index = s_damage_index;
    void *actor;
    int after;

    s_damage_index = -1;
    if (index < 0 || index >= ENEMY_MAX_INSTANCES)
        return;
    actor = s_live_actors[index];
    if (!actor_matches_index(actor, (unsigned int)index))
        return;
    after = ACTOR_HEALTH(actor);
    if (s_damage_before > after)
        enqueue_enemy_hit((unsigned int)index,
                          (unsigned int)(s_damage_before - after));
}
