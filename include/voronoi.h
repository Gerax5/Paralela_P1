#pragma once
#include <stdbool.h>
#include "stippling.h"

typedef struct
{
  int w, h; // dims del canvas
  int cell; // tamaño de celda en px (p.ej. 32)
  int cols, rows;

  // buckets como listas enlazadas implícitas
  int *head; // size = cols*rows, inicializado a -1
  int *next; // size = s->count (uno por punto)
} UniformGrid;

bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);
void gridFree(UniformGrid *g);

// retorna índice del punto más cercano a (x,y), o -1 si no hay
int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);
