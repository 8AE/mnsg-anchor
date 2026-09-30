#include "bosses/control_machine/anchor_control_machine_codec.h"

typedef struct JsonCursor { const char *p, *end; } JsonCursor;
typedef struct JsonOutput { char *p, *end; int valid; } JsonOutput;

static void whitespace(JsonCursor *c)
{
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\n' ||
           *c->p == '\r' || *c->p == '\t')) ++c->p;
}

static int token(JsonCursor *c, char expected)
{
    whitespace(c);
    if (c->p == c->end || *c->p != expected) return 0;
    ++c->p;
    return 1;
}

static int number(JsonCursor *c, unsigned int *out)
{
    unsigned int value = 0, digit, count = 0;
    whitespace(c);
    if (c->p == c->end || *c->p < '0' || *c->p > '9') return 0;
    if (*c->p == '0' && c->p + 1 < c->end &&
        c->p[1] >= '0' && c->p[1] <= '9') return 0;
    while (c->p < c->end && *c->p >= '0' && *c->p <= '9') {
        digit = (unsigned int)(*c->p++ - '0');
        if (++count > 10 || value > (0xffffffffu - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    *out = value;
    return 1;
}

static int name(JsonCursor *c, char *out, unsigned int capacity)
{
    unsigned int n = 0;
    if (!token(c, '"')) return 0;
    while (c->p < c->end && *c->p != '"') {
        if (n + 1 >= capacity || *c->p < ' ' || *c->p == '\\') return 0;
        out[n++] = *c->p++;
    }
    out[n] = 0;
    return token(c, '"') && token(c, ':');
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) ++a, ++b;
    return *a == *b;
}

static int words(JsonCursor *c, unsigned int *out, unsigned int count)
{
    unsigned int i;
    if (!token(c, '[')) return 0;
    for (i = 0; i < count; ++i)
        if ((i && !token(c, ',')) || !number(c, &out[i])) return 0;
    return token(c, ']');
}

static int projectiles(JsonCursor *c, AnchorControlMachineSnapshot *out)
{
    unsigned int i;
    out->projectile_count = 0;
    if (!token(c, '[')) return 0;
    whitespace(c);
    if (c->p < c->end && *c->p == ']') return token(c, ']');
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES; ++i) {
        if ((i && !token(c, ',')) ||
            !words(c, out->projectile[i],
                   ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS)) return 0;
        ++out->projectile_count;
        whitespace(c);
        if (c->p < c->end && *c->p == ']') return token(c, ']');
    }
    return 0;
}

static int snapshot(JsonCursor *c, AnchorControlMachineSnapshot *out)
{
    char key[8];
    unsigned int seen = 0, bit, value;
    int first = 1;
    if (!token(c, '{')) return 0;
    for (;;) {
        whitespace(c);
        if (c->p < c->end && *c->p == '}')
            return token(c, '}') && seen == 7u &&
                   anchor_control_machine_snapshot_valid(out);
        if ((!first && !token(c, ',')) || !name(c, key, sizeof(key))) return 0;
        first = 0;
        if (same(key, "fight")) {
            bit = 1;
            if (!number(c, &value) || value != 2u) return 0;
        } else if (same(key, "r")) {
            bit = 2;
            if (!words(c, out->root, ANCHOR_CONTROL_MACHINE_ROOT_WORDS)) return 0;
        } else if (same(key, "p")) {
            bit = 4;
            if (!projectiles(c, out)) return 0;
        } else return 0;
        if (seen & bit) return 0;
        seen |= bit;
    }
}

static int maybe_snapshot(JsonCursor *c, AnchorControlMachineSnapshot *out,
                          int *present)
{
    whitespace(c);
    *present = 0;
    if (c->end - c->p >= 4 && c->p[0] == 'n' && c->p[1] == 'u' &&
        c->p[2] == 'l' && c->p[3] == 'l') {
        c->p += 4;
        return 1;
    }
    if (!snapshot(c, out)) return 0;
    *present = 1;
    return 1;
}

static int hits(JsonCursor *c, AnchorControlMachineStatus *out)
{
    unsigned int i, j, values[5];
    out->hit_count = 0;
    if (!token(c, '[')) return 0;
    whitespace(c);
    if (c->p < c->end && *c->p == ']') return token(c, ']');
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_HIT_BATCH; ++i) {
        if ((i && !token(c, ',')) || !words(c, values, 5)) return 0;
        for (j = 0; j < 5; ++j) {
            if (!values[j] || values[j] > 0x7fffffffu) return 0;
            out->hits[i][j] = (int)values[j];
        }
        if (values[4] != 1u) return 0;
        ++out->hit_count;
        whitespace(c);
        if (c->p < c->end && *c->p == ']') return token(c, ']');
    }
    return 0;
}

int anchor_control_machine_status_decode(const char *json,
                                         AnchorControlMachineStatus *out)
{
    JsonCursor c;
    char key[16];
    unsigned int length = 0, seen = 0, bit, value;
    int first = 1;
    if (!json || !out) return 0;
    while (length < ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE && json[length])
        ++length;
    if (length == ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE) return 0;
    c.p = json;
    c.end = json + length;
    out->has_state = out->has_preview = out->hit_count = 0;
    if (!token(&c, '{')) return 0;
    for (;;) {
        whitespace(&c);
        if (c.p < c.end && *c.p == '}') break;
        if ((!first && !token(&c, ',')) || !name(&c, key, sizeof(key))) return 0;
        first = 0;
        if (same(key, "state")) {
            bit = 1;
            if (!maybe_snapshot(&c, &out->state, &out->has_state)) return 0;
        } else if (same(key, "preview")) {
            bit = 2;
            if (!maybe_snapshot(&c, &out->preview, &out->has_preview)) return 0;
        } else if (same(key, "encounter")) {
            bit = 4;
            if (!words(&c, out->encounter, 3)) return 0;
        } else if (same(key, "hits")) {
            bit = 8;
            if (!hits(&c, out)) return 0;
        } else {
            if (!number(&c, &value) || value > 0x7fffffffu) return 0;
            if (same(key, "role")) bit = 16, out->role = (int)value;
            else if (same(key, "owner")) bit = 32, out->owner = (int)value;
            else if (same(key, "term")) bit = 64, out->term = value;
            else if (same(key, "revision")) bit = 128, out->revision = value;
            else if (same(key, "paused")) bit = 256, out->paused = (int)value;
            else return 0;
        }
        if (seen & bit) return 0;
        seen |= bit;
    }
    if (!token(&c, '}')) return 0;
    whitespace(&c);
    if (c.p != c.end || seen != 511u || out->role > 2 || out->paused > 1)
        return 0;
    if (out->role && (!out->owner || !out->term ||
        !out->encounter[0] || !out->encounter[1] || !out->encounter[2])) return 0;
    if (out->role != 1 && out->hit_count) return 0;
    if (out->has_state && out->has_preview) return 0;
    return 1;
}

static void emit(JsonOutput *w, char value)
{
    if (w->p >= w->end) w->valid = 0;
    else *w->p++ = value;
}

static void literal(JsonOutput *w, const char *s)
{
    while (*s) emit(w, *s++);
}

static void integer(JsonOutput *w, unsigned int value)
{
    char digits[10];
    unsigned int n = 0;
    do { digits[n++] = (char)('0' + value % 10u); value /= 10u; } while (value);
    while (n) emit(w, digits[--n]);
}

int anchor_control_machine_state_encode(const AnchorControlMachineSnapshot *state,
                                        char *out, unsigned int capacity)
{
    JsonOutput w;
    unsigned int i, j;
    if (!out || !capacity) return 0;
    out[0] = 0;
    if (!anchor_control_machine_snapshot_valid(state)) return 0;
    w.p = out;
    w.end = out + capacity - 1;
    w.valid = 1;
    literal(&w, "{\"fight\":2,\"r\":[");
    for (i = 0; i < ANCHOR_CONTROL_MACHINE_ROOT_WORDS; ++i) {
        if (i) emit(&w, ',');
        integer(&w, state->root[i]);
    }
    literal(&w, "],\"p\":[");
    for (i = 0; i < state->projectile_count; ++i) {
        if (i) emit(&w, ',');
        emit(&w, '[');
        for (j = 0; j < ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS; ++j) {
            if (j) emit(&w, ',');
            integer(&w, state->projectile[i][j]);
        }
        emit(&w, ']');
    }
    literal(&w, "]}");
    *w.p = 0;
    if (!w.valid) out[0] = 0;
    return w.valid;
}
