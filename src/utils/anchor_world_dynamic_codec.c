#include "anchor_world_dynamic.h"
#include "utils/string_utils.h"

int anchor_world_dynamic_row_valid(const int *r) {
  static const int lo[WORLD_DYNAMIC_WORDS] = {
      0, 0,      0, 0,       0,        0,        1,        0,
      0, 0,      0, 0,       -3276800, -3276800, -3276800, 0,
      0, 0,      0, 0,       0,        -100000,  -100000,  -100000,
      0, 0,      0, 0,       0,        0,        0,        -32768,
      0, -32768, 0, 0,       -32768,   -32768,   -32768,   0,
      0, 0,      0, -100000, 0,        -32768,   0,        0,
      0, -32768, 0, 0,       -32768,   0,        0,        0,
      0, 0,      0, 0,       0,        -3276800, -3276800, -3276800,
      0, 0,      0, 0,       0,        0,        0,        -32768,
      0, 0};
  static const int hi[WORLD_DYNAMIC_WORDS] = {
      2147483647, 2147483647, 2147483647, 2147483647, 2147483647, 2,
      5,          256,        1025,       1025,       255,        1,
      3276700,    3276700,    3276700,    1023,       1023,       1023,
      1000000,    65535,      7,          100000,     100000,     100000,
      64000,      64000,      64000,      65535,      65535,      65535,
      65535,      32767,      163,        32767,      7,          255,
      32767,      32767,      32767,      255,        1,          65535,
      14,         100000,     1,          32767,      1023,       65535,
      65535,      32767,      65535,      65535,      32767,      255,
      65535,      255,        255,        255,        255,        100000,
      100000,     3276700,    3276700,    3276700,    2147483647, 1,
      1,          17,         255,        255,        65535,      32767,
      2147483647, 255};
  unsigned int i;
  for (i = 0; i < WORLD_DYNAMIC_WORDS; ++i)
    if (r[i] < lo[i] || r[i] > hi[i])
      return 0;
  if (!r[WD_SERIAL] || (r[WD_LIFE] == WD_CLAIM &&
                        (r[WD_KIND] < WD_COIN || r[WD_KIND] > WD_FOOD)))
    return 0;
  if (r[WD_LIFE] == WD_REMOVED)
    return 1;
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
