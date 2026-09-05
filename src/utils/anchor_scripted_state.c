#include "anchor_remote_collision.h"

/* Native byte fields in g_system (0x8008CCC0): +0x3AE20 and +0x3AE23.
 * These exact exports have no ROM suffix in mnsg.datasyms.toml. */
extern unsigned char D_800C7AE0;
extern unsigned char D_800C7AE3;

int anchor_remote_collision_is_scripted(void)
{
    /* The native late player update, func_801CBAF8_587A08, skips movement
     * collision when either low control-state bit is set. */
    if ((D_800C7AE0 & 3u) != 0)
        return 1;

    /* Scripted movement can keep native world collision active. The native
     * move-toward helper func_801CF770_58B680 sets this byte while supplying
     * synthetic stick input. The Eocs playback task func_80224324_5DF7F4 also
     * holds it at 1 until playback completes and then clears it. Remote
     * bodies must not obstruct either local or received scripted movement. */
    return D_800C7AE3 != 0;
}
