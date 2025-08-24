#include <stdlib.h>
#include <math.h>
#include "stippling.h"
#include "image.h"

/**
 * frand01
 * -------
 * Genera un float pseudoaleatorio en [0, 1] usando un LCG de 32 bits.
 *
 * Descripción:
 *   Avanza el generador congruencial lineal y devuelve un valor normalizado
 *   empleando los 24 bits altos del estado para reducir correlaciones.
 *
 * Parámetros:
 *   st -> puntero al estado interno del RNG; se actualiza in-place.
 *
 * Retorna:
 *   Valor pseudoaleatorio en [0, 1]. El 1.0 es posible (raro). Si necesitas
 *   estrictamente [0, 1), usa 16777216.0f (2^24) como divisor o resta un ε.
 *
 * Notas:
 *   - Determinista (misma semilla -> misma secuencia); no cripto-seguro.
 *   - Fórmula LCG (Numerical Recipes): state = state * 1664525 + 1013904223.
 */
static float frand01(unsigned *st)
{
  // Avanza el LCG
  *st = (*st * 1664525u + 1013904223u);

  // Normaliza a [0, 1] usando 24 bits de precisión
  return ((*st >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

/**
 * stipplingInit
 * -------------
 * Inicializa la estructura Stippling con 'n' puntos distribuidos
 * pseudoaleatoriamente dentro del rectángulo [0, w) x [0, h).
 *
 * Parámetros:
 *   s     -> salida; estructura a inicializar (no debe ser NULL).
 *   n     -> cantidad de puntos; debe ser > 0.
 *   w, h  -> dimensiones del canvas en píxeles; ambos > 0.
 *   seed  -> semilla para el RNG. Si es 0 se usa una por defecto.
 *
 * Retorna:
 *   true  si la memoria se reserva y los puntos se generan correctamente.
 *   false si hay parámetros inválidos o falla la reserva de memoria.
 *
 * Efectos/contrato:
 *   - Reserva s->pts con 'n' entradas (liberar luego con stipplingFree).
 *   - Inicializa s->count, s->width y s->height sólo tras un malloc exitoso.
 *   - Escribe coordenadas en rango [0, w) y [0, h) usando un LCG determinista.
 *
 * Notas:
 *   - La distribución es uniforme e independiente por eje (no Poisson-disc).
 *   - Si malloc falla, no se modifican los campos de 's' (permanece como llegó).
 *   - Complejidad O(n).
 */
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed)
{
  // Validación básica de parámetros
  if (!s || n <= 0 || w <= 0 || h <= 0)
    return false;

  // Reservar memoria para 'n' puntos
  s->pts = (Dot *)malloc(sizeof(Dot) * (size_t)n);
  if (!s->pts)
    return false;

  // Guardar metadatos del conjunto
  s->count = n;
  s->width = w;
  s->height = h;

  // Estado del RNG (usa semilla por defecto si seed == 0)
  unsigned st = seed ? seed : 1234567u;

  // Generar puntos en [0, w) x [0, h)
  for (int i = 0; i < n; i++)
  {
    s->pts[i].x = frand01(&st) * (float)w;
    s->pts[i].y = frand01(&st) * (float)h;
  }

  return true;
}

/**
 * stipplingFree
 * -------------
 * Libera el arreglo de puntos y deja la estructura en estado neutro.
 *
 * Parámetros:
 *   s -> puntero a la estructura Stippling a limpiar (puede ser NULL).
 *
 * Comportamiento/contrato:
 *   - Si 's' es NULL no hace nada.
 *   - Libera 's->pts' si fue reservado por stipplingInit.
 *   - Pone 's->pts' en NULL y resetea count/width/height a 0.
 *
 * Notas:
 *   - No libera la propia estructura 's' (solo su contenido).
 *   - Es segura para llamadas repetidas (idempotente).
 *   - Complejidad O(1).
 */
void stipplingFree(Stippling *s)
{
  if (!s)
    return;

  free(s->pts);  // liberar buffer de puntos (puede ser NULL)
  s->pts = NULL; // dejar puntero en estado seguro
  s->count = 0;  // resetear metadatos
  s->width = 0;
  s->height = 0;
}

/**
 * stipplingRenderStyled
 * ---------------------
 * Dibuja la nube de puntos con radio por-punto en función del brillo de la imagen,
 * y opcionalmente colorea cada punto con el color muestreado de la propia imagen.
 *
 * Parámetros:
 *   s          -> conjunto de puntos a dibujar (no NULL, s->pts != NULL).
 *   ren        -> SDL_Renderer de destino (válido, hilo principal).
 *   canvasW/H  -> dimensiones del canvas donde viven los puntos (en píxeles).
 *   img        -> imagen de referencia; si es válida se usa para brillo/color.
 *   minR       -> radio mínimo (clamp interno a [0.5, maxR]).
 *   maxR       -> radio máximo (si maxR < minR se clampa a minR).
 *   useColor   -> si true, el color del punto se toma de la imagen (sRGB bilineal).
 *   invertTheme-> si true, el color base (cuando no hay color de imagen) es negro; si false, blanco.
 *
 * Comportamiento:
 *   - Para cada punto (x,y), se computan coordenadas UV del canvas a la imagen:
 *       u = (x + 0.5)/canvasW, v = (y + 0.5)/canvasH  (clamp implícito en muestreo).
 *   - Se obtiene luminancia (0..1) con sampleIntensityBilinearUV (Rec.709 en lineal).
 *   - Radio por punto: r = lerp(minR, maxR, 1 - lum)  (claro -> pequeño, oscuro -> grande).
 *   - Color:
 *       * useColor == false: usa color base del tema (blanco/negro según invert).
 *       * useColor == true : usa color sRGB bilineal de la imagen y alfa 230.
 *   - El disco se rellena con scanlines horizontales (SDL_RenderDrawLine).
 *
 * Notas:
 *   - Si img es NULL/ inválida, lum=0 -> r ≈ maxR para todos los puntos (tema aún aplica).
 *   - Complejidad ~ O(Σ_i r_i) ≈ O(N * r_prom), ya que se dibujan ~2*r+1 líneas por punto.
 *   - Cambia el color de dibujo del renderer varias veces; no modifica blending mode.
 *   - Llamar desde el hilo principal (SDL no es thread-safe para render).
 */
void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH,
                           const Image *img, float minR, float maxR,
                           bool useColor, bool invertTheme)
{
  if (!s || !s->pts)
    return;

  // Color base según tema (solo se usa cuando no se toma color de imagen)
  Uint8 baseR = invertTheme ? 0 : 250;
  Uint8 baseG = invertTheme ? 0 : 250;
  Uint8 baseB = invertTheme ? 0 : 250;

  // Clamp suave de radios
  float minRcl = (minR < 0.5f) ? 0.5f : minR;
  float maxRcl = (maxR < minRcl) ? minRcl : maxR;

  for (int i = 0; i < s->count; ++i)
  {
    float x = s->pts[i].x;
    float y = s->pts[i].y;

    // Canvas -> UV (normalizado) para muestrear imagen
    float u = (canvasW > 1) ? (x + 0.5f) / (float)canvasW : 0.0f;
    float v = (canvasH > 1) ? (y + 0.5f) / (float)canvasH : 0.0f;

    // Brillo y color en la posición del punto
    float lum = 0.0f;
    Uint8 r = baseR, g = baseG, b = baseB;

    if (img && img->pixels)
    {
      lum = sampleIntensityBilinearUV(img, u, v); // [0..1] en lineal
      if (useColor)
        sampleRgbBilinearUV(img, u, v, &r, &g, &b);
    }

    // Radio por punto: claro->pequeño, oscuro->grande
    float inv = 1.0f - lum;                          // 0 claro, 1 oscuro
    float radius = minRcl + inv * (maxRcl - minRcl); // lerp(minR,maxR,inv)
    int ir = (radius < 1.0f) ? 1 : (int)radius;      // entero para raster

    // Set de color (alfa más bajo si usamos color de imagen para suavizar)
    if (!useColor)
      SDL_SetRenderDrawColor(ren, r, g, b, 255);
    else
      SDL_SetRenderDrawColor(ren, r, g, b, 230);

    // Relleno del disco por scanlines
    for (int dy = -ir; dy <= ir; ++dy)
    {
      int yy = (int)lroundf(y) + dy;
      int dx = (int)sqrtf((float)(ir * ir - dy * dy));
      int x0 = (int)lroundf(x) - dx;
      int x1 = (int)lroundf(x) + dx;
      SDL_RenderDrawLine(ren, x0, yy, x1, yy);
    }
  }
}
