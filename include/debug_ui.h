#ifndef DEBUG_UI_H
#define DEBUG_UI_H

#include "recompui.h"

/* Shared persistent HUD owned by debug.c. It is the one mouse-capturing
 * context used by the DBG/NET controls and the online-player roster. */
RecompuiContext debug_ui_hud_context(void);
RecompuiResource debug_ui_hud_root(void);

#endif
