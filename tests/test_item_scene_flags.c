/* Host regression harness for local scene bits and shared quest flags in src/progression/item_sync.c.
 *
 * The production translation unit is compiled as-is on the host with a
 * shimmed `modding.h` (the real one emits Mach-O-incompatible section
 * attributes).  Real utils are linked; the anchor/boss/enemy/miracle and
 * recompui surface is stubbed here so the send/monitor paths are observable.
 *
 * Region under test: the sync tables plus apply_flag / apply_incoming_value /
 * apply_incoming_flag / apply_team_state / build_team_state_json /
 * broadcast_team_state_snapshot / monitor_and_send_changes.
 *
 * Verified subject: save bits 0x06B (fl_outerspace) and 0x06C (fl_to_space)
 * are File67 local travel-choice/scene-clear bits.  No item_sync path may read
 * or write them.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MNSG_ITEM_SYNC_SRC
#define MNSG_ITEM_SYNC_SRC "../src/progression/item_sync.c"
#endif

/* The save image needs addressable bytes *before* the flag-array base: the
 * production helpers index negative offsets for the stats block (stat_hpmax at
 * -0x28 and current HP at -0x21).  Separate globals do not get a guaranteed
 * relative order, so the image is emitted as one explicit section block with
 * the pad immediately before the array.  The harness still asserts the
 * adjacency at startup and refuses to run if the layout ever changes. */
#if defined(__APPLE__)
#define MNSG_SAVE_ASM                                                          \
    ".section __DATA,mnsg_save_image\n"                                        \
    ".p2align 3\n"                                                             \
    ".globl _mnsg_save_pad\n"                                                  \
    "_mnsg_save_pad:\n"                                                        \
    ".space 0x200\n"                                                           \
    ".globl _D_8015C608_15D208\n"                                              \
    "_D_8015C608_15D208:\n"                                                    \
    ".space 0x400\n"
#else
#define MNSG_SAVE_ASM                                                          \
    ".section mnsg_save_image,\"aw\",@nobits\n"                                \
    ".p2align 3\n"                                                             \
    ".globl mnsg_save_pad\n"                                                   \
    "mnsg_save_pad:\n"                                                         \
    ".space 0x200\n"                                                           \
    ".globl D_8015C608_15D208\n"                                               \
    "D_8015C608_15D208:\n"                                                     \
    ".space 0x400\n"
#endif
__asm__(MNSG_SAVE_ASM);
#define MNSG_SAVE_PAD_SIZE 0x200
extern unsigned char mnsg_save_pad[];
extern unsigned char D_8015C608_15D208[];
unsigned short D_800C7AB2;

#include MNSG_ITEM_SYNC_SRC

/* ── recording stubs ─────────────────────────────────────────────────── */
#define REC_MAX 128
typedef struct { char name[28]; int value; int queued; int via_flag_api; } SendRec;
static SendRec s_sends[REC_MAX];
static int s_send_count;
static char s_last_custom_type[64];
static char s_last_custom_payload[16384];
static int s_custom_calls;
static int s_suppress_progress;
static int s_failures;
static const char *s_incoming_packet;

static void record_send(const char *name, int value, int queued, int via_flag_api)
{
    if (s_send_count < REC_MAX)
    {
        SendRec *r = &s_sends[s_send_count];
        snprintf(r->name, sizeof(r->name), "%s", name ? name : "");
        r->value = value;
        r->queued = queued;
        r->via_flag_api = via_flag_api;
    }
    ++s_send_count;
}
static void reset_records(void)
{
    memset(s_sends, 0, sizeof(s_sends));
    s_send_count = 0;
    s_last_custom_type[0] = 0;
    s_last_custom_payload[0] = 0;
    s_custom_calls = 0;
    s_suppress_progress = 0;
}
static int send_index_of(const char *name)
{
    int i;
    for (i = 0; i < s_send_count && i < REC_MAX; ++i)
        if (strcmp(s_sends[i].name, name) == 0) return i;
    return -1;
}
static int sent_any(const char *name) { return send_index_of(name) >= 0; }
static int sent_queued(const char *name)
{
    int i = send_index_of(name);
    return i >= 0 && s_sends[i].queued;
}
static int text_has(const char *hay, const char *needle)
{
    return hay && needle && strstr(hay, needle) != 0;
}

static void dump_sends(const char *tag)
{
    int i;
    printf("  [%s] %d send(s):", tag, s_send_count);
    for (i = 0; i < s_send_count && i < REC_MAX; ++i)
        printf(" %s=%d%s", s_sends[i].name, s_sends[i].value,
               s_sends[i].queued ? "(q)" : "");
    printf("\n");
    fflush(stdout);
}

#define CHECK(cond, what)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++s_failures;                                                      \
            printf("  FAIL: %s\n", (what));                                    \
            fflush(stdout);                                                    \
        }                                                                      \
    } while (0)

static void note(const char *what)
{
    printf("- %s\n", what);
    fflush(stdout);
}

/* ── anchor surface ──────────────────────────────────────────────────── */
int anchor_send_flag(const char *flag_name, int flag_value, int add_to_queue)
{
    record_send(flag_name, flag_value, add_to_queue, 1);
    return 1;
}
int anchor_update_team_state(const char *state_json)
{
    snprintf(s_team_state_json, sizeof(s_team_state_json), "%s",
             state_json ? state_json : "");
    return 1;
}
int anchor_send_custom_packet(const char *packet_type, const char *payload_json,
                              const char *target_team_id,
                              unsigned int target_client_id, int add_to_queue)
{
    (void)target_client_id;
    (void)add_to_queue;
    ++s_custom_calls;
    snprintf(s_last_custom_type, sizeof(s_last_custom_type), "%s",
             packet_type ? packet_type : "");
    snprintf(s_last_custom_payload, sizeof(s_last_custom_payload), "%s",
             payload_json ? payload_json : "");
    (void)target_team_id;
    return 1;
}
char *anchor_get_team_id(void)
{
    char *id = (char *)malloc(8);
    if (id) strcpy(id, "team1");
    return id;
}
unsigned int anchor_get_client_id(void) { return 1; }
int anchor_has_packet(void) { return s_incoming_packet != 0; }
char *anchor_poll_packet(void)
{
    char *copy;
    size_t size;
    if (!s_incoming_packet) return 0;
    size = strlen(s_incoming_packet) + 1;
    copy = (char *)malloc(size);
    if (copy) memcpy(copy, s_incoming_packet, size);
    s_incoming_packet = 0;
    return copy;
}
int anchor_is_connected(void) { return 1; }
int anchor_is_disabled(void) { return 0; }
int anchor_set_save_loaded(int is_loaded) { (void)is_loaded; return 0; }
int anchor_request_team_state(const char *team_id) { (void)team_id; return 0; }
void anchor_race_on_flag_synced(const char *n, int v) { (void)n; (void)v; }
void anchor_race_on_remote_flag_synced(const char *n, int v)
{ (void)n; (void)v; }
void anchor_race_on_finish_packet(const char *json) { (void)json; }
int anchor_race_is_active(void) { return 1; }
void anchor_race_on_forced_disconnect(void) {}
int anchor_dialog_world_paused(void) { return 0; }
int anchor_runtime_damage_sync_enabled(void) { return 0; }
int anchor_runtime_no_hit_enabled(void) { return 0; }
int anchor_runtime_one_life_enabled(void) { return 0; }
int anchor_runtime_ryo_sync_enabled(void) { return 0; }
void anchor_miracle_moon_reset(void) {}
void anchor_miracle_moon_update_room(unsigned short room) { (void)room; }
void anchor_miracle_moon_remote_completed(unsigned short room) { (void)room; }
int anchor_miracle_moon_local_pickup_active(void) { return 0; }
void anchor_miracle_star_reset(void) {}
void anchor_miracle_star_update_room(unsigned short room) { (void)room; }
int anchor_miracle_star_local_scene_active(void) { return 0; }
const char *anchor_flag_catalog_find_display_value(const char *key, int value)
{
    (void)value;
    return key;
}
int anchor_flag_catalog_is_important(const char *key) { (void)key; return 0; }
AnchorRomIcon anchor_icon_for_check(const char *key, int value)
{
    (void)key; (void)value;
    return (AnchorRomIcon)0;
}
const AnchorRomIconInfo *anchor_rom_icon_info(AnchorRomIcon icon)
{
    (void)icon;
    return 0;
}
int anchor_rom_load_icon_rgba32(AnchorRomIcon icon, unsigned char *out,
                                unsigned int out_size)
{
    (void)icon; (void)out; (void)out_size;
    return 0;
}

/* ── boss / enemy surface ────────────────────────────────────────────── */
int boss_sync_is_completion_flag(const char *n) { (void)n; return 0; }
int boss_sync_should_defer_flag(const char *n) { (void)n; return 0; }
int boss_sync_send_defeat(const char *n) { (void)n; return 0; }
int boss_sync_apply_remote_defeat(const char *n) { (void)n; return 0; }
int boss_sync_has_active_encounter(const char *n) { (void)n; return 0; }
int boss_sync_has_local_encounter(const char *n) { (void)n; return 0; }
int boss_sync_is_darumanyo_reward_progress(const char *n) { (void)n; return 0; }
int boss_sync_is_tsurami_reward_progress(const char *n) { (void)n; return 0; }
void boss_sync_reset(void) {}
/* The single durable publish point item_sync uses for fields and flag bits. */
int boss_sync_send_local_progress(const char *flag_name, int value,
                                  int add_to_queue)
{
    if (s_suppress_progress)
        return BOSS_SYNC_PROGRESS_SUPPRESSED;
    record_send(flag_name, value, add_to_queue, 0);
    return BOSS_SYNC_PROGRESS_SENT;
}
int enemy_sync_handle_packet(const char *json) { (void)json; return 0; }
void enemy_sync_update(void) {}
void enemy_sync_disconnect(void) {}
void enemy_sync_reset(void) {}

/* ── recomp / recompui surface ───────────────────────────────────────── */
void *recomp_alloc(unsigned long size) { return malloc(size); }
void recomp_free(void *memory) { free(memory); }
int recomp_printf(const char *fmt, ...) { (void)fmt; return 0; }
unsigned char *recomp_get_mod_file_path(void) { return (unsigned char *)""; }
unsigned long recomp_get_config_u32(const char *key) { (void)key; return 0; }
void recomp_free_config_string(char *str) { free(str); }
RecompuiContext recompui_create_context(void) { return (RecompuiContext)1; }
void recompui_open_context(RecompuiContext c) { (void)c; }
void recompui_close_context(RecompuiContext c) { (void)c; }
RecompuiResource recompui_context_root(RecompuiContext c) { (void)c; return 1; }
void recompui_show_context(RecompuiContext c) { (void)c; }
void recompui_hide_context(RecompuiContext c) { (void)c; }
void recompui_set_context_captures_input(RecompuiContext c, int i) { (void)c; (void)i; }
void recompui_set_context_captures_mouse(RecompuiContext c, int m) { (void)c; (void)m; }
RecompuiResource recompui_create_element(RecompuiContext c, RecompuiResource p) { (void)c; (void)p; return 1; }
RecompuiResource recompui_create_label(RecompuiContext c, RecompuiResource p, const char *t, RecompuiLabelStyle s) { (void)c; (void)p; (void)t; (void)s; return 1; }
RecompuiResource recompui_create_imageview(RecompuiContext c, RecompuiResource p, RecompuiTextureHandle h) { (void)c; (void)p; (void)h; return 1; }
RecompuiTextureHandle recompui_create_texture_rgba32(void *d, unsigned long w, unsigned long h) { (void)d; (void)w; (void)h; return (RecompuiTextureHandle)1; }
void recompui_set_imageview_texture(RecompuiResource id, RecompuiTextureHandle h) { (void)id; (void)h; }
void recompui_set_text(RecompuiResource id, const char *t) { (void)id; (void)t; }
void recompui_set_display(RecompuiResource id, RecompuiDisplay d) { (void)id; (void)d; }
void recompui_set_position(RecompuiResource id, RecompuiPosition p) { (void)id; (void)p; }
void recompui_set_flex_direction(RecompuiResource id, RecompuiFlexDirection f) { (void)id; (void)f; }
void recompui_set_flex_shrink(RecompuiResource id, float s) { (void)id; (void)s; }
void recompui_set_align_items(RecompuiResource id, RecompuiAlignItems a) { (void)id; (void)a; }
void recompui_set_font_weight(RecompuiResource id, unsigned long w) { (void)id; (void)w; }
void recompui_set_width(RecompuiResource id, float w, RecompuiUnit u) { (void)id; (void)w; (void)u; }
void recompui_set_width_auto(RecompuiResource id) { (void)id; }
void recompui_set_height(RecompuiResource id, float h, RecompuiUnit u) { (void)id; (void)h; (void)u; }
void recompui_set_right(RecompuiResource id, float r, RecompuiUnit u) { (void)id; (void)r; (void)u; }
void recompui_set_bottom(RecompuiResource id, float b, RecompuiUnit u) { (void)id; (void)b; (void)u; }
void recompui_set_margin_right(RecompuiResource id, float m, RecompuiUnit u) { (void)id; (void)m; (void)u; }
void recompui_set_padding(RecompuiResource id, float p, RecompuiUnit u) { (void)id; (void)p; (void)u; }
void recompui_set_border_width(RecompuiResource id, float w, RecompuiUnit u) { (void)id; (void)w; (void)u; }
void recompui_set_border_radius(RecompuiResource id, float r, RecompuiUnit u) { (void)id; (void)r; (void)u; }
void recompui_set_background_color(RecompuiResource id, const RecompuiColor *c) { (void)id; (void)c; }
void recompui_set_border_color(RecompuiResource id, const RecompuiColor *c) { (void)id; (void)c; }
void recompui_set_color(RecompuiResource id, const RecompuiColor *c) { (void)id; (void)c; }

/* ── scenario helpers ────────────────────────────────────────────────── */
#define SCENE_FLAG_A 0x06Bu /* fl_outerspace */
#define SCENE_FLAG_B 0x06Cu /* fl_to_space  */

static void set_native_bit(unsigned int id)
{
    D_8015C608_15D208[id >> 3] |= (unsigned char)(1u << (id & 7u));
}
static int native_bit(unsigned int id)
{
    return (D_8015C608_15D208[id >> 3] >> (id & 7u)) & 1u;
}
static void clear_save(void)
{
    memset(mnsg_save_pad, 0, MNSG_SAVE_PAD_SIZE);
    memset(D_8015C608_15D208, 0, 0x400);
    SAVE_WRITE32(SAVE_HP_MAX_OFFSET, 100); /* hp_max > 0 => save loaded */
}
static void sync_caches(void)
{
    int i;
    for (i = 0; i < NUM_FIELDS; ++i)
        s_fields[i].cached = SAVE_READ32(s_fields[i].off);
    for (i = 0; i < NUM_FLAGS; ++i)
        s_flag_bits[i].cached = (unsigned char)FLAG_IS_SET(s_flag_bits[i].id);
    s_set_flag_send_timer = 0;
}
static int table_has_name(const char *name)
{
    int i;
    for (i = 0; i < NUM_FLAGS; ++i)
        if (strcmp(s_flag_bits[i].name, name) == 0) return 1;
    for (i = 0; i < NUM_FIELDS; ++i)
        if (strcmp(s_fields[i].name, name) == 0) return 1;
    return 0;
}

int main(void)
{
    int i;
    int hp_or_ryo_field = 0;
    static const unsigned int fish_ids[] = {
        0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
        0xb0,0xb1,0xb2,0xb3,0xb8,0xb9,0xba,0xbb,0xbc,
        0xbd,0xbe,0xbf,0xc0,0xc1
    };

    /* The save image must be laid out as [stats][flag array].  This gate runs
     * before any negative-offset access so a reordered link reports a clear
     * layout error instead of faulting on the page before the array. */
    note("save image layout");
    printf("  pad=%p array=%p delta=%ld\n", (void *)mnsg_save_pad,
           (void *)D_8015C608_15D208,
           (long)((char *)D_8015C608_15D208 - (char *)mnsg_save_pad));
    fflush(stdout);
    if ((char *)mnsg_save_pad + MNSG_SAVE_PAD_SIZE !=
            (char *)D_8015C608_15D208 ||
        MNSG_SAVE_PAD_SIZE < 0x40)
    {
        printf("item scene flags: LAYOUT UNSUPPORTED on this toolchain - the "
               "stats pad is not immediately before the flag array\n");
        return 2;
    }
    fflush(stdout);

    /* ── 1. the two local scene bits are absent from both tables ────── */
    note("1. the two local scene bits are absent from both tables");
    CHECK(!table_has_name("fl_outerspace"),
          "fl_outerspace is not a sync-table key");
    CHECK(!table_has_name("fl_to_space"),
          "fl_to_space is not a sync-table key");
    for (i = 0; i < (int)(sizeof(fish_ids)/sizeof(fish_ids[0])); ++i) {
        int matches = 0;
        for (int j = 0; j < NUM_FLAGS; ++j)
            matches += s_flag_bits[j].id == fish_ids[i];
        CHECK(matches == 1, "each native fish flag is shared exactly once");
    }

    /* ── 2. a local value of 1 survives every incoming form ─────────── */
    note("2. a local value of 1 survives direct deltas and snapshots");
    clear_save();
    set_native_bit(SCENE_FLAG_A); /* the player's own File67 choice */
    set_native_bit(SCENE_FLAG_B);
    sync_caches();
    reset_records();

    CHECK(native_bit(SCENE_FLAG_A) == 1 && native_bit(SCENE_FLAG_B) == 1,
          "both local scene bits start set");
    CHECK(apply_flag("fl_outerspace", 0) == 0,
          "apply_flag(fl_outerspace, 0) applies nothing");
    CHECK(apply_flag("fl_to_space", 0) == 0,
          "apply_flag(fl_to_space, 0) applies nothing");
    (void)apply_flag("fl_outerspace", 1);
    (void)apply_flag("fl_to_space", 1);
    CHECK(apply_incoming_value("fl_outerspace", 0) == 0,
          "apply_incoming_value(fl_outerspace, 0) is a no-op");
    CHECK(apply_incoming_value("fl_to_space", 0) == 0,
          "apply_incoming_value(fl_to_space, 0) is a no-op");
    CHECK(apply_incoming_flag("fl_outerspace", 0) == 0,
          "apply_incoming_flag(fl_outerspace, 0) is a no-op");
    CHECK(apply_incoming_flag("fl_to_space", 1) == 0,
          "apply_incoming_flag(fl_to_space, 1) is a no-op");
    CHECK(native_bit(SCENE_FLAG_A) == 1 && native_bit(SCENE_FLAG_B) == 1,
          "local scene bits still read 1 after incoming 0 and 1");

    /* ── 3. both compact snapshot forms, live and deferred ──────────── */
    note("3. compact snapshots cannot set or clear the local bits");
    apply_team_state("{\"fl_outerspace\":0,\"fl_to_space\":0}", 1);
    apply_team_state("{\"fl_outerspace\":1,\"fl_to_space\":1}", 0);
    CHECK(native_bit(SCENE_FLAG_A) == 1 && native_bit(SCENE_FLAG_B) == 1,
          "neither snapshot form can set or clear the local scene bits");
    clear_save();
    sync_caches();
    (void)apply_flag("fl_outerspace", 1);
    (void)apply_incoming_value("fl_to_space", 1);
    (void)apply_incoming_flag("fl_outerspace", 1);
    CHECK(native_bit(SCENE_FLAG_A) == 0 && native_bit(SCENE_FLAG_B) == 0,
          "direct delta entry points cannot set the local bits from zero");
    clear_save();
    sync_caches();
    apply_team_state("{\"fl_outerspace\":1,\"fl_to_space\":1}", 1);
    apply_team_state("{\"fl_outerspace\":1,\"fl_to_space\":1}", 0);
    CHECK(native_bit(SCENE_FLAG_A) == 0 && native_bit(SCENE_FLAG_B) == 0,
          "snapshots cannot set the local scene bits from zero");

    /* ── 4. outgoing compact snapshot omits them ────────────────────── */
    note("4. outgoing compact snapshot omits them");
    clear_save();
    set_native_bit(SCENE_FLAG_A);
    set_native_bit(SCENE_FLAG_B);
    sync_caches();
    reset_records();
    CHECK(build_team_state_json() != 0, "the compact snapshot builds");
    CHECK(!text_has(s_team_state_json, "fl_outerspace") &&
              !text_has(s_team_state_json, "fl_to_space"),
          "the compact snapshot JSON omits both local scene bits");
    CHECK(anchor_update_team_state(s_team_state_json) != 0,
          "the compact snapshot publishes through anchor_update_team_state");
    CHECK(!text_has(s_team_state_json, "fl_outerspace"),
          "the published compact snapshot still omits fl_outerspace");

    /* ── 5. outgoing monitor publishes nothing for them ─────────────── */
    note("5. outgoing monitor publishes nothing for them");
    clear_save();
    sync_caches();
    /* Change after caching; otherwise the old implementation also sends
     * nothing and this would not exercise acquisition detection. */
    set_native_bit(SCENE_FLAG_A);
    set_native_bit(SCENE_FLAG_B);
    reset_records();
    for (i = 0; i < 8; ++i)
    {
        s_set_flag_send_timer = 0;
        monitor_and_send_changes();
    }
    CHECK(!sent_any("fl_outerspace") && !sent_any("fl_to_space"),
          "monitor_and_send_changes never publishes the local scene bits");
    CHECK(s_send_count == 0,
          "a save holding only the two local bits sends nothing at all");

    /* ── 6. the merge snapshot form omits them too ──────────────────── */
    note("6. the merge snapshot form omits them too");
    reset_records();
    (void)broadcast_team_state_snapshot();
    CHECK(s_custom_calls == 1, "the merge snapshot is sent as a custom packet");
    CHECK(strcmp(s_last_custom_type, "MNSG_TEAM_STATE") == 0,
          "the merge snapshot uses MNSG_TEAM_STATE");
    CHECK(!text_has(s_last_custom_payload, "fl_outerspace") &&
              !text_has(s_last_custom_payload, "fl_to_space"),
          "the merge snapshot payload omits both local scene bits");

    /* ── 7. legitimate neighbours still apply and publish ───────────── */
    note("7. legitimate neighbours still apply and publish");
    clear_save();
    sync_caches();
    reset_records();
    /* fl_kyushu is notification-visible, so its return names the applied
     * change.  fl_mtfuji is intentionally notification-hidden, so its own
     * result is asserted on the save bit rather than on the display name. */
    CHECK(apply_flag("fl_kyushu", 1) != 0, "fl_kyushu (0x017) still applies");
    CHECK(native_bit(0x017) == 1, "fl_kyushu set its native bit");
    (void)apply_flag("fl_mtfuji", 1);
    CHECK(native_bit(0x0C3) == 1,
          "fl_mtfuji (0xC3) still writes its native bit");
    CHECK(apply_flag("fl_shore_entry", 1) != 0,
          "fl_shore_entry (0xC4) still applies");
    CHECK(native_bit(0x0C4) == 1, "fl_shore_entry set its native bit");
    CHECK(apply_flag("mi_star", 1) != 0, "mi_star still applies");
    CHECK(apply_flag("mi_moon", 1) != 0, "mi_moon still applies");
    CHECK(apply_flag("mi_flower", 1) != 0, "mi_flower still applies");
    CHECK(apply_flag("mi_snow", 1) != 0, "mi_snow still applies");
    CHECK(SAVE_READ32(0x250) == 1 && SAVE_READ32(0x254) == 1 &&
              SAVE_READ32(0x258) == 1 && SAVE_READ32(0x25C) == 1,
          "all four miracle words hold their applied value");

    /* The monitor only publishes values that changed since the cache sync, so
     * drive it from freshly set native state, exactly like the game does. */
    clear_save();
    sync_caches();
    set_native_bit(0x017);
    set_native_bit(0x0C3);
    set_native_bit(0x0C4);
    SAVE_WRITE32(0x250, 1);
    SAVE_WRITE32(0x254, 1);
    SAVE_WRITE32(0x258, 1);
    SAVE_WRITE32(0x25C, 1);
    reset_records();
    {
    int before = s_failures;
    for (i = 0; i < 16; ++i)
    {
        s_set_flag_send_timer = 0;
        monitor_and_send_changes();
    }
    CHECK(sent_any("fl_kyushu"),
          "the monitor still publishes the Kyushu travel flag 0x017");
    CHECK(sent_any("fl_mtfuji") && sent_any("fl_shore_entry"),
          "the monitor still publishes 0xC3 and 0xC4");
    CHECK(sent_any("mi_star") && sent_any("mi_moon") &&
              sent_any("mi_flower") && sent_any("mi_snow"),
          "the monitor still publishes the miracle fields");
    CHECK(sent_queued("fl_kyushu") || sent_queued("fl_mtfuji"),
          "published durable deltas use the queued path");
    CHECK(!sent_any("fl_outerspace") && !sent_any("fl_to_space"),
          "publishing neighbours never drags the local bits along");
    if (s_failures != before) dump_sends("monitor");
    }

    reset_records();
    CHECK(build_team_state_json() != 0, "the neighbour snapshot builds");
    CHECK(text_has(s_team_state_json, "fl_kyushu") &&
              text_has(s_team_state_json, "fl_mtfuji") &&
              text_has(s_team_state_json, "fl_shore_entry") &&
              text_has(s_team_state_json, "mi_star") &&
              text_has(s_team_state_json, "mi_moon") &&
              text_has(s_team_state_json, "mi_flower") &&
              text_has(s_team_state_json, "mi_snow"),
          "the snapshot still carries the legitimate neighbours");
    CHECK(!text_has(s_team_state_json, "fl_outerspace") &&
              !text_has(s_team_state_json, "fl_to_space"),
          "the neighbour snapshot still omits the local bits");
    if (s_failures) printf("  snapshot: %s\n", s_team_state_json);
    fflush(stdout);

    note("8. equipment collection flags apply, persist and publish");
    clear_save(); sync_caches(); reset_records();
    (void)apply_incoming_flag("pk_fire_ryo",1);
    (void)apply_incoming_flag("pk_bazooka",1);
    (void)apply_incoming_flag("pk_hammer",1);
    CHECK(native_bit(0x1a4) && native_bit(0x1a5) && native_bit(0x1a6),
          "all three equipment flags apply to their native bits");
    CHECK(build_team_state_json() && text_has(s_team_state_json,"pk_fire_ryo"),
          "Fire Ryo collection is present in the durable snapshot");
    (void)apply_incoming_flag("pk_fire_ryo",0);
    CHECK(native_bit(0x1a4),"stale zero cannot undo Fire Ryo collection");
    clear_save(); sync_caches(); reset_records();
    apply_team_state("{\"pk_fire_ryo\":1,\"pk_bazooka\":1,\"pk_hammer\":1}",1);
    CHECK(native_bit(0x1a4) && native_bit(0x1a5) && native_bit(0x1a6),
          "late-entrant snapshot restores all three collection bits");
    clear_save(); sync_caches(); reset_records();
    set_native_bit(0x1a4);set_native_bit(0x1a5);set_native_bit(0x1a6);
    for(i=0;i<8;++i) {s_set_flag_send_timer=0;monitor_and_send_changes();}
    CHECK(sent_queued("pk_fire_ryo") && sent_queued("pk_bazooka") && sent_queued("pk_hammer"),
          "equipment collections use existing durable queued deltas");

    note("9. Cat Eyes quest purchases share through every progression path");
    {
        static const char *shop_names[3] = {
            "fl_ce_dharma", "fl_ce_notice", "fl_ce_doll"};
        static const unsigned int shop_ids[3] = {0x1C5, 0x1C6, 0x1C7};
        char packet[128];
        clear_save(); sync_caches(); reset_records();
        for (i = 0; i < 3; ++i)
        {
            CHECK(table_has_name(shop_names[i]),
                  "quest purchase marker is in the durable flag table");
            CHECK(apply_incoming_flag(shop_names[i], 1) != 0 &&
                  native_bit(shop_ids[i]),
                  "direct legacy quest delta sets the save bit");
            clear_save(); sync_caches(); reset_records();
            snprintf(packet, sizeof(packet),
                     "{\"type\":\"SET_FLAG\",\"flag\":\"%s\",\"value\":1,\"clientId\":2}",
                     shop_names[i]);
            s_incoming_packet = packet;
            process_incoming_packets();
            CHECK(native_bit(shop_ids[i]),
                  "queued legacy quest delta restores the purchase flag");
            clear_save(); sync_caches(); reset_records();
        }
        apply_team_state("{\"fl_ce_dharma\":1,\"fl_ce_notice\":1,\"fl_ce_doll\":1}", 0);
        apply_team_state("{\"fl_ce_dharma\":1,\"fl_ce_notice\":1,\"fl_ce_doll\":1}", 1);
        CHECK(native_bit(0x1C5) && native_bit(0x1C6) && native_bit(0x1C7),
              "stored and live compact snapshots restore quest purchases");
        CHECK(SAVE_READ32(0x100) == 0,
              "quest purchase flags do not synthesize a Doll count increment");

        /* The native shop writes the save bit; the monitor and snapshots
         * publish its durable quest result without replaying a purchase. */
        clear_save(); sync_caches(); reset_records();
        for (i = 0; i < 3; ++i) set_native_bit(shop_ids[i]);
        reset_records();
        for (i = 0; i < 8; ++i)
        {
            s_set_flag_send_timer = 0;
            monitor_and_send_changes();
        }
        CHECK(sent_queued("fl_ce_dharma") && sent_queued("fl_ce_notice") &&
                  sent_queued("fl_ce_doll"),
              "local quest purchases emit durable SET_FLAG deltas");
        CHECK(build_team_state_json() &&
                  text_has(s_team_state_json, "fl_ce_dharma") &&
                  text_has(s_team_state_json, "fl_ce_notice") &&
                  text_has(s_team_state_json, "fl_ce_doll"),
              "stored compact snapshot includes quest purchases");
        (void)broadcast_team_state_snapshot();
        CHECK(s_custom_calls == 1 &&
                  text_has(s_last_custom_payload, "fl_ce_dharma") &&
                  text_has(s_last_custom_payload, "fl_ce_notice") &&
                  text_has(s_last_custom_payload, "fl_ce_doll"),
              "live merge snapshot includes quest purchases");

        clear_save(); sync_caches(); reset_records();
        item_sync_force_flag("fl_ce_doll");
        CHECK(native_bit(0x1C7) && s_send_count == 1,
              "debug Force broadcasts the shared quest flag");
        CHECK(item_sync_write_local_flag_val("fl_ce_notice", 1) &&
                  native_bit(0x1C6) && s_send_count == 1 &&
                  build_team_state_json() &&
                  text_has(s_team_state_json, "fl_ce_notice"),
              "race local helper persists the shared quest flag for snapshots");
    }

    note("10. keys, world equipment and counts remain shared");
    clear_save(); sync_caches(); reset_records();
    (void)apply_incoming_flag("ky_s_oc_tile", 1);
    (void)apply_incoming_flag("pk_fire_ryo", 1);
    (void)apply_incoming_flag("stat_dolls", 2);
    (void)apply_incoming_flag("stat_doll_p", 1);
    CHECK(native_bit(0x10A) && native_bit(0x1A4) &&
              SAVE_READ32(0x100) == 2 && SAVE_READ32(0x0FC) == 1,
          "dungeon key, found equipment and doll counts still apply");
    CHECK(build_team_state_json() &&
              text_has(s_team_state_json, "ky_s_oc_tile") &&
              text_has(s_team_state_json, "pk_fire_ryo") &&
              text_has(s_team_state_json, "stat_dolls") &&
              text_has(s_team_state_json, "stat_doll_p"),
          "non-shop progression stays in the compact team snapshot");

    /* No new personal health / current-ryo sync was introduced. */
    for (i = 0; i < NUM_FIELDS; ++i)
        if (s_fields[i].off == DS_HP_OFFSET ||
            s_fields[i].off == DS_RYO_OFFSET)
            hp_or_ryo_field = 1;
    CHECK(!hp_or_ryo_field,
          "current health and current ryo are not field-table entries");
    CHECK(!table_has_name("stat_hp") && !table_has_name("stat_ryo") &&
              !table_has_name("cur_hp") && !table_has_name("cur_ryo"),
          "no new health/ryo sync keys exist");

    if (s_failures)
    {
        printf("item scene flags: %d failure(s)\n", s_failures);
        return 1;
    }
    printf("item scene flags: personal scene bits stay local; "
           "shared progression unaffected\n");
    return 0;
}
