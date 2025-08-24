#include "render_sdl.h"
#include <math.h>

static void drawFilledCircle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color col)
{
  SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
  for (int dy = -radius; dy <= radius; ++dy)
  {
    int y = cy + dy;
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, y, cx + dx, y);
  }
}

void renderFrame(SDL_Renderer *ren, int w, int h, double t)
{

  int s = 50 + (int)(20.0 * sin(t * 2.0));
  SDL_Rect r = (SDL_Rect){w / 2 - s, h / 2 - s, 2 * s, 2 * s};
  SDL_SetRenderDrawColor(ren, 40, 180, 220, 255);
  SDL_RenderFillRect(ren, &r);

  int rc = 16 + (int)(6.0 * sin(t * 3.0));
  SDL_SetRenderDrawColor(ren, 255, 220, 80, 255);
}
