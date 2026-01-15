#include "Physics.h"

/* Rect vs rect */
int rect_overlaps(float ax, float ay, float aw, float ah,
                  float bx, float by, float bw, float bh)
{
    return !(ax + aw <= bx ||
             ax >= bx + bw ||
             ay + ah <= by ||
             ay >= by + bh);
}

/* Point vs rect */
int point_in_rect(float px, float py,
                  float rx, float ry, float rw, float rh)
{
    return (px >= rx && px < rx + rw &&
            py >= ry && py < ry + rh);
}

/* Clamp rect inside window */
void rect_clamp_to_window(float* x, float* y,
                          float w, float h,
                          float win_w, float win_h)
{
    if (*x < 0.0f) *x = 0.0f;
    if (*y < 0.0f) *y = 0.0f;

    if (*x + w > win_w) *x = win_w - w;
    if (*y + h > win_h) *y = win_h - h;
}

/* Simple window bounce */
void rect_bounce_window(float* x, float* y,
                         float* vx, float* vy,
                         float w, float h,
                         float win_w, float win_h)
{
    if (*x < 0.0f)
    {
        *x = 0.0f;
        *vx = -*vx;
    }
    else if (*x + w > win_w)
    {
        *x = win_w - w;
        *vx = -*vx;
    }

    if (*y < 0.0f)
    {
        *y = 0.0f;
        *vy = -*vy;
    }
    else if (*y + h > win_h)
    {
        *y = win_h - h;
        *vy = -*vy;
    }
}

void cam_follow(float* cam_x, float* cam_y,
                float target_x, float target_y,
                float view_w, float view_h)
{
    // center camera on target
    *cam_x = target_x - view_w * 0.5f;
    *cam_y = target_y - view_h * 0.5f;
}

void cam_clamp(float* cam_x, float* cam_y,
               float view_w, float view_h,
               float world_w, float world_h)
{
    // if world smaller than view, lock to 0
    if (world_w <= view_w) *cam_x = 0.0f;
    if (world_h <= view_h) *cam_y = 0.0f;

    if (*cam_x < 0.0f) *cam_x = 0.0f;
    if (*cam_y < 0.0f) *cam_y = 0.0f;

    if (*cam_x + view_w > world_w) *cam_x = world_w - view_w;
    if (*cam_y + view_h > world_h) *cam_y = world_h - view_h;
}
