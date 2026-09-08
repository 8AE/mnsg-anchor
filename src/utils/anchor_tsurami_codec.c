#include "utils/anchor_tsurami_codec.h"

typedef struct JsonCursor
{
    const char *p;
    const char *end;
} JsonCursor;

typedef struct JsonOutput
{
    char *p;
    char *end;
    int valid;
} JsonOutput;

static void whitespace(JsonCursor *c)
{
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\n' ||
           *c->p == '\r' || *c->p == '\t'))
        ++c->p;
}

static int token(JsonCursor *c, char expected)
{
    whitespace(c);
    if (c->p == c->end || *c->p != expected)
        return 0;
    ++c->p;
    return 1;
}

static int number(JsonCursor *c, unsigned int *out)
{
    unsigned int value = 0;
    int digits = 0;
    whitespace(c);
    if (c->p == c->end || *c->p < '0' || *c->p > '9')
        return 0;
    if (*c->p == '0' && c->p + 1 < c->end &&
        c->p[1] >= '0' && c->p[1] <= '9')
        return 0;
    while (c->p < c->end && *c->p >= '0' && *c->p <= '9')
    {
        unsigned int digit = (unsigned int)(*c->p++ - '0');
        if (++digits > 10 || value > (0xffffffffu - digit) / 10u)
            return 0;
        value = value * 10u + digit;
    }
    *out = value;
    return 1;
}

static int name(JsonCursor *c, char *out, unsigned int capacity)
{
    unsigned int n = 0;
    if (!token(c, '"'))
        return 0;
    while (c->p < c->end && *c->p != '"')
    {
        if (n + 1 >= capacity || *c->p < ' ' || *c->p == '\\')
            return 0;
        out[n++] = *c->p++;
    }
    out[n] = 0;
    return token(c, '"') && token(c, ':');
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
        ++a, ++b;
    return *a == *b;
}

static int words(JsonCursor *c, unsigned int *out, unsigned int count)
{
    unsigned int i;
    if (!token(c, '['))
        return 0;
    for (i = 0; i < count; ++i)
        if ((i && !token(c, ',')) || !number(c, &out[i]))
            return 0;
    return token(c, ']');
}

static int projectiles(JsonCursor *c, AnchorTsuramiNativeSnapshot *out)
{
    unsigned int i, j;
    out->projectile_count = 0;
    if (!token(c, '['))
        return 0;
    whitespace(c);
    if (c->p < c->end && *c->p == ']')
        return token(c, ']');
    for (i = 0; i < ANCHOR_TSURAMI_MAX_PROJECTILES; ++i)
    {
        for (j = 0; j < ANCHOR_TSURAMI_PROJECTILE_WORDS; ++j)
            if (((i || j) && !token(c, ',')) ||
                !number(c, &out->projectile[i][j]))
                return 0;
        ++out->projectile_count;
        whitespace(c);
        if (c->p < c->end && *c->p == ']')
            return token(c, ']');
    }
    return 0;
}

static int snapshot(JsonCursor *c, AnchorTsuramiNativeSnapshot *out)
{
    char key[8];
    unsigned int seen = 0, bit;
    int first = 1;
    if (!token(c, '{'))
        return 0;
    for (;;)
    {
        whitespace(c);
        if (c->p < c->end && *c->p == '}')
            return token(c, '}') && seen == 31u;
        if ((!first && !token(c, ',')) || !name(c, key, sizeof(key)))
            return 0;
        first = 0;
        if (same(key, "r"))
        {
            bit = 1;
            if (!words(c, out->root, ANCHOR_TSURAMI_ROOT_WORDS))
                return 0;
        }
        else if (same(key, "v"))
        {
            bit = 2;
            if (!words(c, out->visual, ANCHOR_TSURAMI_VISUAL_WORDS))
                return 0;
        }
        else if (same(key, "s"))
        {
            bit = 4;
            if (!number(c, &out->projectile_serial))
                return 0;
        }
        else if (same(key, "t"))
        {
            bit = 8;
            if (!number(c, &out->tick))
                return 0;
        }
        else if (same(key, "p"))
        {
            bit = 16;
            if (!projectiles(c, out))
                return 0;
        }
        else
            return 0;
        if (seen & bit)
            return 0;
        seen |= bit;
    }
}

static int hits(JsonCursor *c, AnchorTsuramiStatus *out)
{
    int i, j;
    unsigned int values[6];
    out->hit_count = 0;
    if (!token(c, '['))
        return 0;
    whitespace(c);
    if (c->p < c->end && *c->p == ']')
        return token(c, ']');
    for (i = 0; i < ANCHOR_TSURAMI_HIT_BATCH; ++i)
    {
        if ((i && !token(c, ',')) || !words(c, values, 6))
            return 0;
        for (j = 0; j < 6; ++j)
        {
            if ((j < 5 && !values[j]) || values[j] > 0x7fffffffu)
                return 0;
            out->hits[i][j] = (int)values[j];
        }
        if (values[4] != 1u && values[4] != 2u && values[4] != 3u &&
            values[4] != 4u && values[4] != 8u)
            return 0;
        ++out->hit_count;
        whitespace(c);
        if (c->p < c->end && *c->p == ']')
            return token(c, ']');
    }
    return 0;
}

int anchor_tsurami_status_decode(const char *json,
                                     AnchorTsuramiStatus *out)
{
    JsonCursor c;
    char key[16];
    unsigned int length = 0, seen = 0, bit, value;
    int first = 1;
    if (!json || !out)
        return 0;
    while (length < ANCHOR_TSURAMI_STATUS_JSON_SIZE && json[length])
        ++length;
    if (length == ANCHOR_TSURAMI_STATUS_JSON_SIZE)
        return 0;
    c.p = json;
    c.end = json + length;
    out->has_state = 0;
    out->hit_count = 0;
    if (!token(&c, '{'))
        return 0;
    for (;;)
    {
        whitespace(&c);
        if (c.p < c.end && *c.p == '}')
            break;
        if ((!first && !token(&c, ',')) || !name(&c, key, sizeof(key)))
            return 0;
        first = 0;
        if (same(key, "state"))
        {
            bit = 1;
            whitespace(&c);
            if (c.end - c.p >= 4 && c.p[0] == 'n' && c.p[1] == 'u' &&
                c.p[2] == 'l' && c.p[3] == 'l')
                c.p += 4;
            else if (snapshot(&c, &out->state))
                out->has_state = 1;
            else
                return 0;
        }
        else if (same(key, "encounter"))
        {
            bit = 2;
            if (!words(&c, out->encounter, 3))
                return 0;
        }
        else if (same(key, "hits"))
        {
            bit = 4;
            if (!hits(&c, out))
                return 0;
        }
        else
        {
            if (!number(&c, &value) || value > 0x7fffffffu)
                return 0;
            if (same(key, "role"))
                bit = 8, out->role = (int)value;
            else if (same(key, "owner"))
                bit = 16, out->owner = (int)value;
            else if (same(key, "term"))
                bit = 32, out->term = value;
            else if (same(key, "revision"))
                bit = 64, out->revision = value;
            else if (same(key, "paused"))
                bit = 128, out->paused = (int)value;
            else
                return 0;
        }
        if (seen & bit)
            return 0;
        seen |= bit;
    }
    if (!token(&c, '}'))
        return 0;
    whitespace(&c);
    if (c.p != c.end || seen != 255u || out->role > 2 || out->paused > 1)
        return 0;
    if (out->role && (!out->owner || !out->term ||
        !out->encounter[0] || !out->encounter[1] || !out->encounter[2] ||
        out->encounter[0] > 0x7fffffffu || out->encounter[1] > 0x7fffffffu ||
        out->encounter[2] > 0x7fffffffu))
        return 0;
    return out->role == 1 || !out->hit_count;
}

static void emit(JsonOutput *w, char value)
{
    if (w->p >= w->end)
        w->valid = 0;
    else
        *w->p++ = value;
}

static void literal(JsonOutput *w, const char *value)
{
    while (*value)
        emit(w, *value++);
}

static void integer(JsonOutput *w, unsigned int value)
{
    char digits[10];
    unsigned int n = 0;
    do
    {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value);
    while (n)
        emit(w, digits[--n]);
}

int anchor_tsurami_state_encode(const AnchorTsuramiNativeSnapshot *state,
                                    char *out, unsigned int capacity)
{
    JsonOutput w;
    unsigned int i, j;
    if (!out || !capacity)
        return 0;
    out[0] = 0;
    if (!state || state->projectile_count > ANCHOR_TSURAMI_MAX_PROJECTILES)
        return 0;
    w.p = out;
    w.end = out + capacity - 1;
    w.valid = 1;
    literal(&w, "{\"r\":[");
    for (i = 0; i < ANCHOR_TSURAMI_ROOT_WORDS; ++i)
    {
        if (i) emit(&w, ',');
        integer(&w, state->root[i]);
    }
    literal(&w, "],\"v\":[");
    for (i = 0; i < ANCHOR_TSURAMI_VISUAL_WORDS; ++i)
    {
        if (i) emit(&w, ',');
        integer(&w, state->visual[i]);
    }
    literal(&w, "],\"s\":");
    integer(&w, state->projectile_serial);
    literal(&w, ",\"t\":");
    integer(&w, state->tick);
    literal(&w, ",\"p\":[");
    for (i = 0; i < state->projectile_count; ++i)
        for (j = 0; j < ANCHOR_TSURAMI_PROJECTILE_WORDS; ++j)
        {
            if (i || j) emit(&w, ',');
            integer(&w, state->projectile[i][j]);
        }
    literal(&w, "]}");
    *w.p = 0;
    if (!w.valid)
        out[0] = 0;
    return w.valid;
}
