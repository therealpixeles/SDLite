#pragma once

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

static inline Vec2 vec2(float x, float y)
{
    Vec2 v = { x, y };
    return v;
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b)
{
    return vec2(a.x + b.x, a.y + b.y);
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b)
{
    return vec2(a.x - b.x, a.y - b.y);
}

static inline Vec2 vec2_scale(Vec2 v, float s)
{
    return vec2(v.x * s, v.y * s);
}

/* ================= CAMERA ================= */

static inline Vec2 cam_apply(Vec2 world_pos, Vec2 cam)
{
    // world -> screen
    return vec2(world_pos.x - cam.x, world_pos.y - cam.y);
}

static inline Vec2 cam_unapply(Vec2 screen_pos, Vec2 cam)
{
    // screen -> world
    return vec2(screen_pos.x + cam.x, screen_pos.y + cam.y);
}
