#define SDL_MAIN_HANDLED
#include <stdio.h>
#include "Global.h"

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG))) return 1;

    const float WIN_W = 800.0f;
    const float WIN_H = 600.0f;

    RenderWindow window;
    window_init(&window, "Camera + Animation Test", (int)WIN_W, (int)WIN_H);

    // ---- Ground tile ----
    Texture grass = (Texture){0};
    load_tex(&grass, window.renderer, "res/gfx/ground_grass_1.png");
    scale_tex(&grass, 64, 64);

    // ---- Character sprites ----
    Texture chr_front = (Texture){0};
    Texture chr_idle  = (Texture){0};
    Texture chr_walkA = (Texture){0};
    Texture chr_walkB = (Texture){0};
    Texture chr_jump  = (Texture){0};

    load_tex(&chr_front, window.renderer, "res/gfx/character_green_front.png");
    load_tex(&chr_idle,  window.renderer, "res/gfx/character_green_idle.png");
    load_tex(&chr_walkA, window.renderer, "res/gfx/character_green_walk_a.png");
    load_tex(&chr_walkB, window.renderer, "res/gfx/character_green_walk_b.png");
    load_tex(&chr_jump,  window.renderer, "res/gfx/character_green_jump.png");

    // scale all character sprites to tile size
    Texture* charTexs[] = { &chr_front, &chr_idle, &chr_walkA, &chr_walkB, &chr_jump };
    for (int i = 0; i < (int)(sizeof(charTexs) / sizeof(charTexs[0])); i++)
        scale_tex(charTexs[i], grass.draw_w, grass.draw_h);

    // ---- Anim sets ----
    Texture* ANIM_IDLE[] = { &chr_idle };            // or &chr_front if you prefer
    Texture* ANIM_WALK[] = { &chr_walkA, &chr_walkB };
    Texture* ANIM_JUMP[] = { &chr_jump };

    // ---- World / Level ----
    float world_w = 3000.0f;
    float world_h = 600.0f;

    float tilew = (float)grass.draw_w;
    float tileh = (float)grass.draw_h;

    float plat_x = 0.0f;
    float plat_y = 450.0f;
    int   plat_n = (int)(world_w / tilew);  // fill the whole world with a platform row

    // ---- Player ----
    Entity player;
    entity_init(&player, 200.0f, 100.0f);
    entity_play_anim(&player, ANIM_IDLE, 1, 1.0f);

    Vec2 vel = vec2(0.0f, 0.0f);
    int on_ground = 0;

    float speed   = 220.0f;
    float gravity = 1800.0f;
    float jump_v  = -650.0f;

    float pw = (float)chr_idle.draw_w;
    float ph = (float)chr_idle.draw_h;

    // ---- Camera ----
    Vec2 cam = vec2(0.0f, 0.0f);

    Uint32 last = SDL_GetTicks();
    int running = 1;
    SDL_Event e;

    while (running)
    {
        float dt = time_dt(&last);

        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) running = 0;

        UpdateInput();

        // ---- Movement ----
        float ax = axis_keys(SDL_SCANCODE_A, SDL_SCANCODE_D);

        vel.x = ax * speed;

        if (on_ground && IsKeyPressed(SDL_SCANCODE_SPACE))
        {
            vel.y = jump_v;
            on_ground = 0;
        }

        vel.y += gravity * dt;

        player.pos.x += vel.x * dt;
        player.pos.y += vel.y * dt;

        // ---- Collide with platform row (top-only landing) ----
        on_ground = 0;

        // Only check tiles near the player (fast)
        int i0 = (int)((player.pos.x - plat_x) / tilew) - 2;
        int i1 = i0 + 6;
        if (i0 < 0) i0 = 0;
        if (i1 > plat_n) i1 = plat_n;

        for (int i = i0; i < i1; i++)
        {
            float tx = plat_x + i * tilew;
            float ty = plat_y;

            if (rect_overlaps(player.pos.x, player.pos.y, pw, ph, tx, ty, tilew, tileh))
            {
                if (vel.y > 0.0f && (player.pos.y + ph - vel.y * dt) <= ty)
                {
                    player.pos.y = ty - ph;
                    vel.y = 0.0f;
                    on_ground = 1;
                }
            }
        }

        // ---- Clamp player to WORLD bounds (not window) ----
        rect_clamp_to_window(&player.pos.x, &player.pos.y, pw, ph, world_w, world_h);

        // ---- Choose animation ----
        if (!on_ground)
            entity_play_anim(&player, ANIM_JUMP, 1, 1.0f);
        else if (ax != 0.0f)
            entity_play_anim(&player, ANIM_WALK, 2, 8.0f);
        else
            entity_play_anim(&player, ANIM_IDLE, 1, 1.0f);

        entity_update(&player, dt);

        // ---- Camera follow + clamp ----
        cam_follow(&cam.x, &cam.y,
                   player.pos.x + pw * 0.5f,
                   player.pos.y + ph * 0.5f,
                   WIN_W, WIN_H);

        cam_clamp(&cam.x, &cam.y, WIN_W, WIN_H, world_w, world_h);

        // ---- Render ----
        window_clear(&window);

        // draw platform row with camera + simple culling
        int first = (int)(cam.x / tilew) - 2;
        int lasti = first + (int)(WIN_W / tilew) + 6;

        if (first < 0) first = 0;
        if (lasti > plat_n) lasti = plat_n;

        for (int i = first; i < lasti; i++)
        {
            float wx = plat_x + i * tilew;
            float wy = plat_y;

            Vec2 s = cam_apply(vec2(wx, wy), cam);
            draw_tex(&grass, window.renderer, s.x, s.y);
        }

        // draw player with camera
        Vec2 p = cam_apply(player.pos, cam);
        draw_tex(player.tex, window.renderer, p.x, p.y);

        window_present(&window);
    }

    // cleanup
    destroy_tex(&chr_front);
    destroy_tex(&chr_idle);
    destroy_tex(&chr_walkA);
    destroy_tex(&chr_walkB);
    destroy_tex(&chr_jump);
    destroy_tex(&grass);

    window_destroy(&window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
