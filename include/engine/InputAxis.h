#pragma once
#include <SDL.h>

static inline float axis_keys(SDL_Scancode negative, SDL_Scancode positive)
{
    float v = 0.0f;
    if (IsKeyDown(negative)) v -= 1.0f;
    if (IsKeyDown(positive)) v += 1.0f;
    return v;
}
