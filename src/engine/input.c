#include "Input.h"
#include <string.h>

static Uint8 curr_keys[SDL_NUM_SCANCODES];
static Uint8 prev_keys[SDL_NUM_SCANCODES];

static Uint32 curr_mouse = 0;
static Uint32 prev_mouse = 0;

static int mouse_x = 0;
static int mouse_y = 0;

void UpdateInput(void)
{
    // Save previous state
    memcpy(prev_keys, curr_keys, sizeof(curr_keys));
    prev_mouse = curr_mouse;

    // Make sure SDL updates input state
    SDL_PumpEvents();

    // Get keyboard
    const Uint8* state = SDL_GetKeyboardState(NULL);
    memcpy(curr_keys, state, sizeof(curr_keys));

    // Get mouse
    curr_mouse = SDL_GetMouseState(&mouse_x, &mouse_y);
}

int IsKeyDown(SDL_Scancode key)
{
    return curr_keys[key];
}

int IsKeyPressed(SDL_Scancode key)
{
    return curr_keys[key] && !prev_keys[key];
}

int IsKeyReleased(SDL_Scancode key)
{
    return !curr_keys[key] && prev_keys[key];
}

int IsMouseDown(Uint8 button)
{
    return (curr_mouse & SDL_BUTTON(button)) != 0;
}

int IsMousePressed(Uint8 button)
{
    Uint32 mask = SDL_BUTTON(button);
    return (curr_mouse & mask) && !(prev_mouse & mask);
}

int IsMouseReleased(Uint8 button)
{
    Uint32 mask = SDL_BUTTON(button);
    return !(curr_mouse & mask) && (prev_mouse & mask);
}

void GetMousePos(int* x, int* y)
{
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}
