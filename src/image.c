#include "image.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>

bool imageLoad(Image *img, const char *path)
{
  if (!img || !path)
    return false;

  img->w = img->h = img->pitchPixels = 0;
  img->pixels = NULL;
  img->surface = NULL;

  SDL_Surface *loaded = IMG_Load(path);
  if (!loaded)
  {
    fprintf(stderr, "IMG_Load(%s): %s\n", path, IMG_GetError());
    return false;
  }

  SDL_Surface *conv = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded);
  if (!conv)
  {
    fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
    return false;
  }

  img->surface = conv;
  img->w = conv->w;
  img->h = conv->h;
  img->pitchPixels = conv->pitch / 4;
  img->pixels = (Uint32 *)conv->pixels;
  return true;
}

void imageFree(Image *img)
{
  if (!img)
    return;
  if (img->surface)
    SDL_FreeSurface(img->surface);
  img->surface = NULL;
  img->pixels = NULL;
  img->w = img->h = img->pitchPixels = 0;
}

static inline void rgbaToUint8(Uint32 px, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
  *r = (px >> 24) & 0xFF;
  *g = (px >> 16) & 0xFF;
  *b = (px >> 8) & 0xFF;
  *a = (px >> 0) & 0xFF;
}

float sampleIntensity(const Image *img, int x, int y)
{
  if (!img || !img->pixels || x < 0 || y < 0 || x >= img->w || y >= img->h)
    return 0.0f;
  Uint32 px = img->pixels[y * img->pitchPixels + x];
  Uint8 R, G, B, A;
  (void)A;
  rgbaToUint8(px, &R, &G, &B, &A);
  // Luma Rec.709
  float lum = 0.2126f * (R / 255.0f) + 0.7152f * (G / 255.0f) + 0.0722f * (B / 255.0f);
  return lum;
}