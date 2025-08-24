#include "lloyd.h"
#include "image.h"
#include "stippling.h"
#include <stdlib.h>
#include <math.h>

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool lloydStep(const Image *img, Stippling *s, int W, int H, int step, float gamma)
{
  if (!s || !s->pts || s->count <= 0 || W <= 0 || H <= 0)
    return false;
  if (!img || !img->pixels)
    return false;
  if (step < 1)
    step = 1;

  int n = s->count;
  double *sumX = (double *)calloc((size_t)n, sizeof(double));
  double *sumY = (double *)calloc((size_t)n, sizeof(double));
  double *sumW = (double *)calloc((size_t)n, sizeof(double));
  if (!sumX || !sumY || !sumW)
  {
    free(sumX);
    free(sumY);
    free(sumW);
    return false;
  }

  for (int y = 0; y < H; y += step)
  {
    int iy = (int)((long long)y * img->h / (long long)H);
    iy = clampi(iy, 0, img->h - 1);

    for (int x = 0; x < W; x += step)
    {
      int ix = (int)((long long)x * img->w / (long long)W);
      ix = clampi(ix, 0, img->w - 1);

      float lum = sampleIntensity(img, ix, iy);
      double w = pow(fmax(0.0f, 1.0f - lum), (double)gamma);
      if (w <= 0.0)
        continue;

      int best = 0;
      float bestD2 = 1e30f;
      for (int i = 0; i < n; i++)
      {
        float dx = (float)x - s->pts[i].x;
        float dy = (float)y - s->pts[i].y;
        float d2 = dx * dx + dy * dy;
        if (d2 < bestD2)
        {
          bestD2 = d2;
          best = i;
        }
      }
      sumX[best] += x * w;
      sumY[best] += y * w;
      sumW[best] += w;
    }
  }

  for (int i = 0; i < n; i++)
  {
    if (sumW[i] > 0.0)
    {
      s->pts[i].x = (float)(sumX[i] / sumW[i]);
      s->pts[i].y = (float)(sumY[i] / sumW[i]);
    }
  }

  free(sumX);
  free(sumY);
  free(sumW);
  return true;
}
