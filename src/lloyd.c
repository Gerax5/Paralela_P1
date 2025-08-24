#include <stdlib.h>
#include <math.h>
#include "lloyd.h"
#include "image.h"
#include "stippling.h"
#include "voronoi.h"

/*
 * lloyd.c
 * --------
 * Implementa una iteracion del algoritmo de Lloyd para distribuir puntos
 * segun el contenido de una imagen. La densidad objetivo se deriva de la
 * luminancia (zonas oscuras pesan mas con un gamma configurable).
 *
 * Notas de diseno:
 *   - El vecino mas cercano se acelera con una grilla uniforme (voronoi.h).
 *   - El muestreo de la imagen se hace en UV con bilineal para evitar aliasing.
 *   - El parametro "step" controla el stride de muestreo del canvas.
 *   - Se acumulan sumas ponderadas por punto y luego se actualizan los centroides.
 */

static inline unsigned lcg(unsigned *st)
{
  *st = (*st * 1664525u + 1013904223u);
  return *st;
}
static inline int irand_range(unsigned *st, int hi) { return (int)(lcg(st) % (unsigned)hi); }

/*
 * clampi
 * ------
 * Acota un entero 'v' al intervalo [lo, hi].
 *
 * Parametros:
 *   v  -> valor a acotar
 *   lo -> limite inferior
 *   hi -> limite superior
 *
 * Retorna:
 *   'lo' si v < lo, 'hi' si v > hi, en otro caso v.
 *
 * Uso:
 *   Utilidad pequeña para asegurar indices validos al mapear coordenadas
 *   o al calcular indices de celdas dentro de la grilla uniforme.
 */
static inline int clampi(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/*
 * lloydStep
 * ----------
 * Ejecuta una iteración de Lloyd sobre la nube de puntos 's' usando
 * la imagen 'img' como campo de densidad. Cada punto se mueve hacia
 * el centro de masa de su celda de Voronoi, con pesos derivados de
 * la oscuridad de la imagen: w = (1 - luminancia)^gamma.
 *
 * Parametros:
 *   img         -> imagen fuente (formato RGBA32) usada para ponderar.
 *   s           -> estado de puntos a actualizar in-place.
 *   W, H        -> dimensiones del canvas (en píxeles) donde viven los puntos.
 *   step        -> stride de muestreo del canvas (>= 1). Valores mayores
 *                  reducen costo a costa de precisión.
 *   gamma       -> controla la contribución de zonas oscuras (1.0 lineal;
 *                  > 1 enfatiza oscuridad; < 1 la atenúa).
 *
 * Retorna:
 *   true  si la iteración se ejecutó y hubo actualización (aunque no
 *         necesariamente todos los puntos se muevan).
 *   false si parámetros inválidos o error de memoria.
 *
 * Detalles de implementación:
 *   - Se construye una grilla uniforme para acelerar "nearest neighbor".
 *     El tamaño de celda 32 px es un compromiso razonable; puede tunearse.
 *   - El muestreo de luminancia se hace en UV con filtrado bilineal para
 *     evitar aliasing (imagen de entrada puede tener distinta resolución
 *     que el canvas W x H).
 *   - Complejidad aproximada: O((W/step * H/step) * k) donde k es el número
 *     de candidatos visitados por la grilla (constante pequeña con ringMax).
 *   - Se usa acumulación por punto: sumX/sumY/sumW. Luego se normaliza.
 */
bool lloydStep(const Image *img, Stippling *s, int W, int H, int step, float gamma)
{
  // Validación de entradas mínimas
  if (!s || !s->pts || s->count <= 0 || W <= 0 || H <= 0)
    return false;
  if (!img || !img->pixels)
    return false;
  if (step < 1)
    step = 1;

  // Índice espacial: grilla uniforme para acelerar nearest
  UniformGrid g = (UniformGrid){0};
  if (!gridBuild(&g, W, H, 32, s))
  {
    // Si fallara la grilla, podríamos continuar con O(N), pero aquí
    // mantenemos la estructura para el flujo normal. (El fallback existe
    // más abajo cuando gridNearest retorna < 0).
  }

  const int n = s->count;

  // Buffers de acumulación por punto (centroide ponderado)
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

  // Barrido del canvas con stride 'step'
  for (int y = 0; y < H; y += step)
  {
    // Coordenada V normalizada en el centro del píxel
    float v = ((float)y + 0.5f) / (float)H;

    for (int x = 0; x < W; x += step)
    {
      // Coordenada U normalizada en el centro del píxel
      float u = ((float)x + 0.5f) / (float)W;

      // Luminancia bilineal en UV para evitar aliasing
      float lum = sampleIntensityBilinearUV(img, u, v);

      // Remapeo de contraste y umbral de blancos
      // t: umbral; valores por encima se consideran “sin peso”
      // c: contraste; 1/(1-t) reescala a [0,1] el rango útil
      const float t = 0.08f; // prueba 0.05..0.15
      const float c = 1.0f / (1.0f - t);

      float dark = 1.0f - lum;                   // oscuridad lineal
      dark = (dark > t) ? (dark - t) * c : 0.0f; // clamp y remapeo

      // Peso por oscuridad: w = (1 - lum)^gamma (clamp contra números negativos)
      double w = pow(dark, (double)gamma);
      if (w <= 0.0)
        continue; // píxel claro o sin aporte

      // Vecino más cercano usando la grilla (anillo 0..2 suele bastar)
      int best = gridNearest(&g, s, (float)x, (float)y, 2);
      if (best < 0)
      {
        // Fallback raro: si la grilla no encuentra candidatos, usar O(N)
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

      // Acumulación ponderada del centroide para el punto ganador
      sumX[best] += (double)x * w;
      sumY[best] += (double)y * w;
      sumW[best] += w;
    }
  }

  // --- Actualización normal (centroides) ---
  for (int i = 0; i < n; i++)
  {
    if (sumW[i] > 0.0)
    {
      s->pts[i].x = (float)(sumX[i] / sumW[i]);
      s->pts[i].y = (float)(sumY[i] / sumW[i]);
    }
  }

  // --- RESEED de huérfanos: mover semillas con sumW == 0 a un píxel oscuro ---
  unsigned st = (unsigned)(n ^ W ^ H) + 0x9E3779B9u; // semilla base reproducible
  for (int i = 0; i < n; i++)
  {
    if (sumW[i] > 0.0)
      continue;

    // intentar unas cuantas veces encontrar un píxel con peso > eps
    const int maxTries = 64;
    for (int t = 0; t < maxTries; ++t)
    {
      int rx = irand_range(&st, W);
      int ry = irand_range(&st, H);
      float u = ((float)rx + 0.5f) / (float)W;
      float v = ((float)ry + 0.5f) / (float)H;
      float lum = sampleIntensityBilinearUV(img, u, v);
      double w = pow(fmaxf(0.0f, 1.0f - lum), (double)gamma);
      if (w > 1e-6)
      { // umbral pequeño
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
