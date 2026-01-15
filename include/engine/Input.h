#pragma once
#include <SDL.h>

void UpdateInput(void);

int IsKeyDown(SDL_Scancode key);
int IsKeyPressed(SDL_Scancode key);
int IsKeyReleased(SDL_Scancode key);

int IsMouseDown(Uint8 button);
int IsMousePressed(Uint8 button);
int IsMouseReleased(Uint8 button);

void GetMousePos(int* x, int* y);
