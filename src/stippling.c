#include <stdlib.h>
#include <math.h>
#include "stippling.h"
#include "image.h"

/*
 * frand01
 * --------
 * Genera un float pseudoaleatorio en [0, 1] a partir de un estado LCG.
 *
 * Descripcion:
 *   Avanza un generador congruencial lineal (LCG) de 32 bits y devuelve
 *   un valor normalizado usando los 24 bits altos del estado.
 *
 * Parametros:
 *   st -> puntero al estado interno del RNG. Se actualiza in-place.
 *
 * Retorna:
 *   Valor pseudoaleatorio en el rango [0, 1]. El valor 1.0 es posible
 *   (aunque poco frecuente). Si necesitas estrictamente [0, 1), cambia
 *   el divisor a 16777216.0f (2^24) o resta un epsilon minimo.
 *
 * Notas:
 *   - Este LCG es rapido y determinista (misma semilla -> misma secuencia).
 *   - No es criptograficamente seguro ni de alta calidad estadistica,
 *     pero es suficiente para distribuir puntos de forma reproducible.
 *   - La formula usada es el classico LCG de "Numerical Recipes":
 *       state = state * 1664525 + 1013904223
 *   - Se toman los 24 bits altos (state >> 8) para reducir correlaciones.
 */
static float frand01(unsigned *st)
{
  // Avanza el LCG
  *st = (*st * 1664525u + 1013904223u);

  // Normaliza a [0, 1] usando 24 bits de precision
  return ((*st >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

/*
 * drawFilledCircle
 * ----------------
 * Dibuja un circulo relleno centrado en (cx, cy) con radio 'radius'
 * usando segmentos horizontales (scanlines).
 *
 * Descripcion:
 *   Recorre dy en [-radius, radius] y, para cada fila, calcula el
 *   semiancho dx = sqrt(radius^2 - dy^2). Luego traza una linea
 *   horizontal desde (cx - dx, y) hasta (cx + dx, y).
 *
 * Parametros:
 *   r      -> SDL_Renderer valido. El color de dibujo debe estar seteado por el caller.
 *   cx, cy -> centro del circulo en coordenadas de pantalla.
 *   radius -> radio en pixeles. Si es < 0, no se dibuja nada.
 *
 * Notas:
 *   - Complejidad O(radius): se emite una linea por fila del circulo.
 *   - No hace clipping manual; SDL_RenderDrawLine se encarga de recortar.
 *   - El color no se modifica aqui. Setear con SDL_SetRenderDrawColor antes de llamar.
 */
static void drawFilledCircle(SDL_Renderer *r, int cx, int cy, int radius)
{
  // Recorre cada fila vertical dentro del disco
  for (int dy = -radius; dy <= radius; ++dy)
  {
    int y = cy + dy; // coordenada y actual

    // Para esta fila, el semiancho horizontal del disco es:
    // dx = sqrt(radius^2 - dy^2). Convertimos a int por truncamiento.
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));

    // Dibuja el segmento horizontal que rellena la fila del circulo
    SDL_RenderDrawLine(r, cx - dx, y, cx + dx, y);
  }
}

/*
 * stipplingInit
 * -------------
 * Inicializa la estructura Stippling con 'n' puntos distribuidos
 * pseudoaleatoriamente dentro del rectangulo [0, w) x [0, h).
 *
 * Parametros:
 *   s     -> salida. Estructura a inicializar.
 *   n     -> cantidad de puntos; debe ser > 0.
 *   w, h  -> dimensiones del canvas en pixeles; ambos deben ser > 0.
 *   seed  -> semilla para el RNG. Si es 0 se usa un valor por defecto.
 *
 * Retorna:
 *   true  si la memoria se reserva y los puntos se generan correctamente.
 *   false si hay parametros invalidos o falla la reserva de memoria.
 *
 * Efectos:
 *   - Reserva s->pts con 'n' entradas (caller debe liberar con stipplingFree).
 *   - Inicializa s->count, s->width y s->height.
 *   - Escribe coordenadas en rango [0, w) y [0, h) usando un LCG determinista.
 *
 * Notas:
 *   - La distribucion es uniforme independiente por eje (no Poisson-disc).
 *   - Si malloc falla, no se modifican los campos de 's' mas alla del intento.
 *   - Complejidad O(n).
 */
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed)
{
  // Validacion basica de parametros
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

/*
 * stipplingFree
 * -------------
 * Libera el arreglo de puntos y deja la estructura en estado neutro.
 *
 * Parametros:
 *   s -> puntero a la estructura Stippling a limpiar (puede ser NULL).
 *
 * Comportamiento:
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

/*
 * stipplingRender
 * ---------------
 * Dibuja la nube de puntos en el renderer usando discos llenos de radio fijo.
 *
 * Parametros:
 *   s      -> estado del stippling (lista de puntos en espacio de ventana).
 *   ren    -> SDL_Renderer valido donde se dibuja.
 *   radius -> radio visual (en pixeles). Si es < 1 se fuerza a 1.
 *
 * Comportamiento:
 *   - Si 's' es NULL o 's->pts' es NULL, no hace nada.
 *   - Ajusta el color de dibujo a casi blanco (250,250,250,255).
 *   - Redondea cada posicion (x,y) a entero y traza un circulo lleno
 *     con un algoritmo por scanlines (drawFilledCircle).
 *
 * Notas:
 *   - No limpia ni presenta el frame; eso lo hace el bucle principal.
 *   - Complejidad aproximada O(N * r), donde r es el radio, porque
 *     cada circulo se dibuja con ~2*r+1 lineas horizontales.
 *   - Si necesitas otro color/estilo, cambia el SDL_SetRenderDrawColor
 *     antes de llamar a esta funcion o expone el color como parametro.
 */
void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius)
{
  if (!s || !s->pts)
    return;

  SDL_SetRenderDrawColor(ren, 250, 250, 250, 255); // color de los puntos
  int r = (radius < 1) ? 1 : radius;               // asegurar radio minimo

  for (int i = 0; i < s->count; i++)
  {
    // Redondeo a pixel entero para evitar aliasing raro de subpixel
    int x = (int)lroundf(s->pts[i].x);
    int y = (int)lroundf(s->pts[i].y);

    // Dibuja un disco lleno centrado en (x,y) con radio r
    drawFilledCircle(ren, x, y, r);
  }
}

void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH,
                           const Image *img, float minR, float maxR,
                           bool useColor, bool invertTheme)
{
  if (!s || !s->pts)
    return;

  // Fondo/tema: solo afecta color por defecto del punto
  Uint8 baseR = invertTheme ? 0 : 250;
  Uint8 baseG = invertTheme ? 0 : 250;
  Uint8 baseB = invertTheme ? 0 : 250;

  float minRcl = (minR < 0.5f) ? 0.5f : minR;
  float maxRcl = (maxR < minRcl) ? minRcl : maxR;

  for (int i = 0; i < s->count; ++i)
  {
    float x = s->pts[i].x;
    float y = s->pts[i].y;

    // UV en canvas -> imagen
    float u = (canvasW > 1) ? (x + 0.5f) / (float)canvasW : 0.0f;
    float v = (canvasH > 1) ? (y + 0.5f) / (float)canvasH : 0.0f;

    // Brillo y color de la imagen en la posicion del punto
    float lum = 0.0f;
    Uint8 r = baseR, g = baseG, b = baseB;

    if (img && img->pixels)
    {
      lum = sampleIntensityBilinearUV(img, u, v); // [0..1]
      if (useColor)
        sampleRgbBilinearUV(img, u, v, &r, &g, &b);
    }

    // Radio: pequeño en zonas claras, grande en oscuras
    float inv = 1.0f - lum; // 0 claro, 1 oscuro
    float radius = minRcl + inv * (maxRcl - minRcl);
    int ir = (radius < 1.0f) ? 1 : (int)radius;

    // Color del punto (tema invertido solo afecta base cuando no usamos color)
    if (!useColor)
      SDL_SetRenderDrawColor(ren, r, g, b, 255);
    else
      SDL_SetRenderDrawColor(ren, r, g, b, 230);

    // Disco lleno (reutiliza tu helper actual)
    // Nota: usamos el mismo algoritmo por scanlines del módulo.
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
