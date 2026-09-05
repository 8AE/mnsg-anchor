#include "anchor_projectiles.h"
#include "utils/json_utils.h"

int anchor_projectile_spawn_valid(const AnchorProjectileSpawn *v)
{
    return v && v->id > 0 && v->kind > 0 && v->kind <= 255 &&
           v->x100 >= -1000000000 && v->x100 <= 1000000000 &&
           v->y100 >= -1000000000 && v->y100 <= 1000000000 &&
           v->z100 >= -1000000000 && v->z100 <= 1000000000 &&
           v->vx100 >= -1000000 && v->vx100 <= 1000000 &&
           v->vy100 >= -1000000 && v->vy100 <= 1000000 &&
           v->vz100 >= -1000000 && v->vz100 <= 1000000 &&
           v->rx >= -32768 && v->rx <= 32767 &&
           v->ry >= -32768 && v->ry <= 32767 &&
           v->rz >= -32768 && v->rz <= 32767 &&
           v->scale100000 > 0 && v->scale100000 <= 1000000;
}

int anchor_projectile_spawn_encode(const AnchorProjectileSpawn *v,
                                    char *out, unsigned int capacity)
{
    MnsgJsonObjectWriter writer;
    if (!out || capacity == 0)
        return 0;
    out[0] = '\0';
    if (!anchor_projectile_spawn_valid(v))
        return 0;
    mnsg_json_writer_begin(&writer, out, capacity);
#define WRITE(field) mnsg_json_writer_add_s32(&writer, #field, v->field)
    if (!WRITE(id) || !WRITE(kind) || !WRITE(x100) || !WRITE(y100) || !WRITE(z100) ||
        !WRITE(vx100) || !WRITE(vy100) || !WRITE(vz100) ||
        !WRITE(rx) || !WRITE(ry) || !WRITE(rz) || !WRITE(scale100000) ||
        !mnsg_json_writer_finish(&writer))
    {
        out[0] = '\0';
        return 0;
    }
#undef WRITE
    return 1;
}

static int read_remote(const char *object, AnchorProjectileRemote *remote)
{
    AnchorProjectileSpawn *v = &remote->spawn;
#define READ(field) mnsg_json_get_s32(object, #field, &v->field)
    if (!READ(id) || !READ(kind) || !READ(x100) || !READ(y100) || !READ(z100) ||
        !READ(vx100) || !READ(vy100) || !READ(vz100) ||
        !READ(rx) || !READ(ry) || !READ(rz) || !READ(scale100000) ||
        !mnsg_json_get_s32(object, "cid", &remote->cid) ||
        !mnsg_json_get_s32(object, "session", &remote->session) ||
        !mnsg_json_get_s32(object, "epoch", &remote->epoch) ||
        !mnsg_json_get_s32(object, "age", &remote->age_ms))
        return 0;
#undef READ
    return anchor_projectile_spawn_valid(v) && remote->cid > 0 &&
           remote->session > 0 && remote->epoch > 0 &&
           remote->age_ms >= 0 && remote->age_ms <= 750;
}

int anchor_projectile_spawns_decode(const char *json, AnchorProjectileRemote *out,
                                     int capacity)
{
    int count = 0;
    if (!json || !out || capacity <= 0 || *json++ != '[')
        return 0;
    /* Python supplies sanitized flat objects; keep field lookup inside each
     * object's bounds so a missing value cannot bleed in from its neighbor. */
    while (*json && *json != ']' && count < capacity)
    {
        char object[512];
        unsigned int used = 0;
        AnchorProjectileRemote remote;
        if (*json != '{')
            return 0;
        do
        {
            if (!*json || *json == '[' || used + 1u >= sizeof(object))
                return 0;
            object[used++] = *json++;
        } while (object[used - 1u] != '}');
        object[used] = '\0';
        if (read_remote(object, &remote))
            out[count++] = remote;
        if (*json == ',')
            ++json;
        else if (*json != ']')
            return 0;
    }
    return count;
}
