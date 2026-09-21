#include "anchor_world_npc.h"
#ifndef WORLD_NPC_PTR
#define WORLD_NPC_PTR(p, o) (*(void **)((char *)(p) + (o)))
#endif
#ifndef WORLD_NPC_DISABLED
#define WORLD_NPC_DISABLED 0x00800000ul
#endif
#define NH(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define NS(p, o) (*(short *)((char *)(p) + (o)))
#define NW(p, o) (*(unsigned int *)((char *)(p) + (o)))
typedef void (*NpcCallback)(void *, void *);
#define NPC_DECLARE(f) extern void f(void *, void *);
WORLD_NPC_PHASES(NPC_DECLARE)
extern short D_08001C50_71D6D0[], D_08001C78_71D6F8[];
extern int func_800141C4_14DC4(unsigned int);
extern void func_8021A764_5D5C34(void *, unsigned int, int, int);
extern void func_80224D50_5E0220(void *, int);
static NpcCallback npc_callbacks[22];
static void npc_addresses(void) {
  unsigned int j = 0;
#define NPC_ADDRESS(f) npc_callbacks[j++] = f;
  WORLD_NPC_PHASES(NPC_ADDRESS)
#undef NPC_ADDRESS
}

int anchor_world_npc_capture(void *a, void *callback, int *r) {
  unsigned int j, model = NH(a, 0x5e);
  for (j = 0; j < WORLD_NPC_WORDS; ++j)
    r[j] = 0;
  npc_addresses();
  /* The local conversation keeps its own return target at +B4. */
  if (NW(a, 0x68) & 0x100u)
    callback = WORLD_NPC_PTR(a, 0xb4);
  for (j = 0; j < 22; ++j)
    if (((unsigned long)callback & ~WORLD_NPC_DISABLED) ==
        ((unsigned long)npc_callbacks[j] & ~WORLD_NPC_DISABLED)) {
      r[0] = (int)j + 1;
      break;
    }
  if (!r[0])
    return 1;
  if (model == 0x2c4 || r[0] == 1 || r[0] == 15) {
    r[1] = NS(a, 0xd8);
    r[2] = NS(a, 0xda);
  }
  if (model == 0x2c4) {
    r[3] = NS(a, 0xdc);
    for (j = 0; j < 4; ++j)
      r[4 + j] = NS(a, 0xe4 + j * 2);
    if (WORLD_NPC_PTR(a, 0xe0) == D_08001C50_71D6D0)
      r[8] = 1;
    if (WORLD_NPC_PTR(a, 0xe0) == D_08001C78_71D6F8)
      r[8] = 2;
  }
  if (NW(a, 0x68) & 0x400u)
    r[8] |= 4;
  return anchor_world_npc_valid(NH(a, 0x5c), model, r);
}
int anchor_world_npc_resident(const int *r) {
  return !r[0] || func_800141C4_14DC4(59) != -1;
}

/* These are native segmented texture selectors, not host pointers. The model
 * binder has already established the local object segment bases. Blinking is
 * intentionally local and its +76 clock is not part of a network checkpoint. */
static int face_selectors(unsigned int model, unsigned int *s) {
  switch (model) {
  case 0x2be:
    s[0] = 0x080025b0;
    s[1] = 0x080045b0;
    s[2] = 0x080035b0;
    break;
  case 0x2bf:
    s[0] = 0x08001db0;
    s[1] = 0x08002db0;
    s[2] = 0x08003db0;
    break;
  case 0x2c0:
    s[0] = 0x08003b10;
    s[1] = 0x08002b10;
    s[2] = 0x08001b10;
    break;
  case 0x2c1:
    s[0] = 0x08001710;
    s[1] = 0x08002710;
    s[2] = 0x08003710;
    break;
  case 0x2c2:
    s[0] = 0x08001d40;
    s[1] = 0x08002d40;
    s[2] = 0x08003d40;
    break;
  case 0x2c4:
    s[0] = 0x08003e70;
    s[1] = 0x08002e70;
    s[2] = 0x08004e70;
    break;
  case 0x2c5:
  case 0x2c7:
    s[0] = 0x08002090;
    s[1] = 0x08003090;
    s[2] = 0x08004090;
    break;
  case 0x2c6:
    s[0] = 0x080028b0;
    s[1] = 0x080018b0;
    s[2] = 0x080038b0;
    break;
  case 0x2c8:
    s[0] = 0x080019a0;
    s[1] = 0x080039a0;
    s[2] = 0x080029a0;
    break;
  case 0x2c9:
    s[0] = 0x08002010;
    s[1] = 0x08004010;
    s[2] = 0x08003010;
    break;
  case 0x2ca:
    s[0] = 0x08001e00;
    s[1] = 0x08002e00;
    s[2] = 0x08003e00;
    break;
  case 0x2cb:
    s[0] = 0x08003b10;
    s[1] = 0x08005b10;
    s[2] = 0x08006b10;
    break;
  case 0x2cc:
  case 0x2cd:
    s[0] = 0x09001000;
    s[1] = 0x09002000;
    s[2] = 0x09003000;
    break;
  case 0x2ce:
    s[0] = 0x080021f0;
    s[1] = 0x080041f0;
    s[2] = 0x080031f0;
    break;
  default:
    return 0;
  }
  return 1;
}

void *anchor_world_npc_restore(void *a, const int *r, int reconstruct) {
  unsigned int j, s[3], model = NH(a, 0x5e);
  if (!r[0] || !anchor_world_npc_valid(NH(a, 0x5c), model, r) ||
      !anchor_world_npc_resident(r))
    return 0;
  npc_addresses();
  if (model == 0x2c4 || r[0] == 1 || r[0] == 15) {
    NS(a, 0xd8) = (short)r[1];
    NS(a, 0xda) = (short)r[2];
  }
  if (model == 0x2c4) {
    NS(a, 0xdc) = (short)r[3];
    for (j = 0; j < 4; ++j)
      NS(a, 0xe4 + j * 2) = (short)r[4 + j];
    WORLD_NPC_PTR(a, 0xe0) =
        (r[8] & 3) == 1 ? D_08001C50_71D6D0 : D_08001C78_71D6F8;
  }
  NW(a, 0x68) = (NW(a, 0x68) & ~0x400u) | ((r[8] & 4) ? 0x400u : 0u);
  if (reconstruct && r[0] != 7 && face_selectors(model, s)) {
    NW(a, 0x90) = s[0];
    NW(a, 0x9c) = s[1];
    NW(a, 0xa0) = s[2];
    func_8021A764_5D5C34(a, s[0], 3, model == 0x2cc || model == 0x2cd ? 1 : 2);
  }
  return (void *)npc_callbacks[r[0] - 1];
}
void anchor_world_npc_face(void *a, const int *r) {
  unsigned int s[3], model = NH(a, 0x5e);
  if (r[0] && r[0] != 7 && face_selectors(model, s))
    func_80224D50_5E0220(a, model == 0x2cc || model == 0x2cd ? 1 : 2);
}
