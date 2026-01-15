#pragma once

#include <SDL.h>

typedef struct RenderWindow
{
    const char* title;
    int width;
    int height;

    SDL_Window* window;
    SDL_Renderer* renderer;
} RenderWindow;

void window_init(RenderWindow* rw, const char* title, int width, int height);
void window_clear(RenderWindow* rw);
void window_present(RenderWindow* rw);
void window_destroy(RenderWindow* rw);