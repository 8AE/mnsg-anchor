#include <stdio.h>
#include <stdlib.h>
#include "impact_test_pointers.h"
#define ANCHOR_IMPACT_SOUNDS_HOST_TEST
#define RECOMP_PATCH
#define IS_PTR(p,o) TP(p,o)
char *anchor_impact_sounds_update(int,unsigned int,unsigned int,unsigned int,const char *);
void recomp_free(void *);
#include "../src/anchor_impact_sounds.c"
static unsigned int state[256], manager[64], child[64], unrelated[64];
void *D_8020EED0_63A2B0 = state, *D_8016DAB4_16E6B4 = child;
volatile unsigned char D_801C09FD_1C15FD, D_801C09C9_1C15C9;
volatile unsigned int D_801C0A00_1C1600[8];
static int owner = 1;
static unsigned int visit = 1;
static char response[73] = "00000000", published[73];
int anchor_impact_native_ready(void) { return 1; }
int anchor_impact_native_is_owner(void) { return owner; }
unsigned int anchor_impact_native_visit(void) { return visit; }
unsigned int anchor_impact_native_stage(void) { return 0x260; }
unsigned int anchor_impact_native_encounter(void) { return 1; }
char *anchor_impact_sounds_update(int active,unsigned int stage,unsigned int boss,unsigned int v,const char *sample) {
    assert(stage == 0x260 && boss == 1 && v == visit); (void)active;
    strcpy(published,sample); return response;
}
void recomp_free(void *p) { (void)p; }
static void queue_clear(void) { D_801C09FD_1C15FD = 0; }
int main(void) {
    unsigned int i, words[9];
    TP(state,0x1BC) = manager; TP(child,4) = manager; TU16(child,0x20) = 1;
    anchor_impact_sounds_tick(1,1,1);
    func_80038C30_39830(0x229,64,120);
    assert(D_801C09FD_1C15FD == 1 && D_801C0A00_1C1600[0] == 0x78400229);
    assert(captured_count == 1 && !D_801C09C9_1C15C9);
    func_80038C30_39830(0x229,64,120); assert(captured_count == 1);
    anchor_impact_sounds_tick(1,1,1); assert(!strcmp(published,"0000000078400229"));
    queue_clear(); func_80038C30_39830(0x240,0,0);
    anchor_impact_sounds_tick(1,1,1); assert(playing == 16 && owner_loops == 16);
    /* Owner handoff stops its former loop before rendering a new owner. */
    queue_clear(); owner = 0; anchor_impact_sounds_tick(1,2,2);
    assert(!playing && D_801C0A00_1C1600[0] == 0x8240);
    queue_clear(); func_80038C30_39830(0x229,64,120);
    assert(!D_801C09FD_1C15FD && !captured_count); /* no speculative duplicate */
    D_8016DAB4_16E6B4 = unrelated;
    func_80038C30_39830(0x229,200,0); /* ordinary game audio keeps exact native behavior */
    assert(D_801C09FD_1C15FD == 1 && D_801C0A00_1C1600[0] == 0x229);
    D_8016DAB4_16E6B4 = child;
    func_80038C30_39830(0x27,0,0); assert(D_801C09FD_1C15FD == 2); /* local music */
    queue_clear(); strcpy(response,"000000107840022900000240");
    anchor_impact_sounds_tick(1,2,2);
    assert(D_801C09FD_1C15FD == 2 && D_801C0A00_1C1600[0] == 0x78400229 && playing == 16);
    /* A missed loop stop is repaired by the next loop-state heartbeat. */
    queue_clear(); strcpy(response,"00000000"); anchor_impact_sounds_tick(1,2,2);
    assert(!playing && D_801C0A00_1C1600[0] == 0x8240);
    /* A full native queue defers remote effects instead of corrupting it. */
    queue_clear(); D_8016DAB4_16E6B4 = unrelated;
    for (i = 0; i < 8; ++i) func_80038C30_39830((unsigned short)(0x100+i),0,0);
    strcpy(response,"0000000000000229"); anchor_impact_sounds_tick(1,2,2);
    assert(deferred_count == 1 && D_801C09FD_1C15FD == 8);
    queue_clear(); strcpy(response,"00000000"); anchor_impact_sounds_tick(1,2,2);
    assert(!deferred_count && D_801C0A00_1C1600[0] == 0x229);
    strcpy(response,"0000001000000240"); queue_clear(); anchor_impact_sounds_tick(1,2,2);
    assert(playing == 16);
    strcpy(response,"00000000"); queue_clear(); anchor_impact_sounds_tick(0,0,0);
    assert(!playing && !enabled && D_801C0A00_1C1600[0] == 0x8240);
    assert(!read_words("0000000000000027",words)); /* no remote music/control */
    assert(!read_words("00000000000007ff",words));
    assert(!read_words("000000ff",words)); assert(!read_words("0000000",words));
    assert(!read_words("0000000000800229",words)); assert(!read_words("000000000000826d",words));
    puts("Impact native audio authority, loop repair, queue capacity and scope tests passed");
    return 0;
}

int anchor_impact_boss_sound_valid(unsigned int e, unsigned int cue) {(void)e;(void)cue;return 0;}
