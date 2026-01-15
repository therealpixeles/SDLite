#pragma once
#include <SDL.h>
#include "Math.h"
#include "Texture.h"

typedef struct EntityAnim
{
    Texture** frames;   // array of Texture* (you provide it)
    int count;          // number of frames
    float fps;          // frames per second
    float t;            // time accumulator
    int index;          // current frame index
    int playing;        // 1 = playing, 0 = paused
} EntityAnim;

typedef struct Entity
{
    Vec2 pos;
    Texture* tex;       // current texture to draw (auto from anim when playing)
    EntityAnim anim;
} Entity;

/* basic entity */
void entity_init(Entity* e, float x, float y);
void entity_set_tex(Entity* e, Texture* tex);
void entity_move(Entity* e, Vec2 delta);

/* animation */
void entity_play_anim(Entity* e, Texture** frames, int count, float fps); // resets if different anim
void entity_stop_anim(Entity* e);                                        // pauses on current frame
void entity_reset_anim(Entity* e);                                       // frame 0, time 0
void entity_update(Entity* e, float dt);                                 // advances anim + updates e->tex

/* draw */
void entity_draw(Entity* e, SDL_Renderer* renderer);
