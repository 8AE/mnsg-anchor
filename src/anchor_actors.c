/**
 * @file anchor_actors.c
 * @brief Phantom actor system – renders same-room teammates as in-world objects.
 *
 * Strategy
 * --------
 * Spawning a proper main-character actor at runtime requires loading the
 * character overlay file and calling an undocumented initialisation function,
 * so we use a simpler "ghost clone" approach instead:
 *
 *   1. When the local player's CLS_BG_W becomes available (non-NULL) and there
 *      are teammates in the same raw room, we allocate a pool of CLS_BG_W
 *      structs with recomp_alloc() and memcpy the player's live struct into
 *      each one.  This gives every phantom the same wave / model / display-list
 *      references as the local character, so they are rendered identically to
 *      the local player – a recognisable human figure that works in every room.
 *
 *   2. Each phantom is inserted at the front of the CLS_W.next singly-linked
 *      list that starts at the player's CLS_BG_W.  The game's rendering loop
 *      walks this chain, so the phantoms are drawn automatically.
 *
 *   3. Every game frame we call anchor_get_teammate_positions_json() and update
 *      the floating-point position (VEC3F_W at offset 0x08) of each live
 *      phantom so their on-screen positions follow the latest network data.
 *
 *   4. On room change (detected by watching D_800C7AB2) we unlink all phantoms
 *      from the old player pointer and free them.  They will be re-created on
 *      the next frame once the new player CLS_BG_W is valid.
 *
 * Limitations
 * -----------
 * - The phantoms share the local player's animation frame counter, so they
 *   animate in sync with the player – acceptable for a ghost indicator.
 * - Physics, collision, and game-logic are NOT driven for the clones; they are
 *   purely visual.
 * - The approach relies on the game rendering CLS_BG_W objects in list order.
 *   If the renderer skips unknown entries this will be a no-op (no crash).
 * - The cloned CLS_BG_W may have pointer fields that are not safe to share
 *   (e.g. behaviour callbacks).  Observed in the field this has been stable,
 *   but if crashes occur suspect this file first.
 */

#include "modding.h"
#include "recomputils.h"
#include "anchor.h"

/* =========================================================================
   Tunables
   ========================================================================= */

/** Maximum number of phantom actors (one per teammate slot). */
#define MAX_PHANTOMS 7

/** Frames between position-sync refreshes (teleporting a phantom that far
 *  each frame would look jerky; at 60 fps every 2 frames ≈ 30 Hz is fine). */
#define PHANTOM_REFRESH_FRAMES 2

/** Y-position used as a "parking" value when a phantom slot is not in use.
 *  Must not appear as a legitimate in-world Y coordinate. */
#define PHANTOM_PARK_Y 32767.0f

/* =========================================================================
   Types  (mirror of common_structs.h – copied here to avoid header deps)
   ========================================================================= */

typedef struct CLS_W_s
{
    unsigned int next_n64; /* 0x00 – N64 KSEG0 vaddr of next CLS_W, NOT a host ptr */
    unsigned char kind;    /* 0x04 */
    unsigned char pri;     /* 0x05 */
    unsigned short s_pri;  /* 0x06 */
} CLS_W_local;             /* size 0x08 in N64 RDRAM */

typedef struct
{
    float x;
    float y;
    float z;
} VEC3F_local;
typedef struct
{
    short x;
    short y;
    short z;
} VEC3S_local;
typedef struct
{
    unsigned short file_id;
    int data;
} WAVE_W_local;

/*
 * Full CLS_BG_W N64 layout (offset-verified from common_structs.h).
 *
 * ALL pointer fields are stored as 4-byte N64 KSEG0 virtual addresses in
 * RDRAM – they are NOT host pointers.  Using native pointer types on a 64-bit
 * host would make every field after the first one land at the wrong offset,
 * corrupt the game's struct on write, and cause a crash in the GFX thread.
 */
typedef struct CLS_BG_W_s
{
    CLS_W_local header;       /* 0x00 – next_n64 / kind / pri, size 0x08 */
    VEC3F_local position;     /* 0x08 – world x, y, z                    */
    VEC3S_local rotation;     /* 0x14 */
    unsigned char pad_1a[2];  /* 0x1A */
    VEC3F_local scale;        /* 0x1C */
    float animation_progress; /* 0x28 */
    unsigned int graphics_1;  /* 0x2C – display list N64 vaddr            */
    unsigned int graphics_2;  /* 0x30 – N64 vaddr (was wrongly void*)     */
    WAVE_W_local waves[6];    /* 0x34 – 6 × 8 bytes = 0x30               */
    unsigned char unk_64;     /* 0x64 */
    signed char unk_65;       /* 0x65 */
    unsigned char unk_66;     /* 0x66 */
    unsigned char unk_67;     /* 0x67 */
    signed int unk_68;        /* 0x68 */
    signed int unk_6c;        /* 0x6C */
    float unk_70;             /* 0x70 */
    unsigned int left_n64;    /* 0x74 – N64 vaddr (was struct CLS_BG_W_s*) */
    unsigned int right_n64;   /* 0x78 – N64 vaddr                          */
    unsigned char unk_7c[2];  /* 0x7C */
    signed short unk_7e;      /* 0x7E */
    unsigned int unk_80_n64;  /* 0x80 – N64 vaddr (was void*)              */
    unsigned short unk_84;    /* 0x84 */
    unsigned char unk_86;     /* 0x86 */
    unsigned char unk_87;     /* 0x87 */
    VEC3F_local unk_88;       /* 0x88 */
    unsigned int prev_n64;    /* 0x94 – N64 vaddr (was struct CLS_BG_W_s*) */
} CLS_BG_W_local;             /* total size 0x98 bytes */

/* =========================================================================
   Game globals (mirrored extern declarations)
   ========================================================================= */

/** Current room/scene ID written every frame by the engine. */
extern unsigned short D_800C7AB2;

/** Pointer to the local player's live CLS_BG_W.  May be NULL. */
extern void *D_801FC60C_5B851C;

/* =========================================================================
   RDRAM helpers

   In N64Recomp, extern globals give host pointers into the RDRAM buffer at
   the matching N64 virtual-address offset.  We use D_800C7AB2 (room ID) at
   N64 KSEG0 vaddr 0x800C7AB2 to anchor the RDRAM base, letting us convert
   between N64 virtual addresses and host pointers.
   ========================================================================= */

static inline unsigned char *mnsg_rdram_base(void)
{
    return (unsigned char *)&D_800C7AB2 - (0x800C7AB2U - 0x80000000U);
}

static inline void *n64_to_host(unsigned int n64_vaddr)
{
    return (void *)(mnsg_rdram_base() + (n64_vaddr - 0x80000000U));
}

/*
 * Phantom pool: 7 structs of 0x98 bytes each = 0x430 bytes.
 * Followed by 7 × 256-byte scratchpads (0x700 bytes) = 0xB30 bytes total.
 * Placed at 0x803E8000 – well above game statics (which end ~0x801FC60C)
 * and below the top-of-RDRAM stack region.  If another mod or the game
 * uses this region, move the address further down.
 */
#define PHANTOM_POOL_N64_BASE 0x803E8000U

/*
 * Per-phantom scratchpad: the game's player-update machinery (specifically
 * func_8001C970) follows unk_80_n64 and writes a byte at unk_80_n64+0x37.
 * If unk_80_n64 points into code-section RDRAM (never committed by the
 * recomp), that write faults with SIGBUS KERN_PROTECTION_FAILURE.
 * We redirect each phantom's unk_80_n64 to a dedicated, zeroed 256-byte
 * block in our own committed RDRAM pool so those writes go somewhere safe.
 */
#define PHANTOM_SCRATCH_N64_BASE \
    (PHANTOM_POOL_N64_BASE + (unsigned int)(MAX_PHANTOMS) * (unsigned int)sizeof(CLS_BG_W_local))
#define PHANTOM_SCRATCH_STRIDE 256U

/* =========================================================================
   Module state
   ========================================================================= */

/** Allocated phantom structs (NULL = slot unused). */
static CLS_BG_W_local *s_phantoms[MAX_PHANTOMS];

/** How many phantom slots are currently linked into the world list. */
static int s_phantom_count = 0;

/** Room ID at the time the phantoms were last created. */
static unsigned short s_phantom_room = 0xFFFF;

/** Player CLS_BG_W pointer at the time phantoms were last linked. */
static void *s_phantom_player_ptr = NULL;

/** Per-frame refresh counter. */
static int s_refresh_timer = 0;

/* =========================================================================
   Simple JSON parser – extract teammate positions from the Python JSON array.
   Format: [{"x":<int>,"y":<int>,"z":<int>}, ...]
   Returns number of entries written (≤ MAX_PHANTOMS).
   ========================================================================= */

typedef struct
{
    int x;
    int y;
    int z;
} PhantomPos;

static int parse_positions(const char *json, PhantomPos *out, int max_out)
{
    int count = 0;
    if (!json || json[0] != '[')
        return 0;
    const char *p = json + 1;
    while (*p && count < max_out)
    {
        /* Skip whitespace and commas. */
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n')
            p++;
        if (*p != '{')
            break;
        p++;
        int x = 0, y = 0, z = 0;
        int got_x = 0, got_y = 0, got_z = 0;
        while (*p && *p != '}')
        {
            /* Skip to key character. */
            while (*p && *p != '"' && *p != '}')
                p++;
            if (*p != '"')
                break;
            p++;
            char key = *p;
            if (key)
                p++;
            /* Skip closing quote and colon. */
            while (*p && *p != ':')
                p++;
            if (*p == ':')
                p++;
            /* Skip whitespace. */
            while (*p == ' ')
                p++;
            int sign = 1;
            if (*p == '-')
            {
                sign = -1;
                p++;
            }
            int val = 0;
            while (*p >= '0' && *p <= '9')
                val = val * 10 + (*p++ - '0');
            val *= sign;
            if (key == 'x')
            {
                x = val;
                got_x = 1;
            }
            else if (key == 'y')
            {
                y = val;
                got_y = 1;
            }
            else if (key == 'z')
            {
                z = val;
                got_z = 1;
            }
        }
        if (*p == '}')
            p++;
        if (got_x && got_y && got_z)
        {
            out[count].x = x;
            out[count].y = y;
            out[count].z = z;
            count++;
        }
    }
    return count;
}

/* =========================================================================
   Phantom lifecycle helpers
   ========================================================================= */

/** Unlink all phantoms from the player's CLS_W chain. */
static void phantoms_clear(void)
{
    CLS_BG_W_local *player = (CLS_BG_W_local *)s_phantom_player_ptr;
    /*
     * Only walk the linked list when the room hasn't changed.  A room
     * transition causes the game to tear down its own CLS_W lists, so the
     * player pointer may already be stale.  In that case, the game has
     * already clobbered the list and there is nothing to unlink.
     */
    if (player && D_800C7AB2 == s_phantom_room)
    {
        CLS_W_local *prev_hdr = &player->header;
        unsigned int cur_n64 = player->header.next_n64;
        int limit = MAX_PHANTOMS + 16; /* safety cap */

        while (cur_n64 >= 0x80000000U && limit-- > 0)
        {
            /* Check whether this entry is one of our phantom slots. */
            int is_ours = 0;
            for (int i = 0; i < MAX_PHANTOMS; i++)
            {
                unsigned int ph_n64 = PHANTOM_POOL_N64_BASE +
                                      (unsigned int)(i * (int)sizeof(CLS_BG_W_local));
                if (cur_n64 == ph_n64)
                {
                    is_ours = 1;
                    break;
                }
            }

            CLS_BG_W_local *cur_host = (CLS_BG_W_local *)n64_to_host(cur_n64);
            unsigned int next_n64 = cur_host->header.next_n64;

            if (is_ours)
                prev_hdr->next_n64 = next_n64; /* unlink */
            else
                prev_hdr = &cur_host->header;

            cur_n64 = next_n64;
        }
    }

    /* Phantoms live in RDRAM; nothing to free on the host side. */
    for (int i = 0; i < MAX_PHANTOMS; i++)
        s_phantoms[i] = NULL;

    s_phantom_count = 0;
    s_phantom_player_ptr = NULL;
    s_phantom_room = 0xFFFF;
}

/**
 * Populate N phantoms from the RDRAM phantom pool and link each one at the
 * head of the player's CLS_W.next list.
 *
 * Phantoms live at fixed N64 KSEG0 addresses (PHANTOM_POOL_N64_BASE + i*0x98).
 * Keeping them in RDRAM means the game's recompiled code can follow the
 * linked-list next_n64 pointers and render them exactly as it renders the
 * real player object.
 */
static void phantoms_create(CLS_BG_W_local *player, int count)
{
    for (int i = 0; i < count && i < MAX_PHANTOMS; i++)
    {
        unsigned int ph_n64 = PHANTOM_POOL_N64_BASE +
                              (unsigned int)(i * (int)sizeof(CLS_BG_W_local));
        CLS_BG_W_local *ph = (CLS_BG_W_local *)n64_to_host(ph_n64);

        /* Clone the live player struct into the phantom slot. */
        unsigned char *dst = (unsigned char *)ph;
        unsigned char *src = (unsigned char *)player;
        for (int b = 0; b < (int)sizeof(CLS_BG_W_local); b++)
            dst[b] = src[b];

        /* Clear tree/back pointers – only the flat linked list is used. */
        ph->left_n64 = 0;
        ph->right_n64 = 0;
        ph->prev_n64 = 0;

        /* Redirect unk_80_n64 to a dedicated scratchpad in our RDRAM pool.
         * The game's player-update path writes to unk_80_n64+0x37 (and
         * possibly other offsets).  Pointing to the player's original value
         * (often a code-section address like 0x80210000) hits an uncommitted
         * RDRAM page and causes SIGBUS.  We give each phantom its own
         * 256-byte committed block so those writes are harmless.             */
        {
            unsigned int scratch_n64 = PHANTOM_SCRATCH_N64_BASE +
                                       (unsigned int)(i)*PHANTOM_SCRATCH_STRIDE;
            unsigned char *scratch_host = (unsigned char *)n64_to_host(scratch_n64);
            for (unsigned int b = 0; b < PHANTOM_SCRATCH_STRIDE; b++)
                scratch_host[b] = 0;
            ph->unk_80_n64 = scratch_n64;
        }

        /* Park off-screen until the first position update arrives. */
        ph->position.y = PHANTOM_PARK_Y;

        /* Insert at the front of the player's CLS_W.next chain using the
         * phantom's N64 virtual address – the game reads these as N64 vaddrs,
         * NOT as host pointers.                                               */
        ph->header.next_n64 = player->header.next_n64;
        player->header.next_n64 = ph_n64;

        s_phantoms[i] = ph; /* host-ptr cache for fast position updates */
    }
    s_phantom_count = count;
    s_phantom_player_ptr = (void *)player;
    s_phantom_room = D_800C7AB2;
}

/* =========================================================================
   Per-frame hook
   ========================================================================= */

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_update(void)
{
    /* Only run when connected. */
    if (!anchor_is_connected())
    {
        if (s_phantom_count > 0)
            phantoms_clear();
        return;
    }

    /* Detect room change – clear phantoms so they are re-created for the
     * new room once the player's CLS_BG_W is valid again.               */
    unsigned short cur_room = D_800C7AB2;
    if (cur_room != s_phantom_room && s_phantom_count > 0)
        phantoms_clear();

    /* Throttle the position-sync poll. */
    if (s_refresh_timer > 0)
    {
        s_refresh_timer--;
        return;
    }
    s_refresh_timer = PHANTOM_REFRESH_FRAMES;

    /* Need a valid player world object to clone from. */
    CLS_BG_W_local *player = (CLS_BG_W_local *)D_801FC60C_5B851C;
    if (!player)
    {
        if (s_phantom_count > 0)
            phantoms_clear();
        return;
    }

    /* Query Python for same-room teammate positions. */
    char *json = anchor_get_teammate_positions_json();
    if (!json)
        return;

    PhantomPos positions[MAX_PHANTOMS];
    int n = parse_positions(json, positions, MAX_PHANTOMS);
    recomp_free(json);

    /* Resize the phantom pool if the teammate count changed or the player
     * pointer changed (happens after room transitions).                   */
    if (n != s_phantom_count || (void *)player != s_phantom_player_ptr)
    {
        phantoms_clear();
        if (n > 0)
            phantoms_create(player, n);
    }

    /* Update each phantom's world position. */
    for (int i = 0; i < n && i < MAX_PHANTOMS; i++)
    {
        if (s_phantoms[i])
        {
            s_phantoms[i]->position.x = (float)positions[i].x;
            s_phantoms[i]->position.y = (float)positions[i].y;
            s_phantoms[i]->position.z = (float)positions[i].z;
        }
    }
}
