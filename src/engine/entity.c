#include "Entity.h"

static int same_anim(Texture** aFrames, int aCount, float aFps,
                     Texture** bFrames, int bCount, float bFps)
{
    return (aFrames == bFrames && aCount == bCount && aFps == bFps);
}

void entity_init(Entity* e, float x, float y)
{
    e->pos = vec2(x, y);
    e->tex = 0;

    e->anim.frames = 0;
    e->anim.count = 0;
    e->anim.fps = 0.0f;
    e->anim.t = 0.0f;
    e->anim.index = 0;
    e->anim.playing = 0;
}

void entity_set_tex(Entity* e, Texture* tex)
{
    e->tex = tex;

    // when manually setting a texture, stop animation by default
    e->anim.frames = 0;
    e->anim.count = 0;
    e->anim.fps = 0.0f;
    e->anim.t = 0.0f;
    e->anim.index = 0;
    e->anim.playing = 0;
}

void entity_move(Entity* e, Vec2 delta)
{
    e->pos = vec2_add(e->pos, delta);
}

void entity_play_anim(Entity* e, Texture** frames, int count, float fps)
{
    if (!frames || count <= 0 || fps <= 0.0f)
        return;

    // only reset if you're switching to a different anim
    if (!same_anim(e->anim.frames, e->anim.count, e->anim.fps, frames, count, fps))
    {
        e->anim.frames = frames;
        e->anim.count = count;
        e->anim.fps = fps;
        e->anim.t = 0.0f;
        e->anim.index = 0;
    }

    e->anim.playing = 1;
    e->tex = e->anim.frames[e->anim.index];
}

void entity_stop_anim(Entity* e)
{
    e->anim.playing = 0;
    // keep current frame texture
}

void entity_reset_anim(Entity* e)
{
    e->anim.t = 0.0f;
    e->anim.index = 0;

    if (e->anim.frames && e->anim.count > 0)
        e->tex = e->anim.frames[0];
}

void entity_update(Entity* e, float dt)
{
    if (!e->anim.playing) return;
    if (!e->anim.frames || e->anim.count <= 0) return;

    e->anim.t += dt;

    float spf = 1.0f / e->anim.fps; // seconds per frame
    while (e->anim.t >= spf)
    {
        e->anim.t -= spf;
        e->anim.index++;
        if (e->anim.index >= e->anim.count)
            e->anim.index = 0;
    }

    e->tex = e->anim.frames[e->anim.index];
}

void entity_draw(Entity* e, SDL_Renderer* renderer)
{
    if (!e || !e->tex) return;
    draw_tex(e->tex, renderer, e->pos.x, e->pos.y);
}
