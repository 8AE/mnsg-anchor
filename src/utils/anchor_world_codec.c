#include "anchor_world.h"
#include "anchor_world_bridge.h"
#include "anchor_world_crane.h"
#include "anchor_world_npc.h"
#include "utils/string_utils.h"

/* Shared temporary-flag mask for the crane checkpoint. Only the wire
 * validation needs the name, so it stays local to the codec. */
#ifndef WORLD_CRANE_TEMP_MASK
#define WORLD_CRANE_TEMP_MASK 0x4bcf
#endif

#include "anchor_world_gate64.h"
#include "anchor_world_doll.h"
#include "anchor_world_counterweight.h"

int anchor_world_row_valid(const int *r) {
  static const int lo[ANCHOR_WORLD_WORDS] = {
      0,       1,      1,      0,      -3276800, -3276800, -3276800, 0,
      0,       0,      0,      0,      0,        0,        -100000,  -100000,
      -100000, -32768, 0,      -32768, -32768,   -32768,   -32768,   -32768,
      -32768,  -32768, 0,      0,      0,        0,        0,        -32768,
      0,       0,      -32768, -32768, -32768,   0,        0,        0,
      -32768,  -32768, -32768, -32768, -32768,   -32768,   -32768,   -32768,
      0, 0};
  static const int hi[ANCHOR_WORLD_WORDS] = {
      255,    2047,  6,          1,       3276700, 3276700, 3276700, 1023,
      1023,   1023,  255,        1000000, 65535,   7,       100000,  100000,
      100000, 32767, 2147483647, 32767,   32767,   32767,   32767,   32767,
      32767,  32767, 64000,      64000,   64000,   31,      163,     32767,
      7,      255,   32767,      32767,   32767,   255,     1,       22,
      32767,  32767, 32767,      32767,   32767,   32767,   32767,   32767,
      2147483647, 2147483647};
  unsigned int i;
  if (r[2] == WORLD_COUNTERWEIGHT) return anchor_world_counterweight_valid(r);
  if (r[2] == WORLD_CRANE) {
    /* Room 0x31 crane/pad/reward checkpoint. Validated independently of the
     * actor/NPC table, whose kind range excludes 7. */
    static const int crane_lo[ANCHOR_WORLD_WORDS] = {
        0,       0x1b9,   7,       0,       -3276800, -3276800, -3276800, 0,
        0,       0,       1,       0,       0,        0,        -100000,  -100000,
        -100000, -1,      1,       1,       -1,       -32768,   1,        -1,
        -32768,  0,       0,       0,       0,        0,        0,        0,
        0,       0,       -32768,  -32768,  -32768,   1,        0,        0,
        -32768,  -32768,  -32768,  -1,      0,        0,        0,        0,
        0,       0};
    static const int crane_hi[ANCHOR_WORLD_WORDS] = {
        255,     0x1b9,   7,       0,       3276700,  3276700,  3276700,  1023,
        1023,    1023,    1,       1000000, 65535,    7,        100000,   100000,
        100000,  60,      19,      5,       10,       32767,    5,        10,
        32767,   0x4bcf,  64000,   64000,   64000,    3,        1,        0,
        1,       3,       32767,   32767,   32767,    13,       1,        1023,
        32767,   32767,   32767,    12,      2,        1,        3,        3,
        2147483647, 2147483647};
    for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
      if (r[i] < crane_lo[i] || r[i] > crane_hi[i])
        return 0;
    return r[1] == WORLD_CRANE_ENTITY && !r[3] && r[10] == 1 &&
           !(r[25] & ~WORLD_CRANE_TEMP_MASK) &&
           (r[37] == 13 || (r[37] >= 1 && r[37] <= 11)) &&
           r[32] == (r[37] == 13) && (r[44] == 2) == (r[32] == 1);
  }
  if (r[2] == WORLD_SHUTTER) {
    /* Room 0xB2 timed shutter. Every reserved word is a fixed zero bound, so
     * this table is the whole checkpoint and needs no cross-word checks. */
    static const int shutter_lo[ANCHOR_WORLD_WORDS] = {
        0,       WORLD_SHUTTER_ENTITY, 8, 0, -3276800, -3276800, -3276800, 0,
        0,       0,       14,      0,       0,        0,        0,        0,
        0,       -1,      1,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0};
    static const int shutter_hi[ANCHOR_WORLD_WORDS] = {
        255,     WORLD_SHUTTER_ENTITY, 8, 0, 3276700,  3276700,  3276700,  1023,
        1023,    1023,    14,      1000000, 65535,    7,        0,        0,
        0,       90,      4,       1,       2147483647, 0,       0,        0,
        0,       0,       64000,   64000,   64000,    3,        0,        0,
        0,       0,       0,       0,       0,        0,        1,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        2147483647, 2147483647};
    for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
      if (r[i] < shutter_lo[i] || r[i] > shutter_hi[i])
        return 0;
    return 1;
  }
  if (r[2] == WORLD_BRIDGE) {
    /* Room 0x15E File51 bridge. Both guard blocks repeat the same 14-word
     * pattern: packed phase/playback/status400, XYZ hundredths, yaw with AA
     * above it, clip, frame, rate with animation flags above it, route timer,
     * three origin halfwords, route substate and the local route PC. The native
     * adapter additionally validates the PC against the immutable route's
     * opcode boundaries before restoring a continuation. */
    static const int bridge_lo[ANCHOR_WORLD_WORDS] = {
        0,       0x240,   9,       0,       0,        0,        1,        0,
        0,       0,       1,       -3276800,-3276800, -3276800, 0,        0,
        0,       0,       -32768,  -32768,  -32768,   -32768,   0,        0,
        1,       -3276800,-3276800,-3276800, 0,       0,        0,        0,
        -32768,  -32768,  -32768,  -32768,  0,        0,        0,        -100000,
        -100000, -100000, -100000, -100000, -100000,  0,        0,        0,
        0,       0};
    static const int bridge_hi[ANCHOR_WORLD_WORDS] = {
        255,     0x240,   9,       1,       3,        1,        3,        1000000,
        0xfffff, 1,       15,      3276700, 3276700,  3276700,  262143,   2,
        1000000, 0x7ffff, 32767,   32767,   32767,    32767,    7,        255,
        15,      3276700, 3276700, 3276700, 262143,   2,        1000000,  0x7ffff,
        32767,   32767,   32767,   32767,   7,        255,      1,        100000,
        100000,  100000,  100000,  100000,  100000,   0,        3,        3,
        2147483647, 2147483647};
    for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
      if (r[i] < bridge_lo[i] || r[i] > bridge_hi[i])
        return 0;
    return (r[WB_GUARD_0] & 3) != 0 && (r[WB_GUARD_1] & 3) != 0;
  }
  if (r[2] == WORLD_GATE64) {
    /* Room 0x14B gate64. Bounds are the whole wire contract apart from the
     * completion cross-check; the native adapter owns the phase-specific model,
     * resource and collision binding. Body and child transforms are hundredths,
     * the child scale is the fixed build value and the timer never feeds
     * authority. */
    static const int gate_lo[ANCHOR_WORLD_WORDS] = {
        0,       0x325,   10,      0,       -3276800, -3276800, -3276800, 0,
        0,       0,       1,       -1,      1,        0,        0,        0,
        0,       -3276800,-3276800,-3276800, 0,       0,        0,        150,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0};
    static const int gate_hi[ANCHOR_WORLD_WORDS] = {
        255,     0x325,   10,      1,       3276700,  3276700,  3276700,  1023,
        1023,    1023,    2,       32767,   10,       1,        0,        0,
        0,       3276700, 3276700, 3276700, 1023,     1023,     1023,     150,
        1,       1,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        1,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        2147483647, 2147483647};
    for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
      if (r[i] < gate_lo[i] || r[i] > gate_hi[i])
        return 0;
    return r[WG64_COMPLETE] == (r[WG64_PHASE] == 10);
  }
  if (r[2] == WORLD_DOLL_CONTAINER) {
    /* Rooms 0x16A / 0x182 File62 doll container. Bounds plus the
     * cycle/phase/pitch contract: idle is the only unstarted cycle, the opening
     * slides in eighths up to 64, the birth pose is fully raised and the
     * closing walks back down the same ramp. The native adapter owns the
     * spawned File_26 Doll and the closing presentation. */
    static const int doll_lo[ANCHOR_WORLD_WORDS] = {
        0,       0x3d6,   11,      0,       -3276800, -3276800, -3276800, 0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0};
    static const int doll_hi[ANCHOR_WORLD_WORDS] = {
        255,     0x3d6,   11,      0,       3276700,  3276700,  3276700,  70,
        1023,    1023,    0,       1000000, 3,        1,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        0,       0,       0,       0,       0,        0,        1,        0,
        0,       0,       0,       0,       0,        0,        0,        0,
        2147483647, 2147483647};
    for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
      if (r[i] < doll_lo[i] || r[i] > doll_hi[i])
        return 0;
    /* A zero cycle only ever means idle, but a finished idle container
     * legitimately holds a live cycle, so the pitch alone is pinned here. */
    if (r[WDC_PHASE] == 0)
      return r[WDC_PITCH] == 0;
    if (!r[WDC_CYCLE])
      return 0;
    if (r[WDC_PHASE] == 1)
      return r[WDC_PITCH] <= 64 && (r[WDC_PITCH] % 8) == 0;
    if (r[WDC_PHASE] == 2)
      return r[WDC_PITCH] == 70;
    return r[WDC_PITCH] == 70 || r[WDC_PITCH] == 54 || r[WDC_PITCH] == 38 ||
           r[WDC_PITCH] == 22 || r[WDC_PITCH] == 6;
  }
  for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
    if (r[i] < lo[i] || r[i] >
        (i == 23 && r[2] == WORLD_PLATFORM && r[1] == 0x3d0 ? WORLD_PHYSICS_ROUND_MAX : hi[i]))
      return 0;
  if (r[2] == WORLD_PLATFORM && (r[1] == WORLD_TOP_ENTITY || r[1] == WORLD_ROTOR_ENTITY)) {
    int top = r[1] == WORLD_TOP_ENTITY;
    if (r[3] || r[7] || r[9] || r[10] || r[11] || r[12] || r[13] ||
        r[17] || r[18] != (top ? WORLD_TOP_PHASE : WORLD_ROTOR_PHASE) ||
        r[26] != 100 || r[27] != 100 || r[28] != 100 || r[29] != 5 || r[25]) return 0;
    for (i=14;i<=16;++i) if (r[i]) return 0;
    for (i=30;i<WORLD_INSTANCE;++i) if (i!=38 && r[i]) return 0;
    if (top)
      return r[19]>=0 && r[19]<=1023 &&
          (((r[20]==2 || r[20]==3) && r[21]==160 && r[22]==1) ||
           (r[20]==0 && ((r[21]==130 && r[22]==2) || (r[21]==80 && r[22]==4))));
    for (i=20;i<=24;++i) if (r[i]) return 0;
    return r[19]==0 || r[19]==1;
  }
  if (r[2] == WORLD_PLATFORM && r[1] == WORLD_ROPE_ENTITY) {
    if (r[3] || r[18] != WORLD_ROPE_PHASE || r[20] != 10 || r[17] ||
        r[7] != (r[19]&1023) || r[10] || r[11] || r[12] != 512 || r[13] != 1 ||
        r[26] != 1000 || r[27] != 1000 || r[28] != 1000 || r[29] != 1)
      return 0;
    for (i = 14; i <= 16; ++i) if (r[i]) return 0;
    for (i = 21; i <= 25; ++i) if (r[i]) return 0;
    for (i = 30; i < WORLD_INSTANCE; ++i) if (i != 38 && r[i]) return 0;
    return 1;
  }
  if (r[2] == WORLD_PLATFORM && r[1] == WORLD_SPIKE_ENTITY) {
    int subtype = r[19], phase = r[20];
    if (r[3] || r[18] != WORLD_SPIKE_PHASE || subtype < 0 || subtype > 2 ||
        (phase != 1 && phase != 3 && phase != 4) ||
        r[21] < 0 || r[21] > (subtype == 2 ? 20 : subtype == 1 ? 23 : 24) ||
        r[22] != (subtype ? 90 : 155) || r[23] != (phase == 4) ||
        r[10] != (subtype == 1 ? 2 : 0) || r[11] > 299 ||
        r[12] != 12 || (r[13] != 0 && r[13] != 2) ||
        (phase == 1 ? r[17] < 0 || r[17] > 155 : r[17] != -1) ||
        r[29] != (phase == 1 ? 1 : 17) ||
        r[26] != 1000 || r[27] != 1000 || r[28] != (subtype ? 1000 : 1200))
      return 0;
    for (i = 14; i <= 16; ++i) if (r[i]) return 0;
    if (r[24] || r[25]) return 0;
    for (i = 30; i < WORLD_INSTANCE; ++i) if (i != 38 && r[i]) return 0;
    return 1;
  }
  if (r[2] == WORLD_PLATFORM && r[1] == 0x3d0) {
    static const int flags[] = {31, 27, 1, 11, 15, 27, 17};
    int p = r[18], animated = p >= 71 && p <= 74;
    if (p < 69 || p > 75 || r[23] < 0 || r[21] < 0 || r[21] > 5 ||
        r[22] < 0 || r[22] > 31 || r[24] < 0 || r[24] > 255 ||
        r[25] < 0 || r[25] > 255 || r[30] > 6 || r[29] != flags[r[30]] ||
        r[10] != (animated ? p == 71 ? 1 : 2 : 0) ||
        ((p == 69 || p == 75) && r[30] > 4) ||
        (p == 70 && r[30] != 5) || (animated && r[30] != 6) ||
        (p != 69 && r[3]) ||
        (p == 73 ? r[17] < 0 || r[17] > 200 : r[17] != 0))
      return 0;
    for (i = 31; i < WORLD_INSTANCE; ++i)
      if (i != 38 && r[i])
        return 0;
    return 1;
  }
  if (r[2] == WORLD_SWITCH &&
      (r[1] != 0x226 || r[18] < 1 || r[18] > 5 ||
       (r[18] > 1 && r[3] != 1) || r[19] < 0 || r[19] > 1 ||
       r[20] < 0 || r[20] >= (r[19] ? 2048 : 800) ||
       r[21] < 0 || r[21] > 1 || r[22] < 0 || r[22] > 1 ||
       r[10] != 0 || r[17] || r[23] || r[24] || r[25] ||
       (r[29] & ~19)))
    return 0;
  if (r[2] == WORLD_PLATFORM && (r[1] == 0x324 || r[1] == 0x326)) {
    int temporary = r[1] == 0x324;
    if (r[18] < (temporary ? 53 : 56) || r[18] > (temporary ? 55 : 59) ||
        (r[18] != (temporary ? 53 : 56) && r[3] != 1) ||
        r[19] < 0 || r[19] > (temporary ? 11 : 7) ||
        r[20] < 0 || r[20] >= (temporary ? 800 : 2048) ||
        r[21] < 0 || r[21] > 1 || r[24] < 0 || r[24] > 1 ||
        (r[24] && (temporary || r[18] != 59)))
      return 0;
  }
  if (r[2] == WORLD_PLATFORM && (r[1] == 0x228 || r[1] == 0x1fe)) {
    int fire = r[1] == 0x1fe;
    if (r[18] < (fire ? 64 : 60) || r[18] > (fire ? 68 : 63) ||
        r[23] != (r[18] >= (fire ? 65 : 62)) || r[39])
      return 0;
    if (!fire) {
      if (r[3] || r[17] || r[24] < 0 || r[24] > 255 ||
          r[25] < 0 || r[25] > 4 || r[30] || r[31] < 0 || r[31] > 255 ||
          r[32] || r[33] || r[37] || r[10] != (r[23] && r[25] == 2 ? 2 : 0))
        return 0;
    } else {
      if (r[30] > 3 || r[10] != (r[23] ? 2 : 0) || r[24] < 0 || r[24] > 5 ||
          r[25] < 0 || r[25] > 10 || (r[18] != 67 && r[25]) ||
          (r[18] == 67 && (r[24] != 1 || !r[25])) ||
          ((r[18] == 65 || r[18] == 66) && r[24] != 0 && r[24] != 5) ||
          (r[18] == 68 && (r[24] < 1 || r[24] > 4)) ||
          r[31] < 0 || r[31] > 2047 || r[32] > 1 || r[33] > 1 ||
          r[40] < 0 || r[40] > 25500 || r[41] < 0 || r[41] > 8000 ||
          (r[42] != 0 && r[42] != 0x200 && r[42] != 0x240))
        return 0;
      if (r[24] >= 1 && r[24] <= 4) {
        if (r[17] < 1 || r[17] > (r[24] == 3 ? 256 : 40))
          return 0;
      } else if (r[17] || r[40] || r[41])
        return 0;
      if (!r[33] && (r[32] || r[37] || r[45] || r[46] || r[47]))
        return 0;
      if (!r[23] && (r[3] || r[24] || r[25] || r[31] || r[33] ||
                      r[42] || r[43] || r[44]))
        return 0;
      return 1; /* These scalar words describe the parent-owned collider. */
    }
  }
  unsigned int model = r[1] == 0x2c3   ? 0x2c2
                       : r[1] == 0x2d8 ? 0x2bf
                       : r[1] == 0x2d9 ? 0x2c1
                                       : (unsigned int)r[1];
  if (r[2] != WORLD_NPC && r[WORLD_NPC_CHECKPOINT])
    return 0;
  return anchor_world_npc_valid(r[1], model, r + WORLD_NPC_CHECKPOINT);
}

typedef struct {
  char *p, *end;
  int ok;
} Output;
static void put(Output *o, char c) {
  if (o->p >= o->end)
    o->ok = 0;
  else
    *o->p++ = c;
}
static void text(Output *o, const char *s) {
  while (*s)
    put(o, *s++);
}
int anchor_world_encode(const int rows[][ANCHOR_WORLD_WORDS],
                        unsigned int count, const unsigned char *dead,
                        char *out, unsigned int capacity) {
  Output o;
  unsigned int i, j;
  char n[12];
  static const char hex[] = "0123456789abcdef";
  if (!out || !capacity || count > ANCHOR_WORLD_MAX)
    return 0;
  o.p = out;
  o.end = out + capacity - 1;
  o.ok = 1;
  text(&o, "{\"a\":[");
  for (i = 0; i < count; ++i) {
    if (!anchor_world_row_valid(rows[i]))
      return 0;
    if (i)
      put(&o, ',');
    put(&o, '[');
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) {
      if (j)
        put(&o, ',');
      mnsg_string_write_s32(n, rows[i][j]);
      text(&o, n);
    }
    put(&o, ']');
  }
  text(&o, "],\"d\":\"");
  for (i = 0; i < 32; ++i) {
    put(&o, hex[dead[i] >> 4]);
    put(&o, hex[dead[i] & 15]);
  }
  text(&o, "\"}");
  *o.p = 0;
  return o.ok;
}

static void ws(const char **p) {
  while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')
    ++*p;
}
static int token(const char **p, char c) {
  ws(p);
  if (**p != c)
    return 0;
  ++*p;
  return 1;
}
static int number(const char **p, int *out) {
  unsigned int v = 0, negative = 0, limit = 0x7fffffff;
  int n = 0;
  ws(p);
  if (**p == '-') {
    negative = 1;
    limit = 0x80000000u;
    ++*p;
  }
  if (**p == '0' && (*p)[1] >= '0' && (*p)[1] <= '9')
    return 0;
  while (**p >= '0' && **p <= '9') {
    unsigned int d = (unsigned int)(*(*p)++ - '0');
    if (++n > 10 || v > (limit - d) / 10u)
      return 0;
    v = v * 10u + d;
  }
  if (!n)
    return 0;
  *out = negative ? (int)(0u - v) : (int)v;
  return 1;
}
static int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}
int anchor_world_decode(const char *json, int rows[][ANCHOR_WORLD_WORDS + 2],
                        unsigned int *count, unsigned char *dead) {
  const char *p = json;
  unsigned int n = 0, i;
  volatile unsigned char seen[256];
  for (i = 0; i < 256; ++i)
    seen[i] = 0;
  if (!p || !token(&p, '{') || !token(&p, '"') || !token(&p, 'a') ||
      !token(&p, '"') || !token(&p, ':') || !token(&p, '['))
    return 0;
  ws(&p);
  while (*p != ']') {
    if (n >= ANCHOR_WORLD_MAX || (n && !token(&p, ',')) || !token(&p, '['))
      return 0;
    for (i = 0; i < ANCHOR_WORLD_WORDS + 2; ++i)
      if ((i && !token(&p, ',')) || !number(&p, &rows[n][i]))
        return 0;
    if (!token(&p, ']') || rows[n][0] <= 0 || rows[n][1] < 0 ||
        rows[n][1] > 1500 || !anchor_world_row_valid(rows[n] + 2) ||
        seen[rows[n][2]])
      return 0;
    seen[rows[n][2]] = 1;
    ++n;
    ws(&p);
  }
  if (!token(&p, ']') || !token(&p, ',') || !token(&p, '"') ||
      !token(&p, 'd') || !token(&p, '"') || !token(&p, ':') || !token(&p, '"'))
    return 0;
  for (i = 0; i < 32; ++i) {
    int a = hex(*p);
    if (a < 0)
      return 0;
    ++p;
    int b = hex(*p);
    if (b < 0)
      return 0;
    ++p;
    dead[i] = (unsigned char)(a * 16 + b);
  }
  if (!token(&p, '"') || !token(&p, '}'))
    return 0;
  ws(&p);
  if (*p)
    return 0;
  *count = n;
  return 1;
}
