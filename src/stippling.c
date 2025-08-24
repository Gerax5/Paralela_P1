#include "stippling.h"
#include <stdlib.h>
#include <math.h>

static float frand01(unsigned *st)
{
  // LCG simple y determinista
  *st = (*st * 1664525u + 1013904223u);
  return ((*st >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
}

static void drawFilledCircle(SDL_Renderer *r, int cx, int cy, int radius)
{
  for (int dy = -radius; dy <= radius; ++dy)
  {
    int y = cy + dy;
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, y, cx + dx, y);
  }
}

bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed)
{
  if (!s || n <= 0 || w <= 0 || h <= 0)
    return false;
  s->pts = (Dot *)malloc(sizeof(Dot) * (size_t)n);
  if (!s->pts)
    return false;
  s->count = n;
  s->width = w;
  s->height = h;

  unsigned st = seed ? seed : 1234567u;
  for (int i = 0; i < n; i++)
  {
    s->pts[i].x = frand01(&st) * (float)w;
    s->pts[i].y = frand01(&st) * (float)h;
  }
  return true;
}

void stipplingFree(Stippling *s)
{
  if (!s)
    return;
  free(s->pts);
  s->pts = NULL;
  s->count = s->width = s->height = 0;
}

void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius)
{
  if (!s || !s->pts)
    return;
  SDL_SetRenderDrawColor(ren, 250, 250, 250, 255);
  int r = (radius < 1) ? 1 : radius;
  for (int i = 0; i < s->count; i++)
  {
    int x = (int)lroundf(s->pts[i].x);
    int y = (int)lroundf(s->pts[i].y);
    drawFilledCircle(ren, x, y, r);
  }
}
