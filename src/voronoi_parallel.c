#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include "voronoi.h"

static inline int clampi(int v, int lo, int hi);
static bool  gridBuildCSR_omp(UniformGrid *g, int w, int h, int cell, const Stippling *s);
static void  gridFreeCSR(UniformGrid *g);
static inline int cellIndexCSR(const UniformGrid *g, int cx, int cy);
static int   gridNearestCSR(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);

/**
 * clampi
 * ------
 * Acota un entero al rango [lo, hi].
 *
 * Parámetros:
 *   v  -> valor de entrada.
 *   lo -> límite inferior.
 *   hi -> límite superior.
 *
 * Retorna:
 *   v limitado al intervalo [lo, hi].
 *
 * Complejidad:
 *   O(1).
 */
static inline int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}



/**
 * gridBuild
 * ---------
 * Construye la grilla CSR paralela para el conjunto de puntos.
 *
 * Parámetros:
 *   g     -> salida; grilla a llenar (no NULL).
 *   w, h  -> dimensiones del canvas (px).
 *   cell  -> tamaño de celda (px, > 0).
 *   s     -> conjunto de puntos (stippling) válido.
 *
 * Retorna:
 *   true  si se construyó correctamente.
 *   false en parámetros inválidos o fallo de memoria.
 *
 * Notas:
 *   - Llama a la variante paralela CSR (gridBuildCSR_omp).
 *   - Libera cualquier estructura previa (legacy o CSR) antes de construir.
 */
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s) {
  return gridBuildCSR_omp(g, w, h, cell, s);
}


/**
 * gridFree
 * --------
 * Libera las estructuras CSR asociadas a la grilla.
 *
 * Parámetros:
 *   g -> grilla a limpiar (puede ser NULL).
 *
 * Notas:
 *   - No libera 'g' en sí; solo sus buffers internos.
 *   - Deja metadatos en 0.
 *   - Idempotente.
 */
void gridFree(UniformGrid *g) {
  if (!g) return;
  gridFreeCSR(g);
  g->w = g->h = g->cell = g->cols = g->rows = 0;
}

/**
 * gridNearest
 * -----------
 * Busca el índice del punto más cercano a (x,y) explorando celdas por anillos,
 * usando los rangos CSR contiguos por celda.
 *
 * Parámetros:
 *   g       -> grilla CSR válida.
 *   s       -> conjunto de puntos (stippling).
 *   x, y    -> consulta en coordenadas canvas.
 *   ringMax -> radio máximo de anillos a expandir.
 *
 * Retorna:
 *   Índice del punto más cercano, o -1 si no hay candidatos válidos.
 *
 * Notas:
 *   - Mantiene la heurística de "cortar cuando encontró algo en r>=1".
 *   - En cada celda recorre un rango [offset[h], offset[h+1]) contiguo.
 *   - Dentro del rango, el bucle se marca con '#pragma omp simd' para
 *     habilitar vectorización (SIMD) de las distancias.
 */
int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax) {
  return gridNearestCSR(g, s, x, y, ringMax);
}

// ---------- IMPLEMENTACIÓN CSR ----------
/**
 * gridNearest
 * -----------
 * Busca el índice del punto más cercano a (x,y) explorando celdas por anillos,
 * usando los rangos CSR contiguos por celda.
 *
 * Parámetros:
 *   g       -> grilla CSR válida.
 *   s       -> conjunto de puntos (stippling).
 *   x, y    -> consulta en coordenadas canvas.
 *   ringMax -> radio máximo de anillos a expandir.
 *
 * Retorna:
 *   Índice del punto más cercano, o -1 si no hay candidatos válidos.
 *
 * Notas:
 *   - Mantiene la heurística de "cortar cuando encontró algo en r>=1".
 *   - En cada celda recorre un rango [offset[h], offset[h+1]) contiguo.
 *   - Dentro del rango, el bucle se marca con '#pragma omp simd' para
 *     habilitar vectorización (SIMD) de las distancias.
 */
static bool gridBuildCSR_omp(UniformGrid *g, int w, int h, int cell, const Stippling *s)
{
  if (!g || !s || !s->pts || s->count <= 0 || w <= 0 || h <= 0 || cell <= 0)
    return false;

  g->w = w; g->h = h; g->cell = cell;
  g->cols = (w + cell - 1) / cell;
  g->rows = (h + cell - 1) / cell;

  const int H = g->cols * g->rows;
  const int N = s->count;

  free(g->head); g->head = NULL;
  free(g->next); g->next = NULL;

  // Limpia de CSR previo
  gridFreeCSR(g);

  g->cellCount  = (int*)calloc((size_t)H, sizeof(int));
  g->cellOffset = (int*)calloc((size_t)H + 1, sizeof(int));
  g->cellPoints = (int*)malloc((size_t)N * sizeof(int));
  if (!g->cellCount || !g->cellOffset || !g->cellPoints) {
    gridFreeCSR(g);
    return false;
  }

  // Conteo de puntos por celda
  #pragma omp parallel for schedule(static)
  for (int i = 0; i < N; ++i) {
    int cx = clampi((int)(s->pts[i].x) / cell, 0, g->cols - 1);
    int cy = clampi((int)(s->pts[i].y) / cell, 0, g->rows - 1);
    int hidx = cy * g->cols + cx;
    #pragma omp atomic update
    g->cellCount[hidx] += 1;
  }

  // Se organizan los puntos por celda, se obtienen los rangos de los indices de los puntos
  g->cellOffset[0] = 0;
  for (int hidx = 0; hidx < H; ++hidx) {
    g->cellOffset[hidx + 1] = g->cellOffset[hidx] + g->cellCount[hidx];
  }

  // Reusar cellCount
  #pragma omp parallel for schedule(static)
  for (int hidx = 0; hidx < H; ++hidx) {
    g->cellCount[hidx] = 0;
  }

  // se dispera cada punto en su rango correspondiente usando esos offsets
  // para cada celda tienes sus puntos contiguos en memoria; así, el nearest neighbor por anillos puede recorrer candidatos rápido y con SIMD.
  #pragma omp parallel for schedule(static)
  for (int i = 0; i < N; ++i) {
    int cx = clampi((int)(s->pts[i].x) / cell, 0, g->cols - 1);
    int cy = clampi((int)(s->pts[i].y) / cell, 0, g->rows - 1);
    int hidx = cy * g->cols + cx;
    int slot;
    // Lectura y escritura concurrente en cellCount[hidx]
    #pragma omp atomic capture
    slot = g->cellCount[hidx]++; // cada hilo recibe un índice distinto
    g->cellPoints[g->cellOffset[hidx] + slot] = i;
  }

  return true;
}

/**
 * gridFreeCSR
 * -----------
 * Libera los buffers CSR (cellCount, cellOffset, cellPoints).
 *
 * Parámetros:
 *   g -> grilla a limpiar (no NULL).
 *
 * Notas:
 *   - Seguro de llamar múltiples veces; deja punteros en NULL.
 */
static void gridFreeCSR(UniformGrid *g) {
  free(g->cellCount);  g->cellCount  = NULL;
  free(g->cellOffset); g->cellOffset = NULL;
  free(g->cellPoints); g->cellPoints = NULL;
}

/**
 * cellIndexCSR
 * ------------
 * Traduce coordenadas de celda (cx,cy) a índice lineal en la grilla.
 *
 * Parámetros:
 *   g  -> grilla válida (no NULL).
 *   cx -> columna de celda.
 *   cy -> fila de celda.
 *
 * Retorna:
 *   Índice lineal en [0..cols*rows) o -1 si está fuera de rango.
 *
 * Complejidad:
 *   O(1).
 */
static inline int cellIndexCSR(const UniformGrid *g, int cx, int cy)
{
  if (cx < 0 || cy < 0 || cx >= g->cols || cy >= g->rows) return -1;
  return cy * g->cols + cx;
}
/**
 * gridNearestCSR
 * --------------
 * Vecino más cercano a (x,y) explorando celdas por anillos y evaluando
 * candidatos en rangos contiguos (CSR) con soporte de vectorización.
 *
 * Parámetros:
 *   g       -> grilla CSR (con cellOffset/cellPoints válidos).
 *   s       -> conjunto de puntos (stippling).
 *   x, y    -> consulta en coordenadas canvas.
 *   ringMax -> radio máximo de anillos a explorar.
 *
 * Retorna:
 *   Índice del punto más cercano o -1 si no hay candidatos.
 *
 * Notas:
 *   - '#pragma omp simd' indica al compilador vectorizar el bucle interno
 *     (SIMD en un solo hilo). Para reducción de mínimo con índice, considerar
 *     una reducción personalizada (declare reduction) si el compilador no
 *     vectoriza por la dependencia del 'best/bestD2'.
 *
 * Complejidad:
 *   Depende de densidad y ringMax; por celda el bucle es O(n_h), con datos contiguos.
 */
static int gridNearestCSR(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax)
{
  if (!g || !s || !s->pts || !g->cellOffset || !g->cellPoints) return -1;

  int cx0 = (int)x / g->cell;
  int cy0 = (int)y / g->cell;

  int   best  = -1;
  float bestD2 = 1e30f;

  for (int r = 0; r <= ringMax; ++r) {
    int foundInRing = 0;

    for (int cy = cy0 - r; cy <= cy0 + r; ++cy) {
      for (int cx = cx0 - r; cx <= cx0 + r; ++cx) {
        if (cx != cx0 - r && cx != cx0 + r && cy != cy0 - r && cy != cy0 + r)
          continue;

        int hidx = cellIndexCSR(g, cx, cy);
        if (hidx < 0) continue;

        int b = g->cellOffset[hidx];
        int e = g->cellOffset[hidx + 1];

        // Datos contiguos → vectorizable
        #pragma omp simd // vectorización por registros
        for (int k = b; k < e; ++k) {
          int i = g->cellPoints[k];
          float dx = s->pts[i].x - x;
          float dy = s->pts[i].y - y;
          float d2 = dx*dx + dy*dy;
          if (d2 < bestD2) { bestD2 = d2; best = i; foundInRing = 1; }
        }
      }
    }
    if (foundInRing && r >= 1) break;
  }
  return best;
}
