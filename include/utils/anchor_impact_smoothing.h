#ifndef ANCHOR_IMPACT_SMOOTHING_H
#define ANCHOR_IMPACT_SMOOTHING_H

/* Presentation only, at the native 30 Hz tick: position, rotation (1024/turn),
 * scale, animation frame, hidden flag. New samples settle in three ticks. */
typedef struct AnchorImpactSmoothPose {
    unsigned int current[11], target[11];
    int valid, remaining;
} AnchorImpactSmoothPose;

static float impact_pose_float(unsigned int word)
{
    union { unsigned int word; float value; } u;
    u.word = word; return u.value;
}
static unsigned int impact_pose_bits(float value)
{
    union { unsigned int word; float value; } u;
    u.value = value; return u.word;
}
static void impact_pose_offer(AnchorImpactSmoothPose *p, const unsigned int *target)
{
    unsigned int i;
    int changed = 0, snap = !p->valid || p->current[10] != target[10];
    float distance = 0;
    for (i = 0; i < 3; ++i) {
        float d = impact_pose_float(target[i]) - impact_pose_float(p->current[i]);
        distance += d*d;
    }
    if (distance > 500.0f*500.0f) snap = 1;
    for (i = 0; i < 11; ++i) {
        if (target[i] != p->target[i]) changed = 1;
        p->target[i] = target[i];
        if (snap) p->current[i] = target[i];
    }
    if (snap) p->remaining = 0;
    else if (changed) p->remaining = 3;
    p->valid = 1;
}
static void impact_pose_step(AnchorImpactSmoothPose *p)
{
    unsigned int i;
    if (!p->remaining) return;
    for (i = 0; i < 10; ++i) {
        unsigned int a = p->current[i], b = p->target[i];
        if (p->remaining == 1 || a == b) p->current[i] = b;
        else if (i >= 3 && i < 6) {
            int delta = (int)((b - a + 512u) & 1023u) - 512;
            p->current[i] = (a == 0x8000 || b == 0x8000) ? b :
                (unsigned int)((int)a + delta/p->remaining) & 1023u;
        } else {
            float x = impact_pose_float(a), y = impact_pose_float(b);
            p->current[i] = i == 9 && (y < x || y-x > 30) ? b :
                impact_pose_bits(x + (y-x)/p->remaining);
        }
    }
    --p->remaining;
}
#endif
