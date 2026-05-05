#include "modding.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"

#define MAX_LOBBY_ROWS 32
#define LOBBY_TEXT_LEN 96
#define JOIN_CHECK_FRAMES 60
#define REFRESH_FRAMES 20

static const RecompuiColor L_BG = {8, 8, 12, 232};
static const RecompuiColor L_PANEL = {14, 14, 22, 255};
static const RecompuiColor L_HEAD = {20, 17, 27, 255};
static const RecompuiColor L_BORDER = {72, 62, 96, 225};
static const RecompuiColor L_WHITE = {248, 248, 252, 255};
static const RecompuiColor L_DIM = {170, 172, 182, 255};
static const RecompuiColor L_PURPLE = {168, 104, 255, 255};
static const RecompuiColor L_TEAL = {0, 190, 165, 255};
static const RecompuiColor L_RED = {220, 68, 68, 255};
static const RecompuiColor L_ROW = {18, 18, 27, 255};

static RecompuiContext s_ctx = RECOMPUI_NULL_CONTEXT;
static RecompuiResource s_title_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_status_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_configure_btn = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_start_btn = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_rows[MAX_LOBBY_ROWS];
static char s_row_text[MAX_LOBBY_ROWS][LOBBY_TEXT_LEN];

static int s_ui_built = 0;
static int s_initialized = 0;
static int s_visible = 0;
static int s_pending_open = 0;
static int s_pending_back = 0;
static int s_pending_configure = 0;
static int s_pending_start = 0;
static int s_join_check_frames = 0;
static int s_refresh_timer = 0;
static int s_join_accepted = 0;

static int text_equal(const char *a, const char *b)
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

static const char *find_text(const char *text, const char *needle)
{
    int i;
    int j;
    if (!text || !needle || !needle[0])
        return 0;
    for (i = 0; text[i]; i++)
    {
        for (j = 0; needle[j] && text[i + j] == needle[j]; j++)
            ;
        if (!needle[j])
            return &text[i];
    }
    return 0;
}

static int parse_int_after(const char *text, const char *key, int fallback)
{
    const char *p = find_text(text, key);
    int value = 0;
    int found = 0;
    if (!p)
        return fallback;
    while (*p && *p != ':')
        p++;
    if (*p == ':')
        p++;
    while (*p >= '0' && *p <= '9')
    {
        value = value * 10 + (*p - '0');
        found = 1;
        p++;
    }
    return found ? value : fallback;
}

static void parse_string_after(const char *text, const char *key, char *out, int out_len)
{
    const char *p = find_text(text, key);
    int i = 0;
    if (!out || out_len <= 0)
        return;
    out[0] = '\0';
    if (!p)
        return;
    while (*p && *p != ':')
        p++;
    if (*p == ':')
        p++;
    while (*p && *p != '"')
        p++;
    if (*p == '"')
        p++;
    while (*p && *p != '"' && i < out_len - 1)
        out[i++] = *p++;
    out[i] = '\0';
}

static void copy_text(char *dst, const char *src, int max_len)
{
    int i = 0;
    if (!dst || max_len <= 0)
        return;
    if (!src)
        src = "";
    while (src[i] && i < max_len - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void append_text(char *dst, const char *src, int max_len)
{
    int i = 0;
    int j = 0;
    while (dst[i] && i < max_len - 1)
        i++;
    if (!src)
        return;
    while (src[j] && i < max_len - 1)
        dst[i++] = src[j++];
    dst[i] = '\0';
}

static void set_status(const char *text, const RecompuiColor *color)
{
    if (s_status_lbl == RECOMPUI_NULL_RESOURCE)
        return;
    recompui_open_context(s_ctx);
    recompui_set_text(s_status_lbl, text);
    recompui_set_color(s_status_lbl, color);
    recompui_close_context(s_ctx);
}

static int local_is_host(void)
{
    unsigned int self_id = anchor_get_client_id();
    unsigned int host_id = anchor_get_race_host_id();
    return self_id != 0 && host_id != 0 && self_id == host_id;
}

static void on_back_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_back = 1;
}

static void on_configure_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_configure = 1;
}

static void on_start_clicked(RecompuiResource res, const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_start = 1;
}

static void update_buttons(void)
{
    int host = local_is_host();
    recompui_open_context(s_ctx);
    if (s_configure_btn != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_configure_btn, host ? "Configure Race" : "Configure Race (Host Only)");
    if (s_start_btn != RECOMPUI_NULL_RESOURCE)
        recompui_set_text(s_start_btn, host ? "Start Race" : "Start Race (Host Only)");
    recompui_close_context(s_ctx);
}

static void refresh_lobby_rows(void)
{
    char *json;
    const char *p;
    char last_team[40];
    int row = 0;

    if (!anchor_is_connected())
        return;

    json = anchor_get_race_lobby_json();
    p = json;
    last_team[0] = '\0';

    recompui_open_context(s_ctx);
    while (p && *p && row < MAX_LOBBY_ROWS)
    {
        char name[40];
        char team[40];
        char status[20];
        const char *obj_end;
        int cid;

        p = find_text(p, "{");
        if (!p)
            break;
        obj_end = find_text(p, "}");
        if (!obj_end)
            break;

        cid = parse_int_after(p, "\"cid\"", 0);
        parse_string_after(p, "\"n\"", name, (int)sizeof(name));
        parse_string_after(p, "\"t\"", team, (int)sizeof(team));
        parse_string_after(p, "\"s\"", status, (int)sizeof(status));
        if (!team[0])
            copy_text(team, "default", (int)sizeof(team));

        if (!text_equal(team, last_team) && row < MAX_LOBBY_ROWS)
        {
            copy_text(s_row_text[row], "Team: ", LOBBY_TEXT_LEN);
            append_text(s_row_text[row], team, LOBBY_TEXT_LEN);
            recompui_set_text(s_rows[row], s_row_text[row]);
            recompui_set_color(s_rows[row], &L_TEAL);
            recompui_set_display(s_rows[row], DISPLAY_BLOCK);
            copy_text(last_team, team, (int)sizeof(last_team));
            row++;
        }

        if (row < MAX_LOBBY_ROWS)
        {
            copy_text(s_row_text[row], "  ", LOBBY_TEXT_LEN);
            append_text(s_row_text[row], name[0] ? name : "Player", LOBBY_TEXT_LEN);
            if (cid == (int)anchor_get_race_host_id())
                append_text(s_row_text[row], "  [Host]", LOBBY_TEXT_LEN);
            if (text_equal(status, "started"))
                append_text(s_row_text[row], "  [Started]", LOBBY_TEXT_LEN);
            recompui_set_text(s_rows[row], s_row_text[row]);
            recompui_set_color(s_rows[row], &L_WHITE);
            recompui_set_display(s_rows[row], DISPLAY_BLOCK);
            row++;
        }
        p = obj_end + 1;
    }

    while (row < MAX_LOBBY_ROWS)
    {
        recompui_set_display(s_rows[row], DISPLAY_NONE);
        row++;
    }
    recompui_close_context(s_ctx);

    if (json)
        recomp_free(json);
    update_buttons();
}

static void race_lobby_ui_init(void)
{
    int i;

    if (s_ui_built)
        return;
    s_ui_built = 1;

    s_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_ctx, 1);
    recompui_set_context_captures_mouse(s_ctx, 1);

    recompui_open_context(s_ctx);
    {
        RecompuiResource root = recompui_context_root(s_ctx);
        RecompuiResource overlay = recompui_create_element(s_ctx, root);
        RecompuiResource panel;
        RecompuiResource header;
        RecompuiResource body;
        RecompuiResource list;
        RecompuiResource actions;

        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &L_BG);

        panel = recompui_create_element(s_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 50.0f, UNIT_PERCENT);
        recompui_set_top(panel, 8.0f, UNIT_PERCENT);
        recompui_set_margin_left(panel, -340.0f, UNIT_DP);
        recompui_set_width(panel, 680.0f, UNIT_DP);
        recompui_set_background_color(panel, &L_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &L_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        header = recompui_create_element(s_ctx, panel);
        recompui_set_display(header, DISPLAY_FLEX);
        recompui_set_flex_direction(header, FLEX_DIRECTION_ROW);
        recompui_set_align_items(header, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(header, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(header, 22.0f, UNIT_DP);
        recompui_set_padding_right(header, 14.0f, UNIT_DP);
        recompui_set_padding_top(header, 16.0f, UNIT_DP);
        recompui_set_padding_bottom(header, 16.0f, UNIT_DP);
        recompui_set_background_color(header, &L_HEAD);
        recompui_set_border_bottom_width(header, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(header, &L_BORDER);

        s_title_lbl = recompui_create_label(s_ctx, header, "Race Lobby", LABELSTYLE_NORMAL);
        recompui_set_color(s_title_lbl, &L_PURPLE);
        recompui_set_font_weight(s_title_lbl, 700);
        recompui_set_font_size(s_title_lbl, 25.0f, UNIT_DP);

        body = recompui_create_element(s_ctx, panel);
        recompui_set_display(body, DISPLAY_FLEX);
        recompui_set_flex_direction(body, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(body, 14.0f, UNIT_DP);
        recompui_set_padding(body, 20.0f, UNIT_DP);

        s_status_lbl = recompui_create_label(s_ctx, body, "Checking race state...", LABELSTYLE_SMALL);
        recompui_set_color(s_status_lbl, &L_DIM);
        recompui_set_font_size(s_status_lbl, 15.0f, UNIT_DP);

        list = recompui_create_element(s_ctx, body);
        recompui_set_display(list, DISPLAY_FLEX);
        recompui_set_flex_direction(list, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(list, 4.0f, UNIT_DP);
        recompui_set_padding(list, 10.0f, UNIT_DP);
        recompui_set_background_color(list, &L_ROW);
        recompui_set_border_radius(list, 5.0f, UNIT_DP);
        recompui_set_min_height(list, 280.0f, UNIT_DP);

        for (i = 0; i < MAX_LOBBY_ROWS; i++)
        {
            s_rows[i] = recompui_create_label(s_ctx, list, "", LABELSTYLE_SMALL);
            recompui_set_font_size(s_rows[i], 14.0f, UNIT_DP);
            recompui_set_color(s_rows[i], &L_WHITE);
            recompui_set_display(s_rows[i], DISPLAY_NONE);
        }

        actions = recompui_create_element(s_ctx, body);
        recompui_set_display(actions, DISPLAY_FLEX);
        recompui_set_flex_direction(actions, FLEX_DIRECTION_ROW);
        recompui_set_justify_content(actions, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_gap(actions, 10.0f, UNIT_DP);

        s_configure_btn = recompui_create_button(s_ctx, actions, "Configure Race", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(s_configure_btn, CURSOR_POINTER);
        recompui_set_font_size(s_configure_btn, 14.0f, UNIT_DP);
        recompui_set_tab_index(s_configure_btn, TAB_INDEX_AUTO);
        recompui_register_callback(s_configure_btn, on_configure_clicked, 0);

        s_start_btn = recompui_create_button(s_ctx, actions, "Start Race", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(s_start_btn, CURSOR_POINTER);
        recompui_set_font_size(s_start_btn, 14.0f, UNIT_DP);
        recompui_set_tab_index(s_start_btn, TAB_INDEX_AUTO);
        recompui_register_callback(s_start_btn, on_start_clicked, 0);

        {
            RecompuiResource back_btn = recompui_create_button(s_ctx, actions, "Back", BUTTONSTYLE_SECONDARY);
            recompui_set_cursor(back_btn, CURSOR_POINTER);
            recompui_set_font_size(back_btn, 14.0f, UNIT_DP);
            recompui_set_tab_index(back_btn, TAB_INDEX_AUTO);
            recompui_register_callback(back_btn, on_back_clicked, 0);
        }
    }
    recompui_close_context(s_ctx);
}

void anchor_race_lobby_open(void)
{
    s_pending_open = 1;
}

RECOMP_CALLBACK("*", recomp_on_init)
void anchor_race_lobby_on_init(void)
{
    race_lobby_ui_init();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_race_lobby_frame_hook(void)
{
    if (!s_initialized)
    {
        s_initialized = 1;
        race_lobby_ui_init();
    }

    if (anchor_startup_menu_is_complete())
    {
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        return;
    }

    if (s_pending_open)
    {
        s_pending_open = 0;
        s_join_check_frames = JOIN_CHECK_FRAMES;
        s_join_accepted = 0;
        s_refresh_timer = 0;
        set_status("Checking race state...", &L_DIM);
        if (!s_visible)
        {
            recompui_show_context(s_ctx);
            s_visible = 1;
        }
    }

    if (!s_visible)
        return;

    if (!anchor_is_connected())
    {
        set_status("Disconnected.", &L_RED);
        return;
    }

    if (!s_join_accepted)
    {
        if (anchor_race_has_started())
        {
            anchor_disconnect();
            set_status("Race already started. Join a pending lobby instead.", &L_RED);
            return;
        }
        if (s_join_check_frames > 0)
        {
            s_join_check_frames--;
            refresh_lobby_rows();
            return;
        }
        s_join_accepted = 1;
        anchor_set_race_lobby_state("lobby", anchor_race_get_config_json());
        set_status(local_is_host() ? "You are host. Configure or start the race." : "Waiting for host.", &L_DIM);
    }

    if (anchor_race_has_started())
    {
        char *config = anchor_get_host_race_config_json();
        if (config && config[0])
            anchor_race_apply_config_json(config);
        if (config)
            recomp_free(config);
        anchor_race_start_from_lobby();
        return;
    }

    if (s_refresh_timer > 0)
        s_refresh_timer--;
    else
    {
        s_refresh_timer = REFRESH_FRAMES;
        refresh_lobby_rows();
        set_status(local_is_host() ? "You are host. Configure or start the race." : "Waiting for host.", &L_DIM);
    }

    if (s_pending_configure)
    {
        s_pending_configure = 0;
        if (!local_is_host())
        {
            set_status("Only the host can configure the race.", &L_RED);
        }
        else
        {
            if (s_visible)
            {
                recompui_hide_context(s_ctx);
                s_visible = 0;
            }
            anchor_startup_race_open();
        }
    }

    if (s_pending_start)
    {
        s_pending_start = 0;
        if (!local_is_host())
        {
            set_status("Only the host can start the race.", &L_RED);
        }
        else if (anchor_race_start_from_lobby())
        {
            anchor_set_race_lobby_state("started", anchor_race_get_config_json());
        }
    }

    if (s_pending_back)
    {
        s_pending_back = 0;
        anchor_disconnect();
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        anchor_startup_menu_open();
    }
}
