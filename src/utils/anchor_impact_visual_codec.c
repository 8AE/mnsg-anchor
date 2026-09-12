#include "utils/anchor_impact_visual_codec.h"
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


int anchor_impact_visual_row_valid(const unsigned int *r)
{
    unsigned int i;
    if (!r[0] || !r[1] || r[1] > 121 || r[2] > 38 ||
        r[3] > 2 || (r[3] && !r[2]) || r[5] > 15 || (r[6] & ~0x3FF01u)) return 0;
    for (i = 7; i <= 16; ++i) {
        if (i >= 10 && i <= 12) { if (r[i] > 65535) return 0; }
        else if ((r[i] & 0x7FFFFFFFu) > 0x49742400u) return 0; /* finite, <=1e6 */
    }
    for (i = 17; i < 29; i += 2)
        if (r[i] > 0x876Fu || r[i+1] >= 0x800000u || (!r[i] && r[i+1])) return 0;
    return 1;
}
int anchor_impact_visual_decode(const char *json, AnchorImpactVisualFrame *frame)
{
    Input in; unsigned int n = 0, i, j, row;
    if (!json || !frame) return 0;
    while (n < ANCHOR_IMPACT_VISUAL_JSON && json[n]) ++n;
    if (n == ANCHOR_IMPACT_VISUAL_JSON) return 0;
    in.p = json; in.end = json+n; frame->count = 0;
    if (!take(&in, '[')) return 0;
    spaces(&in);
    if (in.p < in.end && *in.p != ']') {
        for (;;) {
            if (frame->count == ANCHOR_IMPACT_VISUAL_MAX ||
                (frame->count && !take(&in, ',')) || !take(&in, '[')) return 0;
            row = frame->count;
            for (j = 0; j < ANCHOR_IMPACT_VISUAL_WORDS; ++j)
                if ((j && !take(&in, ',')) || !integer(&in, &frame->rows[row][j])) return 0;
            if (!take(&in, ']') || !anchor_impact_visual_row_valid(frame->rows[row])) return 0;
            for (i = 0; i < row; ++i) if (((frame->rows[i][0]-1) & 63u) ==
                ((frame->rows[row][0]-1) & 63u)) return 0;
            ++frame->count; spaces(&in);
            if (in.p < in.end && *in.p == ']') break;
        }
    }
    if (!take(&in, ']')) return 0;
    spaces(&in); return in.p == in.end;
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


int anchor_impact_visual_encode(const AnchorImpactVisualFrame *frame, char *json, unsigned int capacity)
{
    Output out; unsigned int i;
    if (!frame || !json || capacity < 2 || frame->count > ANCHOR_IMPACT_VISUAL_MAX) return 0;
    out.p = json; out.end = json+capacity-1; out.valid = 1;
    literal(&out, "[");
    for (i = 0; i < frame->count; ++i) {
        if (!anchor_impact_visual_row_valid(frame->rows[i])) return 0;
        if (i) put(&out, ',');
        write_row(&out, frame->rows[i], ANCHOR_IMPACT_VISUAL_WORDS);
    }
    literal(&out, "]"); *out.p = 0; return out.valid;
}
