#include <assert.h>
#include <stdio.h>
#include "impact_test_pointers.h"
#define IMPACT_DAMAGE_PTR(p,o) TP(p,o)
#define ANCHOR_IMPACT_DAMAGE_HOST_TEST
#include "../src/anchor_impact_damage.c"
static unsigned int state[0xC0];
unsigned short D_800C7AB2 = 0x260;
void *D_8020EED0_63A2B0 = state;
int func_801D36CC_5FEAAC(void) { return 25; }
void func_801D3894_5FEC74(void) {}
int main(void) {
    unsigned int boss[64] = {0}, hit_task[64] = {0};
    TP(boss,0x38) = hit_task;
    ((unsigned char *)hit_task)[0x4C] = 0x46;
    TU32(boss,0xAC) = 80;
    AnchorImpactHit hit;
    anchor_impact_damage_reset();
    state[0x60/4] = 100; state[0x68/4] = 100;
    assert(func_801D3954_5FED34(25) == 75);
    assert(func_801D38A4_5FEC84(-2000) == 999);
    assert(func_801D38A4_5FEC84(1000) == 0);
    state[0x68/4] = 1;
    anchor_impact_damage_set_context(1, 0, 1, 0);
    assert(func_801D3954_5FED34(75) == 75); /* election has no context yet */
    anchor_impact_damage_set_context(1, 0, 0, 1);
    assert(func_801D3954_5FED34(999) == 75 && state[0x60/4] == 75);
    assert(func_801D38A4_5FEC84(1) == 1 && state[0x68/4] == 1);
    assert(func_8020451C_62F8FC(boss) == 0 && TU32(boss,0xAC) == 80);
    assert(TU32(boss,0xB0) == 0);
    assert(!anchor_impact_damage_take_local_hit(&hit));
    assert(!anchor_impact_damage_apply(10));
    anchor_impact_damage_set_context(1, 1, 0, 1);
    assert(func_8020451C_62F8FC(boss) == 0x46 && TU32(boss,0xAC) == 55);
    assert(TU32(boss,0xB0) == 40);
    ((unsigned char *)hit_task)[0x4C] = 0x5A;
    assert(func_8020451C_62F8FC(boss) == 0x5A && TU32(boss,0xB0) == 200);
    assert(func_801D38A4_5FEC84(1) == 0);
    assert(anchor_impact_damage_apply(10) && state[0x60/4] == 65);
    anchor_impact_damage_set_context(1, 1, 1, 1);
    assert(func_801D3954_5FED34(65) == 65);
    assert(!anchor_impact_damage_apply(10));
    anchor_impact_damage_reset();
    assert(func_801D3954_5FED34(65) == 0);
    puts("Impact damage authority and native return tests passed");
    return 0;
}
