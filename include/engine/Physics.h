#pragma once

/* AABB overlap */
int rect_overlaps(float ax, float ay, float aw, float ah,
                  float bx, float by, float bw, float bh);

/* Point inside rect */
int point_in_rect(float px, float py,
                  float rx, float ry, float rw, float rh);

/* Keep rect inside window bounds */
void rect_clamp_to_window(float* x, float* y,
                          float w, float h,
                          float win_w, float win_h);

/* Bounce rect off window edges */
void rect_bounce_window(float* x, float* y,
                         float* vx, float* vy,
                         float w, float h,
                         float win_w, float win_h);

/* Camera helpers (camera pos is top-left of view in world coords) */
void cam_follow(float* cam_x, float* cam_y,
                float target_x, float target_y,
                float view_w, float view_h);

void cam_clamp(float* cam_x, float* cam_y,
               float view_w, float view_h,
               float world_w, float world_h);
