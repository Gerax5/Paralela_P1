#include "app.h"
#include "config.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

struct App
{
  SDL_Window *win;
  SDL_Renderer *ren;
  int w, h;
  Uint64 freq, last;
  double accTime;
  int frames;
  double lastFpsUpdate;
};

static void updateFpsTitle(App *app)
{
  double fps = (app->accTime > 0.0) ? (app->frames / app->accTime) : 0.0;
  char title[128];
  snprintf(title, sizeof(title), "%s — FPS: %.1f", defaultTitle, fps);
  SDL_SetWindowTitle(app->win, title);
}

bool appInit(App **outApp, int width, int height, const char *title)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  App *app = (App *)calloc(1, sizeof(App));
  if (!app)
  {
    SDL_Quit();
    return false;
  }

  app->w = (width > 0) ? width : defaultWidth;
  app->h = (height > 0) ? height : defaultHeight;

  app->win = SDL_CreateWindow(title ? title : defaultTitle,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, app->w, app->h, 0);
  app->ren = SDL_CreateRenderer(app->win, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (!app->win || !app->ren)
  {
    fprintf(stderr, "SDL_Create: %s\n", SDL_GetError());
    if (app->ren)
      SDL_DestroyRenderer(app->ren);
    if (app->win)
      SDL_DestroyWindow(app->win);
    free(app);
    SDL_Quit();
    return false;
  }

  app->freq = SDL_GetPerformanceFrequency();
  app->last = SDL_GetPerformanceCounter();
  app->accTime = 0.0;
  app->frames = 0;
  app->lastFpsUpdate = 0.0;

  *outApp = app;
  return true;
}

void appRun(App *app)
{
  if (!app)
    return;
  bool running = true;

  while (running)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        running = false;
    }

    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - app->last) / (double)app->freq;
    app->last = now;
    app->accTime += dt;
    app->frames++;

    if (app->accTime - app->lastFpsUpdate >= 0.25)
    {
      updateFpsTitle(app);
      app->lastFpsUpdate = app->accTime;
    }

    SDL_SetRenderDrawColor(app->ren, 12, 16, 28, 255);
    SDL_RenderClear(app->ren);

    int s = 50 + (int)(20.0 * SDL_sinf((float)app->accTime * 2.0f));
    SDL_Rect r = {app->w / 2 - s, app->h / 2 - s, 2 * s, 2 * s};
    SDL_SetRenderDrawColor(app->ren, 40, 180, 220, 255);
    SDL_RenderFillRect(app->ren, &r);

    SDL_RenderPresent(app->ren);
  }
}

void appShutdown(App *app)
{
  if (!app)
    return;
  if (app->ren)
    SDL_DestroyRenderer(app->ren);
  if (app->win)
    SDL_DestroyWindow(app->win);
  free(app);
  SDL_Quit();
}