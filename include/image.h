#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct
{
  int w, h;
  int pitchPixels; // pitch en píxeles (no bytes)
  Uint32 *pixels;  // formato RGBA8888
  SDL_Surface *surface;
} Image;

bool imageLoad(Image *img, const char *path);
void imageFree(Image *img);

// Intensidad [0..1] usando luminancia (rec. 709). x,y en coordenadas de pixel (nearest).
float sampleIntensity(const Image *img, int x, int y);

// u,v en [0,1] (coordenadas normalizadas). Devuelve luminancia Rec.709 [0..1] con bilinear.
float sampleIntensityBilinearUV(const Image *img, float u, float v);