#include "impact_test_pointers.h"
#include <stdio.h>
#define WORLD_EQUIPMENT_HOST_TEST 1
#define EQ_PTR(p,o) TP(p,o)
#define EQ_DISABLED 0ul
#define RECOMP_HOOK(name)
#include "../src/anchor_world_equipment.c"

unsigned short D_800C7AB2;
static unsigned int a[64],o[64],shine[64],shine_object[64];
static unsigned int saved_flag, clears;
int func_800240DC_24CDC(int flag) { return (unsigned int)flag == saved_flag; }
void func_802139E0_5CEEB0(void *task,void *object) {(void)task;(void)object;}
void func_80213A9C_5CEF6C(void *task) {(void)task;}
void func_80218DA8_5D4278(void *task,int r,unsigned short h,short off) {
  assert(task==a && !r && !h && !off);++clears;
  EQ_W(task,0x48)=0xffffffffu;
  EQ_H(task,0x4e)=EQ_H(task,0x50)=EQ_H(task,0x52)=0;
}
static void unrelated(void *task,void *object) {(void)task;(void)object;}
typedef void (*Hook)(void *,void *);
static void fixture(unsigned int entity,unsigned int room) {
  memset(a,0,sizeof(a));memset(o,0,sizeof(o));
  memset(shine,0,sizeof(shine));memset(shine_object,0,sizeof(shine_object));
  memset(test_ptrs,0,sizeof(test_ptrs));test_ptr_count=0;
  saved_flag=clears=0;D_800C7AB2=(unsigned short)room;
  EQ_PTR(a,0x18)=o;EQ_PTR(a,0xe4)=shine;EQ_PTR(shine,0x18)=shine_object;
  EQ_H(a,0x5c)=EQ_H(a,0x5e)=EQ_H(shine,0x5c)=(unsigned short)entity;
  EQ_H(shine,0x5e)=1;EQ_B(a,0x74)=EQ_B(shine,0x74)=7;
  EQ_W(a,0x60)=0x2006e1;EQ_W(a,0x68)=0x40280;
  EQ_PTR(shine,0xc)=(void *)func_80213A9C_5CEF6C;
}
int main(void) {
  const unsigned int entities[]={0x3d2,0x3d4,0x3d5};
  const unsigned int rooms[]={0x62,0x6e,0x81}, flags[]={0x1a6,0x1a4,0x1a5};
  Hook hooks[]={anchor_world_equipment_hammer,anchor_world_equipment_fire_ryo,
                anchor_world_equipment_bazooka};
  for(unsigned int i=0;i<3;++i) {
    fixture(entities[i],rooms[i]);hooks[i](a,o);
    assert(!clears && EQ_W(a,0x60)==0x2006e1 && EQ_W(a,0x68)==0x40280);
    /* Remote flag wins before the local idle contact branch. */
    saved_flag=flags[i];hooks[i](a,o);
    assert(clears==1 && EQ_W(a,0x60)==0 && EQ_W(a,0x68)==2 && EQ_B(shine,0xd0)==1);
    hooks[i](a,o);assert(clears==1); /* Idempotent until native finalization. */
    fixture(entities[i],rooms[i]);saved_flag=flags[i];EQ_PTR(a,0xe4)=0;
    hooks[i](a,o);assert(clears==1 && EQ_W(a,0x68)==2);
    /* Shine may still be in its scheduled initializer. D0 survives setup. */
    fixture(entities[i],rooms[i]);saved_flag=flags[i];
    EQ_H(shine,0x5e)=(unsigned short)entities[i];
    EQ_PTR(shine,0xc)=(void *)func_802139E0_5CEEB0;
    hooks[i](a,o);assert(clears==1 && EQ_B(shine,0xd0)==1);
    /* Recycled/wrong child slots are left alone. */
    for(unsigned int bad=0;bad<4;++bad) {
      fixture(entities[i],rooms[i]);saved_flag=flags[i];
      if(bad==0) EQ_B(shine,0x74)++;
      if(bad==1) EQ_H(shine,0x5c)++;
      if(bad==2) EQ_PTR(shine,0xc)=(void *)unrelated;
      if(bad==3) EQ_H(shine,0x5e)=2;
      hooks[i](a,o);assert(clears==1 && !EQ_B(shine,0xd0));
    }
    for(unsigned int bad=0;bad<5;++bad) {
      fixture(entities[i],rooms[i]);saved_flag=flags[i];
      if(bad==0) ++D_800C7AB2;
      if(bad==1) EQ_H(a,0x5c)++;
      if(bad==2) EQ_H(a,0x5e)++;
      if(bad==3) saved_flag=0;
      if(bad==4) EQ_PTR(a,0x18)=shine_object;
      hooks[i](a,o);assert(!clears && EQ_W(a,0x60)==0x2006e1 && !EQ_B(shine,0xd0));
    }
  }
  puts("equipment native: all three idle rewards retire safely; local scene callbacks untouched");
  return 0;
}
