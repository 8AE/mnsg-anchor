#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include "anchor_player_damage.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #test); return 1; \
} } while (0)

void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned char D_800C7AE0;
unsigned char D_800C7AE2;
static unsigned char *s_memory;
static unsigned char *s_player;
static unsigned char *s_work;
static int s_connected;
static int s_loaded;
static int s_scripted;
static int s_no_hit;
static int s_armour;
static int s_sudden_impact;
static unsigned int s_hp;
static int s_baseline;
static unsigned int s_excluded;
static int s_native_calls;
static int s_descriptor_valid;
static int s_reentrant_result;
static int s_replace_player;
static int s_cleanup_calls;

int anchor_is_connected(void) { return s_connected; }
int item_sync_save_is_loaded(void) { return s_loaded; }
int anchor_remote_collision_is_scripted(void) { return s_scripted; }
int anchor_race_is_active(void) { return s_no_hit; }
int anchor_runtime_no_hit_enabled(void) { return s_no_hit; }
unsigned int item_sync_local_player_health(void) { return s_hp; }

/* Model the documented baseline operation while the actual item-sync
 * integration is checked separately by source/build review. */
void item_sync_exclude_pvp_damage(unsigned int damage)
{
    s_excluded += damage;
    s_baseline -= (int)damage;
}

int func_801DCD48_598C58(signed char delta)
{
    int hp = (int)s_hp + delta;
    s_hp = hp < 0 ? 0u : (unsigned int)hp;
    return hp < 0;
}

void func_801E8E24_5A4D34(void *player, unsigned char reason)
{
    if (player == s_player && reason == 1)
        s_cleanup_calls++;
}

int func_801D9E9C_595DAC(void *player)
{
    unsigned char *task = player;
    unsigned char *source = *(unsigned char **)(task + 0x38);
    unsigned char *object = *(unsigned char **)(source + 0x18);
    unsigned int damage = s_sudden_impact ? 2u : 1u;
    s_native_calls++;
    s_descriptor_valid = source != task && source[0x4c] == 1 &&
        source[0x6d] == 1 && *(float *)(object + 0x08) == 12.0f &&
        *(float *)(object + 0x0c) == 3.0f &&
        *(float *)(object + 0x10) == -4.0f;
    s_reentrant_result = anchor_player_damage_apply(12.0f, 3.0f, -4.0f);
    task[0x30] &= (unsigned char)~1u;
    if (s_armour)
    {
        s_armour--;
        task[0xd4] = 60;
        return 0;
    }
    s_hp = damage >= s_hp ? 0u : s_hp - damage;
    task[0xcc] = 0x1d;
    if (s_replace_player)
        D_801FC604_5B8514 = 0;
    return 1;
}

static void reset(void)
{
    memset(s_memory, 0, 0x10000);
    s_player = s_memory + 0x1000;
    s_work = s_memory + 0x2000;
    D_801FC604_5B8514 = s_player;
    D_801FC60C_5B851C = s_memory + 0x3000;
    *(void **)(s_player + 0x04) = s_memory + 0x4000;
    *(void **)(s_memory + 0x4000) = s_player;
    *(void **)(s_player + 0x18) = D_801FC60C_5B851C;
    *(void **)(s_player + 0x5c) = s_work;
    *(void **)(s_player + 0xdc) = s_memory + 0x5000;
    s_player[0x30] = 3;
    s_connected = s_loaded = 1;
    s_scripted = s_no_hit = s_armour = s_sudden_impact = 0;
    s_native_calls = s_descriptor_valid = s_replace_player = 0;
    s_cleanup_calls = D_800C7AE0 = D_800C7AE2 = 0;
    s_reentrant_result = -1;
    s_hp = s_baseline = 10;
    s_excluded = 0;
}

static int test_native_hit_is_scoped_and_cannot_reenter_or_replay(void)
{
    reset();
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_descriptor_valid && s_native_calls == 1);
    CHECK(s_cleanup_calls == 1);
    CHECK(s_hp == 9 && s_excluded == 1 && s_baseline == 9);
    CHECK(*(void **)(s_player + 0x38) == 0);
    CHECK(s_reentrant_result == 0);
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    CHECK(s_native_calls == 1);
    return 0;
}

static int test_armour_and_sudden_impact_remain_native(void)
{
    reset();
    s_armour = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_armour == 0 && s_hp == 10 && s_excluded == 0);
    CHECK(s_cleanup_calls == 0);
    CHECK(s_player[0xd4] == 60);
    reset();
    s_sudden_impact = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 8 && s_excluded == 2);
    return 0;
}

static int test_guards_preserve_native_state(void)
{
    reset(); s_scripted = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_player[0xd4] = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_work[0x69] = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); D_800C7AE2 = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); D_800C7AE0 = 8;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_player[0x63] = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); *(unsigned short *)(s_player + 0x96) = 0x92;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); *(unsigned short *)(s_player + 0x98) = 0x93;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_hp = 0;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_connected = 0;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); s_loaded = 0;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); *(void **)(s_player + 0x38) = s_work;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    CHECK(*(void **)(s_player + 0x38) == s_work);
    reset(); D_801FC60C_5B851C = s_work;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); *(void **)(s_memory + 0x4000) = 0;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset(); D_801FC604_5B8514 = (void *)0x80000000u;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    reset();
    CHECK(anchor_player_damage_apply(NAN, 3, -4) == 0);
    CHECK(s_native_calls == 0);
    return 0;
}

static int test_exclusion_preserves_environmental_loss_and_healing(void)
{
    reset(); s_hp = 9; /* one enemy hit before PvP */
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 8 && s_baseline == 9);
    CHECK(s_baseline - s_hp == 1); /* still broadcast enemy damage only */
    reset(); s_hp = 12; /* two-point heal before PvP */
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 11 && s_baseline == 9);
    CHECK(s_hp - s_baseline == 2); /* heal remains shareable */
    reset(); s_baseline = 1; s_hp = 6; s_sudden_impact = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 4 && s_baseline == -1);
    CHECK((int)s_hp - s_baseline == 5); /* full low-HP heal is retained */
    return 0;
}

static int test_no_hit_challenge_and_lethal_hits_do_not_echo(void)
{
    reset(); s_no_hit = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 0 && s_baseline == 0 && s_excluded == 10);
    reset(); s_hp = s_baseline = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 1);
    CHECK(s_hp == 0 && s_baseline == 0 && s_excluded == 1);
    return 0;
}

static int test_owner_change_restores_descriptor_and_stops_accounting(void)
{
    reset(); s_replace_player = 1;
    CHECK(anchor_player_damage_apply(12, 3, -4) == 0);
    CHECK(*(void **)(s_player + 0x38) == 0);
    CHECK(s_excluded == 0);
    return 0;
}

int main(void)
{
    int failed;
    /* Preserve native low-address checks while the host mocks themselves
     * use host-sized pointers. The upper word is 3, which is also a valid
     * character byte in the host pointer stored at task+0x5c. The address
     * sits above macOS's protected 4-GiB page-zero region. */
    s_memory = mmap((void *)0x380000000ull, 0x10000,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (s_memory == MAP_FAILED)
    {
        perror("mmap native test arena");
        return 1;
    }
    if (s_memory != (void *)0x380000000ull)
    {
        fprintf(stderr, "native test arena address unavailable\n");
        munmap(s_memory, 0x10000);
        return 1;
    }
    failed = test_native_hit_is_scoped_and_cannot_reenter_or_replay() ||
        test_armour_and_sudden_impact_remain_native() ||
        test_guards_preserve_native_state() ||
        test_exclusion_preserves_environmental_loss_and_healing() ||
        test_no_hit_challenge_and_lethal_hits_do_not_echo() ||
        test_owner_change_restores_descriptor_and_stops_accounting();
    munmap(s_memory, 0x10000);
    return failed;
}
