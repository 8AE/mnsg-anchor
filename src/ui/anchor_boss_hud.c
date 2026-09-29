/* Room-local boss HUD built from the game's kind-1 sprites and font bits. */
#include "ui/anchor_boss_hud.h"
#include "bosses/anchor_boss_arenas.h"
#include "bosses/anchor_boss_invite_world.h"
#include "bosses/congo/anchor_congo_native.h"
#include "bosses/control_machine/anchor_control_machine_hud.h"
#include "bosses/dharumanyo/anchor_dharumanyo_native.h"
#include "bosses/tsurami/anchor_tsurami_native.h"
#include "platform/modding.h"
#include "platform/recompconfig.h"

#define BOSS_NAME_WIDTH 128u
#define BOSS_NAME_HEIGHT 16u
#define BOSS_NAME_BYTES (BOSS_NAME_WIDTH * BOSS_NAME_HEIGHT * 2u)
#define BOSS_HEART_COUNT 15u
#define BOSS_TITLE_Y 6u
#define BOSS_HEART_Y 25u
#define CONTROL_MACHINE_TITLE_Y 40u
#define CONTROL_MACHINE_HEART_Y 59u
#define BOSS_OBJECT_COUNT 4u
#define BOSS_NAME_PADDING 3u
#define BOSS_GLYPH_WIDTH 8u
#define BOSS_GLYPH_HEIGHT 12u
#define BOSS_GLYPH_PAIR_BYTES 48u

typedef void (*BossHudCallback)(void *, void *);
typedef struct BossHudConfig {
    unsigned short room;
    unsigned short max_health;
    const char *label;
    int (*health)(unsigned int *);
    void *(*owner)(void);
} BossHudConfig;
typedef struct BossHudSceneResource {
    unsigned short file_id, padding;
    unsigned char *data;
} BossHudSceneResource;
typedef struct BossHudSprite {
    unsigned short command, size;
    short x, y, s, t;
} BossHudSprite;

_Static_assert(sizeof(BossHudSprite) == 12, "Native sprite descriptor ABI");
_Static_assert(sizeof("CONTROL MACHINE") - 1u <=
               (BOSS_NAME_WIDTH - 2u * BOSS_NAME_PADDING) /
                   BOSS_GLYPH_WIDTH,
               "Longest boss label exceeds the title texture at zero gap");

static const BossHudConfig s_bosses[] = {
    {ANCHOR_BOSS_ROOM_CONGO, 30u, "CONGO",
     anchor_congo_native_hud_health, anchor_congo_native_root_task},
    {ANCHOR_BOSS_ROOM_DHARUMANYO, 12u, "DHARUMANYO",
     anchor_dharumanyo_native_hud_health,
     anchor_dharumanyo_native_root_task},
    {ANCHOR_BOSS_ROOM_TSURAMI, 12u, "TSURAMI",
     anchor_tsurami_native_hud_health, anchor_tsurami_native_root_task},
    {ANCHOR_BOSS_ROOM_CONTROL_MACHINE, 5u, "CONTROL MACHINE",
     anchor_control_machine_hud_health, anchor_control_machine_hud_task},
};

extern BossHudSceneResource D_80167FC0_168BC0[48];
extern const unsigned char D_800629A0_635A0[];
extern const unsigned char D_8005BB10_5C710[];
extern unsigned char D_8005B730[];
extern unsigned short D_800C7AB2;
extern void osWritebackDCache(void *address, int bytes);
extern void *func_800141C4_14DC4(unsigned int file_id);
extern void *func_80034E08_35A08(void *owner, BossHudCallback update,
                                 unsigned short flags);
extern void func_80034EF8_35AF8(void *task);
extern void *func_80035EEC_36AEC(void *task, short kind,
                                 unsigned int count);
extern void func_800086C4_92C4(void *object, const void *descriptor,
                               const void *texture_bank);

#define BYTE(p, o) (((unsigned char *)(p))[(o)])
#define U16(p, o) (*(unsigned short *)((unsigned char *)(p) + (o)))
#define U32(p, o) (*(unsigned int *)((unsigned char *)(p) + (o)))
#define F32(p, o) (*(float *)((unsigned char *)(p) + (o)))
#define PTR(p, o) (*(void **)((unsigned char *)(p) + (o)))

static unsigned short *s_name_pixels;
static unsigned char s_name_bank[16] __attribute__((aligned(8)));
static BossHudSprite s_title;
static BossHudSprite s_empty[BOSS_HEART_COUNT];
static BossHudSprite s_full[BOSS_HEART_COUNT];
static BossHudSprite s_half;
static void *s_child;
static void *s_objects[BOSS_OBJECT_COUNT];
static void *s_root;
static const BossHudConfig *s_config;
static unsigned int s_visit;
static unsigned int s_last_hp = ~0u;
static unsigned int s_heart_count;
static unsigned int s_heart_x;
static unsigned int s_title_y = BOSS_TITLE_Y;
static unsigned int s_heart_y = BOSS_HEART_Y;

static const BossHudConfig *boss_for_room(unsigned short room)
{
    unsigned int i;
    for (i = 0; i < sizeof(s_bosses) / sizeof(s_bosses[0]); ++i)
        if (s_bosses[i].room == room)
            return &s_bosses[i];
    return 0;
}

static int low_rdram(const void *pointer)
{
    unsigned int address = (unsigned int)(unsigned long)pointer & 0xbfffffffu;
    return address >= 0x80001000u && address < 0x80800000u;
}

static void child_update(void *task, void *object)
{
    (void)task;
    (void)object;
}

static int child_live(void)
{
    const unsigned char *task = s_child;
    void *sibling;
    unsigned int count, owner_depth;
    if (!low_rdram(task) ||
        !s_config || s_config->owner() != s_root)
        return 0;
    /* Scheduler links are a depth-first list. Child +4 is a backlink to the
     * previous task, so later siblings need not leave it equal to the owner. */
    owner_depth = U16(s_root, 0x20u);
    sibling = PTR(s_root, 0);
    for (count = 0; count < 256u && low_rdram(sibling) &&
                    U16(sibling, 0x20u) > owner_depth; ++count)
    {
        if (sibling == task)
            return *(BossHudCallback const *)(task + 0xcu) == child_update;
        sibling = PTR(sibling, 0);
    }
    return 0;
}

static int same_scene(void)
{
    return s_config && D_800C7AB2 == s_config->room && s_visit &&
           anchor_boss_invite_world_visit() == s_visit;
}

static void clear_display(void)
{
    unsigned int i;
    s_child = s_root = 0;
    s_visit = 0;
    s_last_hp = ~0u;
    s_heart_count = 0;
    s_heart_x = 0;
    s_title_y = BOSS_TITLE_Y;
    s_heart_y = BOSS_HEART_Y;
    for (i = 0; i < BOSS_OBJECT_COUNT; ++i)
        s_objects[i] = 0;
}

static void release_display(void)
{
    /* Stage teardown may recycle the task pool. Only delete an object owner
     * while its original linked child is still resident in this visit. */
    if (same_scene() && child_live())
        func_80034EF8_35AF8(s_child);
    clear_display();
}

static unsigned int glyph_width(unsigned char c)
{
    unsigned int width = D_8005BB10_5C710[c - 0x20u];
    return width > BOSS_GLYPH_WIDTH ? BOSS_GLYPH_WIDTH : width;
}

static unsigned int glyph_shade(unsigned char c, unsigned int x,
                                unsigned int y)
{
    unsigned int glyph = (unsigned int)c - 0x20u;
    unsigned int pixel = y * BOSS_GLYPH_WIDTH + x;
    unsigned char packed = D_800629A0_635A0[
        (glyph >> 1) * BOSS_GLYPH_PAIR_BYTES + (pixel >> 1)];
    unsigned int index = (pixel & 1u) ? packed & 15u : packed >> 4;
    return (glyph & 1u) ? index >> 2 : index & 3u;
}

static int write_name_pixels(const char *label)
{
    unsigned int count = 0, text_width = 0, gap = 2u;
    unsigned int cursor, i, x, y;
    if (!s_name_pixels || !label)
        return 0;
    while (label[count] && count < BOSS_NAME_WIDTH)
    {
        unsigned char c = (unsigned char)label[count++];
        if (c < 0x20u || c > 0x7eu)
            return 0;
        text_width += glyph_width(c);
    }
    if (!count || label[count])
        return 0;
    /* All four names fit the 128x16 RGBA16 tile, even if every native glyph
     * is eight pixels wide. Tighten spacing rather than clipping a title. */
    while (gap && text_width + (count - 1u) * gap >
                      BOSS_NAME_WIDTH - 2u * BOSS_NAME_PADDING)
        --gap;
    text_width += (count - 1u) * gap;
    if (text_width > BOSS_NAME_WIDTH - 2u * BOSS_NAME_PADDING)
        return 0;
    for (i = 0; i < BOSS_NAME_WIDTH * BOSS_NAME_HEIGHT; ++i)
        s_name_pixels[i] = 0;
    cursor = (BOSS_NAME_WIDTH - text_width) / 2u;
    for (i = 0; i < count; ++i)
    {
        unsigned char c = (unsigned char)label[i];
        unsigned int width = glyph_width(c);
        for (y = 0; y < BOSS_GLYPH_HEIGHT; ++y)
            for (x = 0; x < width; ++x)
                if (glyph_shade(c, x, y))
                    s_name_pixels[(y + 2u) * BOSS_NAME_WIDTH + cursor + x] =
                        0xffffu;
        cursor += width + gap;
    }
    /* Use the same one-pixel edge as the player nameplate glyph rasterizer.
     * Its background stays transparent for the native screen HUD. */
    for (y = 1; y < BOSS_NAME_HEIGHT - 1u; ++y)
        for (x = 1; x < BOSS_NAME_WIDTH - 1u; ++x)
        {
            unsigned int dst = y * BOSS_NAME_WIDTH + x;
            if (s_name_pixels[dst])
                continue;
            if (s_name_pixels[dst - 1u] == 0xffffu ||
                s_name_pixels[dst + 1u] == 0xffffu ||
                s_name_pixels[dst - BOSS_NAME_WIDTH] == 0xffffu ||
                s_name_pixels[dst + BOSS_NAME_WIDTH] == 0xffffu)
                s_name_pixels[dst] = 0x0001u;
        }
    osWritebackDCache(s_name_pixels, BOSS_NAME_BYTES);
    return 1;
}

/* Stage resources are ready here. Reserve RDP-readable original RDRAM before
 * later scene allocations; mod BSS is a CPU-only source for these pixels. */
RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_boss_hud_load_resources(void)
{
    unsigned int start, end;
    int i;
    clear_display();
    s_name_pixels = 0;
    s_config = boss_for_room(D_800C7AB2);
    if (!s_config)
        return;
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !low_rdram(D_80167FC0_168BC0[i].data))
        return;
    start = (unsigned int)(unsigned long)D_80167FC0_168BC0[i].data &
            0xbfffffffu;
    start = (start + 15u) & ~15u;
    end = start + BOSS_NAME_BYTES;
    if (end > 0x80800000u)
        return;
    s_name_pixels = (unsigned short *)(unsigned long)start;
    D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)end;
    if (!write_name_pixels(s_config->label))
        s_name_pixels = 0;
}

static void set_sprite(BossHudSprite *sprite, unsigned short command,
                       unsigned short size, short x, short y,
                       short s, short t)
{
    sprite->command = command;
    sprite->size = size;
    sprite->x = x;
    sprite->y = y;
    sprite->s = s;
    sprite->t = t;
}

static void setup_sprite_object(void *object, const void *descriptor,
                                const void *bank, float scale,
                                unsigned short draw_order,
                                unsigned int s_mask, unsigned int t_mask)
{
    BYTE(object, 4) = 1u;
    BYTE(object, 5) = 10u;
    U16(object, 6) = draw_order;
    /* Native sprite scaling is ignored unless bit 2 is set. The stock odd
     * heart enables it for its 0.5-scale red sprite over the empty heart. */
    BYTE(object, 8) = (BYTE(object, 8) & 0x1bu) | 0x20u |
                      (scale < 1.0f ? 0x04u : 0u);
    U32(object, 8) = (U32(object, 8) &
                      ~((15u << 15) | (15u << 11))) |
                     (s_mask << 15) | (t_mask << 11);
    U32(object, 0xcu) = 0u;
    U16(object, 0x1cu) = 0u;
    U16(object, 0x20u) = 0u;
    F32(object, 0x2cu) = scale;
    F32(object, 0x30u) = scale;
    func_800086C4_92C4(object, descriptor, bank);
}

static int begin_display(void *root)
{
    unsigned int i;
    void *object;
    void *hud_file = func_800141C4_14DC4(0x7fu);
    if (!s_config || !s_name_pixels ||
        !s_config->max_health ||
        (s_config->max_health + 1u) / 2u > BOSS_HEART_COUNT ||
        !hud_file ||
        hud_file == (void *)(unsigned long)0xffffffffu ||
        !low_rdram(hud_file))
        return 0;
    s_heart_count = (s_config->max_health + 1u) / 2u;
    s_heart_x = 160u - s_heart_count * 4u;
    /* File_58's Control Machine intro clips the original top HUD strip.
     * Keep both native sprite rows inside its (8,38)-(312,202) scissor. */
    if (s_config->room == ANCHOR_BOSS_ROOM_CONTROL_MACHINE)
    {
        s_title_y = CONTROL_MACHINE_TITLE_Y;
        s_heart_y = CONTROL_MACHINE_HEART_Y;
    }
    s_root = root;
    s_visit = anchor_boss_invite_world_visit();
    s_child = func_80034E08_35A08(root, child_update, 0);
    if (!child_live())
    {
        clear_display();
        return 0;
    }
    object = func_80035EEC_36AEC(s_child, 1, BOSS_OBJECT_COUNT);
    for (i = 0; i < BOSS_OBJECT_COUNT; ++i)
    {
        if (!low_rdram(object))
        {
            release_display();
            return 0;
        }
        s_objects[i] = object;
        object = PTR(object, 0);
    }

    U16(s_name_bank, 0) = BOSS_NAME_WIDTH;
    U16(s_name_bank, 2) = BOSS_NAME_HEIGHT;
    BYTE(s_name_bank, 4) = 0x10u; /* RGBA5551, direct RDRAM pointer. */
    BYTE(s_name_bank, 5) = BYTE(s_name_bank, 6) = BYTE(s_name_bank, 7) = 0;
    U32(s_name_bank, 8) = (unsigned int)(unsigned long)s_name_pixels;
    U16(s_name_bank, 12) = U16(s_name_bank, 14) = 0u;

    set_sprite(&s_title, 0xc000u, 0x8010u, 96, (short)s_title_y, 0, 0);
    for (i = 0; i < s_heart_count; ++i)
        set_sprite(&s_empty[i], i + 1u == s_heart_count ?
                   0xc001u : 0x4001u, 0x0808u,
                   (short)(s_heart_x + i * 8u), (short)s_heart_y,
                   0x0100, 0x0300);
    set_sprite(&s_half, 0xc001u, 0x0808u, 0, 0, 0, 0x0300);
    setup_sprite_object(s_objects[0], &s_title, s_name_bank, 1.0f,
                        5u, 7u, 4u);
    setup_sprite_object(s_objects[1], s_empty, D_8005B730, 1.0f,
                        3u, 6u, 5u);
    setup_sprite_object(s_objects[2], 0, D_8005B730, 1.0f,
                        4u, 6u, 5u);
    setup_sprite_object(s_objects[3], 0, D_8005B730, 0.5f,
                        4u, 6u, 5u);
    return 1;
}

static void refresh_health(unsigned int hp)
{
    unsigned int full = hp / 2u;
    unsigned int i;
    for (i = 0; i < full; ++i)
        set_sprite(&s_full[i], i + 1u == full ? 0xc001u : 0x4001u,
                   0x0808u, (short)(s_heart_x + i * 8u),
                   (short)s_heart_y, 0, 0x0300);
    func_800086C4_92C4(s_objects[2], full ? s_full : 0, D_8005B730);
    /* The stock odd-health object keeps one red 8x8 source at half scale,
     * offset by two pixels from the matching full-heart slot. */
    U16(s_objects[3], 0x1cu) =
        (unsigned short)(s_heart_x + full * 8u + 2u);
    U16(s_objects[3], 0x20u) = s_heart_y + 2u;
    func_800086C4_92C4(s_objects[3],
                        (hp & 1u) ? &s_half : 0, D_8005B730);
    s_last_hp = hp;
}

void anchor_boss_hud_update(void)
{
    unsigned int hp;
    void *root;
    if (recomp_get_config_u32("anchor_show_boss_health_bar") == 1u)
    {
        if (s_child) release_display();
        return;
    }
    if (!s_config || D_800C7AB2 != s_config->room ||
        !s_config->health(&hp) || hp == 0u || hp > s_config->max_health)
    {
        if (s_child) release_display();
        return;
    }
    root = s_config->owner();
    if (!low_rdram(root))
    {
        if (s_child) release_display();
        return;
    }
    if (s_child && (root != s_root || !same_scene() || !child_live()))
        release_display();
    if (!s_child && !begin_display(root))
        return;
    if (hp != s_last_hp)
        refresh_health(hp);
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_boss_hud_frame(void)
{
    anchor_boss_hud_update();
}
