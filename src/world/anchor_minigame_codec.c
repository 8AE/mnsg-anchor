#include "world/anchor_minigame_codec.h"

void anchor_minigame_zero(void *memory, unsigned int size)
{
    volatile unsigned char *bytes = memory;
    while (size--) *bytes++ = 0;
}

typedef struct { const char *p, *end; } Cursor;
typedef struct { char *p, *end; int ok; } Output;
static AnchorMinigameStatus s_decoded;

static void space(Cursor *c)
{
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\n' ||
           *c->p == '\r' || *c->p == '\t')) ++c->p;
}
static int take(Cursor *c, char ch)
{
    space(c);
    if (c->p == c->end || *c->p != ch) return 0;
    ++c->p;
    return 1;
}
static int number(Cursor *c, int *out)
{
    unsigned int n = 0, digit, limit;
    int negative = 0;
    space(c);
    if (c->p < c->end && *c->p == '-') negative = 1, ++c->p;
    if (c->p == c->end || *c->p < '0' || *c->p > '9') return 0;
    if (*c->p == '0' && c->p + 1 < c->end && c->p[1] >= '0' && c->p[1] <= '9') return 0;
    limit = negative ? 0x80000000u : 0x7fffffffu;
    while (c->p < c->end && *c->p >= '0' && *c->p <= '9') {
        digit = (unsigned int)(*c->p++ - '0');
        if (n > (limit - digit) / 10u) return 0;
        n = n * 10u + digit;
    }
    *out = negative ? (n == 0x80000000u ? (-2147483647 - 1) : -(int)n) : (int)n;
    return 1;
}
static int key(Cursor *c, char name[16])
{
    unsigned int n = 0;
    if (!take(c, '"')) return 0;
    while (c->p < c->end && *c->p != '"') {
        if (n >= 15 || *c->p < ' ' || *c->p == '\\') return 0;
        name[n++] = *c->p++;
    }
    name[n] = 0;
    return take(c, '"') && take(c, ':');
}
static int equal(const char *a, const char *b)
{
    while (*a && *a == *b) ++a, ++b;
    return *a == *b;
}
static int row(Cursor *c, int *out, unsigned int *count, unsigned int min,
               unsigned int max)
{
    unsigned int n = 0;
    if (!take(c, '[')) return 0;
    space(c);
    if (c->p < c->end && *c->p == ']') { *count = 0; return !min && take(c, ']'); }
    while (n < max) {
        if ((n && !take(c, ',')) || !number(c, &out[n++])) return 0;
        space(c);
        if (c->p < c->end && *c->p == ']') {
            *count = n;
            return n >= min && take(c, ']');
        }
    }
    return 0;
}
static int actors(Cursor *c, AnchorMinigameState *out)
{
    unsigned int n = 0;
    if (!take(c, '[')) return 0;
    space(c);
    if (c->p < c->end && *c->p == ']') { out->actor_count = 0; return take(c, ']'); }
    while (n < ANCHOR_MINIGAME_MAX_ACTORS) {
        if ((n && !take(c, ',')) || !row(c, out->actors[n], &out->actor_words[n],
                                       3, ANCHOR_MINIGAME_MAX_ACTOR_WORDS)) return 0;
        ++n;
        space(c);
        if (c->p < c->end && *c->p == ']') {
            out->actor_count = n;
            return take(c, ']');
        }
    }
    return 0;
}
static int inputs(Cursor *c, AnchorMinigameStatus *out)
{
    unsigned int n = 0, j;
    if (!take(c, '[')) return 0;
    space(c);
    if (c->p < c->end && *c->p == ']') { out->input_count = 0; return take(c, ']'); }
    while (n < ANCHOR_MINIGAME_MAX_INPUTS) {
        if ((n && !take(c, ',')) || !row(c, out->inputs[n], &out->input_words[n],
                                       5, ANCHOR_MINIGAME_RECEIVED_INPUT_WORDS)) return 0;
        for (j = 0; j < 5; ++j) if (out->inputs[n][j] <= 0) return 0;
        ++n;
        space(c);
        if (c->p < c->end && *c->p == ']') { out->input_count = n; return take(c, ']'); }
    }
    return 0;
}

int anchor_minigame_state_valid(const AnchorMinigameState *s)
{
    unsigned int i, j;
    if (!s || s->ready < 0 || s->ready > 1 || s->running < 0 || s->running > 1 ||
        s->edge < 0 || s->edge > 1 || s->result < 0 || s->result > 2 ||
        !s->controller_count || s->controller_count > ANCHOR_MINIGAME_MAX_WORDS ||
        s->actor_count > ANCHOR_MINIGAME_MAX_ACTORS ||
        s->input_count > ANCHOR_MINIGAME_MAX_INPUTS ||
        s->input_ack_count > ANCHOR_MINIGAME_MAX_INPUTS ||
        s->adopted_generation > 0x7fffffffu || s->local_ack > 0x7fffffffu ||
        s->full_tick > 0x7fffffffu || s->source_tick > 0x7fffffffu) return 0;
    for (i = 0; i < s->actor_count; ++i) {
        if (s->actor_words[i] < 3 || s->actor_words[i] > ANCHOR_MINIGAME_MAX_ACTOR_WORDS ||
            s->actors[i][0] < 0 || s->actors[i][1] < 0 || s->actors[i][2] < 0) return 0;
        for (j = 0; j < i; ++j)
            if (s->actors[i][0] == s->actors[j][0] &&
                s->actors[i][1] == s->actors[j][1] &&
                s->actors[i][2] == s->actors[j][2]) return 0;
    }
    for (i = 0; i < s->input_count; ++i)
        if (s->input_words[i] < 2 || s->input_words[i] > ANCHOR_MINIGAME_INPUT_WORDS ||
            s->inputs[i][0] <= 0 || s->inputs[i][1] <= 0) return 0;
    for (i = 0; i < s->input_ack_count; ++i) {
        for (j = 0; j < 4; ++j) if (s->input_ack[i][j] <= 0) return 0;
        for (j = 0; j < i; ++j)
            if (s->input_ack[i][0] == s->input_ack[j][0] &&
                s->input_ack[i][1] == s->input_ack[j][1] &&
                s->input_ack[i][2] == s->input_ack[j][2]) return 0;
    }
    return 1;
}

int anchor_minigame_status_decode(const char *json, AnchorMinigameStatus *out)
{
    Cursor c;
    AnchorMinigameStatus *s = &s_decoded;
    unsigned int length = 0, seen = 0, bit, n, i;
    char name[16];
    int first = 1, value, values[4];
    if (!json || !out) return 0;
    while (length < ANCHOR_MINIGAME_JSON_SIZE && json[length]) ++length;
    if (length == ANCHOR_MINIGAME_JSON_SIZE) return 0;
    anchor_minigame_zero(s, sizeof(*s));
    c.p = json; c.end = json + length;
    if (!take(&c, '{')) return 0;
    for (;;) {
        space(&c);
        if (c.p < c.end && *c.p == '}') break;
        if ((!first && !take(&c, ',')) || !key(&c, name)) return 0;
        first = 0;
        if (equal(name, "token")) {
            bit = 8;
            if (!row(&c, values, &n, 0, 4) || (n != 0 && n != 4)) return 0;
            for (i = 0; i < n; ++i) {
                if (values[i] <= 0) return 0;
                s->token[i] = (unsigned int)values[i];
            }
        } else if (equal(name, "c")) {
            bit = 256;
            if (!row(&c, s->state.controller, &s->state.controller_count,
                     1, ANCHOR_MINIGAME_MAX_WORDS)) return 0;
        } else if (equal(name, "a")) {
            bit = 512;
            if (!actors(&c, &s->state)) return 0;
        } else if (equal(name, "inputs")) {
            bit = 1024;
            if (!inputs(&c, s)) return 0;
        } else if (equal(name, "members")) {
            bit = 4096;
            if (!row(&c, s->participants, &s->participant_count,
                     0, ANCHOR_MINIGAME_MAX_PARTICIPANTS)) return 0;
            for (i = 0; i < s->participant_count; ++i)
                if (s->participants[i] <= 0 ||
                    (i && s->participants[i] <= s->participants[i - 1])) return 0;
        } else {
            if (!number(&c, &value) || value < 0) return 0;
            if (equal(name, "ready")) bit = 1, s->ready = value;
            else if (equal(name, "owner")) bit = 2, s->owner = value;
            else if (equal(name, "isOwner")) bit = 4, s->is_owner = value;
            else if (equal(name, "generation")) bit = 16, s->generation = (unsigned int)value;
            else if (equal(name, "tick")) bit = 32, s->tick = (unsigned int)value;
            else if (equal(name, "result")) bit = 64, s->result = value;
            else if (equal(name, "takeover")) bit = 128, s->takeover = value;
            else if (equal(name, "running")) bit = 2048, s->state.running = value;
            else if (equal(name, "localAck")) bit = 8192, s->local_ack = (unsigned int)value;
            else if (equal(name, "fullTick")) bit = 16384, s->full_tick = (unsigned int)value;
            else return 0;
        }
        if (seen & bit) return 0;
        seen |= bit;
    }
    if (!take(&c, '}')) return 0;
    space(&c);
    if (c.p != c.end || (seen & 31u) != 31u || s->ready > 1 || s->is_owner > 1 ||
        s->takeover > 1 || s->result > 2 || s->state.running > 1 ||
        ((seen & 2816u) != 0 && (seen & 2816u) != 2816u)) return 0;
    if (s->owner) {
        if (s->token[0] < 1 || s->token[0] > 4 || !s->token[1] ||
            !s->token[2] || !s->token[3] || !s->generation) return 0;
    } else if (s->ready || s->is_owner || s->generation || s->token[0] ||
               s->result || s->input_count || s->local_ack || s->full_tick ||
               s->participant_count || (seen & 768u)) return 0;
    if (!s->is_owner && s->input_count) return 0;
    s->has_state = (seen & 768u) == 768u;
    /* c/a/running is emitted only after transport assembly of a ready owner's
     * complete graph. status.ready describes this participant's readiness,
     * which remains zero until this checkpoint bootstraps its native graph. */
    s->state.ready = s->has_state;
    s->state.result = s->result;
    s->state.local_ack = s->local_ack;
    s->state.full_tick = s->full_tick;
    s->state.source_tick = s->tick;
    if (s->has_state && ((!s->tick && !(s->takeover && s->is_owner &&
        s->generation >= 2u)) || !anchor_minigame_state_valid(&s->state))) return 0;
    *out = *s;
    return 1;
}

static void emit(Output *o, char c)
{
    if (o->p == o->end) o->ok = 0;
    else *o->p++ = c;
}
static void literal(Output *o, const char *s) { while (*s) emit(o, *s++); }
static void integer(Output *o, int value)
{
    char digits[10];
    unsigned int n = 0, v;
    if (value < 0) emit(o, '-');
    v = value < 0 ? 0u - (unsigned int)value : (unsigned int)value;
    do { digits[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) emit(o, digits[--n]);
}
static void write_row(Output *o, const int *values, unsigned int n)
{
    unsigned int i;
    emit(o, '[');
    for (i = 0; i < n; ++i) { if (i) emit(o, ','); integer(o, values[i]); }
    emit(o, ']');
}
int anchor_minigame_state_encode(const AnchorMinigameState *s,
                                char *out, unsigned int capacity)
{
    Output o;
    unsigned int i;
    if (!out || !capacity) return 0;
    out[0] = 0;
    if (!anchor_minigame_state_valid(s)) return 0;
    o.p = out; o.end = out + capacity - 1; o.ok = 1;
    literal(&o, "{\"c\":"); write_row(&o, s->controller, s->controller_count);
    literal(&o, ",\"a\":[");
    for (i = 0; i < s->actor_count; ++i) {
        if (i) emit(&o, ',');
        write_row(&o, s->actors[i], s->actor_words[i]);
    }
    literal(&o, "],\"inputs\":[");
    for (i = 0; i < s->input_count; ++i) {
        if (i) emit(&o, ',');
        write_row(&o, s->inputs[i], s->input_words[i]);
    }
    literal(&o, "],\"inputAck\":[");
    for (i = 0; i < s->input_ack_count; ++i) {
        if (i) emit(&o, ',');
        write_row(&o, s->input_ack[i], 4);
    }
    literal(&o, "],\"adoptedGeneration\":"); integer(&o, (int)s->adopted_generation);
    literal(&o, ",\"ready\":"); integer(&o, s->ready);
    literal(&o, ",\"running\":"); integer(&o, s->running);
    literal(&o, ",\"edge\":"); integer(&o, s->edge);
    literal(&o, ",\"result\":"); integer(&o, s->result); emit(&o, '}');
    if (!o.ok) { out[0] = 0; return 0; }
    *o.p = 0;
    return 1;
}
