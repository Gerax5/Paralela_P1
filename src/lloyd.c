#include <stdlib.h>
#include <math.h>
#include "lloyd.h"
#include "image.h"
#include "stippling.h"
#include "voronoi.h"
#include "config.h"

/*
 * lloyd.c
 * --------
 * Implementa una iteración del algoritmo de Lloyd (Centroidal Voronoi Tessellation)
 * para redistribuir puntos en 2D según un campo de densidad derivado de una imagen.
 *
 * Idea:
 *   - Para cada muestra del lienzo (W×H) —tomada con stride `step`— se busca el
 *     punto más cercano (NN) y se acumula su centroide ponderado.
 *   - El peso por muestra se calcula a partir de la luminancia de la imagen:
 *         w = (1 - luminancia)^gamma    (zonas oscuras pesan más)
 *     (en modo diagnóstico puede invertirse a w = (luminancia)^gamma).
 *   - Tras el barrido, cada punto se mueve al centro de masa de su celda.
 *
 * Notas de diseño:
 *   - La búsqueda de vecino más cercano se acelera con una grilla uniforme
 *     (ver voronoi.h/UniformGrid).
 *   - La imagen se muestrea en coordenadas UV con filtrado bilineal para evitar aliasing.
 *   - `step` controla la densidad de muestreo (rendimiento vs. precisión).
 *   - Se acumulan sumas ponderadas por punto (sumX, sumY, sumW) y luego se normaliza.
 *
 * Complejidad aproximada:
 *   O( (W/step * H/step) * k ), donde k es el número de candidatos consultados por la grilla.
 *
 * Robustez:
 *   - Maneja puntos "huérfanos" (sumW==0) resembrándolos aleatoriamente en zonas con peso.
 *   - Valida parámetros básicos (W,H, punteros, etc.).
 */

/*
 * lcg
 * ---
 * Avanza un generador congruencial lineal de 32 bits y devuelve el nuevo estado.
 *
 * Fórmula (Numerical Recipes):
 *     state = state * 1664525 + 1013904223
 *
 * Parámetros:
 *   st -> (in/out) puntero al estado interno del RNG.
 *
 * Retorno:
 *   Nuevo valor de `*st`. (También se usa implícitamente como aleatorio de 32b).
 *
 * Propiedades:
 *   - Muy rápido y determinista (misma semilla -> misma secuencia).
 *   - No criptográficamente seguro; calidad estadística suficiente para utilidades
 *     como resembrado de puntos en este contexto.
 *   - Periodo ~2^32.
 */
static inline unsigned lcg(unsigned *st)
{
  *st = (*st * 1664525u + 1013904223u);
  return *st;
}

/*
 * irand_range
 * -----------
 * Devuelve un entero uniforme aproximado en [0, hi) usando el LCG anterior.
 *
 * Parámetros:
 *   st -> (in/out) estado del RNG (se actualiza con lcg()).
 *   hi -> cota superior exclusiva del rango. Debe ser > 0.
 *
 * Retorno:
 *   Un entero en [0, hi).
 *
 * Notas y advertencias:
 *   - Implementación vía módulo: lcg(st) % hi. Para `hi` que no divide 2^32,
 *     existe un sesgo modular mínimo; aceptable para propósitos gráficos.
 *   - Precondición: hi > 0. Si hi == 0, el comportamiento no está definido.
 *   - Costo O(1).
 */
static inline int irand_range(unsigned *st, int hi)
{
  return (int)(lcg(st) % (unsigned)hi);
}

/*
 * clampi
 * ------
 * Acota un entero `v` al intervalo cerrado [lo, hi].
 *
 * Parámetros:
 *   v  -> valor de entrada.
 *   lo -> límite inferior.
 *   hi -> límite superior (debe cumplir hi >= lo).
 *
 * Retorno:
 *   - `lo` si v < lo
 *   - `hi` si v > hi
 *   - `v` en caso contrario
 *
 * Uso típico:
 *   - Asegurar índices válidos al mapear a celdas de la grilla o al acceder
 *     buffers de imagen.
 *
 * Complejidad:
 *   O(1).
 */
static inline int clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/***
 * lloydStep
 * ----------
 * Ejecuta una iteración de Lloyd (Centroidal Voronoi) sobre la nube de puntos `s`
 * usando la imagen `img` como campo de densidad. Cada muestra del lienzo aporta
 * masa al punto más cercano y luego cada punto se mueve al centroide ponderado.
 *
 * Peso por muestra:
 *   - Modo brillo (debug/nativo):   w = (lum)^gamma
 *   - Modo oscuridad (clásico):     w = (1 - lum)^gamma
 *   El modo activo se selecciona en compile-time con STIPPLE_WEIGHT_BY_BRIGHTNESS.
 *
 * Parámetros:
 *   img   -> imagen fuente (RGBA32) usada para calcular luminancia (Rec.709) con muestreo bilineal.
 *   s     -> conjunto de puntos (modificado in-place).
 *   W,H   -> dimensiones del canvas en píxeles donde viven los puntos.
 *   step  -> stride de muestreo del canvas (>= 1). Mayor => más rápido, menos preciso.
 *   gamma -> exponente del peso ( >1 enfatiza, <1 atenúa el modo elegido).
 *
 * Retorna:
 *   true  si la iteración se ejecutó correctamente (aunque algunos puntos no se muevan).
 *   false si hay parámetros inválidos o falla la reserva de memoria auxiliar.
 *
 * Detalles de implementación:
 *   - NN acelerado con grilla uniforme (cell=32 px). Si no encuentra candidatos,
 *     hace fallback O(N).
 *   - Muestreo de luminancia en UV con filtrado bilineal (evita aliasing).
 *   - Acumulación por punto: sumX/sumY/sumW (doble precisión). Luego normaliza.
 *   - Huérfanos (sumW==0): re-seed aleatorio en zona con peso > 0.
 *
 * Complejidad aproximada:
 *   O( (W/step * H/step) * k ), con k ~ puntos inspeccionados por la grilla (pequeño).
 */
bool lloydStep(const Image *img, Stippling *s, int W, int H, int step, float gamma)
{
  // Validación mínima
  if (!s || !s->pts || s->count <= 0 || W <= 0 || H <= 0)
    return false;
  if (!img || !img->pixels)
    return false;
  if (step < 1)
    step = 1;

  // Índice espacial para nearest-neighbor (grilla uniforme)
  UniformGrid g = (UniformGrid){0};
  if (!gridBuild(&g, W, H, 32, s))
  {
    // Si la grilla falla, seguimos; gridNearest hará fallback a O(N) más abajo.
  }

  const int n = s->count;

  // Buffers de acumulación (centroides ponderados)
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

  // Barrido del canvas en pasos de 'step'
  for (int y = 0; y < H; y += step)
  {
    float v = ((float)y + 0.5f) / (float)H; // UV centrado en el píxel

    for (int x = 0; x < W; x += step)
    {
      float u = ((float)x + 0.5f) / (float)W; // UV centrado en el píxel

      // Luminancia bilineal (Rec.709, en lineal)
      float lum = sampleIntensityBilinearUV(img, u, v);

      // Peso según modo de compilación
      double w;
#if STIPPLE_WEIGHT_BY_BRIGHTNESS
      // Favorece zonas claras (comportamiento “debug/nativo”)
      w = pow(fmax(0.0, lum), (double)gamma);
#else
      // Favorece zonas oscuras (modo clásico)
      w = pow(fmax(0.0, 1.0 - lum), (double)gamma);
#endif

      if (w <= 0.0)
        continue; // sin aporte

      // NN por grilla (anillo 0..2 suele bastar)
      int best = gridNearest(&g, s, (float)x, (float)y, 2);
      if (best < 0)
      {
        // Fallback O(N) si la celda/anillos estaban vacíos
        best = 0;
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
      }

      // Acumulación ponderada para el punto ganador
      sumX[best] += (double)x * w;
      sumY[best] += (double)y * w;
      sumW[best] += w;
    }
  }

  // Actualización de centroides
  for (int i = 0; i < n; i++)
  {
    if (sumW[i] > 0.0)
    {
      s->pts[i].x = (float)(sumX[i] / sumW[i]);
      s->pts[i].y = (float)(sumY[i] / sumW[i]);
    }
  }

  // Re-seed de huérfanos (sumW == 0): probar posiciones aleatorias con peso > ε
  unsigned st = (unsigned)(n ^ W ^ H) + 0x9E3779B9u; // semilla base reproducible
  for (int i = 0; i < n; i++)
  {
    if (sumW[i] > 0.0)
      continue;

    const int maxTries = 64; // intentos razonables
    for (int t = 0; t < maxTries; ++t)
    {
      int rx = irand_range(&st, W);
      int ry = irand_range(&st, H);
      float u = ((float)rx + 0.5f) / (float)W;
      float v = ((float)ry + 0.5f) / (float)H;
      float lum = sampleIntensityBilinearUV(img, u, v);

      double w =
#if STIPPLE_WEIGHT_BY_BRIGHTNESS
          pow(fmaxf(0.0f, lum), (double)gamma);
#else
          pow(fmaxf(0.0f, 1.0f - lum), (double)gamma);
#endif

      if (w > 1e-6) // pequeño umbral para evitar blancos puros (o negros en modo brillo)
      {
        s->pts[i].x = (float)rx;
        s->pts[i].y = (float)ry;
        break;
      }
    }
  }

  // Limpieza
  free(sumX);
  free(sumY);
  free(sumW);
  gridFree(&g);

  return true;
}
