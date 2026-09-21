/* Execute the production bridge adapter with native-sized pointer slots.
 * Native substitutes model only the audited File51 side effects; this is not
 * a game runtime or a two-client test. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#define WORLD_BRIDGE_HOST_TEST 1
#define BP(p,o) TP(p,o)
#define BR_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/anchor_world_bridge.c"

unsigned short D_800C7AB2;
unsigned char D_800C7AE2;
void *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];
static unsigned int tasks[24][64], objects[24][64];
static unsigned char saves[4], temps[4];
static unsigned short files[] = {0x280,0x158};
static unsigned int clips[] = {1,1,1};
static void *models[] = {files,clips};
static int next_actor, missing_file, fail_births, births, shadows, binds, sounds[1024];
static int talk_target, vm_event[24], vm_calls, blink_calls;
static float animation_limit;
static void dialogue(void *a,void *o) { (void)a;(void)o; }
static void cleanup(void *a,void *o) { (void)a;(void)o; }
static int index_of(void *a) {
  for (int i=0;i<24;++i) if (a == tasks[i]) return i;
  assert(0);return -1;
}
int func_800141C4_14DC4(unsigned int f) { return (int)f == missing_file ? -1 : 0x1000; }
int func_800240DC_24CDC(int f) { assert(f>=0 && f<4);return saves[f]; }
int func_80023E94_24A94(int f) { assert(f>=0 && f<4);return temps[f]; }
void func_80023DF0_249F0(int f) { assert(f>=0 && f<4);temps[f]=1; }
void func_80038B98_39798(unsigned int sound) { assert(sound<1024);++sounds[sound]; }
void func_8021664C_5D1B1C(void *a,unsigned int clip,float rate,unsigned int loop) {
  ++binds;anchor_world_bridge_animation(a,clip);
  void *o=BP(a,0x18);BF(o,0x28)=0;BH(o,0x7e)=(unsigned short)(rate*256);
  BB(o,0x7c)=(unsigned char)loop;
}
void func_80216E1C_5D22EC(void *a,unsigned int slot) { (void)a;assert(!slot); }
float func_8001B5AC_1C1AC(void *o) { (void)o;return animation_limit; }
void func_80224D50_5E0220(void *a,int bank) { (void)a;assert(bank==1);++blink_calls; }
void func_80226840_5E1D10(void *a,unsigned short route) {
  void *o=BP(a,0x18);BH(a,0xc4)=route;BS(a,0xc6)=0;
  for (int j=0;j<3;++j) BS(a,0xc8+j*2)=(short)BF(o,8+j*4);
  BB(a,0xce)=BB(a,0xcf)=0;BB(a,0xaa)=(unsigned char)(BH(o,0x16)>>2);BP(a,0x9c)=a;
}
int func_802268A8_5E1D78(void *a) { ++vm_calls;return vm_event[index_of(a)]; }
static void task(int i,BridgeCallback cb,unsigned int entity) {
  memset(tasks[i],0,sizeof(tasks[i]));memset(objects[i],0,sizeof(objects[i]));
  BP(tasks[i],0x18)=objects[i];BP(tasks[i],0xc)=(void *)cb;
  BB(tasks[i],0x74)=1;BH(tasks[i],0x5c)=BH(tasks[i],0x5e)=(unsigned short)entity;
}
void *func_802171A8_5D2678(void *parent,BridgeCallback cb,unsigned char category) {
  assert(category==0 || category==3);++births;
  if (fail_births) { --fail_births;return 0; }
  assert(next_actor<24);int i=next_actor++;task(i,cb,0);
  assert(anchor_world_bridge_owns(tasks[i])); /* Birth hook runs before allocator return. */
  BP(tasks[i],0xdc)=parent;return tasks[i];
}
void func_080002D8_70DA18(void *a,void *o) {
  ++shadows;BH(a,0x5e)=0x2cc;
  BW(a,0x60)=0x1a800761;BW(a,0x64)=0x20;BW(a,0x68)=0x400;
  BH(a,0xa4)=(unsigned short)BW(a,0xd4);BP(a,0x9c)=(void *)0x9002000ul;
  func_8021664C_5D1B1C(a,1,.25f,1);
  if (saves[1]) {
    BF(o,0x10)+=20;BF(o,8)+=BW(a,0xd8) ? -15:15;
    BP(a,0xc)=(void *)func_08000000_70D740;
  } else BP(a,0xc)=(void *)func_08000174_70D8B4;
}
static void native_talk(void *a) {
  if (index_of(a)!=talk_target) return;
  talk_target=-1;anchor_world_bridge_talk(a);
  BP(a,0xb4)=BP(a,0xc);BW(a,0x68)|=0x100;D_800C7AE2=1;
  BP(a,0xc)=(void *)dialogue;anchor_world_bridge_talk_return();
}
void func_08000174_70D8B4(void *a,void *o) {
  (void)o;native_talk(a);
  if (BW(a,0x68)&0x4000u) {
    BW(a,0x68)&=~0x4000u;if(saves[0]) temps[0]=1;
    func_8021664C_5D1B1C(a,1,.25f,1);
  }
  if (saves[0] && temps[0]) {
    BW(a,0x60)&=~0x10000000u;
    func_80226840_5E1D10(a,BW(a,0xd8) ? 0x43:0x44);
    BP(a,0xc)=(void *)func_08000088_70D7C8;
  }
}
void func_08000000_70D740(void *a,void *o) { (void)o;native_talk(a); }
void func_08000088_70D7C8(void *a,void *o) {
  (void)a;(void)o;assert(!"unsafe native route wrapper must not run under bridge control");
}
void func_08000598_70DCD8(void *a,void *o) {
  (void)o;if (temps[1]) {
    BW(a,0x60)|=1;func_80038B98_39798(0x15e);BP(a,0xc)=(void *)func_08000530_70DC70;
  }
}
void func_08000530_70DC70(void *a,void *o) { (void)a;(void)o; }
void func_080004AC_70DBEC(void *a,void *o) { (void)a;(void)o; }
void func_080006B4_70DDF4(void *a,void *o) {
  (void)o;if (temps[1]) { assert(D_8016DAB4_16E6B4==a);anchor_world_bridge_delete();BW(a,0x68)|=2; }
}
static void fixture(int fresh) {
  anchor_world_bridge_reset(1);memset(tasks,0,sizeof(tasks));memset(objects,0,sizeof(objects));
  memset(saves,0,sizeof(saves));memset(temps,0,sizeof(temps));memset(sounds,0,sizeof(sounds));
  memset(vm_event,0,sizeof(vm_event));memset(D_80236984_5F1E54,0,sizeof(D_80236984_5F1E54));
  D_800C7AB2=WORLD_BRIDGE_ROOM;D_800C7AE2=0;missing_file=-1;animation_limit=60;
  next_actor=4;fail_births=births=shadows=binds=vm_calls=blink_calls=0;talk_target=-1;
  saves[0]=1;saves[1]=(unsigned char)fresh;
  int ids[4]={0x240,0x2d0,0x2d0,0x311};
  D_80236984_5F1E54[0x240]=D_80236984_5F1E54[0x2cc]=models;
  D_80236984_5F1E54[0x311]=D_80236984_5F1E54[1]=models;
  for (int i=0;i<4;++i) {
    task(i,i==0 ? (fresh ? func_080004AC_70DBEC:func_08000598_70DCD8):
         i==3 ? func_080006B4_70DDF4:func_080002D8_70DA18,(unsigned int)ids[i]);
    BW(tasks[i],0xd4)=i==1 ? 0x10e:i==2 ? 0x110:0;BW(tasks[i],0xd8)=i==2 ? 0x10000:0;
    BF(objects[i],8)=i==1 ? -431:i==2 ? -451:0;BF(objects[i],16)=29;BH(objects[i],22)=0x200;
    anchor_world_bridge_register(tasks[i],(unsigned int)ids[i],i==2);
    assert(anchor_world_bridge_owns(tasks[i]));
    if(i==1 || i==2) func_080002D8_70DA18(tasks[i],objects[i]);
    if(i==0) { BW(tasks[i],0x60)=0x802006e0;BH(objects[i],0x7e)=12; }
    anchor_world_bridge_post(tasks[i]);anchor_world_bridge_post_return();
  }
  if(fresh) { D_8016DAB4_16E6B4=tasks[3];anchor_world_bridge_delete();BW(tasks[3],0x68)|=2; }
  anchor_world_bridge_control(tasks[0],0,0,0);
}
static void snapshot(int *r) {
  memset(r,0,ANCHOR_WORLD_WORDS*sizeof(*r));r[1]=0x240;r[2]=WORLD_BRIDGE;
  assert(anchor_world_bridge_capture(tasks[0],r));assert(anchor_world_row_valid(r));
}
static void step(int i) {
  void *a=tasks[i];D_8016DAB4_16E6B4=a;
  BridgeCallback cb=(BridgeCallback)((unsigned long)BP(a,0xc)&~BR_DISABLED);
  assert(cb);cb(a,objects[i]);anchor_world_bridge_post(a);anchor_world_bridge_post_return();
}
static void route_row(int *r) {
  snapshot(r);r[WB_FLAGS]=1;
  for(int i=0;i<2;++i) {
    int k=i ? WB_GUARD_1:WB_GUARD_0;
    r[k]=14;r[k+1]=-42000-i*1000;r[k+2]=0;r[k+3]=6000;
    r[k+4]=0x200|(0x80<<10);r[k+5]=0;r[k+6]=1200;r[k+7]=0x10080;
    r[k+8]=17;r[k+9]=-431-i*20;r[k+10]=0;r[k+11]=29;r[k+12]=3;r[k+13]=0;
  }
}
static void atomic_test(void) {
  fixture(0);int r[ANCHOR_WORLD_WORDS];route_row(r);
  unsigned int old[4][64];memcpy(old,objects,sizeof(old));
  missing_file=0x493;assert(!anchor_world_bridge_apply(tasks[0],r));
  assert(!memcmp(old,objects,sizeof(old)));missing_file=-1;
  for(int gate=0;gate<3;++gate) {
    BW(tasks[2],0x68)=gate==0 ? 0x500:gate==1 ? 0x4400:0x400;
    BP(tasks[2],0xc)=gate==2 ? (void *)cleanup:(void *)func_08000174_70D8B4;
    assert(!anchor_world_bridge_apply(tasks[0],r));assert(!memcmp(old,objects,sizeof(old)));
  }
  BW(tasks[2],0x68)=0x400;BP(tasks[2],0xc)=(void *)func_08000174_70D8B4;
  r[WB_GUARD_0+13]=1;assert(!anchor_world_bridge_apply(tasks[0],r));r[WB_GUARD_0+13]=0;
  assert(anchor_world_bridge_apply(tasks[0],r));assert(shadows==2);
  assert(BP(tasks[1],0x9c)==tasks[1] && BH(tasks[1],0xc4)==0x44 && BH(tasks[2],0xc4)==0x43);
  assert(BW(tasks[1],0x60)==0x0a800761 && BF(objects[1],8)==-420);
  int previous_binds=binds;assert(anchor_world_bridge_apply(tasks[0],r));assert(binds==previous_binds);
  r[WB_GUARD_0]=r[WB_GUARD_1]=15;D_800C7AE2=1;
  assert(anchor_world_bridge_apply(tasks[0],r));assert(shadows==2 && D_800C7AE2==1);
  assert(BW(tasks[1],0x60)==0x1a800761 && BW(tasks[1],0x64)==0x20);
}
static void dialogue_test(void) {
  fixture(0);int r[ANCHOR_WORLD_WORDS];route_row(r);
  talk_target=1;anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  assert(BP(tasks[1],0xb4)==(void *)func_08000174_70D8B4);
  assert(BP(tasks[1],0xc)==(void *)dialogue && br_owns_lock==1 && D_800C7AE2);
  assert(!anchor_world_bridge_apply(tasks[0],r));
  BW(tasks[1],0x68)=0x400;BP(tasks[1],0xc)=(void *)cleanup;
  assert(!anchor_world_bridge_apply(tasks[0],r));anchor_world_bridge_begin();
  assert(BP(tasks[1],0xc)==(void *)cleanup);anchor_world_bridge_end();
  BP(tasks[1],0xc)=BP(tasks[1],0xb4);BW(tasks[1],0x68)|=0x4000;
  assert(!anchor_world_bridge_apply(tasks[0],r));anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  assert(!(BW(tasks[1],0x68)&0x4000) && temps[0]);
  assert(anchor_world_bridge_apply(tasks[0],r));vm_event[1]=1;
  anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  assert(!D_800C7AE2 && !br_owns_lock && shadows==2);
  assert(BP(tasks[1],0xc)==(void *)func_08000000_70D740);
  /* An imported start cannot carry a still-unconsumed accepted result into ROUTE. */
  fixture(0);BW(tasks[1],0x68)|=0x4000;anchor_world_bridge_control(tasks[0],0,0,1);
  anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  assert(!(BW(tasks[1],0x68)&0x4000) && BH(tasks[1],0xc4)==0x44);
}
static void lock_supersession_test(void) {
  fixture(0);br_owns_lock=1;D_800C7AE2=1;
  anchor_world_bridge_other_lock();release_lock(0);assert(D_800C7AE2==1);
  br_owns_lock=1;task(10,cleanup,0x200);
  anchor_world_bridge_other_talk(tasks[10]);anchor_world_bridge_other_talk_return();
  assert(br_owns_lock==1); /* A rejected contact check does not take the lock. */
  anchor_world_bridge_other_talk(tasks[10]);BP(tasks[10],0xc)=(void *)dialogue;
  anchor_world_bridge_other_talk_return();release_lock(0);assert(!br_owns_lock && D_800C7AE2==1);
}
static void reconstruction_test(void) {
  fixture(0);int r[ANCHOR_WORLD_WORDS];route_row(r);
  fixture(1);assert(br_removed && br_fresh);fail_births=1;
  assert(!anchor_world_bridge_apply(tasks[0],r));assert(next_actor==4);
  assert(!anchor_world_bridge_apply(tasks[0],r));assert(next_actor==5);
  assert(!anchor_world_bridge_apply(tasks[0],r));assert(next_actor==5); /* Pending birth is reused. */
  step(4);assert(anchor_world_bridge_apply(tasks[0],r));
  assert(!br_removed && !br_fresh && alive(&br_nodes[3]) && !(BW(tasks[4],0x68)&2));
  assert(BF(objects[4],16)==-9 && shadows==2);
  r[WB_BLOCKER_REMOVED]=1;r[WB_FLAGS]=3;
  assert(anchor_world_bridge_apply(tasks[0],r));assert(BW(tasks[4],0x68)&2);
  fixture(0);route_row(r);anchor_world_bridge_reuse(tasks[2]);BW(tasks[2],0x68)|=2;
  missing_file=0x152;assert(!anchor_world_bridge_apply(tasks[0],r));assert(!births);
  missing_file=-1;assert(!anchor_world_bridge_apply(tasks[0],r));assert(next_actor==5);
  assert(!anchor_world_bridge_apply(tasks[0],r));assert(next_actor==5);
  saves[1]=1;step(4);assert(shadows==3 && phase(&br_nodes[2],br_guard)==3);
  assert(anchor_world_bridge_apply(tasks[0],r));
  assert(BH(tasks[4],0xc4)==0x43 && BF(objects[4],8)==-430 && shadows==3);
  /* Allocation failure can leave a guard without a shadow. A received phase
   * must preserve that local outcome instead of inventing a shadow object. */
  BW(tasks[4],0x60)&=~0x08000000u;r[WB_GUARD_1]=15;
  assert(anchor_world_bridge_apply(tasks[0],r));assert(!(BW(tasks[4],0x60)&0x08000000u));
}
static void lifetime_test(void) {
  fixture(0);anchor_world_bridge_begin();
  task(4,func_08000174_70D8B4,0x2d0);anchor_world_bridge_register(tasks[4],0x2d0,0);
  assert(BP(tasks[1],0xc)==(void *)func_08000174_70D8B4);anchor_world_bridge_end();
  fixture(0);br_nodes[1].parent=tasks[0];br_nodes[1].parent_generation=1;br_nodes[1].proxy=1;
  anchor_world_bridge_begin();++BB(tasks[0],0x74);anchor_world_bridge_reuse(tasks[0]);
  assert(BW(tasks[1],0x68)&2);assert(BP(tasks[1],0xc)==(void *)func_08000174_70D8B4);
  anchor_world_bridge_end();
  fixture(0);anchor_world_bridge_begin();anchor_world_bridge_reset(0);
  assert(BP(tasks[1],0xc)==(void *)func_08000174_70D8B4 && !(BW(tasks[1],0x68)&2));
}
static void pause_and_handoff_test(void) {
  fixture(0);int r[ANCHOR_WORLD_WORDS];route_row(r);assert(anchor_world_bridge_apply(tasks[0],r));
  anchor_world_bridge_control(tasks[0],1,1,1);anchor_world_bridge_begin();step(1);
  assert(!vm_calls);BW(tasks[1],0x60)|=0x800301;BF(tasks[1],0x78)=3;
  unsigned int old=BW(tasks[1],0x60);anchor_world_bridge_post(tasks[1]);
  assert(!(BW(tasks[1],0x60)&0x800301) && !BF(tasks[1],0x78));
  anchor_world_bridge_post_return();assert(BW(tasks[1],0x60)==old && BF(tasks[1],0x78)==3);
  anchor_world_bridge_end();anchor_world_bridge_control(tasks[0],0,0,1);
  vm_event[1]=3;anchor_world_bridge_begin();step(1);step(1);anchor_world_bridge_end();assert(sounds[0x162]==1);
  vm_event[1]=2;anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  anchor_world_bridge_begin();step(0);step(3);anchor_world_bridge_end();
  assert(temps[1] && br_removed && sounds[0x15e]==1);
  fixture(0);saves[1]=1;anchor_world_bridge_begin();step(1);anchor_world_bridge_end();
  assert(temps[0] && BH(tasks[1],0xc4)==0x44); /* Departed dialogue owner already committed save1. */
}
int main(void) {
  atomic_test();dialogue_test();lock_supersession_test();reconstruction_test();lifetime_test();pause_and_handoff_test();
  puts("world bridge: atomic restore, local dialogue/locks/shadows, route prediction, reconstruction, lifetime and handoff passed");
  return 0;
}
