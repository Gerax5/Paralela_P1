#include "lloyd.h"
#include "image.h"
#include "stippling.h"
#include <stdlib.h>
#include <math.h>
#include "voronoi.h"

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool lloydStep(const Image *img, Stippling *s, int W, int H, int step, float gamma)
{
  if (!s || !s->pts || s->count <= 0 || W <= 0 || H <= 0)
    return false;
  if (!img || !img->pixels)
    return false;
  if (step < 1)
    step = 1;

  // Construye índice espacial (grilla uniforme)
  UniformGrid g = (UniformGrid){0};
  if (!gridBuild(&g, W, H, 32, s))
  {
    // Si fallara, podríamos continuar con fallback O(N),
    // pero la grilla debería construirse bien con parámetros válidos.
  }

  const int n = s->count;
  double *sumX = (double *)calloc((size_t)n, sizeof(double));
  double *sumY = (double *)calloc((size_t)n, sizeof(double));
  double *sumW = (double *)calloc((size_t)n, sizeof(double));
  if (!sumX || !sumY || !sumW)
  {
    free(sumX);
    free(sumY);
    free(sumW);
    gridFree(&g);
    return false;
  }

  for (int y = 0; y < H; y += step)
  {
    // mapeo a UV (centro del píxel)
    float v = ((float)y + 0.5f) / (float)H;

    for (int x = 0; x < W; x += step)
    {
      float u = ((float)x + 0.5f) / (float)W;

      // luminancia bilineal en UV
      float lum = sampleIntensityBilinearUV(img, u, v);

      // peso por oscuridad
      double w = pow(fmaxf(0.0f, 1.0f - lum), (double)gamma);
      if (w <= 0.0)
        continue;

      // vecino más cercano usando la grilla (ringMax=2 suele bastar)
      int best = gridNearest(&g, s, (float)x, (float)y, 2);
      if (best < 0)
      {
        // Fallback extremadamente raro: buscar O(N)
        best = 0;
        float bestD2 = 1e30f;
        for (int i = 0; i < n; i++)
        {
          float dx = x - s->pts[i].x, dy = y - s->pts[i].y;
          float d2 = dx * dx + dy * dy;
          if (d2 < bestD2)
          {
            bestD2 = d2;
            best = i;
          }
        }
      }

      sumX[best] += (double)x * w;
      sumY[best] += (double)y * w;
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
  gridFree(&g);
  return true;
}
