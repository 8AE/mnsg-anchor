#include "modding.h"
#include "recompui.h"
#include "anchor.h"
#include "anchor_runtime.h"

static const RecompuiColor SM_BG = {8, 8, 12, 246};
static const RecompuiColor SM_PANEL = {15, 15, 22, 255};
static const RecompuiColor SM_PANEL_ALT = {21, 21, 31, 255};
static const RecompuiColor SM_BORDER = {62, 62, 82, 225};
static const RecompuiColor SM_BLUE = {42, 148, 255, 255};
static const RecompuiColor SM_TEAL = {0, 190, 165, 255};
static const RecompuiColor SM_WHITE = {245, 245, 250, 255};
static const RecompuiColor SM_DIM = {170, 172, 182, 255};

static RecompuiContext s_menu_ctx = RECOMPUI_NULL_CONTEXT;
static int s_ui_built = 0;
static int s_initialized = 0;
static int s_visible = 0;
static int s_pending_open = 0;
static int s_pending_single = 0;
static int s_pending_multiplayer = 0;

static void on_single_clicked(RecompuiResource res,
                              const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_single = 1;
}

static void on_multiplayer_clicked(RecompuiResource res,
                                   const RecompuiEventData *ev, void *ud)
{
    (void)res;
    (void)ud;
    if (ev->type == UI_EVENT_CLICK)
        s_pending_multiplayer = 1;
}

static RecompuiResource make_mode_button(RecompuiContext ctx, RecompuiResource parent,
                                         const char *title_text, const char *desc_text,
                                         const RecompuiColor *accent,
                                         RecompuiEventHandler *cb)
{
    RecompuiResource card = recompui_create_element(ctx, parent);
    recompui_set_display(card, DISPLAY_FLEX);
    recompui_set_flex_direction(card, FLEX_DIRECTION_COLUMN);
    recompui_set_gap(card, 10.0f, UNIT_DP);
    recompui_set_padding(card, 18.0f, UNIT_DP);
    recompui_set_background_color(card, &SM_PANEL_ALT);
    recompui_set_border_radius(card, 7.0f, UNIT_DP);
    recompui_set_border_width(card, 1.0f, UNIT_DP);
    recompui_set_border_color(card, &SM_BORDER);
    recompui_set_flex_grow(card, 1.0f);

    RecompuiResource title = recompui_create_label(ctx, card, title_text, LABELSTYLE_NORMAL);
    recompui_set_color(title, accent);
    recompui_set_font_weight(title, 700);
    recompui_set_font_size(title, 24.0f, UNIT_DP);

    RecompuiResource desc = recompui_create_label(ctx, card, desc_text, LABELSTYLE_SMALL);
    recompui_set_color(desc, &SM_DIM);
    recompui_set_font_size(desc, 15.0f, UNIT_DP);

    RecompuiResource button = recompui_create_button(ctx, card, "Select", BUTTONSTYLE_PRIMARY);
    recompui_set_cursor(button, CURSOR_POINTER);
    recompui_set_font_size(button, 17.0f, UNIT_DP);
    recompui_set_margin_top(button, 8.0f, UNIT_DP);
    recompui_set_tab_index(button, TAB_INDEX_AUTO);
    recompui_register_callback(button, cb, 0);

    return card;
}

static void startup_menu_init(void)
{
    if (s_ui_built)
        return;
    s_ui_built = 1;

    s_menu_ctx = recompui_create_context();
    recompui_set_context_captures_input(s_menu_ctx, 1);
    recompui_set_context_captures_mouse(s_menu_ctx, 1);

    recompui_open_context(s_menu_ctx);
    {
        RecompuiResource root = recompui_context_root(s_menu_ctx);

        RecompuiResource overlay = recompui_create_element(s_menu_ctx, root);
        recompui_set_position(overlay, POSITION_ABSOLUTE);
        recompui_set_left(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_top(overlay, 0.0f, UNIT_PERCENT);
        recompui_set_width(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_height(overlay, 100.0f, UNIT_PERCENT);
        recompui_set_background_color(overlay, &SM_BG);

        RecompuiResource panel = recompui_create_element(s_menu_ctx, root);
        recompui_set_position(panel, POSITION_ABSOLUTE);
        recompui_set_left(panel, 50.0f, UNIT_PERCENT);
        recompui_set_top(panel, 13.0f, UNIT_PERCENT);
        recompui_set_margin_left(panel, -360.0f, UNIT_DP);
        recompui_set_width(panel, 720.0f, UNIT_DP);
        recompui_set_background_color(panel, &SM_PANEL);
        recompui_set_border_radius(panel, 8.0f, UNIT_DP);
        recompui_set_border_width(panel, 1.5f, UNIT_DP);
        recompui_set_border_color(panel, &SM_BORDER);
        recompui_set_display(panel, DISPLAY_FLEX);
        recompui_set_flex_direction(panel, FLEX_DIRECTION_COLUMN);

        RecompuiResource accent = recompui_create_element(s_menu_ctx, panel);
        recompui_set_width(accent, 100.0f, UNIT_PERCENT);
        recompui_set_height(accent, 5.0f, UNIT_DP);
        recompui_set_background_color(accent, &SM_BLUE);
        recompui_set_border_top_left_radius(accent, 8.0f, UNIT_DP);
        recompui_set_border_top_right_radius(accent, 8.0f, UNIT_DP);

        RecompuiResource header = recompui_create_element(s_menu_ctx, panel);
        recompui_set_display(header, DISPLAY_FLEX);
        recompui_set_flex_direction(header, FLEX_DIRECTION_COLUMN);
        recompui_set_gap(header, 8.0f, UNIT_DP);
        recompui_set_padding_left(header, 24.0f, UNIT_DP);
        recompui_set_padding_right(header, 24.0f, UNIT_DP);
        recompui_set_padding_top(header, 22.0f, UNIT_DP);
        recompui_set_padding_bottom(header, 18.0f, UNIT_DP);
        recompui_set_border_bottom_width(header, 1.0f, UNIT_DP);
        recompui_set_border_bottom_color(header, &SM_BORDER);

        RecompuiResource title = recompui_create_label(
            s_menu_ctx, header, "Mystical Ninjas Starring Goemon: Anchor Multiplayer", LABELSTYLE_NORMAL);
        recompui_set_color(title, &SM_WHITE);
        recompui_set_font_weight(title, 800);
        recompui_set_font_size(title, 30.0f, UNIT_DP);

        RecompuiResource subtitle = recompui_create_label(
            s_menu_ctx, header, "Choose how to start this session.", LABELSTYLE_SMALL);
        recompui_set_color(subtitle, &SM_DIM);
        recompui_set_font_size(subtitle, 16.0f, UNIT_DP);

        RecompuiResource body = recompui_create_element(s_menu_ctx, panel);
        recompui_set_display(body, DISPLAY_FLEX);
        recompui_set_flex_direction(body, FLEX_DIRECTION_ROW);
        recompui_set_gap(body, 16.0f, UNIT_DP);
        recompui_set_padding(body, 24.0f, UNIT_DP);

        make_mode_button(s_menu_ctx, body, "Single Player",
                         "Launch the game offline.", &SM_BLUE, on_single_clicked);
        make_mode_button(s_menu_ctx, body, "Multiplayer",
                         "Connect through Anchor before starting.", &SM_TEAL,
                         on_multiplayer_clicked);
    }
    recompui_close_context(s_menu_ctx);
}

void anchor_startup_menu_open(void)
{
    s_pending_open = 1;
}

RECOMP_CALLBACK("*", recomp_on_init)
void anchor_startup_menu_on_init(void)
{
    anchor_runtime_init_defaults();
    startup_menu_init();
    anchor_startup_menu_open();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_startup_menu_frame_hook(void)
{
    if (!s_initialized)
    {
        s_initialized = 1;
        anchor_runtime_init_defaults();
        startup_menu_init();
        s_pending_open = 1;
    }

    if (anchor_startup_menu_is_complete())
    {
        if (s_visible)
        {
            recompui_hide_context(s_menu_ctx);
            s_visible = 0;
        }
        return;
    }

    if (s_pending_open)
    {
        s_pending_open = 0;
        if (!s_visible)
        {
            recompui_show_context(s_menu_ctx);
            s_visible = 1;
        }
    }
    else if (s_visible)
    {
        recompui_hide_context(s_menu_ctx);
        recompui_show_context(s_menu_ctx);
    }

    if (s_pending_single)
    {
        s_pending_single = 0;
        anchor_disconnect();
        anchor_startup_menu_finish();
        if (s_visible)
        {
            recompui_hide_context(s_menu_ctx);
            s_visible = 0;
        }
    }

    if (s_pending_multiplayer)
    {
        s_pending_multiplayer = 0;
        if (s_visible)
        {
            recompui_hide_context(s_menu_ctx);
            s_visible = 0;
        }
        anchor_startup_multiplayer_open();
    }
}
