/* Production File64 adapter, with native task storage and audited effects. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#define WORLD_GATE64_HOST_TEST 1
#define GP(p,o) TP(p,o)
#define G64_DISABLED (1ul<<(sizeof(unsigned long)*8-1))
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/world/anchor_world_gate64.c"
unsigned short D_800C7AB2;
unsigned char D_800C7AE2;
void *D_8016DAB4_16E6B4;
unsigned char D_8015C608_15D208[512];
void *D_80236984_5F1E54[1026];
static unsigned int tasks[8][64],objects[8][64],contact[64];
static unsigned short files[]={1500,1501};
static unsigned int slots[]={0,1,2,3};
static void *model[]={files,slots};
static int save_a1,missing,births,fail_birth,locks,cutscenes,cleanups,rise_fx,split_fx;
int func_800141C4_14DC4(unsigned int file){return (int)file==missing?-1:0x1234;}
int func_800240DC_24CDC(int flag){assert(flag==0xa1);return save_a1;}
void func_80024038_24C38(int flag){assert(flag==0xa1);save_a1=1;}
void func_80038BC8_397C8(unsigned int sound){assert(sound<=0xffff);}
void func_80221F70_5DD440(void){assert(!D_800C7AE2);D_800C7AE2=1;++locks;}
void func_80221EBC_5DD38C(void){}
float func_80003E10_4A10(unsigned short angle){return sinf((float)angle*6.28318530718f/1024);}
float func_80003EA0_4AA0(unsigned short angle){return cosf((float)angle*6.28318530718f/1024);}
void func_80216E1C_5D22EC(void *a,unsigned int slot){
  assert(slot>=1&&slot<=3);anchor_world_gate64_model(a,GP(a,0x18),slot);
  for(unsigned int j=0;j<3;++j)GF(GP(a,0x18),0x1c+j*4)=1;
}
static void task(unsigned int i,GateCallback callback){
  memset(tasks[i],0,sizeof(tasks[i]));memset(objects[i],0,sizeof(objects[i]));
  GP(tasks[i],0x18)=objects[i];GP(tasks[i],0xc)=(void *)callback;GB(tasks[i],0x74)=1;
  GH(tasks[i],0x5c)=i?0:0x325;GH(tasks[i],0x5e)=0x24f;
}
void func_080001BC_724DCC(void *a,void *o){(void)a;(void)o;assert(!"native hit must defer controls until arbitration");}
void func_0800033C_724F4C(void *a,void *o){(void)a;(void)o;assert(0);}
void func_0800037C_724F8C(void *a,void *o){
  (void)o;assert(gate_scene&&locks);++cutscenes;
  if(!GS(a,0x8a)){GP(a,0xd8)=tasks[7];GB(a,0xd0)=0;GP(a,0xc)=(void *)func_0800042C_72503C;}
  else --GS(a,0x8a);
}
void func_0800042C_72503C(void *a,void *o){(void)o;assert(gate_scene&&GP(a,0xd8));++cutscenes;}
void func_08000AD8_7256E8(void *a,void *o){
  (void)o;assert(gate_scene);++cleanups;save_a1=1;D_800C7AE2=0;D_8016DAB4_16E6B4=a;
  anchor_world_gate64_delete();GW(a,0x68)|=2;
}
void func_08000B9C_7257AC(void *a,void *o){
  GH(a,0x5e)=0x24f;func_80216E1C_5D22EC(a,3);
  for(unsigned int j=0;j<3;++j)GF(o,0x1c+j*4)=0.15f;
  GP(a,0xc)=(void *)func_08000C0C_72581C;
}
void func_08000C0C_72581C(void *a,void *o){(void)a;(void)o;}
void *func_80220410_5DB8E0(void *parent,GateCallback cb,float x,float y,float z,unsigned short yaw){
  assert(parent==tasks[0]);++births;if(fail_birth)return 0;task(2,cb);
  GF(objects[2],8)=x;GF(objects[2],12)=y;GF(objects[2],16)=z;GH(objects[2],22)=yaw;return tasks[2];
}
void *func_08000DE4_7259F4(void *a){assert(a==tasks[0]);++rise_fx;return tasks[3];}
void *func_08000C18_725828(void *a){assert(a==tasks[0]);++split_fx;return tasks[3];}
static void fixture(void){
  anchor_world_gate64_reset(1);memset(test_ptrs,0,sizeof(test_ptrs));test_ptr_count=0;memset(D_8015C608_15D208,0,sizeof(D_8015C608_15D208));
  memset(D_80236984_5F1E54,0,sizeof(D_80236984_5F1E54));
  D_800C7AB2=0x14b;D_800C7AE2=0;save_a1=0;missing=-1;births=fail_birth=locks=cutscenes=cleanups=rise_fx=split_fx=0;
  D_80236984_5F1E54[0x24f]=model;task(0,func_080001BC_724DCC);task(1,func_08000C0C_72581C);task(3,func_08000C0C_72581C);
  GP(tasks[0],0xd4)=tasks[1];GW(tasks[0],0x60)=0x82a00fe1;
  for(unsigned int i=0;i<2;++i){GF(objects[i],8)=-515;GF(objects[i],12)=-230;GF(objects[i],16)=-49;GH(objects[i],22)=719;}
  anchor_world_gate64_register(tasks[0]);anchor_world_gate64_post(tasks[0]);anchor_world_gate64_post_return();
  anchor_world_gate64_control(tasks[0],0,0,1);anchor_world_gate64_begin();assert(live(0)&&live(1));
}
static void row(int *r){memset(r,0,50*sizeof(*r));r[1]=0x325;r[2]=10;assert(anchor_world_gate64_capture(tasks[0],r));assert(anchor_world_row_valid(r));}
static void step(void){D_8016DAB4_16E6B4=tasks[0];raw(tasks[0])(tasks[0],objects[0]);}
static void apply_phase(int p,int t){int r[50];row(r);r[12]=p;r[11]=t;r[25]=(p==10);if(p>=7||(p==6&&t>1))r[10]=2;assert(anchor_world_gate64_apply(tasks[0],r));}
static void hits_and_camera(void){
  fixture();GP(tasks[0],0x38)=contact;GB(contact,0x4c)=0x16;GW(tasks[0],0x68)|=1;step();assert(phase()==2&&!locks);
  for(int i=0;i<8;++i)step();assert(phase()==1);
  GW(D_8015C608_15D208,0xa4)=2;GW(tasks[0],0x68)|=1;step();assert(phase()==3&&!locks);
  anchor_world_gate64_control(tasks[0],0,0,0);
  for(int i=0;i<80;++i)step();assert(GS(tasks[0],0x8a)==60&&!locks&&!cutscenes);
  anchor_world_gate64_control(tasks[0],1,0,1);step();assert(!locks&&!cutscenes);
  anchor_world_gate64_control(tasks[0],0,0,1);
  while(GS(tasks[0],0x8a)>0){step();assert(!locks&&!cutscenes);}
  D_800C7AE2=1; /* A different local sequence started during the arbitration wait. */
  for(int i=0;i<10;++i){step();assert(phase()==3&&!GS(tasks[0],0x8a)&&!locks&&!cutscenes);}
  assert(D_800C7AE2);D_800C7AE2=0;
  step();assert(locks==1&&gate_scene&&cutscenes==1);
  save_a1=1; /* A team completion arriving during this local camera is deferred. */
  int r[50];row(r);assert(r[3]);assert(!anchor_world_gate64_apply(tasks[0],r));
  assert(!r[25]&&phase()==4&&!(GW(tasks[0],0x68)&2u));
  gate_real=func_08000AD8_7256E8;step();row(r);assert(r[25]&&cleanups==1&&!r[3]);
}
static void spectator_and_handoff(void){
  fixture();apply_phase(5,1);anchor_world_gate64_control(tasks[0],1,0,1);
  for(int i=0;i<100;++i)step();assert(phase()==5&&GS(tasks[0],0x8a)==61&&!locks&&!cutscenes);
  apply_phase(6,1);float y=GF(objects[0],12);for(int i=0;i<80;++i)step();
  assert(phase()==6&&GS(tasks[0],0x8a)==49&&GF(objects[0],12)==y+960&&rise_fx==1);
  anchor_world_gate64_control(tasks[0],0,0,1);
  /* A replica at its phase boundary advances once after taking over. */
  for(int i=0;i<180;++i)step();assert(save_a1&&gate_complete&&!locks&&!cutscenes);
  /* A losing simultaneous hit must not acquire a camera on later handoff. */
  fixture();GP(tasks[0],0x38)=contact;GB(contact,0x4c)=0x16;
  GW(D_8015C608_15D208,0xa4)=1;GW(tasks[0],0x68)|=1;step();
  assert(gate_local_hit&&phase()==3);apply_phase(3,30);
  anchor_world_gate64_control(tasks[0],0,0,1);
  for(int i=0;i<350;++i)step();assert(save_a1&&!locks&&!cutscenes);
}
static void reconstruction_and_failure(void){
  fixture();int r[50];row(r);r[12]=7;r[11]=40;r[10]=2;r[24]=1;r[17]=12345;
  missing=files[0];assert(!anchor_world_gate64_apply(tasks[0],r));missing=-1;
  assert(anchor_world_gate64_apply(tasks[0],r));assert(fabsf(GF(objects[1],8)-123.45f)<.001f);
  step();assert(!rise_fx&&!split_fx&&!locks&&!cutscenes);
  anchor_world_gate64_reuse(tasks[1]);GP(tasks[0],0xd4)=0;task(1,func_08000C0C_72581C);
  fail_birth=1;assert(!anchor_world_gate64_apply(tasks[0],r));fail_birth=0;
  assert(!anchor_world_gate64_apply(tasks[0],r));assert(births==2);
  func_08000B9C_7257AC(tasks[2],objects[2]);assert(anchor_world_gate64_apply(tasks[0],r));
  assert(births==2&&GP(tasks[0],0xd4)==tasks[2]);
  anchor_world_gate64_reset(0);
  for(int i=0;i<100;++i){anchor_world_gate64_control(0,0,0,0);step();}
  assert(save_a1&&!cutscenes&&!locks);
  row(r);assert(r[25]);bind(0,0);bind(1,0);memset(r,0,sizeof(r));r[1]=0x325;r[2]=10;
  assert(anchor_world_gate64_capture(0,r)&&r[25]&&anchor_world_row_valid(r));
  assert(anchor_world_gate64_apply(0,r));
}
static void paused_and_bad_float(void){
  fixture();int r[50];row(r);r[12]=6;r[11]=10;r[10]=2;assert(anchor_world_gate64_apply(tasks[0],r));
  anchor_world_gate64_control(tasks[0],1,1,1);float y=GF(objects[0],12);step();assert(GF(objects[0],12)==y);
  GW(objects[0],8)=0x7fc00000u;assert(!anchor_world_gate64_capture(tasks[0],r));
  fixture();anchor_world_gate64_control(0,0,0,0);step();assert(phase()==1);
}
static void split_boundary_and_rejected_checkpoint(void){
  fixture();int r[50];row(r);r[12]=7;r[11]=30;r[10]=2;
  r[4]=r[6]=r[17]=r[19]=0;r[8]=r[21]=0;
  assert(anchor_world_gate64_apply(tasks[0],r));step();
  /* Native t=30 belongs to both motion intervals: three plus two units. */
  assert(GF(objects[0],16)==5&&GF(objects[1],16)==-5);
  assert(GH(objects[0],24)==5&&GH(objects[1],24)==1019);
  int bad[50];memcpy(bad,r,sizeof(bad));bad[11]=92;bad[4]=90000;
  assert(!anchor_world_gate64_apply(tasks[0],bad)&&GF(objects[0],8)==0);
  memcpy(bad,r,sizeof(bad));bad[10]=1;
  assert(!anchor_world_gate64_apply(tasks[0],bad)&&gate_slot==2);
  memcpy(bad,r,sizeof(bad));bad[12]=3;bad[10]=1;bad[11]=61;
  assert(!anchor_world_gate64_apply(tasks[0],bad)&&phase()==7&&!locks);
  fixture();save_a1=1;row(r);
  assert(r[25]&&(GW(tasks[0],0x68)&2u)&&(GW(tasks[1],0x68)&2u)&&!locks);
}
int main(void){hits_and_camera();spectator_and_handoff();reconstruction_and_failure();paused_and_bad_float();split_boundary_and_rejected_checkpoint();puts("world gate64 native tests passed");return 0;}
