#include "anchor_world_dynamic.h"
#include "anchor_world_npc.h"
#include "anchor_world_slicer.h"
#include "anchor_world_random.h"
#include "anchor_world_bomb.h"
#include "anchor_world_wave.h"
#include "utils/string_utils.h"

int anchor_world_dynamic_row_valid(const int *r) {
  static const int lo[WORLD_DYNAMIC_WORDS] = {
      0,      0,      0, 0,       0,        0,        1,        0,
      0,      0,      0, 0,       -3276800, -3276800, -3276800, 0,
      0,      0,      0, 0,       0,        -100000,  -100000,  -100000,
      0,      0,      0, 0,       0,        0,        0,        -32768,
      0,      -32768, 0, 0,       -32768,   -32768,   -32768,   0,
      0,      0,      0, -100000, 0,        -32768,   0,        0,
      0,      -32768, 0, 0,       -32768,   0,        0,        0,
      0,      0,      0, 0,       0,        -3276800, -3276800, -3276800,
      0,      0,      0, 0,       0,        0,        0,        -32768,
      0,      0,      0, -32768,  -32768,   -32768,   -32768,   -32768,
      -32768, -32768, 0};
  static const int hi[WORLD_DYNAMIC_WORDS] = {
      2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2,
      11,         256,        1025,       1025,       255,        1,
      3276700,    3276700,    3276700,    1023,       1023,       1023,
      1000000,    65535,      7,          100000,     100000,     100000,
      64000,      64000,      64000,      65535,      65535,      65535,
      65535,      32767,      163,        32767,      7,          255,
      32767,      32767,      32767,      255,        1,          65535,
      35,         100000,     1,          32767,      1023,       65535,
      65535,      32767,      65535,      65535,      32767,      255,
      65535,      255,        255,        255,        255,        100000,
      100000,     3276700,    3276700,    3276700,    2147483647, 1,
      1,          17,         255,        255,        65535,      32767,
      2147483647, 255,        22,         32767,      32767,      32767,
      32767,      32767,      32767,      32767,      6};
  unsigned int i;
  for (i = 0; i < WORLD_DYNAMIC_WORDS; ++i) {
    int min=lo[i],max=hi[i];
    if ((r[WD_KIND]==WD_SLICER || r[WD_KIND]==WD_RANDOM || r[WD_KIND]==WD_BOMB || r[WD_KIND]==WD_WAVE) &&
        (i==WS_INSTANCE || i==WS_RECEIPT)) { min=0;max=2147483647; }
    if (r[WD_KIND]==WD_RANDOM && (i==WR_TARGET_X || i==WR_TARGET_Z)) {
      min=-WR_TARGET_LIMIT;max=WR_TARGET_LIMIT;
    }
    if (r[WD_KIND]==WD_WAVE && i>=WW_TARGET_X && i<=WW_TARGET_Z) {
      min=-WW_TARGET_LIMIT;max=WW_TARGET_LIMIT;
    }
    if (r[i]<min || r[i]>max) return 0;
  }
  if (r[WD_KIND]==WD_WAVE)
    return r[WD_SERIAL]>0 && anchor_world_wave_valid(r);
  if (r[WD_KIND]==WD_BOMB)
    return r[WD_SERIAL]>0 && anchor_world_bomb_valid(r);
  if (r[WD_KIND]==WD_RANDOM)
    return r[WD_SERIAL]>0 && anchor_world_random_valid(r);
  if (r[WD_KIND]==WD_SLICER)
    return r[WD_SERIAL]>0 && anchor_world_slicer_valid(r);
  if (!anchor_world_npc_valid(r[WD_ENTITY], r[WD_MODEL],
                              r + WD_NPC_CHECKPOINT) ||
      (r[WD_KIND] != WD_NPC && r[WD_NPC_CHECKPOINT]))
    return 0;
  if (!r[WD_SERIAL] ||
      (r[WD_LIFE] == WD_CLAIM &&
       (r[WD_KIND] < WD_COIN || r[WD_KIND] > WD_FOOD) &&
       r[WD_KIND] != WD_SHUTTER_ENEMY && r[WD_KIND] != WD_DOLL))
    return 0;
  /* The nested File_26 Doll is pinned before the removal early return, so a
   * tombstone still carries the typed container identity: fixed entity and
   * model, clip 2, no animation, the one-based container parent, the single
   * spawn ordinal and the fixed birth X/Z. It falls in hundreds of a unit and
   * rests at 3400. */
  if (r[WD_KIND] == WD_DOLL) {
    unsigned int j;
    if (r[WD_ENTITY] || r[WD_MODEL] || r[WD_CLIP] != 2 || r[WD_ANIMATED] ||
        r[WD_PARENT] < 1 || r[WD_PARENT] > 256 || r[WD_ORDINAL] != 1 ||
        r[WD_X] != -600 || r[WD_Z] != -10200 ||
        r[WD_VX] || r[WD_VY] || r[WD_VZ] ||
        r[WD_TALKABLE] || r[WD_DIALOG] || r[WD_BUSY] || r[WD_ROUTE] != 163)
      return 0;
    for (j = WD_NPC_CHECKPOINT; j < WORLD_DYNAMIC_WORDS; ++j)
      if (r[j])
        return 0;
    if (r[WD_PHASE] == 16)
      return r[WD_Y] >= 3500 && r[WD_Y] <= 12500 && (r[WD_Y] % 100) == 0;
    if (r[WD_PHASE] == 17)
      return r[WD_Y] == 3400;
    return 0;
  }
  if (r[WD_LIFE] == WD_REMOVED)
    return 1;
  /* A shutter enemy is a one-hit native child with no health word, so the
   * recipe pins the only appearance it can have. The word table already caps
   * the ordinal at INT_MAX; the recipe only has to exclude zero. Base Y is a
   * boolean here, and its set value carries the native route status 0x400. */
  if (r[WD_KIND] == WD_SHUTTER_ENEMY)
    return r[WD_ENTITY] == 0xFC && r[WD_MODEL] == 0xFB && !r[WD_CLIP] &&
           r[WD_ANIMATED] && r[WD_PHASE] == 15 && r[WD_ROUTE] == 57 &&
           r[WD_PARENT] >= 1 && r[WD_PARENT] <= 256 && r[WD_ORDINAL] >= 1 &&
           !r[WD_TALKABLE] && !r[WD_DIALOG] && !r[WD_BUSY] &&
           r[WD_BOUNCE] >= 0 && r[WD_BOUNCE] <= 255 &&
           (r[WD_BASE_Y] == 0 || r[WD_BASE_Y] == 1);
  if (r[WD_KIND] == WD_NPC &&
      !((r[WD_MODEL] == 0x8b && !r[WD_ANIMATED]) ||
        (r[WD_MODEL] >= 0x2bd && r[WD_MODEL] <= 0x401 && r[WD_ANIMATED])))
    return 0;
  if (r[WD_KIND] == WD_COIN &&
      (r[WD_MODEL] != 1 || r[WD_CLIP] != 4 || r[WD_ANIMATED]))
    return 0;
  if (r[WD_KIND] == WD_HEALTH &&
      (r[WD_MODEL] != 1 || r[WD_CLIP] != 3 || r[WD_ANIMATED]))
    return 0;
  if (r[WD_KIND] == WD_FOOD &&
      (r[WD_MODEL] != 0x85 || r[WD_CLIP] || r[WD_ANIMATED]))
    return 0;
  if (r[WD_KIND] == WD_HAZARD) {
    int phase = r[WD_PHASE];
    int model = phase <= 6 ? 0x191 : phase <= 10 ? 0x1a4 : 0x19b;
    int clip = (phase == 9 || phase == 10) ? 3 : 0;
    if (r[WD_MODEL] != model || r[WD_CLIP] != clip || !r[WD_ANIMATED])
      return 0;
  }
  return (r[WD_KIND] == WD_NPC && r[WD_PHASE] == 0) ||
         (r[WD_KIND] == WD_COIN &&
          (r[WD_PHASE] == 1 || r[WD_PHASE] == 2 || r[WD_PHASE] == 13)) ||
         (r[WD_KIND] == WD_HEALTH && (r[WD_PHASE] == 3 || r[WD_PHASE] == 14)) ||
         (r[WD_KIND] == WD_FOOD && r[WD_PHASE] == 4) ||
         (r[WD_KIND] == WD_HAZARD && r[WD_PHASE] >= 5 && r[WD_PHASE] <= 12);
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

int anchor_world_dynamic_encode(const int rows[][WORLD_DYNAMIC_WORDS],
                                unsigned int count, char *out,
                                unsigned int size) {
  Output o;
  unsigned int i, j;
  char n[12];
  if (!out || !size || count > WORLD_DYNAMIC_MAX)
    return 0;
  o.p = out;
  o.end = out + size - 1;
  o.ok = 1;
  text(&o, "{\"a\":[");
  for (i = 0; i < count; ++i) {
    if (!anchor_world_dynamic_row_valid(rows[i]))
      return 0;
    if (i)
      put(&o, ',');
    put(&o, '[');
    for (j = 0; j < WORLD_DYNAMIC_WORDS; ++j) {
      if (j)
        put(&o, ',');
      mnsg_string_write_s32(n, rows[i][j]);
      text(&o, n);
    }
    put(&o, ']');
  }
  text(&o, "]}");
  *o.p = 0;
  return o.ok;
}
int anchor_world_dynamic_decode(const char *json,
                                int rows[][WORLD_DYNAMIC_WORDS],
                                unsigned int *count, unsigned int *leader) {
  const char *p = json;
  unsigned int n = 0, i, j;
  int l;
  if (!p || !token(&p, '{') || !token(&p, '"') || !token(&p, 'a') ||
      !token(&p, '"') || !token(&p, ':') || !token(&p, '['))
    return 0;
  ws(&p);
  while (*p != ']') {
    if (n >= WORLD_DYNAMIC_MAX || (n && !token(&p, ',')) || !token(&p, '['))
      return 0;
    for (i = 0; i < WORLD_DYNAMIC_WORDS; ++i)
      if ((i && !token(&p, ',')) || !number(&p, &rows[n][i]))
        return 0;
    if (!token(&p, ']') || !anchor_world_dynamic_row_valid(rows[n]) ||
        !rows[n][WD_CID] || !rows[n][WD_SESSION] || !rows[n][WD_VISIT])
      return 0;
    for (j = 0; j < n; ++j)
      if (rows[j][0] == rows[n][0] && rows[j][1] == rows[n][1] &&
          rows[j][2] == rows[n][2] && rows[j][3] == rows[n][3])
        return 0;
    ++n;
    ws(&p);
  }
  if (!token(&p, ']') || !token(&p, ',') || !token(&p, '"') ||
      !token(&p, 'l') || !token(&p, '"') || !token(&p, ':') ||
      !number(&p, &l) || l < 0 || !token(&p, '}'))
    return 0;
  ws(&p);
  if (*p)
    return 0;
  *leader = (unsigned int)l;
  *count = n;
  return 1;
}
