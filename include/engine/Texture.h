#pragma once

#include <SDL.h>

typedef struct Texture
{
    SDL_Texture* sdl_texture;

    int width;
    int height;

    int draw_w;
    int draw_h;
} Texture;

int  load_tex(Texture* tex, SDL_Renderer* renderer, const char* path);
void destroy_tex(Texture* tex);
void draw_tex(Texture* tex, SDL_Renderer* renderer, int x, int y);
void scale_tex(Texture* tex, int w, int h);
void scale_tex_f(Texture* tex, float sx, float sy);