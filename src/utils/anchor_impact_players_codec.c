#include "utils/anchor_impact_players_codec.h"

typedef struct Input { const char *p, *end; } Input;
typedef struct Output { char *p, *end; int valid; } Output;

static void spaces(Input *in)
{
    while (in->p < in->end && (*in->p == ' ' || *in->p == '\n' ||
           *in->p == '\r' || *in->p == '\t')) ++in->p;
}

static int take(Input *in, char c)
{
    spaces(in);
    if (in->p == in->end || *in->p != c) return 0;
    ++in->p;
    return 1;
}

static int integer(Input *in, unsigned int *out)
{
    unsigned int v = 0, digit;
    spaces(in);
    if (in->p == in->end || *in->p < '0' || *in->p > '9') return 0;
    if (*in->p == '0' && in->p + 1 < in->end &&
        in->p[1] >= '0' && in->p[1] <= '9') return 0;
    do {
        digit = (unsigned int)(*in->p++ - '0');
        if (v > (0xffffffffu - digit) / 10u) return 0;
        v = v * 10u + digit;
    } while (in->p < in->end && *in->p >= '0' && *in->p <= '9');
    *out = v;
    return 1;
}

static int key(Input *in)
{
    char text[9];
    unsigned int n = 0;
    if (!take(in, '"')) return 0;
    while (in->p < in->end && *in->p != '"') {
        if (n == 8) return 0;
        text[n++] = *in->p++;
    }
    if (!take(in, '"') || !take(in, ':')) return 0;
    if (n == 1 && text[0] == 'c') return 2;
    if (n == 1 && text[0] == 'a') return 4;
    if (n == 8 && text[0] == 'a' && text[1] == 'c' && text[2] == 'c' &&
        text[3] == 'e' && text[4] == 'p' && text[5] == 't' &&
        text[6] == 'e' && text[7] == 'd') return 1;
    return 0;
}

static int finite_word(unsigned int value)
{
    return (value & 0x7f800000u) != 0x7f800000u;
}

static int rows(Input *in, unsigned int out[][10], unsigned int *count,
                unsigned int max, int attack)
{
    unsigned int i, j;
    *count = 0;
    if (!take(in, '[')) return 0;
    spaces(in);
    if (in->p < in->end && *in->p == ']') return take(in, ']');
    for (i = 0; i < max; ++i) {
        if ((i && !take(in, ',')) || !take(in, '[')) return 0;
        for (j = 0; j < 10; ++j)
            if ((j && !take(in, ',')) || !integer(in, &out[i][j])) return 0;
        if (!take(in, ']') || !out[i][0] || !out[i][1] || !out[i][2] ||
            (attack ? (out[i][3] < 1 || out[i][3] > 2) : out[i][3] > 1)) return 0;
        if (attack && out[i][3] == 2) {
            for (j = 4; j < 8; ++j) if (out[i][j] > 65535u) return 0;
            for (j = 8; j < 10; ++j)
                if (!out[i][j] || out[i][j] > 0x7fffffffu) return 0;
        } else for (j = 4; j < (attack ? 10u : 7u); ++j)
            if (!finite_word(out[i][j])) return 0;
        if (!attack)
            for (j = 7; j < 10; ++j) if (out[i][j] > 0xffffu) return 0;
        ++*count;
        spaces(in);
        if (in->p < in->end && *in->p == ']') return take(in, ']');
    }
    return 0;
}

int anchor_impact_players_decode(const char *json, AnchorImpactPlayerStatus *out)
{
    Input in;
    unsigned int n = 0, seen = 0;
    int field;
    if (!json || !out) return 0;
    while (n < ANCHOR_IMPACT_PLAYER_STATUS_SIZE && json[n]) ++n;
    if (n == ANCHOR_IMPACT_PLAYER_STATUS_SIZE) return 0;
    in.p = json; in.end = json + n;
    out->cursor_count = out->attack_count = out->accepted = 0;
    if (!take(&in, '{')) return 0;
    do {
        if (seen && !take(&in, ',')) return 0;
        field = key(&in);
        if (!field || (seen & (unsigned int)field)) return 0;
        seen |= (unsigned int)field;
        if (field == 1) { if (!integer(&in, &out->accepted)) return 0; }
        else if (field == 2) {
            if (!rows(&in, out->cursors, &out->cursor_count, ANCHOR_IMPACT_PLAYER_PEERS, 0)) return 0;
        } else if (!rows(&in, out->attacks, &out->attack_count, ANCHOR_IMPACT_PLAYER_BATCH, 1)) return 0;
    } while (seen != 7);
    if (!take(&in, '}')) return 0;
    spaces(&in);
    return in.p == in.end;
}

static void put(Output *out, char c)
{
    if (out->p == out->end) out->valid = 0;
    else *out->p++ = c;
}

static void literal(Output *out, const char *s)
{
    while (*s) put(out, *s++);
}

static void write_integer(Output *out, unsigned int v)
{
    char digits[10];
    unsigned int n = 0;
    do { digits[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) put(out, digits[--n]);
}

static void write_row(Output *out, const unsigned int *row, unsigned int n)
{
    unsigned int i;
    put(out, '[');
    for (i = 0; i < n; ++i) {
        if (i) put(out, ',');
        write_integer(out, row[i]);
    }
    put(out, ']');
}

int anchor_impact_players_encode(const AnchorImpactPlayerSample *sample,
                                char *json, unsigned int capacity)
{
    Output out;
    unsigned int i;
    if (!sample || !json || capacity < 2 || sample->attack_count > ANCHOR_IMPACT_PLAYER_BATCH) return 0;
    out.p = json; out.end = json + capacity - 1; out.valid = 1;
    literal(&out, "{\"c\":"); write_row(&out, sample->cursor, 7);
    literal(&out, ",\"a\":[");
    for (i = 0; i < sample->attack_count; ++i) {
        if (i) put(&out, ',');
        write_row(&out, sample->attacks[i], 8);
    }
    literal(&out, "]}");
    *out.p = 0;
    return out.valid;
}
