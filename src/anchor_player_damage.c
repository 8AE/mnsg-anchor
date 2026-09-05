#include "anchor_player_damage.h"

#include "anchor_remote_collision.h"
#include "anchor_runtime.h"
#include "item_sync.h"

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned char D_800C7AE0;
extern unsigned char D_800C7AE2;
extern int anchor_is_connected(void);

/* Native real-player damage intake. The ordinary type-1 attacker path reads
 * attacker+0x18 (object), +0x4c (hit type), +0x6d (damage), then consumes the
 * object's position. It does not retain the attacker in that path. Its
 * callers and FUN_801DA758 establish these fields independently of a
 * playable player task. The native intake handles armour, character/action
 * context, hurt animation, knockback, and the next normal death check. */
extern int func_801D9E9C_595DAC(void *player);
extern void func_801E8E24_5A4D34(void *player, unsigned char reason);
extern int func_801DCD48_598C58(signed char delta);
extern int anchor_race_is_active(void);

/* A transient generic hit source, never registered with an engine task
 * list, renderer, actor manager or the shared collision pass. */
typedef union PlayerHitSource
{
    unsigned int alignment;
    unsigned char bytes[0x80];
} PlayerHitSource;

typedef union PlayerHitObject
{
    unsigned int alignment;
    unsigned char bytes[0x20];
} PlayerHitObject;

static int s_applying_hit;

static void clear_hit_storage(void *storage, unsigned int size)
{
    volatile unsigned char *bytes = storage;
    unsigned int i;
    /* The mod has no libc memset import. Volatile stores keep the compiler
     * from lowering aggregate initialization into that missing function. */
    for (i = 0; i < size; ++i)
        bytes[i] = 0;
}

static int is_rdram_pointer(const void *pointer)
{
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return physical >= 0x1000u && physical < 0x800000u;
}

static int valid_hit_coordinate(float value)
{
    return value >= -10000000.0f && value <= 10000000.0f;
}

int anchor_player_damage_apply(float hit_x, float hit_y, float hit_z)
{
    PlayerHitSource source;
    PlayerHitObject object;
    unsigned char *player = D_801FC604_5B8514;
    unsigned char *work;
    void *backlink;
    void *previous_attacker;
    unsigned int before_hp;
    unsigned int after_hp;
    unsigned int previous_invulnerability;
    int accepted;

    if (s_applying_hit || !anchor_is_connected() ||
        !item_sync_save_is_loaded() || anchor_remote_collision_is_scripted() ||
        D_800C7AE2 != 0 || (D_800C7AE0 != 0 && D_800C7AE0 != 4) ||
        !valid_hit_coordinate(hit_x) || !valid_hit_coordinate(hit_y) ||
        !valid_hit_coordinate(hit_z) || !is_rdram_pointer(player) ||
        !is_rdram_pointer(D_801FC60C_5B851C))
        return 0;
    /* D9E9C services floor hazards and crushing before its attacker slot.
     * Leave those exclusively to the ordinary native call so their damage
     * cannot be mistaken for the received PvP event in team accounting. */
    if (player[0x63] != 0 ||
        *(unsigned short *)(player + 0x96) == 0x92 ||
        *(unsigned short *)(player + 0x96) == 0x93 ||
        *(unsigned short *)(player + 0x98) == 0x92 ||
        *(unsigned short *)(player + 0x98) == 0x93)
        return 0;
    backlink = *(void **)(player + 0x04);
    work = *(unsigned char **)(player + 0x5c);
    if (!is_rdram_pointer(backlink) || *(void **)backlink != player ||
        *(void **)(player + 0x18) != D_801FC60C_5B851C ||
        !is_rdram_pointer(*(void **)(player + 0xdc)) ||
        !is_rdram_pointer(work) || player[0x60] >= 4 ||
        work[0x69] != 0 || player[0xd4] != 0 ||
        (player[0x30] & 1) == 0)
        return 0;
    before_hp = item_sync_local_player_health();
    if (before_hp == 0)
        return 0;

    /* Keep an actual native attacker already detected this frame. The
     * normal pre-update will consume it; a PvP event cannot replace it. */
    previous_attacker = *(void **)(player + 0x38);
    if (previous_attacker)
        return 0;

    clear_hit_storage(source.bytes, sizeof(source.bytes));
    clear_hit_storage(object.bytes, sizeof(object.bytes));
    *(void **)(source.bytes + 0x18) = object.bytes;
    source.bytes[0x4c] = 1;
    source.bytes[0x6d] = 1;
    *(float *)(object.bytes + 0x08) = hit_x;
    *(float *)(object.bytes + 0x0c) = hit_y;
    *(float *)(object.bytes + 0x10) = hit_z;
    previous_invulnerability = player[0xd4];
    s_applying_hit = 1;
    *(void **)(player + 0x38) = source.bytes;
    accepted = func_801D9E9C_595DAC(player);
    *(void **)(player + 0x38) = previous_attacker;
    if (D_801FC604_5B8514 != player ||
        *(void **)(player + 0x18) != D_801FC60C_5B851C)
    {
        s_applying_hit = 0;
        return 0;
    }
    /* Match the ordinary CB824 caller: accepted hits cancel the eligible
     * owned projectile tasks before their next native update. */
    if (accepted)
        func_801E8E24_5A4D34(player, 1);
    after_hp = item_sync_local_player_health();

    /* A type-1 hit deals one half-heart through native armour handling.
     * Sudden Impact doubles vulnerability in FUN_801DA758. Preserve the
     * configured No Hit race challenge without broadcasting this PvP loss
     * as team damage and damaging the attacker a second time. */
    if (after_hp < before_hp && after_hp > 0 &&
        anchor_race_is_active() && anchor_runtime_no_hit_enabled())
    {
        (void)func_801DCD48_598C58((signed char)-(int)after_hp);
        after_hp = item_sync_local_player_health();
    }
    if (after_hp < before_hp)
        item_sync_exclude_pvp_damage(before_hp - after_hp);
    s_applying_hit = 0;

    /* Armour absorbs HP damage but native intake still starts recovery
     * invulnerability and returns zero. Treat that as a consumed attack. */
    return accepted != 0 || after_hp < before_hp ||
           player[0xd4] != previous_invulnerability;
}
