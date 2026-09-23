#ifndef ANCHOR_WORLD_NPC_H
#define ANCHOR_WORLD_NPC_H

/* File 59 continuations. The wire contains a nine-word scalar checkpoint;
 * texture/resource addresses, dialogue return pointers and pickpocket route
 * pointers are resolved only from this client's native resources. */
#define WORLD_NPC_WORDS 9
#define WORLD_NPC_PHASES(X)                                                    \
  X(func_08000150_71BBD0)                                                      \
  X(func_08000230_71BCB0)                                                      \
  X(func_080002EC_71BD6C)                                                      \
  X(func_080003A8_71BE28)                                                      \
  X(func_08000464_71BEE4)                                                      \
  X(func_080005F8_71C078)                                                      \
  X(func_080000C0_71BB40)                                                      \
  X(func_08000AB0_71C530)                                                      \
  X(func_08000C24_71C6A4)                                                      \
  X(func_08000E44_71C8C4)                                                      \
  X(func_08000EE0_71C960)                                                      \
  X(func_08000F94_71CA14)                                                      \
  X(func_08000FD4_71CA54)                                                      \
  X(func_08001400_71CE80)                                                      \
  X(func_0800156C_71CFEC)                                                      \
  X(func_08001680_71D100)                                                      \
  X(func_0800173C_71D1BC)                                                      \
  X(func_080017F8_71D278)                                                      \
  X(func_080018B4_71D334)                                                      \
  X(func_08001970_71D3F0)                                                      \
  X(func_08001A4C_71D4CC)                                                      \
  X(func_08001B28_71D5A8)

static inline int anchor_world_npc_valid(unsigned int entity,
                                         unsigned int model, const int *r) {
  static const unsigned short models[22] = {
      0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c1, 0x2c2, 0x2c2, 0x2c4,
      0x2c4, 0x2c4, 0x2c4, 0x2c4, 0x2c4, 0x2c5, 0x2c6, 0x2c7,
      0x2ca, 0x2c9, 0x2c8, 0x2cb, 0x2cc, 0x2cd};
  unsigned int j;
  if (!r[0]) {
    for (j = 1; j < WORLD_NPC_WORDS; ++j)
      if (r[j])
        return 0;
    return 1;
  }
  if (r[0] < 1 || r[0] > 22 ||
      (model != models[r[0] - 1] && !(r[0] == 19 && model == 0x2ce)))
    return 0;
  if (!(entity == model || (entity == 0x2c3 && r[0] == 7) ||
        (entity == 0x2ce && r[0] == 19) || (entity == 0x2d8 && r[0] == 3) ||
        (entity == 0x2d9 && r[0] == 5)))
    return 0;
  for (j = 1; j <= 7; ++j)
    if (r[j] < -32768 || r[j] > 32767)
      return 0;
  if (r[8] < 0 || r[8] > 6 || (r[8] & 3) == 3)
    return 0;
  if (model == 0x2c4) {
    if ((r[8] & 3) < 1 || (r[8] & 3) > 2 || r[4] < 0 ||
        r[4] > ((r[8] & 3) == 1 ? 16 : 24) || (r[4] & 1) ||
        (r[5] != -2 && r[5] != 0 && r[5] != 2) || r[6] < 0 || r[6] > 1 ||
        r[7] < 0 || r[7] > 9999)
      return 0;
    /* The escape callback indexes a live waypoint before moving to a
     * sentinel. Never resume that callback at a sentinel or without a step. */
    if (r[0] == 13 && (r[4] < 2 || !r[5]))
      return 0;
  } else {
    if (r[8] & 3)
      return 0;
    for (j = 3; j <= 7; ++j)
      if (r[j])
        return 0;
    /* Only the gated town arrivals use the D8 word. */
    if (r[0] == 1 || r[0] == 15) {
      if (r[1] || r[2] < 0 || r[2] > 2)
        return 0;
    } else if (r[1] || r[2])
      return 0;
  }
  return 1;
}

int anchor_world_npc_capture(void *actor, void *callback, int *row);
int anchor_world_npc_resident(const int *row);
void *anchor_world_npc_restore(void *actor, const int *row, int reconstruct);
void anchor_world_npc_face(void *actor, const int *row);
#endif
