/* File64's two-piece weapon gate. Only its activating simulator owns a
 * camera. Replicas run the native motion equations against local model slots. */
#ifndef WORLD_GATE64_HOST_TEST
#include "modding.h"
#endif
#include "anchor_world.h"
#include "anchor_world_gate64.h"
#define GB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define GH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define GS(p,o) (*(short *)((char *)(p)+(o)))
#define GW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define GF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef GP
#define GP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef G64_DISABLED
#define G64_DISABLED 0x00800000ul
#endif
typedef void (*GateCallback)(void *,void *);
extern unsigned short D_800C7AB2;
extern unsigned char D_800C7AE2;
extern void *D_8016DAB4_16E6B4;
extern unsigned char D_8015C608_15D208[];
extern void *D_80236984_5F1E54[];
extern int func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern void func_80024038_24C38(int);
extern void func_80038BC8_397C8(unsigned int);
extern void func_80221F70_5DD440(void);
extern void func_80221EBC_5DD38C(void);
extern void func_80216E1C_5D22EC(void *,unsigned int);
extern void *func_80220410_5DB8E0(void *,GateCallback,float,float,float,unsigned short);
extern float func_80003E10_4A10(unsigned short);
extern float func_80003EA0_4AA0(unsigned short);
#define G64_DECLARE(f) extern void f(void *,void *);
G64_DECLARE(func_080001BC_724DCC)
G64_DECLARE(func_0800033C_724F4C)
G64_DECLARE(func_0800037C_724F8C)
G64_DECLARE(func_0800042C_72503C)
G64_DECLARE(func_08000AD8_7256E8)
G64_DECLARE(func_08000B9C_7257AC)
G64_DECLARE(func_08000C0C_72581C)
extern void *func_08000C18_725828(void *);
extern void *func_08000DE4_7259F4(void *);
typedef struct {
  void *actor,*object;
  unsigned char generation,ready;
} GateNode;
static GateNode gate_nodes[2];
static GateCallback gate_real;
static GateCallback volatile gate_callbacks[5];
static void addresses(void) {
  /* Runtime stores prevent LLVM from emitting a static table of relocatable
   * overlay imports, which RecompModTool cannot resolve. */
  gate_callbacks[0]=func_080001BC_724DCC;gate_callbacks[1]=func_0800033C_724F4C;
  gate_callbacks[2]=func_0800037C_724F8C;gate_callbacks[3]=func_08000AD8_7256E8;
  gate_callbacks[4]=func_0800042C_72503C;
}
static int gate_enabled,gate_remote,gate_paused,gate_confirmed;
static int gate_complete,gate_restore,gate_observed,gate_local_hit,gate_scene;
static int gate_proxy,gate_slot=1,gate_fx_rise,gate_fx_split;
static int gate_suspend;
static unsigned int gate_post_flags[2];
static float gate_post_velocity[2][3];
static int gate_post_index=-1;
static void gate_callback(void *,void *);
static int same(unsigned int i) {
  GateNode *n=&gate_nodes[i];
  return n->actor && GP(n->actor,0x18)==n->object && n->object &&
      GB(n->actor,0x74)==n->generation &&
      (i || GH(n->actor,0x5c)==WORLD_GATE64_ENTITY);
}
static int live(unsigned int i) {
  return same(i) && gate_nodes[i].ready && !(GW(gate_nodes[i].actor,0x68)&2u);
}
static GateCallback raw(void *a) {
  return (GateCallback)((unsigned long)GP(a,0xc)&~G64_DISABLED);
}
static void bind(unsigned int i,void *a) {
  GateNode *n=&gate_nodes[i];n->actor=a;n->object=a?GP(a,0x18):0;
  n->generation=a?GB(a,0x74):0;n->ready=0;
}
static void install(void) {
  if (live(0)) GP(gate_nodes[0].actor,0xc)=(void *)((unsigned long)gate_callback |
      ((unsigned long)GP(gate_nodes[0].actor,0xc)&G64_DISABLED));
}
static int phase(void) {
  if (gate_complete) return 10;
  if (!same(0)) return 0;
  GateCallback c=gate_real;
  if (raw(gate_nodes[0].actor)!=gate_callback) c=raw(gate_nodes[0].actor);
  if (c==func_080001BC_724DCC) return 1;
  if (c==func_0800033C_724F4C) return 2;
  if (c==func_0800037C_724F8C) return 3;
  if (c==func_08000AD8_7256E8) return 9;
  if (c!=func_0800042C_72503C) return 0;
  unsigned int p=GB(gate_nodes[0].actor,0xd0);
  return p==0?4:p==2?5:p==1?6:p==3?7:p==4?8:0;
}
static void set_phase(int p) {
  void *a=gate_nodes[0].actor;
  addresses();gate_real=gate_callbacks[p==1?0:p==2?1:p==3?2:p==9?3:4];
  if (p>=4&&p<=8) GB(a,0xd0)=(unsigned char)(p==4?0:p==5?2:p==6?1:p==7?3:4);
  install();
}
void anchor_world_gate64_reset(int room_changed) {
  if (!gate_scene&&phase()>=3&&phase()<10) gate_proxy=1;
  if (phase()<3) gate_proxy=0;
  if (same(0)&&raw(gate_nodes[0].actor)==gate_callback) {
    /* A spectator has no camera handle. Never unwrap it into the native
     * camera continuation on disconnect or a roster rebuild. */
    if (!gate_proxy||gate_scene) GP(gate_nodes[0].actor,0xc)=(void *)gate_real;
    else if (room_changed) GP(gate_nodes[0].actor,0xc)=(void *)func_08000C0C_72581C;
  }
  gate_enabled=gate_remote=gate_paused=gate_confirmed=gate_restore=gate_suspend=0;
  if (room_changed) {
    bind(0,0);bind(1,0);gate_real=0;gate_complete=gate_observed=0;
    gate_local_hit=gate_scene=gate_proxy=gate_fx_rise=gate_fx_split=0;gate_slot=1;
  }
}
void anchor_world_gate64_register(void *a) {
  if (D_800C7AB2!=WORLD_GATE64_ROOM||!a) return;
  if (same(0)&&gate_nodes[0].actor!=a&&raw(gate_nodes[0].actor)==gate_callback)
    GP(gate_nodes[0].actor,0xc)=(void *)(gate_scene?gate_real:func_08000C0C_72581C);
  bind(0,a);bind(1,0);gate_real=raw(a);gate_restore=gate_observed;
  gate_local_hit=gate_scene=gate_proxy=gate_fx_rise=gate_fx_split=0;gate_slot=1;
  gate_complete=!!func_800240DC_24CDC(0xa1);
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_gate64_reuse(void *a) {
  for (unsigned int i=0;i<2;++i) if (gate_nodes[i].actor==a) {
    if (!i) {
      if (same(1)) GW(gate_nodes[1].actor,0x68)|=2u;
      bind(1,0);gate_real=0;
    } else if (same(0)&&GP(gate_nodes[0].actor,0xd4)==a) {
      GP(gate_nodes[0].actor,0xd4)=0;
    }
    bind(i,0);if(gate_observed&&!gate_complete) gate_restore=1;
  }
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_world_gate64_delete(void) {
  if (D_800C7AB2==WORLD_GATE64_ROOM&&D_8016DAB4_16E6B4==gate_nodes[0].actor) {
    if(func_800240DC_24CDC(0xa1)) gate_complete=1;
    gate_nodes[0].ready=0;gate_nodes[1].ready=0;
  }
}
RECOMP_HOOK("func_80216CE0_5D21B0")
void anchor_world_gate64_model(void *a,void *o,unsigned int slot) {
  (void)o;
  if (a==gate_nodes[0].actor&&(slot==1||slot==2)) gate_slot=(int)slot;
}
static int resident(void) {
  if(func_800141C4_14DC4(64)==-1||func_800141C4_14DC4(0x152)==-1) return 0;
  void **m=D_80236984_5F1E54[0x24f];
  if(!m||!m[0]||!m[1]) return 0;
  const unsigned short *files=m[0];const unsigned int *slots=m[1];
  if(func_800141C4_14DC4(files[0])==-1||func_800141C4_14DC4(files[1])==-1) return 0;
  return slots[1]&&slots[2]&&slots[3];
}
static int prepare(void) {
  if(!live(0)||!resident()) return 0;
  void *a=gate_nodes[0].actor,*child=GP(a,0xd4);
  if(child&&(!same(1)||gate_nodes[1].actor!=child)) {
    /* Admit only the constructor's own child continuation. A stale pointer
     * in the parent cannot claim a reused task slot. */
    GateCallback c=raw(child);
    if(c==func_08000B9C_7257AC||c==func_08000C0C_72581C) bind(1,child);
  }
  if(same(1)&&!(GW(gate_nodes[1].actor,0x68)&2u)) {
    if(raw(gate_nodes[1].actor)==func_08000C0C_72581C&&GH(gate_nodes[1].actor,0x5e)==0x24f)
      gate_nodes[1].ready=1;
    return live(1);
  }
  void *o=GP(a,0x18);
  child=func_80220410_5DB8E0(a,func_08000B9C_7257AC,GF(o,8),GF(o,12),GF(o,16),GH(o,22));
  if(!child) return 0;
  GH(child,0x28)=64;GW(child,0x2c)=(unsigned int)func_800141C4_14DC4(64);
  GP(a,0xd4)=child;bind(1,child);return 0; /* Native initializer runs once. */
}
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_gate64_post(void *a) {
  gate_post_index=-1;
  if(D_800C7AB2!=WORLD_GATE64_ROOM) return;
  if(a==gate_nodes[0].actor&&!gate_nodes[0].ready&&GH(a,0x5c)==WORLD_GATE64_ENTITY&&GP(a,0x18)) {
    gate_nodes[0].object=GP(a,0x18);gate_nodes[0].generation=GB(a,0x74);gate_real=raw(a);
  }
  if(same(0)&&a==gate_nodes[0].actor) {
    if(phase()) { gate_nodes[0].ready=1;if(raw(a)!=gate_callback) gate_real=raw(a); }
    if(!gate_enabled&&phase()>=3&&phase()<10&&!gate_proxy) gate_scene=1;
  }
  if(!gate_suspend&&(!gate_enabled||(!gate_restore&&!gate_paused))) return;
  for(unsigned int i=0;i<2;++i) if(live(i)&&gate_nodes[i].actor==a) {
    gate_post_index=(int)i;gate_post_flags[i]=GW(a,0x60);GW(a,0x60)&=~0x800301u;
    for(unsigned int j=0;j<3;++j) { gate_post_velocity[i][j]=GF(a,0x78+j*4);GF(a,0x78+j*4)=0; }
  }
}
RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_gate64_post_return(void) {
  int i=gate_post_index;gate_post_index=-1;
  if(i<0||!live((unsigned int)i)) return;
  void *a=gate_nodes[i].actor;GW(a,0x60)=gate_post_flags[i];
  for(unsigned int j=0;j<3;++j) GF(a,0x78+j*4)=gate_post_velocity[i][j];
}
static int quantize(float f,int *out) {
  volatile union {float f;unsigned int u;} v;v.f=f;
  if((v.u&0x7f800000u)==0x7f800000u) return 0;
  f*=100;if(!(f>=-3276800&&f<=3276700)) return 0;*out=(int)f;return 1;
}
int anchor_world_gate64_capture(void *a,int *r) {
  if(D_800C7AB2!=WORLD_GATE64_ROOM) return 0;
  if(gate_complete||(!gate_scene&&func_800240DC_24CDC(0xa1))) {
    /* Canonical removal: model/scale are schema placeholders, never an
     * instruction to bind geometry. Completion applies before task access. */
    r[10]=2;r[12]=10;r[23]=150;r[25]=1;gate_complete=1;gate_observed=1;
    /* Durable completion can arrive after an idle copy was instantiated.
     * An active local camera must instead finish its own native cleanup. */
    for(unsigned int i=0;i<2;++i) if(same(i)) GW(gate_nodes[i].actor,0x68)|=2u;
    return 1;
  }
  if(a!=gate_nodes[0].actor||gate_restore||!live(0)||!live(1)||!phase()||!resident()||
      (gate_enabled&&gate_suspend)) return 0;
  r[3]=gate_scene;r[10]=gate_slot;r[11]=GS(a,0x8a);r[12]=phase();
  r[13]=!(GW(a,0x60)&0x800000u);r[23]=150;
  r[24]=!!(GW(gate_nodes[1].actor,0x60)&0x20u);
  for(unsigned int i=0;i<2;++i) {
    void *o=gate_nodes[i].object;unsigned int k=i?17:4,ang=i?20:7;
    for(unsigned int j=0;j<3;++j) {
      if(!quantize(GF(o,8+j*4),&r[k+j])) return 0;
      r[ang+j]=GH(o,0x14+j*2)&1023;
    }
  }
  gate_observed=1;return 1;
}
int anchor_world_gate64_needs_restore(void) {return gate_restore;}
int anchor_world_gate64_apply(void *a,const int *r) {
  if(D_800C7AB2!=WORLD_GATE64_ROOM||r[2]!=WORLD_GATE64||!anchor_world_row_valid(r)) return 0;
  /* A local sequence must release its own camera and control. Arbitration
   * keeps it authoritative; never acknowledge replacing an active camera. */
  if(gate_scene&&phase()<10) return 0;
  if(r[25]) {
    func_80024038_24C38(0xa1);gate_complete=1;
    for(unsigned int i=0;i<2;++i) if(same(i)) GW(gate_nodes[i].actor,0x68)|=2u;
    gate_restore=0;gate_observed=1;return 1;
  }
  int p=r[WG64_PHASE],timer=r[WG64_TIMER];
  if((p==2&&(timer<0||timer>7))||(p==3&&(timer<0||timer>60))||
     (p>=4&&timer<0)||(p==5&&(timer<1||timer>61))||
     (p==6&&(timer<1||timer>49))||(p==7&&(timer<1||timer>91))||
     (p<=5&&r[10]!=1)||((p>=7||(p==6&&timer>1))&&r[10]!=2)) return 0;
  if(a!=gate_nodes[0].actor||!prepare()) return 0;
  if(gate_slot!=r[10]) {func_80216E1C_5D22EC(a,(unsigned int)r[10]);gate_slot=r[10];}
  for(unsigned int i=0;i<2;++i) {
    void *o=gate_nodes[i].object,*task=gate_nodes[i].actor;unsigned int k=i?17:4,ang=i?20:7;
    for(unsigned int j=0;j<3;++j) {
      GF(o,8+j*4)=(float)r[k+j]/100;GH(o,0x14+j*2)=(unsigned short)r[ang+j];
      GF(o,0x1c+j*4)=0.15f;GF(task,0x78+j*4)=0;
    }
  }
  GW(a,0x60)=0x82a00fe1u & ~(r[13]?0x800000u:0u);GW(a,0x68)&=~1u;
  void *child=gate_nodes[1].actor;GW(child,0x60)=(GW(child,0x60)&~0x20u)|((unsigned int)r[24]<<5);
  GS(a,0x8a)=(short)r[11];gate_proxy=1;gate_local_hit=0;set_phase(r[12]);
  if(r[12]>6||(r[12]==6&&r[11]>1)) gate_fx_rise=1;
  if(r[12]>7||(r[12]==7&&r[11]>20)) gate_fx_split=1;
  gate_restore=0;gate_observed=1;return 1;
}
void anchor_world_gate64_control(void *a,int remote,int paused,int confirmed) {
  int enabled=D_800C7AB2==WORLD_GATE64_ROOM&&a&&a==gate_nodes[0].actor;
  /* The scheduler clears control before its pause/active checks. Preserve a
   * paused session's suspension, but do not re-suspend a disconnected proxy
   * after reset(0) released it to finish without a camera. */
  gate_suspend=enabled?0:(gate_enabled||gate_suspend);
  gate_enabled=enabled;
  gate_remote=remote;gate_paused=paused;gate_confirmed=confirmed;
}
void anchor_world_gate64_begin(void) {
  if(!gate_enabled||gate_complete) return;
  if(!prepare()) {gate_suspend=1;return;}
  if(raw(gate_nodes[0].actor)!=gate_callback) gate_real=raw(gate_nodes[0].actor);
  install();
}
static void native_call(GateCallback f,void *a,void *o) {
  GP(a,0xc)=(void *)f;f(a,o);
  if(same(0)&&gate_nodes[0].ready) {gate_real=raw(a);install();}
}
static void hit(void *a) {
  if(!(GW(a,0x68)&1u)) return;
  GB(a,0x8d)=30;void *contact=GP(a,0x38);if(!contact) return;
  unsigned int type=GB(contact,0x4c);int powered=0;
  for(unsigned int j=0;j<4;++j) {
    unsigned int v=GW(D_8015C608_15D208,0xa4+j*4);if(v==1||v==2) powered=1;
  }
  func_80038BC8_397C8(0x24f);GW(a,0x68)&=~1u;
  if(powered&&(type==0x16||type==0x17||type==0x23||type==0x24)) {
    func_80038BC8_397C8(0x119);GS(a,0x8a)=60;gate_local_hit=1;set_phase(3);
  } else {GS(a,0x8a)=7;set_phase(2);}
}
static void gate_callback(void *a,void *o) {
  if(!live(0)||a!=gate_nodes[0].actor||D_800C7AB2!=WORLD_GATE64_ROOM) return;
  int p=phase();if(!p||p==10) return;
  if(gate_suspend) return;
  if(gate_scene) {native_call(gate_real,a,o);return;}
  if(gate_enabled&&(gate_restore||gate_paused)) return;
  if(gate_enabled&&!gate_confirmed&&p>=3) return;
  if(!live(1)) {if(!prepare()) return;}
  if(p==1) {hit(a);return;}
  int t=GS(a,0x8a);
  if(p==2) {GS(a,0x8a)=(short)(t-1);if(!t)set_phase(1);return;}
  if(p==3) {
    if(!t&&gate_enabled&&!gate_remote&&gate_confirmed&&gate_local_hit) {
      /* Keep the native 60-update delay available for competing hit claims.
       * Acquire local controls only when the confirmed winner needs its camera. */
      if(D_800C7AE2) return; /* Another local sequence acquired controls meanwhile. */
      func_80221F70_5DD440();func_80221EBC_5DD38C();gate_scene=1;
      native_call(gate_real,a,o);return;
    }
    /* A pending local hit first reaches transport arbitration. */
    if(gate_enabled&&!gate_confirmed) return;
    if(!t&&gate_enabled&&gate_remote) return;
    GS(a,0x8a)=(short)(t-1);if(!t){GS(a,0x8a)=0;set_phase(4);}return;
  }
  gate_proxy=1;
  if(p==4) {if(!gate_enabled||!gate_remote){GS(a,0x8a)=1;set_phase(5);}return;}
  if(p>=8) {
    if(!gate_enabled||!gate_remote) {
      func_80024038_24C38(0xa1);gate_complete=1;
      for(unsigned int i=0;i<2;++i) if(same(i)) GW(gate_nodes[i].actor,0x68)|=2u;
    }
    return;
  }
  void *child=gate_nodes[1].actor,*co=gate_nodes[1].object;
  if(gate_enabled&&gate_remote&&((p==5&&t>60)||(p==6&&t>48)||(p==7&&t>90))) return;
  if(p==5) {
    if(t>60){GS(a,0x8a)=1;set_phase(6);return;}
    GW(a,0x60)&=~0x800000u;
    if(t==1)func_80038BC8_397C8(0x251);
    if(!(t&3)){GF(o,8)+=1;GF(co,8)=GF(o,8);}
    else if((t&3)==2){GF(o,8)-=1;GF(co,8)=GF(o,8);}
    if(t==60){func_80038BC8_397C8(0x8251);if(!gate_enabled||!gate_remote){t=0;set_phase(6);}}
  } else if(p==6) {
    if(t>48){GS(a,0x8a)=1;set_phase(7);return;}
    if(t==1) {
      if(gate_slot!=2){func_80216E1C_5D22EC(a,2);gate_slot=2;}
      for(unsigned int j=0;j<3;++j)GF(o,0x1c+j*4)=0.15f;
      GW(child,0x60)|=0x20u;
      if(!gate_fx_rise){func_08000DE4_7259F4(a);gate_fx_rise=1;func_80038BC8_397C8(0x13c);}
    }
    GF(o,12)+=20;GF(co,12)=GF(o,12);GH(o,22)=(GH(o,22)+64)&1023;GH(co,22)=GH(o,22);
    if(t==48&&(!gate_enabled||!gate_remote)){t=0;set_phase(7);}
  } else if(p==7) {
    if(t>90){GS(a,0x8a)=1;set_phase(8);return;}
    if(t==20&&!gate_fx_split) {
      void *fx=func_08000C18_725828(a);
      if(!fx) {GS(a,0x8a)=(short)(t+1);return;}
      void *fo=GP(fx,0x18);if(fo){GF(fo,12)-=20;GF(fo,8)-=20;}
      gate_fx_split=1;func_80038BC8_397C8(0x13d);
    }
    if(t>19) {
      float step=(t<31?3.0f:0.0f)+(t>29?2.0f:0.0f);
      GF(o,8)+=func_80003E10_4A10(GH(o,22))*step;
      GF(o,16)+=func_80003EA0_4AA0(GH(o,22))*step;
      GF(co,8)-=func_80003E10_4A10(GH(co,22))*step;
      GF(co,16)-=func_80003EA0_4AA0(GH(co,22))*step;
      if(t>29){GH(o,24)=(GH(o,24)+5)&1023;GH(co,24)=(GH(co,24)-5)&1023;}
    }
    if(t==90&&(!gate_enabled||!gate_remote)){t=0;set_phase(8);}
  }
  GS(a,0x8a)=(short)(t+1);
}
