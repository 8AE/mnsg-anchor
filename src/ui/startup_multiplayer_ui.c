#include "modding.h"
#include "recompconfig.h"
#include "recomputils.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"

static const RecompuiColor MP_BG = {8, 8, 12, 232};
static const RecompuiColor MP_PANEL = {14, 14, 22, 255};
static const RecompuiColor MP_HEAD = {20, 20, 30, 255};
static const RecompuiColor MP_BORDER = {60, 60, 82, 225};
static const RecompuiColor MP_WHITE = {248, 248, 252, 255};
static const RecompuiColor MP_DIM = {166, 168, 178, 255};
static const RecompuiColor MP_TEAL = {0, 190, 165, 255};
static const RecompuiColor MP_GREEN = {70, 190, 90, 255};
static const RecompuiColor MP_RED = {220, 68, 68, 255};
static const RecompuiColor MP_STATUS = {9, 9, 16, 255};

static RecompuiContext s_ctx = RECOMPUI_NULL_CONTEXT;
static int s_ui_built = 0;
static int s_initialized = 0;
static int s_visible = 0;

static RecompuiResource s_in_host = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_port = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_room = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_name = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_in_team = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_title_lbl = RECOMPUI_NULL_RESOURCE;
static RecompuiResource s_status_lbl = RECOMPUI_NULL_RESOURCE;

static void set_status_text(const char *text, int ok);
static void try_connect_from_ui(void);
static int s_pending_open = 0;
static int s_pending_back = 0;
static int s_pending_connect = 0;
static int s_launches_race = 0;

static void int_to_str(int value, char *out)
{
    char tmp[12];
    int len = 0;
    if (value == 0)
    {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    if (value < 0)
    {
        *out++ = '-';
        value = -value;
    }
    while (value)
    {
        tmp[len++] = '0' + (value % 10);
        value /= 10;
    }
    for (int i = len - 1; i >= 0; i--)
        *out++ = tmp[i];
    *out = '\0';
}

static int str_to_int(const char *s, int fallback)
{
    if (!s || !*s)
        return fallback;
    int val = 0;
    int neg = 0;
    if (*s == '-')
    {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

static void make_field(RecompuiContext ctx, RecompuiResource parent,
                       const char *label_text, const char *default_text,
                       RecompuiResource *out)
{
    RecompuiResource lbl = recompui_create_label(ctx, parent, label_text, LABELSTYLE_SMALL);
    recompui_set_color(lbl, &MP_DIM);
    recompui_set_font_size(lbl, 16.0f, UNIT_DP);

    *out = recompui_create_textinput(ctx, parent);
    recompui_set_font_size(*out, 18.0f, UNIT_DP);
    recompui_set_max_width(*out, 100.0f, UNIT_PERCENT);
    recompui_set_input_text(*out, default_text);
}

static void on_back_clicked(RecompuiResource res,
                            const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_back = 1;
}

static void on_connect_clicked(RecompuiResource res,
                               const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_connect = 1;
}

static void set_status_text(const char *text, int ok)
{
    if (s_status_lbl == RECOMPUI_NULL_RESOURCE)
        return;

    recompui_open_context(s_ctx);
    recompui_set_text(s_status_lbl, text);
    recompui_set_color(s_status_lbl, ok ? &MP_GREEN : &MP_RED);
    recompui_close_context(s_ctx);
}

static void try_connect_from_ui(void)
{
    recompui_open_context(s_ctx);
    char *host_str = recompui_get_input_text(s_in_host);
    char *port_str = recompui_get_input_text(s_in_port);
    char *room_str = recompui_get_input_text(s_in_room);
    char *name_str = recompui_get_input_text(s_in_name);
    char *team_str = recompui_get_input_text(s_in_team);
    recompui_close_context(s_ctx);

    int port = str_to_int(port_str, 43383);
    if (port <= 0 || port > 65535)
        port = 43383;

    if (anchor_is_connected())
        anchor_disconnect();

    int ok = anchor_connect(
        (host_str && host_str[0]) ? host_str : "anchor.hm64.org",
        port,
        (room_str && room_str[0]) ? room_str : "mnsg-recomp",
        (name_str && name_str[0]) ? name_str : "Player",
        0,
        (team_str && team_str[0]) ? team_str : "default");

    if (ok)
    {
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        if (s_launches_race)
            anchor_race_lobby_open();
        else
            anchor_startup_menu_finish();
    }
    else
    {
        set_status_text("Connection failed. Check address and port.", 0);
    }

    if (host_str)
        recomp_free(host_str);
    if (port_str)
        recomp_free(port_str);
    if (room_str)
        recomp_free(room_str);
    if (name_str)
        recomp_free(name_str);
    if (team_str)
        recomp_free(team_str);
}

static void multiplayer_ui_init(void)
{
    if (s_ui_built)
        return;
    s_ui_built = 1;

    char *cfg_host = recomp_get_config_string("anchor_host");
    char *cfg_room = recomp_get_config_string("anchor_room_id");
    char *cfg_name = recomp_get_config_string("anchor_player_name");
    char *cfg_team = recomp_get_config_string("anchor_team_id");

    int cfg_port = (int)recomp_get_config_double("anchor_port");
    if (cfg_port <= 0)
        cfg_port = 43383;

    static char port_str[8];
    int_to_str(cfg_port, port_str);
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
        recompui_set_background_color(overlay, &MP_BG);

        RecompuiResource panel = recompui_create_element(s_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 50.0f, UNIT_PERCENT);
        recompui_set_top(panel, 8.0f, UNIT_PERCENT);
        recompui_set_margin_left(panel, -300.0f, UNIT_DP);
        recompui_set_width(panel, 600.0f, UNIT_DP);
        recompui_set_background_color(panel, &MP_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &MP_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        RecompuiResource header = recompui_create_element(s_ctx, panel);
        recompui_set_display(header, DISPLAY_FLEX);
        recompui_set_flex_direction(header, FLEX_DIRECTION_ROW);
        recompui_set_align_items(header, ALIGN_ITEMS_CENTER);
        recompui_set_justify_content(header, JUSTIFY_CONTENT_SPACE_BETWEEN);
        recompui_set_padding_left(header, 22.0f, UNIT_DP);
        recompui_set_padding_right(header, 14.0f, UNIT_DP);
        recompui_set_padding_top(header, 16.0f, UNIT_DP);
        recompui_set_padding_bottom(header, 16.0f, UNIT_DP);
        recompui_set_background_color(header, &MP_HEAD);
        recompui_set_border_bottom_width(header, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(header, &MP_BORDER);

        s_title_lbl = recompui_create_label(
            s_ctx, header, "Multiplayer", LABELSTYLE_NORMAL);
        recompui_set_color(s_title_lbl, &MP_TEAL);
        recompui_set_font_weight(s_title_lbl, 700);
        recompui_set_font_size(s_title_lbl, 24.0f, UNIT_DP);

        RecompuiResource back_btn = recompui_create_button(
            s_ctx, header, "Back", BUTTONSTYLE_SECONDARY);
        recompui_set_cursor(back_btn, CURSOR_POINTER);
        recompui_set_font_size(back_btn, 15.0f, UNIT_DP);
        recompui_set_tab_index(back_btn, TAB_INDEX_AUTO);
        recompui_register_callback(back_btn, on_back_clicked, 0);

        RecompuiResource body = recompui_create_element(s_ctx, panel);
        recompui_set_display(body, DISPLAY_FLEX);
        recompui_set_flex_direction(body, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(body, 14.0f, UNIT_DP);
        recompui_set_padding(body, 22.0f, UNIT_DP);

        RecompuiResource addr_row = recompui_create_element(s_ctx, body);
        recompui_set_display(addr_row, DISPLAY_FLEX);
        recompui_set_flex_direction(addr_row, FLEX_DIRECTION_ROW);
        recompui_set_gap(addr_row, 12.0f, UNIT_DP);
        recompui_set_align_items(addr_row, ALIGN_ITEMS_FLEX_END);

        RecompuiResource host_col = recompui_create_element(s_ctx, addr_row);
        recompui_set_display(host_col, DISPLAY_FLEX);
        recompui_set_flex_direction(host_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(host_col, 1.0f);
        recompui_set_gap(host_col, 5.0f, UNIT_DP);
        make_field(s_ctx, host_col, "Server Address",
                   (cfg_host && cfg_host[0]) ? cfg_host : "localhost", &s_in_host);

        RecompuiResource port_col = recompui_create_element(s_ctx, addr_row);
        recompui_set_display(port_col, DISPLAY_FLEX);
        recompui_set_flex_direction(port_col, FLEX_DIRECTION_COLUMN);
        recompui_set_width(port_col, 110.0f, UNIT_DP);
        recompui_set_gap(port_col, 5.0f, UNIT_DP);
        make_field(s_ctx, port_col, "Port", port_str, &s_in_port);

        RecompuiResource room_col = recompui_create_element(s_ctx, body);
        recompui_set_display(room_col, DISPLAY_FLEX);
        recompui_set_flex_direction(room_col, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(room_col, 5.0f, UNIT_DP);
        make_field(s_ctx, room_col, "Room ID",
                   (cfg_room && cfg_room[0]) ? cfg_room : "mnsg-recomp", &s_in_room);

        RecompuiResource nt_row = recompui_create_element(s_ctx, body);
        recompui_set_display(nt_row, DISPLAY_FLEX);
        recompui_set_flex_direction(nt_row, FLEX_DIRECTION_ROW);
        recompui_set_gap(nt_row, 12.0f, UNIT_DP);
        recompui_set_align_items(nt_row, ALIGN_ITEMS_FLEX_END);

        RecompuiResource name_col = recompui_create_element(s_ctx, nt_row);
        recompui_set_display(name_col, DISPLAY_FLEX);
        recompui_set_flex_direction(name_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(name_col, 1.0f);
        recompui_set_gap(name_col, 5.0f, UNIT_DP);
        make_field(s_ctx, name_col, "Player Name",
                   (cfg_name && cfg_name[0]) ? cfg_name : "Player", &s_in_name);

        RecompuiResource team_col = recompui_create_element(s_ctx, nt_row);
        recompui_set_display(team_col, DISPLAY_FLEX);
        recompui_set_flex_direction(team_col, FLEX_DIRECTION_COLUMN);
        recompui_set_flex_grow(team_col, 1.0f);
        recompui_set_gap(team_col, 5.0f, UNIT_DP);
        make_field(s_ctx, team_col, "Team ID",
                   (cfg_team && cfg_team[0]) ? cfg_team : "default", &s_in_team);

        RecompuiResource status_row = recompui_create_element(s_ctx, body);
        recompui_set_padding(status_row, 12.0f, UNIT_DP);
        recompui_set_min_height(status_row, 44.0f, UNIT_DP);
        recompui_set_background_color(status_row, &MP_STATUS);
        recompui_set_border_radius(status_row, 5.0f, UNIT_DP);

        s_status_lbl = recompui_create_label(
            s_ctx, status_row, "Ready to connect.", LABELSTYLE_SMALL);
        recompui_set_color(s_status_lbl, &MP_DIM);
        recompui_set_font_size(s_status_lbl, 15.0f, UNIT_DP);

        RecompuiResource button_row = recompui_create_element(s_ctx, body);
        recompui_set_display(button_row, DISPLAY_FLEX);
        recompui_set_flex_direction(button_row, FLEX_DIRECTION_ROW);
        recompui_set_justify_content(button_row, JUSTIFY_CONTENT_FLEX_END);

        RecompuiResource connect_btn = recompui_create_button(
            s_ctx, button_row, "Connect", BUTTONSTYLE_PRIMARY);
        recompui_set_cursor(connect_btn, CURSOR_POINTER);
        recompui_set_font_size(connect_btn, 17.0f, UNIT_DP);
        recompui_set_tab_index(connect_btn, TAB_INDEX_AUTO);
        recompui_register_callback(connect_btn, on_connect_clicked, 0);
    }
    recompui_close_context(s_ctx);

    if (cfg_host)
        recomp_free_config_string(cfg_host);
    if (cfg_room)
        recomp_free_config_string(cfg_room);
    if (cfg_name)
        recomp_free_config_string(cfg_name);
    if (cfg_team)
        recomp_free_config_string(cfg_team);
}

void anchor_startup_multiplayer_open(void)
{
    s_launches_race = 0;
    s_pending_open = 1;
}

void anchor_startup_multiplayer_open_for_race(void)
{
    s_launches_race = 1;
    s_pending_open = 1;
}

RECOMP_CALLBACK("*", recomp_on_init)
void anchor_startup_multiplayer_on_init(void)
{
    anchor_runtime_init_defaults();
    multiplayer_ui_init();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_startup_multiplayer_frame_hook(void)
{
    if (!s_initialized)
    {
        s_initialized = 1;
        multiplayer_ui_init();
    }

    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!anchor_startup_menu_is_complete() && !s_visible)
        {
            recompui_open_context(s_ctx);
            recompui_set_text(s_title_lbl,
                              s_launches_race ? "Race: Connect Online" : "Multiplayer");
            recompui_set_text(s_status_lbl,
                              s_launches_race ? "Connect before configuring the race." : "Ready to connect.");
            recompui_set_color(s_status_lbl, &MP_DIM);
            recompui_close_context(s_ctx);
            recompui_show_context(s_ctx);
            s_visible = 1;
        }
    }

    if (s_pending_back)
    {
        s_pending_back = 0;
        if (s_visible)
        {
            recompui_hide_context(s_ctx);
            s_visible = 0;
        }
        anchor_startup_menu_open();
    }

    if (s_pending_connect)
    {
        s_pending_connect = 0;
        try_connect_from_ui();
    }
}
