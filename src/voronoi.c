#include "voronoi.h"
#include <stdlib.h>
#include <math.h>

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int cellIndex(const UniformGrid *g, int cx, int cy)
{
  if (cx < 0 || cy < 0 || cx >= g->cols || cy >= g->rows)
    return -1;
  return cy * g->cols + cx;
}

bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s)
{
  if (!g || !s || !s->pts || s->count <= 0 || w <= 0 || h <= 0 || cell <= 0)
    return false;
  g->w = w;
  g->h = h;
  g->cell = cell;
  g->cols = (w + cell - 1) / cell;
  g->rows = (h + cell - 1) / cell;

  size_t H = (size_t)g->cols * (size_t)g->rows;
  g->head = (int *)malloc(H * sizeof(int));
  g->next = (int *)malloc((size_t)s->count * sizeof(int));
  if (!g->head || !g->next)
  {
    free(g->head);
    free(g->next);
    return false;
  }
  for (size_t i = 0; i < H; i++)
    g->head[i] = -1;

  for (int i = 0; i < s->count; i++)
  {
    int cx = clampi((int)(s->pts[i].x) / cell, 0, g->cols - 1);
    int cy = clampi((int)(s->pts[i].y) / cell, 0, g->rows - 1);
    int hidx = cy * g->cols + cx;
    g->next[i] = g->head[hidx];
    g->head[hidx] = i;
  }
  return true;
}

void gridFree(UniformGrid *g)
{
  if (!g)
    return;
  free(g->head);
  free(g->next);
  g->head = g->next = NULL;
}

int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax)
{
  if (!g || !s || !s->pts)
    return -1;
  int cx0 = (int)x / g->cell;
  int cy0 = (int)y / g->cell;

  int best = -1;
  float bestD2 = 1e30f;

  for (int r = 0; r <= ringMax; ++r)
  {
    int foundInRing = 0;
    for (int cy = cy0 - r; cy <= cy0 + r; ++cy)
    {
      for (int cx = cx0 - r; cx <= cx0 + r; ++cx)
      {
        // solo borde del anillo r
        if (cx != cx0 - r && cx != cx0 + r && cy != cy0 - r && cy != cy0 + r)
          continue;
        int hidx = cellIndex(g, cx, cy);
        if (hidx < 0)
          continue;
        for (int i = g->head[hidx]; i != -1; i = g->next[i])
        {
          float dx = s->pts[i].x - x;
          float dy = s->pts[i].y - y;
          float d2 = dx * dx + dy * dy;
          if (d2 < bestD2)
          {
            bestD2 = d2;
            best = i;
            foundInRing = 1;
          }
        }
      }
    }
    // si encontramos en r=0, ya es suficiente para la mayoría de casos;
    // también podemos cortar si la distancia ya es < (r*cell)^2, etc.
    if (foundInRing && r >= 1)
      break;
  }
  return best;
}
