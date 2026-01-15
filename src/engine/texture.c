#include "Texture.h"
#include <SDL_image.h>
#include <stdio.h>

int load_tex(Texture* tex, SDL_Renderer* renderer, const char* path)
{
    tex->sdl_texture = NULL;
    tex->width = tex->height = 0;
    tex->draw_w = tex->draw_h = 0;

    SDL_Surface* surface = IMG_Load(path);
    if (!surface)
    {
        printf("IMG_Load failed for '%s': %s\n", path, IMG_GetError());
        return 0;
    }

    tex->sdl_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex->sdl_texture)
    {
        printf("SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return 0;
    }

    tex->width  = surface->w;
    tex->height = surface->h;

    // default draw size = original size
    tex->draw_w = tex->width;
    tex->draw_h = tex->height;

    SDL_FreeSurface(surface);
    return 1;
}

void scale_tex(Texture* tex, int w, int h)
{
    tex->draw_w = w;
    tex->draw_h = h;
}

void scale_tex_f(Texture* tex, float sx, float sy)
{
    tex->draw_w = (int)(tex->width  * sx);
    tex->draw_h = (int)(tex->height * sy);
}

void draw_tex(Texture* tex, SDL_Renderer* renderer, int x, int y)
{
    SDL_Rect dst = { x, y, tex->draw_w, tex->draw_h };
    SDL_RenderCopy(renderer, tex->sdl_texture, NULL, &dst);
}

void destroy_tex(Texture* tex)
{
    if (tex->sdl_texture)
    {
        SDL_DestroyTexture(tex->sdl_texture);
        tex->sdl_texture = NULL;
    }
}
