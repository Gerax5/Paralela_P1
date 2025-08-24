#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct
{
  float x, y;
} Dot;

typedef struct
{
  Dot *pts;
  int count;
  int width, height;
} Stippling;

bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);
void stipplingFree(Stippling *s);
void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius);