// main.c
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    float x, y;      // posición
    float vx, vy;    // velocidad
    int   r;         // radio
    SDL_Color c;     // color
} Ball;

// círculo lleno con líneas horizontales
static void draw_filled_circle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color col) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        int y = cy + dy;
        int dx = (int)sqrtf((float)(radius*radius - dy*dy));
        SDL_RenderDrawLine(r, cx - dx, y, cx + dx, y);
    }
}

static float frand(float a, float b) {
    return a + (float)rand() / (float)RAND_MAX * (b - a);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <# de objetos>\n", argv[0]);
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "El número de objetos debe ser > 0\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const int W = 800, H = 600;
    SDL_Window* win = SDL_CreateWindow("N circulos en movimiento",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       W, H, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
                                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL_Create: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    srand((unsigned)time(NULL));

    Ball* balls = (Ball*)malloc(sizeof(Ball) * (size_t)N);
    if (!balls) { fprintf(stderr, "Sin memoria\n"); return 1; }

    const float golden = 2.39996322972865332f; 
    float scale = (fminf((float)W, (float)H) * 0.47f) / fmaxf(1.0f, sqrtf((float)N));

    for (int i = 0; i < N; ++i) {
        float a = golden * (float)i;
        float r = scale * sqrtf((float)i + 1.0f);

        balls[i].x = W/2.0f + cosf(a) * r;
        balls[i].y = H/2.0f + sinf(a) * r;

        balls[i].vx = frand(-180.0f, 180.0f);
        balls[i].vy = frand(-160.0f, 160.0f);

        balls[i].r = (N <= 100) ? 8 : (N <= 500 ? 5 : 3);

        Uint8 R = (Uint8)(128 + 127 * sinf(0.07f * i));
        Uint8 G = (Uint8)(128 + 127 * sinf(0.11f * i + 1.0f));
        Uint8 B = (Uint8)(128 + 127 * sinf(0.13f * i + 2.0f));
        balls[i].c = (SDL_Color){ R, G, B, 255 };
    }

    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / (double)freq;
        last = now;

        for (int i = 0; i < N; ++i) {
            Ball* b = &balls[i];
            b->x += b->vx * (float)dt;
            b->y += b->vy * (float)dt;

            if (b->x - b->r < 0)    { b->x = (float)b->r;       b->vx = -b->vx; }
            if (b->x + b->r > W)    { b->x = (float)(W-b->r);   b->vx = -b->vx; }
            if (b->y - b->r < 0)    { b->y = (float)b->r;       b->vy = -b->vy; }
            if (b->y + b->r > H)    { b->y = (float)(H-b->r);   b->vy = -b->vy; }
        }

        // render
        SDL_SetRenderDrawColor(ren, 12, 16, 28, 255);
        SDL_RenderClear(ren);
        for (int i = 0; i < N; ++i) {
            draw_filled_circle(ren, (int)balls[i].x, (int)balls[i].y, balls[i].r, balls[i].c);
        }
        SDL_RenderPresent(ren);
    }

    free(balls);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
