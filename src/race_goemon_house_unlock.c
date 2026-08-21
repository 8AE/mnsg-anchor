#include "modding.h"
#include "item_sync.h"

/* Save-data base used by the race unlock check. SAVE_WARP_GOEMON_HOUSE at
 * +0x2a4 is the randomizer warp/unlock flag for Goemon's house. */
extern unsigned char D_8015C608_15D208[];

/* Current room id. The hook treats room 0x1d1 as an in-room confirmation that
 * the starter save has reached Goemon's house even before the warp flag exists. */
extern signed short D_800C7AB2;

int anchor_race_is_active(void);
void anchor_set_current_character_if_needed(void);

#define SAVE_READ32(off) (*(int *)((char *)D_8015C608_15D208 + (off)))

#define ROOM_GOEMON_HOUSE 0x1D1
#define SAVE_WARP_GOEMON_HOUSE 0x2A4

static int s_goemon_house_unlock_applied = 0;

static void apply_goemon_house_character_unlock(void)
{
    if (s_goemon_house_unlock_applied)
        return;

    if (!anchor_race_is_active())
        return;

    if (!item_sync_save_is_loaded())
        return;

    if (SAVE_READ32(SAVE_WARP_GOEMON_HOUSE) == 0 &&
        D_800C7AB2 != ROOM_GOEMON_HOUSE)
        return;

    if (SAVE_READ32(SAVE_WARP_GOEMON_HOUSE) == 0)
        item_sync_write_local_flag_val("wp_goemon_h", 1);

    item_sync_write_local_flag_val("chr_goemon", 1);
    item_sync_write_local_flag_val("chr_ebisu", 1);
    anchor_set_current_character_if_needed();
    s_goemon_house_unlock_applied = 1;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_race_goemon_house_unlock_frame_hook(void)
{
    apply_goemon_house_character_unlock();
}
