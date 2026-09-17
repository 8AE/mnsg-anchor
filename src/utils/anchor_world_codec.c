#include "anchor_world.h"
#include "utils/string_utils.h"

int anchor_world_row_valid(const int *r) {
  static const int lo[ANCHOR_WORLD_WORDS] = {
      0,       1,      1,      0,      -3276800, -3276800, -3276800, 0,
      0,       0,      0,      0,      0,        0,        -100000,  -100000,
      -100000, -32768, 0,      -32768, -32768,   -32768,   -32768,   -32768,
      -32768,  -32768, 0,      0,      0,        0,        0,        -32768,
      0,       0,      -32768, -32768, -32768,   0,        0};
  static const int hi[ANCHOR_WORLD_WORDS] = {255, 2047, 5, 1, 3276700, 3276700, 3276700, 1023, 1023, 1023, 255, 1000000, 65535, 7, 100000, 100000, 100000, 32767, 2147483647, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 64000, 64000, 64000, 31, 163, 32767, 7, 255, 32767, 32767, 32767, 255, 1};
  unsigned int i;
  for (i = 0; i < ANCHOR_WORLD_WORDS; ++i)
    if (r[i] < lo[i] || r[i] > hi[i])
      return 0;
  return 1;
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
