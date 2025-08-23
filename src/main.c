// src/main.c — Hola SDL + FPS en el título (ESC para salir)
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  const int W = 800, H = 600;
  SDL_Window *win = SDL_CreateWindow("Voronoi Stippling — bootstrap",
                                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
                                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!win || !ren)
  {
    fprintf(stderr, "SDL_Create: %s\n", SDL_GetError());
    if (ren)
      SDL_DestroyRenderer(ren);
    if (win)
      SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  Uint64 freq = SDL_GetPerformanceFrequency();
  Uint64 last = SDL_GetPerformanceCounter();
  double accTime = 0.0;
  int frames = 0;
  double fpsShownAt = 0.0;

  bool running = true;
  while (running)
  {
    // Eventos
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        running = false;
    }

    // Timing
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - last) / (double)freq;
    last = now;
    accTime += dt;
    frames++;

    // Actualiza título con FPS ~4 veces por segundo
    if (accTime - fpsShownAt >= 0.25)
    {
      double fps = frames / accTime;
      char title[128];
      snprintf(title, sizeof(title), "Voronoi Stippling — FPS: %.1f", fps);
      SDL_SetWindowTitle(win, title);
      fpsShownAt = accTime;
    }

    // Render
    SDL_SetRenderDrawColor(ren, 12, 16, 28, 255);
    SDL_RenderClear(ren);
    // Dibujito simple para ver movimiento (un rectángulo que cambia tamaño)
    int s = 50 + (int)(20.0 * SDL_sinf((float)accTime * 2.0f));
    SDL_Rect r = {W / 2 - s, H / 2 - s, 2 * s, 2 * s};
    SDL_SetRenderDrawColor(ren, 40, 180, 220, 255);
    SDL_RenderFillRect(ren, &r);

    SDL_RenderPresent(ren);
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
