/* File_40 0x1A9: seven placed "falling bomb block" roots and their twelve
 * transient explosion children. A placed root arms on player proximity, falls,
 * ramps its fuse tint and then commits exactly one explosion; four spinning
 * rings (variants 1/2/4/8, of which 4 and 8 are registered attackers) and
 * eight particles are the children.
 *
 * The root never replays its native proximity AI: the shared phase plus the
 * aggregated activation word drive both clients, and only the client the
 * bridge names as committer runs func_080006D4. The children run the verified
 * pure alpha/scale/yaw/timer arithmetic on every copy, but only the owner
 * commits an expiry. Nothing here awards loot. */
#ifndef ANCHOR_WORLD_BOMB_INC
#define ANCHOR_WORLD_BOMB_INC

#include "anchor_world_bomb.h"

extern void func_0800042C_6E957C(void *, void *);
extern void func_08000508_6E9658(void *, void *);
extern void func_08000560_6E96B0(void *, void *);
extern void func_080005F8_6E9748(void *, void *);
extern void func_080006D4_6E9824(void *, void *);
extern void func_08000950_6E9AA0(void *, void *);
extern void func_08000984_6E9AD4(void *, void *);
extern void func_08000AD0_6E9C20(void *, void *);
extern void func_08000C74_6E9DC4(void *, void *);
extern void func_08000E24_6E9F74(void *, void *);
/* Verified ABI: (actor, alpha, r, g, b) returning the procedural display-list
 * pointer at object+0x80. No current-task dependency. */
extern void *func_8021DF60_5D9430(void *, unsigned int, unsigned int,
                                  unsigned int, unsigned int);
extern void *func_8021A26C_5D573C(void *, unsigned int, unsigned int,
                                  unsigned int, unsigned int);
extern void *func_8021DD4C_5D921C(void *, unsigned int, unsigned int,
                                  unsigned int, unsigned int);
/* Verified prototype shared with the RNG harness. */
extern void func_80218DA8_5D4278(void *, int, unsigned short, short);
extern void func_80216E1C_5D22EC(void *, unsigned int);
/* The native sound argument is the ADDRESS 0x8020CBF0, not its contents. */
extern void func_8000F420_10020(unsigned int, unsigned char *, void *, float);
extern int func_8021B7AC_5D6C7C(void *, float);
extern void func_80034ED4_35AD4(void);
extern unsigned char D_8020CBF0_5C8B00[];

static void bomb_native(void *, void *);

/* Role -> caller-assigned +0xE8 variant bit, in the order of the six native
 * func_802171A8 calls inside func_080006D4. */
static const int bomb_role_variant[5] = {0, 1, 4, 2, 8};

static DynamicCallback volatile d_bomb_phases[6];
static DynamicCallback bomb_phase(int phase) {
  /* Volatile stores prevent LLVM from converting this selection into a
   * static pointer table. Overlay callbacks require runtime relocations. */
  d_bomb_phases[0]=func_08000508_6E9658;
  d_bomb_phases[1]=func_08000560_6E96B0;
  d_bomb_phases[2]=func_080005F8_6E9748;
  d_bomb_phases[3]=func_080006D4_6E9824;
  d_bomb_phases[4]=func_08000AD0_6E9C20;
  d_bomb_phases[5]=func_08000E24_6E9F74;
  return d_bomb_phases[phase>=WB_IDLE && phase<=WB_PARTICLE
                           ? phase-WB_IDLE
                           : WB_EXPLODE-WB_IDLE];
}

/* XZ proximity to the local target at task+0x84. Native func_8021B7AC returns
 * non-zero while the target is inside the radius; a missing target is never
 * dereferenced. */
static int bomb_near(void *a, float radius) {
  return DPTR(a,0x84) && func_8021B7AC_5D6C7C(a,radius)!=0;
}

/* The native word is the helper's display-list pointer OR'd with its tag bit:
 * 0x20000000 for the root and the rings, 0x60000000 for the particles. */
static void bomb_dl(void *o, void *dl, unsigned int tag) {
  unsigned long p=(unsigned long)dl;
  if (!p) return;
  DPTR(o,0x30)=(void *)(p|(unsigned long)tag);
}

static int bomb_scope(const int *r) {
  int xyz[3];
  if (!anchor_world_bomb_placed(D_800C7AB2,(unsigned int)r[WD_PARENT],xyz))
    return 0;
  for (unsigned int j=0;j<3;++j)
    if (r[WD_BIRTH_X+j]!=xyz[j]) return 0;
  return 1;
}

/* Pinned resource identity. Both descriptors are read from the main ROM model
 * table, so a row can only bind the verified bomb model and slot commands:
 *   model 0x1A9 -> files {0x1DE,0x160}, clip slot 0 = 0x08000198
 *   model 0x7E  -> files {0x191,0x0000}, clip slots 0..8 as listed below
 * The File_40 overlay and the common 0x152 sentinel must also be resident. */
static int bomb_resource_valid(const int *r) {
  static const unsigned int child_clips[9] = {
    0x08000100u, 0x080002c0u, 0x08000500u, 0x08000610u, 0x080006f0u,
    0x080007d0u, 0x080008b0u, 0x08000990u, 0x08000b60u};
  void *model;
  const unsigned short *files;
  const unsigned int *clips;
  unsigned int i;
  if (!anchor_world_bomb_valid(r) || !bomb_scope(r)) return 0;
  if (func_800141C4_14DC4(0x28)==-1 || func_800141C4_14DC4(0x152)==-1)
    return 0;
  model=D_80236984_5F1E54[r[WD_MODEL]];
  if (!model) return 0;
  files=(const unsigned short *)DPTR(model,0);
  clips=(const unsigned int *)DPTR(model,4);
  if (!files || !clips) return 0;
  if (!r[WB_ROLE]) {
    if (files[0]!=0x1de || files[1]!=0x160 || clips[0]!=0x08000198u)
      return 0;
  } else {
    if (files[0]!=0x191 || files[1]!=0x0000) return 0;
    for (i=0;i<9;++i)
      if (clips[i]!=child_clips[i]) return 0;
  }
  if (files[0] && func_800141C4_14DC4(files[0])==-1) return 0;
  if (files[1] && func_800141C4_14DC4(files[1])==-1) return 0;
  return 1;
}

/* Typed child identity. Birth coordinates come from the immutable room/parent
 * placement, never from a root record, so a late entrant reconstructs a child
 * even when no root copy is live. */
static void bomb_child_establish(DynamicActor *d, void *a) {
  d->kind=WORLD_BOMB;
  d->bomb_birth=1;
  d->generation=B(a,0x74);
  d->row[WD_KIND]=WORLD_BOMB;
  d->row[WD_ENTITY]=WORLD_BOMB_ENTITY;
  d->row[WD_MODEL]=WORLD_BOMB_CHILD_MODEL;
  d->row[WD_PARENT]=(int)d->parent;
  d->row[WB_UNUSED]=0;
  d->row[WD_ROUTE]=163;
  d->native=(void *)bomb_phase(WB_RING);
}

/* A placed root is identified by its live continuation and by the verified
 * room/index placement, exactly like the Slicer emitter and RNG spawner. */
static DynamicActor *bomb_register_root(void *a) {
  int xyz[3];
  unsigned int parent=0, i;
  DynamicActor *d=0;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=WORLD_BOMB_ENTITY ||
      !anchor_world_actor_placed(a) ||
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)!=(unsigned long)func_08000508_6E9658)
    return 0;
  anchor_world_actor_authority(a,&parent);
  if (!anchor_world_bomb_placed(D_800C7AB2,parent,xyz)) return 0;
  /* The live X/Z must still name the placement; Y may already have fallen. */
  for (i=0;i<3;++i) {
    int q;
    if (!quantize(F(DPTR(a,0x18),8+i*4),100,-3276800,3276700,&q)) return 0;
    if (i!=1 && q!=xyz[i]) return 0;
  }
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *old=&d_actors[i];
    if (old->used && old->kind==WORLD_BOMB && !old->row[WB_ROLE] &&
        old->parent==(unsigned short)parent) {
      if (alive(old)) return 0;
      d=old;break;
    }
  }
  if (!d) d=allocate(a);
  else {
    d->actor=a;
    if (++d_serial>0x7fffffffu) d_serial=1;
    d->serial=d_serial;
    d->dirty=d->have=0;d->saved=0;d->owner=0;d->proxy=0;
    d->bomb_initialized=d->bomb_frozen=0;d->bomb_particles=0;
    clear(d->net,sizeof(d->net));
    /* Claims and tombstones belong to the room identity. A fresh native
     * incarnation must not undo a hit accepted before a proximity unload. */
    d->claimed=d->row[WD_LIFE]==WD_CLAIM;
  }
  if (!d) return 0;
  d->kind=WORLD_BOMB;
  d->placed=d->ready=d->eligible=d->bomb_birth=1;
  /* The placement code already ran the native root initializer, so the local
   * bindings exist before the first apply. */
  d->bomb_initialized=1;
  d->parent=(unsigned short)parent;
  d->generation=B(a,0x74);
  /* The placed root is driven by the pure phase machine, never by the native
   * proximity AI, so its recorded continuation is the bounded replacement. */
  d->native=(void *)bomb_native;
  d->row[WD_KIND]=WORLD_BOMB;
  d->row[WD_ENTITY]=d->row[WD_MODEL]=WORLD_BOMB_ENTITY;
  d->row[WB_ROLE]=WB_ROLE_ROOT;d->row[WD_ORDINAL]=0;
  d->row[WD_PARENT]=(int)parent;d->row[WB_INSTANCE]=(int)d->serial;
  d->row[WB_RECEIPT]=d->row[WB_ESTABLISHED]=0;d->row[WB_PRESENT]=1;
  if (d->row[WD_PHASE]<WB_IDLE || d->row[WD_PHASE]>WB_EXPLODE)
    d->row[WD_PHASE]=WB_IDLE;
  if (d->row[WB_ALPHA]<0 || d->row[WB_ALPHA]>WB_FUSE_CAP) d->row[WB_ALPHA]=0;
  if (d->row[WB_CAUSE]<WB_CAUSE_NONE || d->row[WB_CAUSE]>WB_CAUSE_FUSE)
    d->row[WB_CAUSE]=WB_CAUSE_NONE;
  /* A killed root keeps its room tombstone across a proximity cull. */
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  return d;
}

/* Register a locally created explosion child. The common child hook runs
 * before the caller ORs +0xE8, so a null callback only records the birth and
 * the post/initializer pass supplies the real continuation. */
static DynamicActor *bomb_child(DynamicActor *root, DynamicActor *child,
                                DynamicCallback callback) {
  unsigned int role;
  if (!root || !child || root->kind!=WORLD_BOMB || root->row[WB_ROLE] ||
      !child->bomb_birth)
    return 0;
  if (!callback) {
    child->parent=root->parent;
    child->row[WD_PARENT]=(int)root->parent;
    child->row[WD_KIND]=WORLD_BOMB;
    child->row[WD_ENTITY]=WORLD_BOMB_ENTITY;
    return child;
  }
  if (!child->actor || !DPTR(child->actor,0x18)) return 0;
  if (callback==func_08000984_6E9AD4) {
    unsigned int variant=W(child->actor,0xe8)&0xfu;
    if (variant==1) role=WB_ROLE_V1;
    else if (variant==4) role=WB_ROLE_V4;
    else if (variant==2) role=WB_ROLE_V2;
    else if (variant==8) role=WB_ROLE_V8;
    else return 0;
  } else if (callback==func_08000C74_6E9DC4) {
    /* Particles take the next role 5..12 in birth order. Only the single
     * committer explodes, so the ordinal is stable for this root. */
    if (root->bomb_particles>=WB_PARTICLES) return 0;
    role=WB_ROLE_PARTICLE+root->bomb_particles++;
  } else return 0;
  bomb_child_establish(child,child->actor);
  child->ready=child->eligible=1;
  child->placed=0;
  child->owner=d_self;
  child->row[WB_ROLE]=(int)role;
  child->row[WD_ORDINAL]=(int)role;
  child->row[WB_VARIANT]=role<=WB_ROLE_V8 ? bomb_role_variant[role] : 0;
  child->row[WB_INSTANCE]=(int)child->serial;
  child->row[WB_ESTABLISHED]=0;child->row[WB_PRESENT]=1;
  child->row[WB_CAUSE]=WB_CAUSE_NONE;
  child->row[WB_ALPHA]=0xff;
  child->row[WD_PHASE]=role<=WB_ROLE_V8 ? WB_RING : WB_PARTICLE;
  child->row[WD_LIFE]=WD_LIVE;
  child->row[WD_LANDED]=0;child->row[WD_BASE_Y]=0;
  return child;
}

/* Fill the bomb words for one row. Self-contained: the caller routes a
 * WORLD_BOMB actor here instead of the generic child capture. */
static int bomb_capture(DynamicActor *d) {
  int *r=d->row;
  void *a=d->actor, *o;
  int role=r[WB_ROLE];
  unsigned int j;
  if (d->kind!=WORLD_BOMB) return 0;
  /* A durable tombstone and a frozen hold both keep the retained checkpoint:
   * a removal is never rewritten back to live, and the zero velocity written
   * only to suppress the local post is never published. */
  if (r[WD_LIFE]==WD_REMOVED) {
    /* A durable tombstone keeps its words; only presence is refreshed. */
    r[WB_PRESENT]=alive(d)!=0;
    return anchor_world_bomb_valid(r) && bomb_scope(r);
  }
  if (d->bomb_frozen) {
    /* A frozen hold still publishes the live claim, the pause state, the
     * incarnation and the root's local proximity input, but never overwrites
     * the retained physical pose, flags or velocity with the zeros this
     * adapter wrote only to suppress the local post. */
    r[WD_LIFE]=d->claimed ? WD_CLAIM : WD_LIVE;
    r[WD_LANDED]=d->claimed!=0;
    r[WD_PAUSED]=anchor_world_is_paused()!=0;
    r[WB_INSTANCE]=(int)d->serial;
    r[WB_PRESENT]=alive(d)!=0;
    if (!role && alive(d)) r[WD_BASE_Y]=bomb_near(a,WB_PROXIMITY_ARM);
    return anchor_world_bomb_valid(r) && bomb_scope(r);
  }
  r[WD_KIND]=WORLD_BOMB;
  r[WD_PARENT]=d->parent;
  r[WD_OWNER]=(int)d->owner;
  r[WD_LIFE]=d->claimed ? WD_CLAIM : WD_LIVE;
  r[WD_ENTITY]=WORLD_BOMB_ENTITY;
  r[WD_ROUTE]=163;
  r[WD_ORDINAL]=role;
  r[WB_INSTANCE]=(int)d->serial;
  r[WB_PRESENT]=alive(d)!=0;
  r[WB_UNUSED]=0;
  r[WD_LANDED]=d->claimed!=0;
  r[WD_PAUSED]=anchor_world_is_paused()!=0;
  r[WD_TALKABLE]=r[WD_DIALOG]=r[WD_BUSY]=0;
  r[WD_ANIMATED]=r[WD_FRAME]=r[WD_RATE]=r[WD_ANIM_FLAGS]=0;
  for (j=WD_PATH_TIMER;j<=WD_PATH_PC;++j) r[j]=0;
  r[WD_RADIUS]=WB_ROOT_RADIUS;r[WD_HEIGHT]=WB_ROOT_HEIGHT;r[WD_OFFSET]=0;
  r[WD_ATTACK_HEIGHT]=r[WD_ATTACK_OFFSET]=0;
  /* Physical scalars are read live, never invented: the particles carry
   * gravity 15 at +0x75 and the root carries the native no-loot bit in its
   * +0x64 aux word, so a reconstruction must preserve both. */
  r[WD_GRAVITY]=B(a,0x75);
  r[WD_AUX_LO]=(int)(W(a,0x64)&65535u);
  r[WD_AUX_HI]=(int)(W(a,0x64)>>16);
  r[WD_MASK]=H(a,0x96);
  r[WD_MASK94]=H(a,0x94);
  r[WD_BODY_OFFSET2]=S(a,0x42);
  r[WD_SHADOW_SCALE]=B(a,0x6e);
  r[WD_SHADOW_OFFSET]=B(a,0x6f);
  for (j=0;j<4;++j) r[WD_DIM0+j]=B(a,0x98+j);
  { /* Immutable typed placement for both roles. */
    int xyz[3];
    if (!anchor_world_bomb_placed(D_800C7AB2,d->parent,xyz)) return 0;
    for (j=0;j<3;++j) r[WD_BIRTH_X+j]=xyz[j];
  }
  if (!role) {
    r[WD_MODEL]=WORLD_BOMB_ENTITY;
    r[WD_CLIP]=0;r[WB_VARIANT]=0;
    r[WD_ATTACK]=r[WD_ATTACK_RADIUS]=r[WD_SPHERE]=0;
    /* Word 45 is the activation input: the local proximity of any player,
     * which the bridge ORs across clients. */
    r[WD_BASE_Y]=alive(d) ? bomb_near(a,WB_PROXIMITY_ARM) : r[WD_BASE_Y];
    r[WD_TIMER]=0;
    r[WB_ALPHA]=alive(d) ? B(a,0xd0) : r[WB_ALPHA];
    if (r[WB_ALPHA]<0 || r[WB_ALPHA]>WB_FUSE_CAP) r[WB_ALPHA]=0;
    r[WD_BOUNCE]=0;
    r[WD_PHASE]=r[WD_PHASE]>=WB_IDLE && r[WD_PHASE]<=WB_EXPLODE
                    ? r[WD_PHASE] : WB_IDLE;
  } else {
    int attack=role==WB_ROLE_V4 || role==WB_ROLE_V8;
    r[WD_MODEL]=WORLD_BOMB_CHILD_MODEL;
    r[WD_CLIP]=role<=WB_ROLE_V8 ? 8 : 0;
    r[WB_VARIANT]=role<=WB_ROLE_V8 ? bomb_role_variant[role] : 0;
    r[WD_ATTACK]=attack ? WB_ATTACK_TYPE : 0;
    r[WD_ATTACK_RADIUS]=attack ? WB_ATTACK_RADIUS : 0;
    r[WD_SPHERE]=attack ? 17 : 0;
    r[WD_BASE_Y]=0;
    r[WD_TIMER]=alive(d) ? S(a,0x8a) : r[WD_TIMER];
    r[WB_ALPHA]=alive(d) ? B(a,0xd0) : r[WB_ALPHA];
    if (r[WB_ALPHA]<0 || r[WB_ALPHA]>255) r[WB_ALPHA]=0;
    r[WD_BOUNCE]=alive(d) ? B(a,0x8c) : r[WD_BOUNCE];
    if (r[WD_BOUNCE]<0 || r[WD_BOUNCE]>255) r[WD_BOUNCE]=0;
    r[WD_PHASE]=role<=WB_ROLE_V8 ? WB_RING : WB_PARTICLE;
  }
  if (!alive(d)) return anchor_world_bomb_valid(r) && bomb_scope(r);
  o=DPTR(a,0x18);
  for (j=0;j<3;++j) {
    if (!quantize(F(o,8+j*4),100,-3276800,3276700,&r[WD_X+j]) ||
        !quantize(F(a,0x78+j*4),1000,-100000,100000,&r[WD_VX+j]) ||
        !quantize(F(o,0x1c+j*4),1000,0,64000,&r[WD_SX+j]))
      return 0;
    r[WD_PITCH+j]=H(o,0x14+j*2)&1023;
  }
  if (!quantize(F(o,0x68),1000,0,100000,&r[WD_OBJECT_RADIUS]) ||
      !quantize(F(o,0x6c),1000,0,100000,&r[WD_OBJECT_HEIGHT]))
    return 0;
  r[WD_FLAGS_LO]=(int)(W(a,0x60)&65535u);
  r[WD_FLAGS_HI]=(int)(W(a,0x60)>>16);
  return anchor_world_bomb_valid(r) && bomb_scope(r);
}

/* Restore the native scalars a decoded row owns. The root keeps the native
 * +0x90 hit continuation so a deferred accepted hit still routes; the child
 * binds its verified static slot and colour helper exactly once. */
static int bomb_apply_extra(DynamicActor *d, const int *r) {
  void *a=d->actor, *o;
  int role=r[WB_ROLE];
  unsigned int j;
  if (!alive(d)) return 0;
  o=DPTR(a,0x18);
  if (!role) {
    unsigned int fuse=(unsigned int)r[WB_ALPHA];
    int hot=r[WD_PHASE]==WB_FUSE || r[WD_PHASE]==WB_EXPLODE;
    W(a,0x60)=(r[WD_PHASE]==WB_IDLE ? 0x002006e1u : 0x02a007e1u) |
              (W(a,0x60)&0x08000000u);
    /* Native no-loot bit; the generic aux bits come back from the row. */
    W(a,0x64)=(((unsigned int)r[WD_AUX_LO]|((unsigned int)r[WD_AUX_HI]<<16))&
               0x1022u)|0x8000u;
    B(a,0x8d)=WB_ROOT_HEALTH;
    B(a,0xd0)=(unsigned char)fuse;
    DPTR(a,0x90)=(void *)func_08000950_6E9AA0;
    d->bomb_initialized=1;
    bomb_dl(o,func_8021DF60_5D9430(a,fuse,hot?0xffu:0u,hot?0x20u:0u,0),
            0x20000000u);
  } else {
    int attack=role==WB_ROLE_V4 || role==WB_ROLE_V8;
    unsigned int alpha=(unsigned int)r[WB_ALPHA];
    W(a,0x60)=role<=WB_ROLE_V8 ? 0x004006e0u : 0x02c007e0u;
    W(a,0x64)=((unsigned int)r[WD_AUX_LO]|((unsigned int)r[WD_AUX_HI]<<16))&
              0x1022u;
    B(a,0x8d)=1;
    B(a,0xd0)=(unsigned char)alpha;
    W(a,0xe8)=(unsigned int)(role<=WB_ROLE_V8 ? bomb_role_variant[role] : 0);
    S(a,0x8a)=(short)r[WD_TIMER];
    B(a,0x4c)=(unsigned char)(attack ? WB_ATTACK_TYPE : 0);
    W(a,0x48)=attack ? 0xffffffffu : 0;
    H(a,0x4e)=(unsigned short)(attack ? WB_ATTACK_RADIUS : 0);
    H(a,0x50)=0;S(a,0x52)=0;
    DPTR(a,0x90)=0;
    B(a,0x75)=(unsigned char)r[WD_GRAVITY];
    H(a,0x96)=(unsigned short)r[WD_MASK];
    H(a,0x94)=(unsigned short)r[WD_MASK94];
    S(a,0x42)=(short)r[WD_BODY_OFFSET2];
    B(a,0x6e)=(unsigned char)r[WD_SHADOW_SCALE];
    B(a,0x6f)=(unsigned char)r[WD_SHADOW_OFFSET];
    for (j=0;j<4;++j) B(a,0x98+j)=(unsigned char)r[WD_DIM0+j];
    F(o,0x68)=(float)r[WD_OBJECT_RADIUS]/1000.0f;
    F(o,0x6c)=(float)r[WD_OBJECT_HEIGHT]/1000.0f;
    if (!d->bomb_initialized) {
      /* One static bind per local child. No randomised initializer runs on a
       * replica: the checkpoint already carries the scale, yaw and launch
       * state the owner chose. */
      d->bomb_initialized=1;
      func_80216DF8_5D22C8(a,role<=WB_ROLE_V8 ? 8u : 0u);
      if (role==WB_ROLE_V2 || role==WB_ROLE_V8) H(o,0x14)=0x200;
    }
    bomb_dl(o,attack ? func_8021DD4C_5D921C(a,alpha,0,0,0)
                     : func_8021A26C_5D573C(a,alpha,0,0,0),
            role<=WB_ROLE_V8 ? 0x20000000u : 0x60000000u);
    /* A reconstructed copy runs the bounded replacement, never the native
     * continuation that would retire a replica's task locally. */
    if (d->proxy) d->native=(void *)bomb_native;
  }
  for (j=0;j<3;++j) {
    F(o,8+j*4)=(float)r[WD_X+j]/100.0f;
    H(o,0x14+j*2)=(unsigned short)r[WD_PITCH+j];
    F(a,0x78+j*4)=(float)r[WD_VX+j]/1000.0f;
    F(o,0x1c+j*4)=(float)r[WD_SX+j]/1000.0f;
  }
  if (!role) H(o,0x14)=(unsigned short)r[WD_PITCH];
  return 1;
}

/* Record the shared claim. The committer later runs the native explosion; no
 * other copy fans out. */
static void bomb_claim(DynamicActor *d, int cause) {
  d->claimed=1;
  /* Owner and committer are transport decisions: the simulation owner is
   * independent of the sticky committer arbiter, so a claim records only the
   * shared state and never rewrites either field. */
  d->row[WD_LIFE]=WD_CLAIM;
  d->row[WB_CAUSE]=cause;
  d->row[WD_LANDED]=1;
  d->row[WD_PHASE]=WB_EXPLODE;
  capture(d);
}

/* Defer the accepted hit before native mutation. Clearing the native hit bits
 * makes func_80218350 no longer own this damage, and the surviving-hit
 * bookkeeping (status bit 1, immunity 60) matches the native path so the game
 * does not re-trigger. Every first accepted hit explodes the root: a one-point
 * hit survives to the custom continuation, and a lethal hit reaches the same
 * continuation because the bomb has no fragment bit. */
static void bomb_damage(void *actor) {
  DynamicActor *d=lookup(actor);
  unsigned int status;
  if (!d_active || d_room!=D_800C7AB2 || !d || d->kind!=WORLD_BOMB ||
      d->row[WB_ROLE] || !alive(d) || d->row[WD_LIFE]!=WD_LIVE) return;
  status=W(actor,0x68);
  if (!(status&0x40000u) &&
      !((status&0x80u) && !(status&1u) && DPTR(actor,0x38))) return;
  if (d->claimed) return;
  W(actor,0x68)=(status&~0x40080u)|1u;
  B(actor,0x8c)=60;
  bomb_claim(d,WB_CAUSE_HIT);
}

/* One bounded child step: the verified pure alpha/scale/yaw/timer arithmetic
 * from func_08000AD0 (rings) and func_08000E24 (particles). Every copy runs it,
 * so a replica predicts the same fade; only the owner commits the expiry. */
static void bomb_child_step(DynamicActor *d, void *a) {
  int *r=d->row;
  void *o=DPTR(a,0x18);
  int role=r[WB_ROLE];
  unsigned int alpha=(unsigned int)r[WB_ALPHA];
  int owner=d->owner==d_self;
  int expired=0;
  int stepped=0;
  unsigned int j;
  if (role<=WB_ROLE_V8) {
    if (role==WB_ROLE_V1 || role==WB_ROLE_V2) {
      unsigned int next=(unsigned int)(unsigned char)(alpha-0x30u);
      if (next<0x30u) {
        /* The native step lands below the 0x30 floor and retires. Only the
         * owner applies that wrap; a replica clamps at the last visible value
         * and holds, so it never wraps to 0xFF or grows its scale forever
         * while it waits for the owner's tombstone. */
        if (owner || (unsigned char)alpha>=0x30u) { alpha=next;stepped=1; }
        if (owner) expired=1;
      } else { alpha=next;stepped=1; }
      if (stepped) {
        bomb_dl(o,func_8021A26C_5D573C(a,alpha,0,0,0),0x20000000u);
        /* The native update widens each scale to double, multiplies and rounds
         * back to float, so the arithmetic is preserved in double here. */
        for (j=0;j<3;++j)
          F(o,0x1c+j*4)=(float)((double)F(o,0x1c+j*4)*1.2);
      }
    } else {
      short old=S(a,0x8a);
      alpha=alpha<2u ? 2u : alpha-2u;
      /* Native order: the timer always decrements and the child is deleted on
       * the update whose OLD value was zero, i.e. 91 updates from 90. A
       * replica holds at the endpoint instead of running past it. */
      if (old>0 || owner) {
        S(a,0x8a)=(short)(old-1);
        stepped=1;
        if (old==0) expired=1;
      }
      if (stepped) {
        bomb_dl(o,func_8021DD4C_5D921C(a,alpha,0,0,0),0x20000000u);
        for (j=0;j<3;++j)
          F(o,0x1c+j*4)=(float)((double)F(o,0x1c+j*4)*1.005);
      }
    }
    /* Only the ring update spins the object yaw; the particle update never
     * touches +0x16. */
    H(o,0x16)=(unsigned short)((H(o,0x16)+0x20u)&0x3ffu);
  } else {
    unsigned int next=(unsigned int)(unsigned char)(alpha-0x10u);
    if (next<0x10u) {
      if (owner || (unsigned char)alpha>=0x10u) { alpha=next;stepped=1; }
      if (owner) expired=1;
    } else { alpha=next;stepped=1; }
    if (stepped)
      bomb_dl(o,func_8021A26C_5D573C(a,alpha,0,0,0),0x60000000u);
  }
  B(a,0xd0)=(unsigned char)alpha;
  r[WB_ALPHA]=(int)alpha;
  if (expired && owner) {
    /* A silent expiry is a shared removal with no loot, so the retiring client
     * is the committer and the tombstone never looks like a committed kill. */
    r[WD_LANDED]=0;
    r[WD_COMMITTER]=(int)d_self;
    func_80034ED4_35AD4();
  }
}

/* Bounded replacement for the native child continuation. This callback can
 * outlive the tracking record across a disconnect or reset; without one it
 * simply keeps the native lifetime so the task still ends. */
/* Hand a tracked task back to the native continuation for its current phase.
 * The parent calls this before it releases a record (disconnect, room unload
 * or reset) so an idle root is never deleted by the bounded wrapper. */
static void bomb_detach(DynamicActor *d) {
  void *a;
  if (!d || d->kind!=WORLD_BOMB) return;
  a=d->actor;
  if (!a || !alive(d)) return;
  if (!d->bomb_initialized) {
    /* An uninitialized proxy holds no retained bindings, so it is retired
     * rather than handed to a native continuation that would fault. */
    W(a,0x68)|=2u;
    return;
  }
  /* Restore the retained physical flags, collision scalars, velocity and phase
   * before handing the task back, so the native continuation resumes from the
   * checkpoint instead of from the zeros written while frozen. */
  bomb_apply_extra(d,d->row);
  DPTR(a,0xc)=(void *)bomb_phase(d->row[WD_PHASE]);
}

/* Bounded replacement for the native continuation. Without a tracking record
 * it hands the task back to the native callback for its phase instead of
 * deleting it, so an idle root or a live child still ends natively. */
static void bomb_native(void *a, void *o) {
  DynamicActor *d=lookup(a);
  if (!d) {
    int phase;
    if (W(a,0x60)==0x02c007e0u) phase=WB_PARTICLE;
    else if (W(a,0x60)==0x004006e0u) phase=WB_RING;
    else if (H(a,0x5c)==H(a,0x5e)) phase=WB_IDLE;
    else phase=WB_RING;
    {
      DynamicCallback real=bomb_phase(phase);
      DPTR(a,0xc)=(void *)real;
      real(a,o);
    }
    return;
  }
  if (d->kind!=WORLD_BOMB) return;
  if (!d->row[WB_ROLE]) return; /* the root is driven by the phase machine */
  bomb_child_step(d,a);
}

/* Root pure-AI wrapper plus the child owner/replica split. */
static int bomb_callback(DynamicActor *d, void *a, void *o) {
  int *r=d->row;
  int role=r[WB_ROLE];
  if (r[WD_LIFE]==WD_REMOVED) {
    if (!role) {
      if (r[WD_LANDED] && (unsigned int)r[WD_COMMITTER]==d_self) {
        /* Exactly one committer runs the native explosion, exactly once. */
        if (!d->death_started) {
          d->death_started=1;
          func_080006D4_6E9824(a,o);
        }
      }
      /* Every other copy retires its root without a fanout. */
    }
    W(a,0x68)|=2u;
    return 1;
  }
  /* Force one full checkpoint write when resuming from a freeze: the frozen
   * tick zeroed the local velocity to suppress the post, so apply() must run
   * before the gate can be evaluated, not after. */
  if (d->bomb_frozen) d->dirty=1;
  if (!d->have || !apply(d) || !r[WB_RECEIPT] || !d->owner ||
      r[WD_PAUSED] || d->claimed) {
    /* Freeze motion, but keep the last physical checkpoint. */
    d->bomb_frozen=1;
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    W(a,0x60)=0;
    return 1;
  }
  d->bomb_frozen=0;
  if (!role) {
    int phase=r[WD_PHASE];
    if (d->owner!=d_self) {
      /* A replica mirrors the shared phase and claims whenever its own player
       * is inside the fuse radius of a phase the native AI would explode. */
      W(a,0x60)=phase==WB_IDLE ? 0x002006e1u : 0x02a007e1u;
      if (phase==WB_EXPLODE) { bomb_claim(d,r[WB_CAUSE]);return 1; }
      /* A replica respects the same native order as the owner: while the block
       * is still airborne the native falling callback overwrites the scheduled
       * explosion with the fuse, so only a grounded proximity may claim. */
      if (phase==WB_FUSE && bomb_near(a,WB_PROXIMITY_FUSE)) {
        bomb_claim(d,WB_CAUSE_PROXIMITY);return 1;
      }
      if (phase==WB_FALL && (W(a,0x68)&0x20u) &&
          bomb_near(a,WB_PROXIMITY_FUSE)) {
        bomb_claim(d,WB_CAUSE_PROXIMITY);return 1;
      }
      return 1;
    }
    /* Owner: the pure phase machine. The arm input is the aggregated word. */
    if (phase==WB_IDLE) {
      if (d->net[WD_BASE_Y]) { phase=WB_FALL;r[WB_CAUSE]=WB_CAUSE_PROXIMITY; }
    } else if (phase==WB_FALL) {
      /* Native order: a grounded proximity schedules the explosion, then an
       * airborne block overwrites that with the fuse and its launch sound. */
      if (W(a,0x68)&0x20u) {
        if (bomb_near(a,WB_PROXIMITY_FUSE)) {
          phase=WB_EXPLODE;r[WB_CAUSE]=WB_CAUSE_PROXIMITY;
        }
      } else {
        func_8000F420_10020(0x272,D_8020CBF0_5C8B00,o,400.0f);
        phase=WB_FUSE;
      }
    } else if (phase==WB_FUSE) {
      /* Native order: a proximity schedules the explosion, then the fuse
       * counter and tint advance unconditionally in the same tick. */
      unsigned int fuse=(unsigned int)r[WB_ALPHA]+WB_FUSE_STEP;
      if (fuse>=WB_FUSE_CAP) {
        fuse=WB_FUSE_CAP;phase=WB_EXPLODE;r[WB_CAUSE]=WB_CAUSE_FUSE;
      }
      if (bomb_near(a,WB_PROXIMITY_FUSE)) {
        phase=WB_EXPLODE;r[WB_CAUSE]=WB_CAUSE_PROXIMITY;
      }
      B(a,0xd0)=(unsigned char)fuse;
      bomb_dl(o,func_8021DF60_5D9430(a,fuse,0xff,0x20,0),0x20000000u);
      r[WB_ALPHA]=(int)fuse;
    }
    W(a,0x60)=phase==WB_IDLE ? 0x002006e1u : 0x02a007e1u;
    r[WD_PHASE]=phase;
    if (phase==WB_EXPLODE) { bomb_claim(d,r[WB_CAUSE]);return 1; }
    capture(d);
    return 1;
  }
  /* Children predict locally on every copy; only the owner retires. */
  bomb_child_step(d,a);
  if (alive(d)) capture(d);
  return 1;
}
#endif
