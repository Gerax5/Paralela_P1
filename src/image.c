#include "image.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>

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

static inline float lumaFromPixel(Uint32 px)
{
  Uint8 R = (px >> 24) & 0xFF;
  Uint8 G = (px >> 16) & 0xFF;
  Uint8 B = (px >> 8) & 0xFF;
  // Rec.709
  return 0.2126f * (R / 255.0f) + 0.7152f * (G / 255.0f) + 0.0722f * (B / 255.0f);
}

float sampleIntensityBilinearUV(const Image *img, float u, float v)
{
  if (!img || !img->pixels || img->w <= 0 || img->h <= 0)
    return 0.0f;
  // clamp a [0,1]
  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  int x1 = (x0 + 1 < img->w) ? x0 + 1 : x0;
  int y1 = (y0 + 1 < img->h) ? y0 + 1 : y0;

  float tx = x - (float)x0;
  float ty = y - (float)y0;

  Uint32 p00 = img->pixels[y0 * img->pitchPixels + x0];
  Uint32 p10 = img->pixels[y0 * img->pitchPixels + x1];
  Uint32 p01 = img->pixels[y1 * img->pitchPixels + x0];
  Uint32 p11 = img->pixels[y1 * img->pitchPixels + x1];

  float l00 = lumaFromPixel(p00);
  float l10 = lumaFromPixel(p10);
  float l01 = lumaFromPixel(p01);
  float l11 = lumaFromPixel(p11);

  float l0 = l00 * (1.f - tx) + l10 * tx;
  float l1 = l01 * (1.f - tx) + l11 * tx;
  return l0 * (1.f - ty) + l1 * ty;
}
