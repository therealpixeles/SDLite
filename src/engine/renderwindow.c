#include "RenderWindow.h"
#include <stdio.h>

void window_init(RenderWindow* rw, const char* title, int width, int height)
{
	rw->title = title;
	rw->width = width;
	rw->height = height;

	rw->window = SDL_CreateWindow(
		title, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		width, 
		height, 
		SDL_WINDOW_SHOWN);

	if (!rw->window)
	{
		printf("INFO: WINDOW FAILED TO CREATE. ERR: %s\n", SDL_GetError());
		return;
	}

	rw->renderer = SDL_CreateRenderer(
        rw->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!rw->renderer)
    {
        printf("INFO: RENDERER FAILED TO CREATE: ERR: %s\n", SDL_GetError());
        SDL_DestroyWindow(rw->window);
        rw->window = NULL;
    }
}

void window_clear(RenderWindow* rw)
{
	SDL_SetRenderDrawColor(rw->renderer, 20, 20, 20, 255);
    SDL_RenderClear(rw->renderer);
}

void window_present(RenderWindow* rw)
{
	SDL_RenderPresent(rw->renderer);
}

void window_destroy(RenderWindow* rw)
{
    if (rw->renderer)
        SDL_DestroyRenderer(rw->renderer);

    if (rw->window)
        SDL_DestroyWindow(rw->window);

    rw->renderer = NULL;
    rw->window = NULL;
}