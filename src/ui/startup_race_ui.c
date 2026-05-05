#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"
#include "anchor_flag_catalog.h"

void item_sync_force_flag(const char *name);
void item_sync_force_flag_val(const char *name, int val);
int item_sync_save_is_loaded(void);
int item_sync_write_local_flag_val(const char *name, int val);

extern void func_8000B640_C240(void);
extern void func_8000B5D0_C1D0(void);
extern void func_8003521C_35E1C(void *func_ptr);
extern void func_801CD890_660740(void);
extern unsigned char *D_8015C5C8_15D1C8;

#define RACE_MAX_FLAGS 320
#define RACE_MAX_CATEGORIES 64
#define RACE_TEXT_LEN 96

static const RecompuiColor R_BG = {8, 8, 12, 230};
static const RecompuiColor R_PANEL = {14, 14, 22, 255};
static const RecompuiColor R_HEAD = {20, 17, 27, 255};
static const RecompuiColor R_CARD = {18, 18, 27, 255};
static const RecompuiColor R_CARD_ALT = {23, 23, 34, 255};
static const RecompuiColor R_BORDER = {72, 62, 96, 225};
static const RecompuiColor R_WHITE = {248, 248, 252, 255};
static const RecompuiColor R_DIM = {170, 172, 182, 255};
static const RecompuiColor R_PURPLE = {168, 104, 255, 255};
static const RecompuiColor R_GREEN = {70, 190, 90, 255};
static const RecompuiColor R_RED = {220, 68, 68, 255};

static RecompuiContext s_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_setup_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_goal_view = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_category_views[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_category_views[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_status_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_goal_btns[RACE_MAX_FLAGS];
static RecompuiResource s_start_btns[RACE_MAX_FLAGS];
static RecompuiResource s_category_count_lbls[RACE_MAX_CATEGORIES];
static RecompuiResource s_goal_category_count_lbls[RACE_MAX_CATEGORIES];
static char s_goal_btn_text[RACE_MAX_FLAGS][RACE_TEXT_LEN];
static char s_start_btn_text[RACE_MAX_FLAGS][RACE_TEXT_LEN];
static char s_category_count_text[RACE_MAX_CATEGORIES][32];
static char s_goal_category_count_text[RACE_MAX_CATEGORIES][32];
static unsigned char s_start_selected[RACE_MAX_FLAGS];
static const char *s_category_names[RACE_MAX_CATEGORIES];
static int s_category_start[RACE_MAX_CATEGORIES];
static int s_category_end[RACE_MAX_CATEGORIES];

static int s_ui_built = 0;
static int s_initialized = 0;
static int s_visible = 0;
static int s_pending_open = 0;
static int s_pending_back = 0;
static int s_pending_start = 0;
static int s_pending_choose_goal = 0;
static int s_pending_goal_done = 0;
static int s_pending_category_idx = -1;
static int s_pending_goal_category_idx = -1;
static int s_pending_goal_idx = -1;
static int s_pending_toggle_idx = -1;
static int s_goal_idx = -1;
static int s_flag_count = 0;
static int s_category_count = 0;
static int s_active_category = -1;
static int s_active_goal_category = -1;
static int s_screen = 0;
static int s_race_apply_pending = 0;
static int s_race_autoload_pending = 0;
static int s_race_pending_count = 0;
static const char *s_race_pending_keys[RACE_MAX_FLAGS];
static int s_race_pending_values[RACE_MAX_FLAGS];

static void apply_pending_race_flags(void)
{
    int i;

    if (!s_race_apply_pending)
        return;

    if (!item_sync_save_is_loaded())
        return;

    for (i = 0; i < s_race_pending_count; i++)
    {
        if (s_race_pending_keys[i])
            item_sync_write_local_flag_val(s_race_pending_keys[i], s_race_pending_values[i]);
    }

    s_race_apply_pending = 0;
    s_race_pending_count = 0;
    recomp_printf("[Race] Applied configured starting flags to loaded save.\n");
}

static void copy_text(char *dst, const char *src, int max_len)
{
    int i = 0;
    if (!dst || max_len <= 0)
        return;
    if (!src)
        src = "";
    while (i < max_len - 1 && src[i])
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void make_toggle_text(char *dst, const char *prefix, const char *label)
{
    int i = 0;
    int j = 0;
    while (i < RACE_TEXT_LEN - 1 && prefix && prefix[j])
        dst[i++] = prefix[j++];
    j = 0;
    while (i < RACE_TEXT_LEN - 1 && label && label[j])
        dst[i++] = label[j++];
    dst[i] = '\0';
}

static int text_equals(const char *a, const char *b)
{
    int i = 0;

    if (!a || !b)
        return 0;

    while (a[i] && b[i])
    {
        if (a[i] != b[i])
            return 0;
        i++;
    }

    return a[i] == b[i];
}

static int is_default_start_flag(const AnchorFlagEntry *entry)
{
    if (!entry || !entry->key)
        return 0;

    return text_equals(entry->key, "chr_goemon") ||
           text_equals(entry->key, "chr_ebisu");
}

static void set_status(const char *text, int ok)
{
    if (s_status_lbl == RECOMPUI_NULL_RESOURCE)
        return;

    recompui_open_context(s_ctx);
    recompui_set_text(s_status_lbl, text);
    recompui_set_color(s_status_lbl, ok ? &R_GREEN : &R_RED);
    recompui_close_context(s_ctx);
}

static void append_uint(char *dst, int *pos, int max_len, int value)
{
    char tmp[12];
    int len = 0;
    int i;

    if (value == 0)
    {
        if (*pos < max_len - 1)
            dst[(*pos)++] = '0';
        return;
    }

    while (value && len < (int)sizeof(tmp))
    {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (i = len - 1; i >= 0 && *pos < max_len - 1; i--)
        dst[(*pos)++] = tmp[i];
}

static void update_category_count(int cat_idx)
{
    int selected = 0;
    int total = 0;
    int pos = 0;
    int i;

    if (cat_idx < 0 || cat_idx >= s_category_count)
        return;

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;
        total++;
        if (i >= 0 && i < RACE_MAX_FLAGS && s_start_selected[i])
            selected++;
    }

    s_category_count_text[cat_idx][pos++] = '[';
    append_uint(s_category_count_text[cat_idx], &pos, 32, selected);
    if (pos < 31)
        s_category_count_text[cat_idx][pos++] = '/';
    append_uint(s_category_count_text[cat_idx], &pos, 32, total);
    if (pos < 31)
        s_category_count_text[cat_idx][pos++] = ']';
    s_category_count_text[cat_idx][pos] = '\0';

    recompui_open_context(s_ctx);
    if (s_category_count_lbls[cat_idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_category_count_lbls[cat_idx], s_category_count_text[cat_idx]);
    recompui_close_context(s_ctx);
}

static int category_for_flag(int flag_idx)
{
    int i;
    for (i = 0; i < s_category_count; i++)
    {
        if (flag_idx >= s_category_start[i] && flag_idx < s_category_end[i])
            return i;
    }
    return -1;
}

static void update_goal_category_count(int cat_idx)
{
    int total = 0;
    int pos = 0;
    int i;

    if (cat_idx < 0 || cat_idx >= s_category_count)
        return;

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (entry && entry->key)
            total++;
    }

    if (category_for_flag(s_goal_idx) == cat_idx)
    {
        const char *text = "[Goal]";
        while (text[pos] && pos < 31)
        {
            s_goal_category_count_text[cat_idx][pos] = text[pos];
            pos++;
        }
    }
    else
    {
        s_goal_category_count_text[cat_idx][pos++] = '[';
        append_uint(s_goal_category_count_text[cat_idx], &pos, 32, total);
        if (pos < 31)
            s_goal_category_count_text[cat_idx][pos++] = ']';
    }
    s_goal_category_count_text[cat_idx][pos] = '\0';

    recompui_open_context(s_ctx);
    if (s_goal_category_count_lbls[cat_idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_goal_category_count_lbls[cat_idx], s_goal_category_count_text[cat_idx]);
    recompui_close_context(s_ctx);
}

static void set_screen(int screen)
{
    int i;
    s_screen = screen;

    recompui_open_context(s_ctx);
    recompui_set_display(s_setup_view, screen == 0 ? DISPLAY_FLEX : DISPLAY_NONE);
    recompui_set_display(s_goal_view, screen == 1 ? DISPLAY_FLEX : DISPLAY_NONE);
    for (i = 0; i < s_category_count; i++)
    {
        if (s_category_views[i] != RECOMPUI_NULL_RESOURCE)
            recompui_set_display(s_category_views[i],
                                  (screen == 2 && i == s_active_category) ? DISPLAY_FLEX : DISPLAY_NONE);
        if (s_goal_category_views[i] != RECOMPUI_NULL_RESOURCE)
            recompui_set_display(s_goal_category_views[i],
                                  (screen == 3 && i == s_active_goal_category) ? DISPLAY_FLEX : DISPLAY_NONE);
    }
    recompui_close_context(s_ctx);
}

static void update_goal_label(void)
{
    const AnchorFlagEntry *entry = anchor_flag_catalog_get(s_goal_idx);

    recompui_open_context(s_ctx);
    if (entry && entry->key)
        recompui_set_text(s_goal_lbl, entry->display);
    else
        recompui_set_text(s_goal_lbl, "No end condition selected");
    recompui_close_context(s_ctx);
}

static void update_start_button(int idx)
{
    const AnchorFlagEntry *entry = anchor_flag_catalog_get(idx);
    if (!entry || !entry->key || idx < 0 || idx >= RACE_MAX_FLAGS)
        return;

    make_toggle_text(s_start_btn_text[idx],
                     s_start_selected[idx] ? "[x] " : "[ ] ",
                     entry->display);

    recompui_open_context(s_ctx);
    if (s_start_btns[idx] != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_start_btns[idx], s_start_btn_text[idx]);
    recompui_close_context(s_ctx);

    update_category_count(category_for_flag(idx));
}

static void update_goal_buttons(int old_idx, int new_idx)
{
    const AnchorFlagEntry *entry;

    recompui_open_context(s_ctx);

    if (old_idx >= 0 && old_idx < s_flag_count && old_idx < RACE_MAX_FLAGS)
    {
        entry = anchor_flag_catalog_get(old_idx);
        if (entry && entry->key && s_goal_btns[old_idx] != RECOMPUI_NULL_RESOURCE)
        {
            make_toggle_text(s_goal_btn_text[old_idx], "", entry->display);
            recompui_set_text(s_goal_btns[old_idx], s_goal_btn_text[old_idx]);
        }
    }

    if (new_idx >= 0 && new_idx < s_flag_count && new_idx < RACE_MAX_FLAGS)
    {
        entry = anchor_flag_catalog_get(new_idx);
        if (entry && entry->key && s_goal_btns[new_idx] != RECOMPUI_NULL_RESOURCE)
        {
            make_toggle_text(s_goal_btn_text[new_idx], "[Goal] ", entry->display);
            recompui_set_text(s_goal_btns[new_idx], s_goal_btn_text[new_idx]);
        }
    }

    recompui_close_context(s_ctx);
}

static void on_back_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_back = 1;
}

static void on_choose_goal_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_choose_goal = 1;
}

static void on_goal_done_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_done = 1;
}

static void on_category_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_category_idx = (int)(unsigned long)ud;
}

static void on_goal_category_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_category_idx = (int)(unsigned long)ud;
}

static void on_goal_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_goal_idx = (int)(unsigned long)ud;
}

static void on_start_flag_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_toggle_idx = (int)(unsigned long)ud;
}

static void on_start_race_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_start = 1;
}

static RecompuiResource make_grid_row(RecompuiResource section)
{
    RecompuiResource row = recompui_create_element(s_ctx, section);
    recompui_set_display(row, DISPLAY_FLEX);
    recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
    recompui_set_gap(row, 8.0f, UNIT_DP);
    recompui_set_min_height(row, 38.0f, UNIT_DP);
    return row;
}

static void add_start_flag_button(RecompuiResource row, int idx, const AnchorFlagEntry *entry)
{
    make_toggle_text(s_start_btn_text[idx],
                     s_start_selected[idx] ? "[x] " : "[ ] ",
                     entry->display);

    s_start_btns[idx] = recompui_create_button(s_ctx, row, s_start_btn_text[idx], BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(s_start_btns[idx], CURSOR_POINTER);
    recompui_set_font_size(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_text_align(s_start_btns[idx], TEXT_ALIGN_LEFT);
    recompui_set_flex_grow(s_start_btns[idx], 1.0f);
    recompui_set_flex_basis(s_start_btns[idx], 50.0f, UNIT_PERCENT);
    recompui_set_height(s_start_btns[idx], 34.0f, UNIT_DP);
    recompui_set_padding_top(s_start_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_bottom(s_start_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_left(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_padding_right(s_start_btns[idx], 12.0f, UNIT_DP);
    recompui_set_tab_index(s_start_btns[idx], TAB_INDEX_AUTO);
    recompui_register_callback(s_start_btns[idx], on_start_flag_clicked, (void *)(unsigned long)idx);
}

static void add_goal_flag_button(RecompuiResource row, int idx, const AnchorFlagEntry *entry)
{
    copy_text(s_goal_btn_text[idx], entry->display, RACE_TEXT_LEN);

    s_goal_btns[idx] = recompui_create_button(s_ctx, row, s_goal_btn_text[idx], BUTTONSTYLE_SECONDARY);
    recompui_set_cursor(s_goal_btns[idx], CURSOR_POINTER);
    recompui_set_font_size(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_text_align(s_goal_btns[idx], TEXT_ALIGN_LEFT);
    recompui_set_flex_grow(s_goal_btns[idx], 1.0f);
    recompui_set_flex_basis(s_goal_btns[idx], 50.0f, UNIT_PERCENT);
    recompui_set_height(s_goal_btns[idx], 34.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_btns[idx], 3.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_btns[idx], 12.0f, UNIT_DP);
    recompui_set_tab_index(s_goal_btns[idx], TAB_INDEX_AUTO);
    recompui_register_callback(s_goal_btns[idx], on_goal_clicked, (void *)(unsigned long)idx);
}

static void build_category_index(void)
{
    int current = -1;
    int i;

    s_category_count = 0;
    for (i = 0; i < s_flag_count; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry)
            continue;

        if (!entry->key)
        {
            if (current >= 0)
                s_category_end[current] = i;
            if (s_category_count < RACE_MAX_CATEGORIES)
            {
                current = s_category_count++;
                s_category_names[current] = entry->display;
                s_category_start[current] = i + 1;
                s_category_end[current] = i + 1;
            }
            continue;
        }

        if (current >= 0)
            s_category_end[current] = i + 1;
    }
}

static void build_category_picker(RecompuiResource list)
{
    int i;

    for (i = 0; i < s_category_count; i++)
    {
        RecompuiResource row = recompui_create_element(s_ctx, list);
        recompui_set_display(row, DISPLAY_FLEX);
        recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(row, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_min_height(row, 52.0f, UNIT_DP);
        recompui_set_padding_left(row, 16.0f, UNIT_DP);
        recompui_set_padding_right(row, 12.0f, UNIT_DP);
        recompui_set_background_color(row, (i & 1) ? &R_CARD_ALT : &R_CARD);
        recompui_set_border_radius(row, 3.0f, UNIT_DP);

        RecompuiResource name = recompui_create_label(s_ctx, row, s_category_names[i], LABELSTYLE_SMALL);
        recompui_set_color(name, &R_PURPLE);
        recompui_set_font_weight(name, 700);
        recompui_set_font_size(name, 18.0f, UNIT_DP);
        recompui_set_flex_grow(name, 1.0f);

        s_category_count_lbls[i] = recompui_create_label(s_ctx, row, "[0/0]", LABELSTYLE_ANNOTATION);
        recompui_set_color(s_category_count_lbls[i], &R_DIM);
        recompui_set_font_size(s_category_count_lbls[i], 14.0f, UNIT_DP);
        recompui_set_margin_right(s_category_count_lbls[i], 12.0f, UNIT_DP);

        RecompuiResource button = recompui_create_button(s_ctx, row, "Configure", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(button, CURSOR_POINTER);
        recompui_set_font_size(button, 13.0f, UNIT_DP);
        recompui_set_height(button, 32.0f, UNIT_DP);
        recompui_set_width(button, 112.0f, UNIT_DP);
        recompui_set_padding_top(button, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(button, 4.0f, UNIT_DP);
        recompui_set_tab_index(button, TAB_INDEX_AUTO);
        recompui_register_callback(button, on_category_clicked, (void *)(unsigned long)i);
    }
}

static void build_goal_category_picker(RecompuiResource list)
{
    int i;

    for (i = 0; i < s_category_count; i++)
    {
        RecompuiResource row = recompui_create_element(s_ctx, list);
        recompui_set_display(row, DISPLAY_FLEX);
        recompui_set_flex_direction(row, FLEX_DIRECTION_ROW);
        recompui_set_align_items(row, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(row, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_min_height(row, 52.0f, UNIT_DP);
        recompui_set_padding_left(row, 16.0f, UNIT_DP);
        recompui_set_padding_right(row, 12.0f, UNIT_DP);
        recompui_set_background_color(row, (i & 1) ? &R_CARD_ALT : &R_CARD);
        recompui_set_border_radius(row, 3.0f, UNIT_DP);

        RecompuiResource name = recompui_create_label(s_ctx, row, s_category_names[i], LABELSTYLE_SMALL);
        recompui_set_color(name, &R_PURPLE);
        recompui_set_font_weight(name, 700);
        recompui_set_font_size(name, 18.0f, UNIT_DP);
        recompui_set_flex_grow(name, 1.0f);

        s_goal_category_count_lbls[i] = recompui_create_label(s_ctx, row, "[0]", LABELSTYLE_ANNOTATION);
        recompui_set_color(s_goal_category_count_lbls[i], &R_DIM);
        recompui_set_font_size(s_goal_category_count_lbls[i], 14.0f, UNIT_DP);
        recompui_set_margin_right(s_goal_category_count_lbls[i], 12.0f, UNIT_DP);

        RecompuiResource button = recompui_create_button(s_ctx, row, "Choose", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(button, CURSOR_POINTER);
        recompui_set_font_size(button, 13.0f, UNIT_DP);
        recompui_set_height(button, 32.0f, UNIT_DP);
        recompui_set_width(button, 112.0f, UNIT_DP);
        recompui_set_padding_top(button, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(button, 4.0f, UNIT_DP);
        recompui_set_tab_index(button, TAB_INDEX_AUTO);
        recompui_register_callback(button, on_goal_category_clicked, (void *)(unsigned long)i);
    }
}

static void build_category_detail_view(RecompuiResource parent, int cat_idx)
{
    RecompuiResource row = RECOMPUI_NULL_RESOURCE;
    int cols = 0;
    int i;

    s_category_views[cat_idx] = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_category_views[cat_idx], DISPLAY_NONE);
    recompui_set_flex_direction(s_category_views[cat_idx], FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_category_views[cat_idx], 10.0f, UNIT_DP);
    recompui_set_padding_left(s_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_right(s_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_top(s_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_category_views[cat_idx], 1.0f);

    RecompuiResource title = recompui_create_label(
        s_ctx, s_category_views[cat_idx], s_category_names[cat_idx], LABELSTYLE_NORMAL);
    recompui_set_color(title, &R_PURPLE);
    recompui_set_font_weight(title, 700);
    recompui_set_font_size(title, 20.0f, UNIT_DP);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_category_views[cat_idx],
        "Choose the flags this race should grant before the run starts.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_category_views[cat_idx]);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 4.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;

        if (row == RECOMPUI_NULL_RESOURCE || cols >= 2)
        {
            row = make_grid_row(list);
            cols = 0;
        }
        add_start_flag_button(row, i, entry);
        cols++;
    }
}

static void build_goal_category_detail_view(RecompuiResource parent, int cat_idx)
{
    RecompuiResource row = RECOMPUI_NULL_RESOURCE;
    int cols = 0;
    int i;

    s_goal_category_views[cat_idx] = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_goal_category_views[cat_idx], DISPLAY_NONE);
    recompui_set_flex_direction(s_goal_category_views[cat_idx], FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_goal_category_views[cat_idx], 10.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_category_views[cat_idx], 18.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_category_views[cat_idx], 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_goal_category_views[cat_idx], 1.0f);

    RecompuiResource title = recompui_create_label(
        s_ctx, s_goal_category_views[cat_idx], s_category_names[cat_idx], LABELSTYLE_NORMAL);
    recompui_set_color(title, &R_PURPLE);
    recompui_set_font_weight(title, 700);
    recompui_set_font_size(title, 20.0f, UNIT_DP);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_goal_category_views[cat_idx],
        "Pick the flag that ends the race when it becomes synced.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_goal_category_views[cat_idx]);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 4.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    for (i = s_category_start[cat_idx]; i < s_category_end[cat_idx]; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!entry || !entry->key)
            continue;

        if (row == RECOMPUI_NULL_RESOURCE || cols >= 2)
        {
            row = make_grid_row(list);
            cols = 0;
        }
        add_goal_flag_button(row, i, entry);
        cols++;
    }
}

static void build_setup_view(RecompuiResource parent)
{
    s_setup_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_setup_view, DISPLAY_FLEX);
    recompui_set_flex_direction(s_setup_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_setup_view, 10.0f, UNIT_DP);
    recompui_set_padding_left(s_setup_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_setup_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_setup_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_setup_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_setup_view, 1.0f);

    RecompuiResource goal_card = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(goal_card, DISPLAY_FLEX);
    recompui_set_flex_direction(goal_card, FLEX_DIRECTION_ROW);
    recompui_set_align_items(goal_card, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(goal_card, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(goal_card, 12.0f, UNIT_DP);
    recompui_set_padding_left(goal_card, 16.0f, UNIT_DP);
    recompui_set_padding_right(goal_card, 12.0f, UNIT_DP);
    recompui_set_padding_top(goal_card, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(goal_card, 10.0f, UNIT_DP);
    recompui_set_min_height(goal_card, 66.0f, UNIT_DP);
    recompui_set_background_color(goal_card, &R_CARD_ALT);
    recompui_set_border_radius(goal_card, 6.0f, UNIT_DP);
    recompui_set_border_width(goal_card, 1.0f, UNIT_DP);
    recompui_set_border_color(goal_card, &R_BORDER);

    RecompuiResource goal_text = recompui_create_element(s_ctx, goal_card);
    recompui_set_display(goal_text, DISPLAY_FLEX);
    recompui_set_flex_direction(goal_text, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(goal_text, 4.0f, UNIT_DP);
    recompui_set_flex_grow(goal_text, 1.0f);

    RecompuiResource goal_title = recompui_create_label(s_ctx, goal_text, "Race End Condition", LABELSTYLE_SMALL);
    recompui_set_color(goal_title, &R_DIM);
    recompui_set_font_size(goal_title, 14.0f, UNIT_DP);

    s_goal_lbl = recompui_create_label(s_ctx, goal_text, "No end condition selected", LABELSTYLE_NORMAL);
    recompui_set_color(s_goal_lbl, &R_WHITE);
    recompui_set_font_size(s_goal_lbl, 18.0f, UNIT_DP);

    RecompuiResource choose_btn = recompui_create_button(s_ctx, goal_card, "Choose Goal", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(choose_btn, CURSOR_POINTER);
    recompui_set_font_size(choose_btn, 13.0f, UNIT_DP);
    recompui_set_height(choose_btn, 36.0f, UNIT_DP);
    recompui_set_width(choose_btn, 140.0f, UNIT_DP);
    recompui_set_padding_top(choose_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(choose_btn, 4.0f, UNIT_DP);
    recompui_set_padding_left(choose_btn, 10.0f, UNIT_DP);
    recompui_set_padding_right(choose_btn, 10.0f, UNIT_DP);
    recompui_set_tab_index(choose_btn, TAB_INDEX_AUTO);
    recompui_register_callback(choose_btn, on_choose_goal_clicked, 0);

    RecompuiResource intro = recompui_create_label(
        s_ctx, s_setup_view,
        "Starting flags are applied when the race begins. Pick only what the route should begin with.",
        LABELSTYLE_SMALL);
    recompui_set_color(intro, &R_DIM);
    recompui_set_font_size(intro, 14.0f, UNIT_DP);
    recompui_set_margin_top(intro, 2.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_setup_view);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 3.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    build_category_picker(list);
}

static void build_goal_view(RecompuiResource parent)
{
    s_goal_view = recompui_create_element(s_ctx, parent);
    recompui_set_display(s_goal_view, DISPLAY_NONE);
    recompui_set_flex_direction(s_goal_view, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(s_goal_view, 10.0f, UNIT_DP);
    recompui_set_padding_left(s_goal_view, 18.0f, UNIT_DP);
    recompui_set_padding_right(s_goal_view, 18.0f, UNIT_DP);
    recompui_set_padding_top(s_goal_view, 12.0f, UNIT_DP);
    recompui_set_padding_bottom(s_goal_view, 12.0f, UNIT_DP);
    recompui_set_flex_grow(s_goal_view, 1.0f);

    RecompuiResource top = recompui_create_element(s_ctx, s_goal_view);
    recompui_set_display(top, DISPLAY_FLEX);
    recompui_set_flex_direction(top, FLEX_DIRECTION_ROW);
    recompui_set_align_items(top, ALIGN_ITEMS_CENTER);
    recompui_set_justify_content(top, JUSTIFY_CONTENT_SPACE_BETWEEN);
    recompui_set_gap(top, 12.0f, UNIT_DP);

    RecompuiResource label = recompui_create_label(s_ctx, top, "Choose Race End Condition", LABELSTYLE_NORMAL);
    recompui_set_color(label, &R_WHITE);
    recompui_set_font_weight(label, 700);
    recompui_set_font_size(label, 20.0f, UNIT_DP);

    RecompuiResource done_btn = recompui_create_button(s_ctx, top, "Done", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(done_btn, CURSOR_POINTER);
    recompui_set_font_size(done_btn, 13.0f, UNIT_DP);
    recompui_set_height(done_btn, 36.0f, UNIT_DP);
    recompui_set_width(done_btn, 110.0f, UNIT_DP);
    recompui_set_padding_top(done_btn, 4.0f, UNIT_DP);
    recompui_set_padding_bottom(done_btn, 4.0f, UNIT_DP);
    recompui_set_tab_index(done_btn, TAB_INDEX_AUTO);
    recompui_register_callback(done_btn, on_goal_done_clicked, 0);

    RecompuiResource hint = recompui_create_label(
        s_ctx, s_goal_view,
        "Any synced multiplayer flag can be used as the finish condition.",
        LABELSTYLE_SMALL);
    recompui_set_color(hint, &R_DIM);
    recompui_set_font_size(hint, 14.0f, UNIT_DP);

    RecompuiResource list = recompui_create_element(s_ctx, s_goal_view);
    recompui_set_display(list, DISPLAY_FLEX);
    recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(list, 3.0f, UNIT_DP);
    recompui_set_flex_grow(list, 1.0f);
    recompui_set_overflow_y(list, OVERFLOW_SCROLL);
    recompui_set_padding_left(list, 10.0f, UNIT_DP);
    recompui_set_padding_right(list, 10.0f, UNIT_DP);
    recompui_set_padding_bottom(list, 10.0f, UNIT_DP);

    build_goal_category_picker(list);
}

static void race_ui_init(void)
{
    int i;

    if (s_ui_built)
        return;
    s_ui_built = 1;

    for (i = 0; i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        s_goal_btns[i] = RECOMPUI_NULL_RESOURCE;
        s_start_btns[i] = RECOMPUI_NULL_RESOURCE;
        s_start_selected[i] = is_default_start_flag(entry);
        s_goal_btn_text[i][0] = '\0';
        s_start_btn_text[i][0] = '\0';
    }
    for (i = 0; i < RACE_MAX_CATEGORIES; i++)
    {
        s_category_views[i] = RECOMPUI_NULL_RESOURCE;
        s_goal_category_views[i] = RECOMPUI_NULL_RESOURCE;
        s_category_count_lbls[i] = RECOMPUI_NULL_RESOURCE;
        s_goal_category_count_lbls[i] = RECOMPUI_NULL_RESOURCE;
        s_category_count_text[i][0] = '\0';
        s_goal_category_count_text[i][0] = '\0';
        s_category_names[i] = 0;
        s_category_start[i] = 0;
        s_category_end[i] = 0;
    }

    s_flag_count = anchor_flag_catalog_count();
    if (s_flag_count > RACE_MAX_FLAGS)
        s_flag_count = RACE_MAX_FLAGS;
    build_category_index();

    s_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_ctx, 1);
    recompui_set_context_captures_mouse(s_ctx, 1);

    recompui_open_context(s_ctx);
    {
        RecompuiResource root = recompui_context_root(s_ctx);

        RecompuiResource overlay = recompui_create_element(s_ctx, root);
        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &R_BG);

        RecompuiResource panel = recompui_create_element(s_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 5.0f, UNIT_PERCENT);
        recompui_set_top(panel, 4.0f, UNIT_PERCENT);
        recompui_set_width(panel, 90.0f, UNIT_PERCENT);
        recompui_set_height(panel, 92.0f, UNIT_PERCENT);
        recompui_set_background_color(panel, &R_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &R_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        RecompuiResource header = recompui_create_element(s_ctx, panel);
        recompui_set_display(header, DISPLAY_FLEX);
        recompui_set_flex_direction(header, FLEX_DIRECTION_ROW);
        recompui_set_align_items(header, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(header, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(header, 22.0f, UNIT_DP);
        recompui_set_padding_right(header, 12.0f, UNIT_DP);
        recompui_set_padding_top(header, 14.0f, UNIT_DP);
        recompui_set_padding_bottom(header, 14.0f, UNIT_DP);
        recompui_set_background_color(header, &R_HEAD);
        recompui_set_border_bottom_width(header, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(header, &R_BORDER);

        RecompuiResource title = recompui_create_label(s_ctx, header, "Race Setup", LABELSTYLE_NORMAL);
        recompui_set_color(title, &R_PURPLE);
        recompui_set_font_weight(title, 700);
        recompui_set_font_size(title, 26.0f, UNIT_DP);

        RecompuiResource back_btn = recompui_create_button(s_ctx, header, "Back", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(back_btn, CURSOR_POINTER);
        recompui_set_font_size(back_btn, 13.0f, UNIT_DP);
        recompui_set_height(back_btn, 34.0f, UNIT_DP);
        recompui_set_width(back_btn, 96.0f, UNIT_DP);
        recompui_set_padding_top(back_btn, 4.0f, UNIT_DP);
        recompui_set_padding_bottom(back_btn, 4.0f, UNIT_DP);
        recompui_set_tab_index(back_btn, TAB_INDEX_AUTO);
        recompui_register_callback(back_btn, on_back_clicked, 0);

        build_setup_view(panel);
        build_goal_view(panel);
        for (i = 0; i < s_category_count; i++)
        {
            build_category_detail_view(panel, i);
            build_goal_category_detail_view(panel, i);
        }

        RecompuiResource footer = recompui_create_element(s_ctx, panel);
        recompui_set_display(footer, DISPLAY_FLEX);
        recompui_set_flex_direction(footer, FLEX_DIRECTION_ROW);
        recompui_set_align_items(footer, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(footer, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_gap(footer, 12.0f, UNIT_DP);
        recompui_set_padding_left(footer, 16.0f, UNIT_DP);
        recompui_set_padding_right(footer, 12.0f, UNIT_DP);
        recompui_set_padding_top(footer, 10.0f, UNIT_DP);
        recompui_set_padding_bottom(footer, 10.0f, UNIT_DP);
        recompui_set_border_top_width(footer, 1.0f, UNIT_DP);
        recompui_set_border_top_color(footer, &R_BORDER);

        s_status_lbl = recompui_create_label(
            s_ctx, footer,
            "Configure the race, then start online.",
            LABELSTYLE_SMALL);
        recompui_set_color(s_status_lbl, &R_DIM);
        recompui_set_font_size(s_status_lbl, 14.0f, UNIT_DP);

        RecompuiResource start_btn = recompui_create_button(s_ctx, footer, "Start Race", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(start_btn, CURSOR_POINTER);
        recompui_set_font_size(start_btn, 14.0f, UNIT_DP);
        recompui_set_height(start_btn, 38.0f, UNIT_DP);
        recompui_set_width(start_btn, 150.0f, UNIT_DP);
        recompui_set_padding_top(start_btn, 5.0f, UNIT_DP);
        recompui_set_padding_bottom(start_btn, 5.0f, UNIT_DP);
        recompui_set_tab_index(start_btn, TAB_INDEX_AUTO);
        recompui_register_callback(start_btn, on_start_race_clicked, 0);
    }
    recompui_close_context(s_ctx);

    for (i = 0; i < s_category_count; i++)
    {
        update_category_count(i);
        update_goal_category_count(i);
    }
}

static void start_race(void)
{
    int i;

    if (s_goal_idx < 0 || !anchor_flag_catalog_get(s_goal_idx) ||
        !anchor_flag_catalog_get(s_goal_idx)->key)
    {
        set_status("Select a race end condition first.", 0);
        return;
    }

    s_race_pending_count = 0;
    for (i = 0; i < s_flag_count && i < RACE_MAX_FLAGS; i++)
    {
        const AnchorFlagEntry *entry = anchor_flag_catalog_get(i);
        if (!s_start_selected[i] || !entry || !entry->key)
            continue;

        s_race_pending_keys[s_race_pending_count] = entry->key;
        s_race_pending_values[s_race_pending_count] =
            entry->force_val != 0 ? entry->force_val : 1;
        s_race_pending_count++;
    }

    /*
     * TODO: Race protocol.
     * This is where the selected end condition and starting flags should be
     * published to Anchor once the race packet format exists. For now this
     * queues the configured starting flags and auto-loads the first file.
     * The flags are written when the game has initialized save memory.
     */
    s_race_apply_pending = 1;
    s_race_autoload_pending = 1;
    anchor_startup_menu_finish();
    if (s_visible)
    {
        recompui_hide_context(s_ctx);
        s_visible = 0;
    }
}

void anchor_startup_race_open(void)
{
    s_pending_open = 1;
}

RECOMP_CALLBACK("*", recomp_on_init)
void anchor_startup_race_on_init(void)
{
    race_ui_init();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_startup_race_frame_hook(void)
{
    if (!s_initialized)
    {
        s_initialized = 1;
        race_ui_init();
    }

    if (anchor_startup_menu_is_complete())
    {
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        apply_pending_race_flags();
        return;
    }

    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!s_visible)
        {
            s_active_category = -1;
            s_active_goal_category = -1;
            set_screen(0);
            update_goal_label();
            recompui_show_context(s_ctx);
            s_visible = 1;
        }
    }

    if (s_pending_choose_goal)
    {
        s_pending_choose_goal = 0;
        set_screen(1);
    }

    if (s_pending_goal_done)
    {
        s_pending_goal_done = 0;
        s_active_goal_category = -1;
        set_screen(0);
        update_goal_label();
    }

    if (s_pending_category_idx >= 0)
    {
        s_active_category = s_pending_category_idx;
        s_pending_category_idx = -1;
        set_screen(2);
    }

    if (s_pending_goal_category_idx >= 0)
    {
        s_active_goal_category = s_pending_goal_category_idx;
        s_pending_goal_category_idx = -1;
        set_screen(3);
    }

    if (s_pending_goal_idx >= 0)
    {
        int old_goal = s_goal_idx;
        int old_cat = category_for_flag(old_goal);
        int new_cat;
        s_goal_idx = s_pending_goal_idx;
        s_pending_goal_idx = -1;
        new_cat = category_for_flag(s_goal_idx);
        update_goal_buttons(old_goal, s_goal_idx);
        update_goal_category_count(old_cat);
        update_goal_category_count(new_cat);
        set_status("Race end condition selected.", 1);
    }

    if (s_pending_toggle_idx >= 0)
    {
        int idx = s_pending_toggle_idx;
        s_pending_toggle_idx = -1;
        if (idx >= 0 && idx < s_flag_count && idx < RACE_MAX_FLAGS)
        {
            s_start_selected[idx] = !s_start_selected[idx];
            update_start_button(idx);
            set_status(s_start_selected[idx] ? "Starting flag added." : "Starting flag removed.", 1);
        }
    }

    if (s_pending_back)
    {
        s_pending_back = 0;
        if (s_screen == 3)
        {
            s_active_goal_category = -1;
            set_screen(1);
        }
        else if (s_screen == 1 || s_screen == 2)
        {
            s_active_category = -1;
            s_active_goal_category = -1;
            set_screen(0);
            update_goal_label();
        }
        else
        {
            if (s_visible)
            {
                recompui_hide_context(s_ctx);
                s_visible = 0;
            }
            anchor_disconnect();
            anchor_startup_menu_open();
        }
    }

    if (s_pending_start)
    {
        s_pending_start = 0;
        start_race();
    }
}

RECOMP_HOOK_RETURN("func_8000B640_C240")
void anchor_race_save_start_hook(void)
{
    apply_pending_race_flags();
}

RECOMP_HOOK_RETURN("func_8000B5D0_C1D0")
void anchor_race_file_started_hook(void)
{
    apply_pending_race_flags();
}

RECOMP_HOOK("func_801CE1F0_6610A0")
void anchor_race_file_select_autoload(void *entity, int param2)
{
    (void)entity;
    (void)param2;

    if (!s_race_autoload_pending)
        return;

    s_race_autoload_pending = 0;
    func_8000B640_C240();
    func_8000B5D0_C1D0();
    if (D_8015C5C8_15D1C8)
        *(int *)(D_8015C5C8_15D1C8 + 0x3B040) = -1;
    func_8003521C_35E1C(func_801CD890_660740);
    apply_pending_race_flags();
    recomp_printf("[Race] Auto-loaded race file from file select.\n");
}
