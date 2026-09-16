/* Authoritative native Impact audio. Final commands include pan/volume and
 * global loop stops. Replicas never replay native damage/AI to make sounds. */
#include "anchor_impact_sounds.h"
#include "anchor_impact_native.h"
#include "anchor_impact_boss.h"
#include "anchor_remote_model_pool.h"
#ifndef ANCHOR_IMPACT_SOUNDS_HOST_TEST
#include "modding.h"
#include "anchor.h"
#include "recomputils.h"
#endif
extern void *D_8020EED0_63A2B0, *D_8016DAB4_16E6B4;
extern volatile unsigned char D_801C09FD_1C15FD, D_801C09C9_1C15C9;
extern volatile unsigned int D_801C0A00_1C1600[];
#ifndef IS_PTR
#define IS_PTR(p,o) (*(void **)((unsigned char *)(p)+(o)))
#endif
#define IS_U16(p,o) (*(unsigned short *)((unsigned char *)(p)+(o)))
static const unsigned short loops[] = {0x122,0x130,0x152,0x23E,0x240,0x241};
/* Literal effect cues observed in file_13; arbitrary network IDs never reach audio. */
static const unsigned short cues[] = {
    0x106, 0x108, 0x10A, 0x10F, 0x113, 0x116, 0x117, 0x118, 0x119, 0x11A,
    0x11B, 0x11E, 0x11F, 0x120, 0x121, 0x122, 0x123, 0x12E, 0x12F, 0x130,
    0x131, 0x14E, 0x150, 0x151, 0x152, 0x153, 0x169, 0x16A, 0x16B, 0x16C,
    0x181, 0x229, 0x22A, 0x22B, 0x22D, 0x22E, 0x230, 0x231, 0x232, 0x233,
    0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23A, 0x23B, 0x23E, 0x240,
    0x241, 0x242, 0x245, 0x246, 0x247, 0x248, 0x24A, 0x24F, 0x26B, 0x274,
    0x276, 0x279, 0x27A, 0x27B, 0x27C, 0x27F, 0x280, 0x281, 0x282, 0x283,
    0x284, 0x286, 0x2A8, 0x34E,
};
static unsigned int captured[8], captured_count, playing, owner_loops;
static unsigned int scope_owner, scope_term, scope_visit, scope_stage, scope_boss;
static void *scope_manager;
static int enabled, replaying, was_owner;
typedef struct DeferredSound { unsigned int command, age; } DeferredSound;
static DeferredSound deferred[32];
static unsigned int deferred_count;

static int valid(void *p)
{
#ifdef ANCHOR_IMPACT_SOUNDS_HOST_TEST
    return p != 0;
#else
    unsigned int a = (unsigned int)(unsigned long)p;
    return !(a&3u) && ((a >= 0x80001000u && a < 0x80800000u) ||
                       anchor_remote_model_pool_contains(p));
#endif
}
static int battle_task(void)
{
    void *p = D_8016DAB4_16E6B4;
    unsigned int i, depth;
    if (!enabled || !anchor_impact_native_ready() || !valid(scope_manager) ||
        !valid(D_8020EED0_63A2B0) || IS_PTR(D_8020EED0_63A2B0,0x1BC) != scope_manager ||
        scope_visit != anchor_impact_native_visit() || scope_boss != anchor_impact_native_encounter()) return 0;
    depth = IS_U16(scope_manager,0x20);
    for (i = 0; valid(p) && i < 512; ++i) {
        if (p == scope_manager) return 1;
        if (IS_U16(p,0x20) <= depth) break;
        p = IS_PTR(p,4);
    }
    return 0;
}
static int loop_index(unsigned int id)
{
    unsigned int i;
    for (i = 0; i < 6; ++i) if ((id&0x7FFFu) == loops[i]) return (int)i;
    return -1;
}
static int sound_valid(unsigned int command)
{
    unsigned int i, id = command&0xFFFFu;
    if (((command>>16)&255u) >= 128) return 0;
    if (id&0x8000u) return loop_index(id) >= 0;
    for (i = 0; i < sizeof(cues)/sizeof(cues[0]); ++i) if (id == cues[i]) return 1;
    return anchor_impact_boss_sound_valid(anchor_impact_native_encounter(),id);
}
static void track(unsigned int *mask, unsigned int id)
{
    int i = loop_index(id);
    if (i < 0) return;
    if (id&0x8000u) *mask &= ~(1u<<i);
    else *mask |= 1u<<i;
}

/* Verified USA 38C30 leaf: 8 slots, dedupe low 16 bits, pan only 1..127,
 * zero volume means native default, and C09C9 guards the queue mutation.
 * A patch is needed to suppress speculative follower audio BEFORE enqueue. */
RECOMP_PATCH void func_80038C30_39830(unsigned short id, unsigned char pan, unsigned char volume)
{
    unsigned int i, count, command;
    int scoped;
    if (!id) return;
    D_801C09C9_1C15C9 = 255;
    scoped = !replaying && battle_task() && sound_valid(id);
    if (scoped && !anchor_impact_native_is_owner()) {
        D_801C09C9_1C15C9 = 0;
        return;
    }
    command = id | (pan && pan < 128 ? (unsigned int)pan<<16 : 0) | (unsigned int)volume<<24;
    count = D_801C09FD_1C15FD;
    if (count < 8) {
        for (i = 0; i < count; ++i) if ((D_801C0A00_1C1600[i]&0xFFFFu) == id) {
            D_801C09C9_1C15C9 = 0;
            return;
        }
        D_801C0A00_1C1600[count] = command;
        D_801C09FD_1C15FD = (unsigned char)(count+1);
        if (scoped) {
            track(&owner_loops,id);
            track(&playing,id);
            if (captured_count < 8) captured[captured_count++] = command;
        }
    }
    D_801C09C9_1C15C9 = 0;
}
static int replay(unsigned int command)
{
    unsigned int i, id = command&0xFFFFu;
    if (D_801C09FD_1C15FD >= 8) return 0;
    for (i = 0; i < D_801C09FD_1C15FD; ++i)
        if ((D_801C0A00_1C1600[i]&0xFFFFu) == id) return 0;
    replaying = 1;
    func_80038C30_39830((unsigned short)id,(unsigned char)(command>>16),(unsigned char)(command>>24));
    replaying = 0;
    track(&playing,id);
    return 1;
}
static void hex_word(char *out, unsigned int word)
{
    unsigned int i;
    for (i = 0; i < 8; ++i) out[i] = "0123456789abcdef"[(word>>(28-i*4))&15u];
}
static int read_words(const char *in, unsigned int *out)
{
    unsigned int n = 0, word = 0, digits = 0, d;
    if (!in) return 0;
    while (*in && digits < 72) {
        char c = *in++;
        if (c >= '0' && c <= '9') d = c-'0';
        else if (c >= 'a' && c <= 'f') d = c-'a'+10;
        else return 0;
        word = (word<<4)|d;
        if (++digits%8 == 0) { out[n++] = word; word = 0; }
    }
    if (*in || digits%8 || !n || out[0]&~63u) return 0;
    for (d = 1; d < n; ++d) if (!sound_valid(out[d])) return 0;
    return (int)n;
}
void anchor_impact_sounds_tick(int active, unsigned int owner, unsigned int term)
{
    unsigned int stage = anchor_impact_native_stage(), visit = anchor_impact_native_visit();
    unsigned int desired = 0, words[9], i, n, kept = 0, budget = 4;
    int local_owner = anchor_impact_native_is_owner();
    void *manager = valid(D_8020EED0_63A2B0) ? IS_PTR(D_8020EED0_63A2B0,0x1BC) : 0;
    char sample[73], *reply;
    if (!active || manager != scope_manager || owner != scope_owner || term != scope_term ||
        visit != scope_visit || stage != scope_stage || scope_boss != anchor_impact_native_encounter() ||
        local_owner != was_owner) {
        captured_count = deferred_count = owner_loops = 0;
    }
    scope_manager = manager; scope_owner = owner; scope_term = term;
    scope_visit = visit; scope_stage = stage; was_owner = local_owner;
    scope_boss = anchor_impact_native_encounter();
    if (!active && !enabled && !playing) return;
    enabled = active;
    hex_word(sample,owner_loops);
    for (i = 0; i < captured_count; ++i) hex_word(sample+8+i*8,captured[i]);
    sample[8+captured_count*8] = 0;
    captured_count = 0;
    reply = anchor_impact_sounds_update(active,stage,anchor_impact_native_encounter(),visit,sample);
    n = (unsigned int)read_words(reply,words);
    if (active && local_owner) desired = owner_loops;
    else if (active && n) {
        desired = words[0];
        for (i = 1; i < n && deferred_count < 32; ++i)
            deferred[deferred_count++] = (DeferredSound){words[i],0};
    }
    if (reply) recomp_free(reply);
    for (i = 0; i < deferred_count; ++i) {
        DeferredSound s = deferred[i];
        int loop = loop_index(s.command&0xFFFFu);
        /* Final loop state supersedes delayed starts/stops from older frames. */
        if (loop >= 0 && (!!(desired&(1u<<loop)) == !!(s.command&0x8000u))) continue;
        if (s.age++ > 22) continue;
        if (budget && replay(s.command)) --budget;
        else deferred[kept++] = s;
    }
    deferred_count = kept;
    for (i = 0; i < 6; ++i) {
        if (!((playing^desired)&(1u<<i))) continue;
        if (budget && replay(loops[i] | (desired&(1u<<i) ? 0 : 0x8000u))) --budget;
    }
}
