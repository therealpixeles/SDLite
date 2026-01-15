#pragma once
#include "Texture.h"
#include <SDL.h>

static inline void draw_row(Texture* tex, SDL_Renderer* r,
                            float x, float y, int count)
{
    float step = (float)tex->draw_w;
    for (int i = 0; i < count; i++)
        draw_tex(tex, r, x + i * step, y);
}

static inline void draw_grid(Texture* tex, SDL_Renderer* r,
                             float x, float y, int cols, int rows)
{
    float sx = (float)tex->draw_w;
    float sy = (float)tex->draw_h;
    for (int ry = 0; ry < rows; ry++)
        for (int cx = 0; cx < cols; cx++)
            draw_tex(tex, r, x + cx * sx, y + ry * sy);
}
