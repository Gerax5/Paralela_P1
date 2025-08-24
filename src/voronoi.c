#include <stdlib.h>
#include <math.h>
#include "voronoi.h"

/**
 * Módulo: voronoi.c
 * ------------------
 * Índice espacial ligero (grilla uniforme) para acelerar la búsqueda del
 * vecino más cercano durante Lloyd. La grilla divide el canvas en celdas
 * de tamaño fijo y encadena los índices de puntos que caen en cada celda.
 *
 * Estructura:
 *   - head[cols*rows]: índice del primer punto en cada celda o -1 si vacía.
 *   - next[count]: lista enlazada implícita; next[i] apunta al siguiente
 *                  punto en la misma celda o -1 si es el último.
 *
 * Ver definiciones de tipos en voronoi.h (UniformGrid) y de puntos en stippling.h.
 */

/**
 * clampi
 * ------
 * Acota el entero 'v' al rango [lo, hi].
 *
 * Propósito:
 *   Uso interno para asegurar índices válidos (celdas, columnas, filas).
 *
 * Parámetros:
 *   v  -> valor a acotar.
 *   lo -> límite inferior permitido.
 *   hi -> límite superior permitido.
 *
 * Retorna:
 *   El valor acotado a [lo, hi].
 *
 * Complejidad:
 *   O(1).
 */
static inline int clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/**
 * cellIndex
 * ---------
 * Traduce coordenadas de celda (cx, cy) a índice lineal en el arreglo 'head'.
 *
 * Convenciones:
 *   - El índice lineal es: idx = cy * g->cols + cx.
 *   - Si (cx, cy) está fuera de [0..cols) x [0..rows), retorna -1.
 *
 * Parámetros:
 *   g  -> grilla válida (no NULL).
 *   cx -> coordenada de columna de celda.
 *   cy -> coordenada de fila de celda.
 *
 * Retorna:
 *   Índice lineal en 'head' o -1 si (cx,cy) está fuera de rango.
 *
 * Complejidad:
 *   O(1).
 */
static inline int cellIndex(const UniformGrid *g, int cx, int cy)
{
  if (cx < 0 || cy < 0 || cx >= g->cols || cy >= g->rows)
    return -1;
  return cy * g->cols + cx;
}

/**
 * gridBuild
 * ---------
 * Construye la grilla uniforme a partir del conjunto de puntos 's'.
 *
 * Parámetros:
 *   g    -> salida. Grilla a inicializar (head/next se asignan aquí).
 *   w,h  -> dimensiones del canvas en píxeles.
 *   cell -> tamaño de celda en píxeles (p.ej., 32). Debe ser > 0.
 *   s    -> conjunto de puntos (coordenadas en [0..w) x [0..h)).
 *
 * Flujo:
 *   1) Calcula cols, rows como ceil(w/cell), ceil(h/cell).
 *   2) Reserva head[cols*rows] y next[s->count].
 *   3) Inicializa head con -1 (bucket vacío).
 *   4) Para cada punto i:
 *        - Calcula celda (cx, cy) con clamp a los bordes.
 *        - Inserta i al frente de la lista de la celda (push-front O(1)).
 *
 * Retorno:
 *   true  si la grilla se construyó correctamente.
 *   false si hay parámetros inválidos o falla de memoria
 *         (en ese caso libera cualquier bloque parcialmente asignado).
 *
 * Complejidad:
 *   - Tiempo:  O(cols*rows + N)
 *   - Memoria: O(cols*rows + N)
 *
 * Notas:
 *   - Precondición: si 'g' ya contenía memoria, el llamador debe invocar
 *     gridFree(g) antes de llamar a gridBuild para evitar fugas.
 *   - Los puntos fuera de [0..w,h) se clampean a la celda de borde más cercana.
 */
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s)
{
  // Validación básica de argumentos
  if (!g || !s || !s->pts || s->count <= 0 || w <= 0 || h <= 0 || cell <= 0)
    return false;

  // Parámetros geométricos de la grilla
  g->w = w;
  g->h = h;
  g->cell = cell;
  g->cols = (w + cell - 1) / cell; // ceil(w/cell)
  g->rows = (h + cell - 1) / cell; // ceil(h/cell)

  // Reserva de buckets (head) y enlaces (next)
  size_t H = (size_t)g->cols * (size_t)g->rows;
  g->head = (int *)malloc(H * sizeof(int));
  g->next = (int *)malloc((size_t)s->count * sizeof(int));
  if (!g->head || !g->next)
  {
    // Limpieza en caso de fallo parcial
    free(g->head);
    free(g->next);
    g->head = g->next = NULL;
    return false;
  }

  // Inicializa cada bucket a -1 (lista vacía)
  for (size_t i = 0; i < H; i++)
    g->head[i] = -1;

  // Clasifica cada punto en su celda y lo inserta al frente de la lista
  for (int i = 0; i < s->count; i++)
  {
    // Coordenadas de celda con clamp a los bordes
    int cx = clampi((int)(s->pts[i].x) / cell, 0, g->cols - 1);
    int cy = clampi((int)(s->pts[i].y) / cell, 0, g->rows - 1);

    // Índice lineal del bucket
    int hidx = cy * g->cols + cx;

    // Inserción LIFO en O(1)
    g->next[i] = g->head[hidx];
    g->head[hidx] = i;
  }

  return true;
}

/**
 * gridFree
 * --------
 * Libera la memoria asociada a la grilla uniforme creada por gridBuild.
 *
 * Parámetros:
 *   g -> puntero a la grilla a limpiar. Puede ser NULL.
 *
 * Comportamiento:
 *   - Si 'g' es NULL no hace nada.
 *   - Libera los arreglos 'head' y 'next' si existen.
 *   - Deja los punteros internos en NULL y los escalares en 0 para estado neutro.
 *
 * Notas:
 *   - No libera la estructura UniformGrid en sí; solo sus buffers internos.
 *   - Es segura para llamadas repetidas (idempotente).
 */
void gridFree(UniformGrid *g)
{
  if (!g)
    return;

  // Buffers reservados en gridBuild
  free(g->head);
  free(g->next);

  // Estado neutro tras liberar
  g->head = NULL;
  g->next = NULL;
  g->w = 0;
  g->h = 0;
  g->cell = 0;
  g->cols = 0;
  g->rows = 0;
}

/**
 * gridNearest
 * -----------
 * Busca el índice del punto más cercano a (x, y) usando la grilla uniforme `g`
 * como índice espacial. Recorre celdas en “anillos” concéntricos alrededor de
 * la celda base y examina solo el borde de cada anillo para reducir visitas.
 *
 * Requisitos / precondiciones:
 *   - `g` debe haber sido construido con gridBuild para el mismo `Stippling s`
 *     (mismas posiciones y mismo canvas).
 *   - `g->cell > 0`, `g->head` y `g->next` válidos.
 *
 * Parámetros:
 *   g        -> grilla uniforme previamente construida.
 *   s        -> nube de puntos (usa s->pts[i].x/y).
 *   x, y     -> coordenadas de consulta en píxeles (espacio de canvas).
 *   ringMax  -> máximo número de anillos a explorar:
 *               0 = solo la celda base; 1 = base + vecinas inmediatas; etc.
 *               Si es negativo, no se itera y retorna -1.
 *
 * Retorno:
 *   Índice del punto más cercano en `s->pts` o -1 si no hay candidatos
 *   (grilla/entrada inválida o anillos sin puntos).
 *
 * Detalles de implementación:
 *   - La celda base se obtiene por truncamiento: cx0 = (int)x / cell, cy0 = (int)y / cell.
 *   - Para cada anillo r en [0..ringMax], se itera el borde del cuadrado
 *     [cx0-r..cx0+r] × [cy0-r..cy0+r].
 *   - Se calcula distancia euclidiana al cuadrado (sin sqrt) para comparar.
 *   - Heurística de corte: si se encontró al menos un candidato y r >= 1, se interrumpe.
 *
 * Complejidad esperada:
 *   Promedio O(k), donde k es el número de puntos contenidos en las pocas celdas
 *   visitadas (ringMax pequeño suele bastar: 1–2).
 */
int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax)
{
  if (!g || !s || !s->pts)
    return -1;

  // Celda base donde cae (x,y)
  int cx0 = (int)x / g->cell;
  int cy0 = (int)y / g->cell;

  int best = -1;        // índice del mejor candidato
  float bestD2 = 1e30f; // mejor distancia al cuadrado hallada

  // Explora anillos crecientes alrededor de (cx0, cy0)
  for (int r = 0; r <= ringMax; ++r)
  {
    int foundInRing = 0; // hubo algún candidato en este anillo

    // Recorre filas del anillo r
    for (int cy = cy0 - r; cy <= cy0 + r; ++cy)
    {
      // Recorre columnas del anillo r
      for (int cx = cx0 - r; cx <= cx0 + r; ++cx)
      {
        // Solo borde del anillo: evita visitar el interior
        if (cx != cx0 - r && cx != cx0 + r && cy != cy0 - r && cy != cy0 + r)
          continue;

        // Índice del bucket; -1 si (cx,cy) está fuera de la grilla
        int hidx = cellIndex(g, cx, cy);
        if (hidx < 0)
          continue;

        // Itera lista de puntos de esta celda
        for (int i = g->head[hidx]; i != -1; i = g->next[i])
        {
          float dx = s->pts[i].x - x;
          float dy = s->pts[i].y - y;
          float d2 = dx * dx + dy * dy; // distancia al cuadrado

          if (d2 < bestD2)
          {
            bestD2 = d2;
            best = i;
            foundInRing = 1;
          }
        }
      }
    }

    // Heurística: si ya hubo algún candidato y r >= 1, corta.
    if (foundInRing && r >= 1)
      break;
  }

  return best; // -1 si no se encontró ninguno
}
