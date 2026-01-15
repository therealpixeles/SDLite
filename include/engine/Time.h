#pragma once
#include <SDL.h>

static inline float time_dt(Uint32* last_ticks)
{
    Uint32 now = SDL_GetTicks();
    float dt = (now - *last_ticks) / 1000.0f;
    *last_ticks = now;
    return dt;
}
