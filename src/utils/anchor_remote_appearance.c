#include "anchor_remote_appearance.h"
#include "anchor_player_models.h"

void anchor_remote_appearance_apply_hurt(void *object, int appearance_flags,
                                        unsigned short native_frame)
{
    unsigned char *bytes = object;
    unsigned char hidden =
        (appearance_flags & ANCHOR_APPEARANCE_HURT_RECOVERY) ?
        (unsigned char)(native_frame & 1u) : 0;

    if (!bytes)
        return;
    /* FUN_801D9CE8 alternates this bit during player +0xD4 recovery.
     * FUN_80016C44 tests object +0x64 bit0 before drawing kind-2 models.
     * Preserve unrelated flags and every gameplay/animation field. */
    bytes[0x64] = (unsigned char)((bytes[0x64] & 0xfeu) | hidden);
}
