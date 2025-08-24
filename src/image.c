#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>
#include "image.h"

/*
 * imageLoad
 * ----------
 * Carga una imagen desde disco usando SDL_image y la deja en un formato
 * de pixeles uniforme (RGBA8888) para facilitar el muestreo.
 *
 * Flujo:
 *   1) IMG_Load(path) -> SDL_Surface en el formato original del archivo.
 *   2) SDL_ConvertSurfaceFormat(..., SDL_PIXELFORMAT_RGBA32) para homogenizar a RGBA8888.
 *   3) Guarda punteros y metadatos en 'img' (surface, pixels, w, h, pitchPixels).
 *
 * Parametros:
 *   img   -> salida. Estructura donde se escriben los datos de la imagen.
 *   path  -> ruta del archivo (PNG/JPG u otros soportados por SDL_image).
 *
 * Retorna:
 *   true  si la imagen se cargo y convirtio correctamente.
 *   false si hubo algun error (se loguea a stderr).
 *
 * Notas:
 *   - 'img->pixels' apunta al buffer interno de 'img->surface'; no se debe liberar aparte.
 *   - 'pitchPixels' es el pitch en unidades de pixeles, no en bytes (pitch/4 para RGBA8888).
 *   - En caso de error, no quedan recursos colgados (se libera 'loaded' si corresponde).
 */
bool imageLoad(Image *img, const char *path)
{
  // Validar punteros de entrada
  if (!img || !path)
    return false;

  // Limpiar la estructura de salida por si llega sucia
  img->w = img->h = img->pitchPixels = 0;
  img->pixels = NULL;
  img->surface = NULL;

  // Cargar el archivo con SDL_image (formato original del recurso)
  SDL_Surface *loaded = IMG_Load(path);
  if (!loaded)
  {
    fprintf(stderr, "IMG_Load(%s): %s\n", path, IMG_GetError());
    return false;
  }

  // Convertir a un formato de 32 bits conocido (RGBA8888)
  SDL_Surface *conv = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded); // ya no se necesita el surface con formato original
  if (!conv)
  {
    fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
    return false;
  }

  // Completar la estructura de salida con la superficie convertida
  img->surface = conv;
  img->w = conv->w;
  img->h = conv->h;
  img->pitchPixels = conv->pitch / 4;   // bytes por fila / 4 bytes por pixel
  img->pixels = (Uint32 *)conv->pixels; // acceso directo a RGBA8888

  return true;
}

/*
 * imageFree
 *-----------
 * Libera la SDL_Surface asociada a la imagen y deja la estructura en estado neutro.
 *
 * Parametros:
 *   img -> puntero a la imagen a limpiar. Puede ser NULL.
 *
 * Comportamiento:
 *   - Si 'img' es NULL no hace nada.
 *   - Si 'img->surface' existe, la libera con SDL_FreeSurface.
 *   - Pone a NULL los punteros y a 0 los campos escalares.
 *
 * Notas:
 *   - No libera la propia estructura 'Image' (solo su contenido).
 *   - 'img->pixels' apunta al buffer interno de 'img->surface'; al liberar
 *     la surface, ese puntero queda invalido, por eso se setea a NULL.
 *   - Es segura para llamadas repetidas.
 */
void imageFree(Image *img)
{
  // Validar puntero
  if (!img)
    return;

  // Liberar surface si existe
  if (img->surface)
    SDL_FreeSurface(img->surface);

  // Resetear campos a un estado conocido
  img->surface = NULL;
  img->pixels = NULL;
  img->w = img->h = img->pitchPixels = 0;
}

/*
 * rgbaToUint8
 * -----------
 * Desempaqueta un pixel de 32 bits en sus componentes de 8 bits (R,G,B,A)
 * usando corrimientos de bits.
 *
 * Suposiciones importantes:
 *   - El valor 'px' esta en layout 0xRRGGBBAA cuando se interpreta como Uint32.
 *     Esto coincide con SDL_PIXELFORMAT_RGBA32 en big-endian.
 *   - En plataformas little-endian, SDL mapea RGBA32 a ABGR8888; estos shifts
 *     no coincidirian con el canal esperado. Si necesitas independencia de
 *     endianess y formato, usa SDL_GetRGBA(px, surface->format, &R,&G,&B,&A)
 *     o aplica las masks/shifts de 'surface->format'.
 *
 * Parametros:
 *   px -> pixel empaquetado (32 bits).
 *   r,g,b,a -> punteros de salida para los 4 canales (no deben ser NULL).
 */
static inline void rgbaToUint8(Uint32 px, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
  // Extrae cada canal desplazando y enmascarando a 8 bits
  *r = (px >> 24) & 0xFF;
  *g = (px >> 16) & 0xFF;
  *b = (px >> 8) & 0xFF;
  *a = (px >> 0) & 0xFF;
}

/*
 * sampleIntensity
 * ---------------
 * Devuelve la luminancia (luma) Rec.709 del pixel (x, y) en el rango [0..1].
 *
 * Parametros:
 *   img -> imagen ya convertida a SDL_PIXELFORMAT_RGBA32 y con 'pixels' valido.
 *   x,y -> coordenadas de pixel en espacio de imagen (origen arriba-izquierda).
 *
 * Comportamiento:
 *   - Si 'img' es NULL, 'pixels' es NULL o (x,y) esta fuera de rango, retorna 0.0f.
 *   - Lee el pixel usando indice y * pitchPixels + x.
 *   - Desempaqueta a R,G,B (ignora A) y aplica luma Rec.709:
 *       Y = 0.2126*R + 0.7152*G + 0.0722*B   (tras normalizar a [0..1]).
 *
 * Notas:
 *   - 'pitchPixels' es el pitch en unidades de pixel (no bytes).
 *   - rgbaToUint8 asume layout 0xRRGGBBAA al interpretar el Uint32. Si te preocupa
 *     el formato real de la superficie en distintas plataformas/backend, usa
 *     SDL_GetRGBA(px, img->surface->format, &R, &G, &B, &A).
 *   - El canal alfa no se usa para la intensidad.
 */
float sampleIntensity(const Image *img, int x, int y)
{
  // Validaciones rapidas y salida segura en caso de indices invalidos
  if (!img || !img->pixels || x < 0 || y < 0 || x >= img->w || y >= img->h)
    return 0.0f;

  // Acceso lineal al pixel en formato RGBA32
  Uint32 px = img->pixels[y * img->pitchPixels + x];

  // Desempaquetar canales (A no se usa)
  Uint8 R, G, B, A;
  (void)A;
  rgbaToUint8(px, &R, &G, &B, &A);

  // Luma Rec.709 en [0..1]
  float lum = 0.2126f * (R / 255.0f) + 0.7152f * (G / 255.0f) + 0.0722f * (B / 255.0f);

  return lum;
}

/*
 * lumaFromPixel
 * --------------
 * Calcula la luminancia Rec.709 en [0..1] a partir de un pixel empaquetado RGBA32.
 *
 * Parametros:
 *   px -> valor de 32 bits con canales en orden 0xRRGGBBAA.
 *
 * Detalles:
 *   - Extrae R, G, B por desplazamientos y mascara.
 *   - Normaliza cada canal a [0..1] y aplica la formula de luma Rec.709:
 *       Y = 0.2126*R + 0.7152*G + 0.0722*B
 *
 * Notas:
 *   - Este helper asume layout 0xRRGGBBAA. Si la superficie tuviera otro
 *     formato o si hay dudas por plataforma, preferir SDL_GetRGBA con
 *     img->surface->format en llamadas de mas alto nivel.
 */
static inline float lumaFromPixel(Uint32 px)
{
  Uint8 R = (px >> 24) & 0xFF; // extraer canal rojo
  Uint8 G = (px >> 16) & 0xFF; // extraer canal verde
  Uint8 B = (px >> 8) & 0xFF;  // extraer canal azul

  // Luma Rec.709 en [0..1]
  return 0.2126f * (R / 255.0f) + 0.7152f * (G / 255.0f) + 0.0722f * (B / 255.0f);
}

/*
 * sampleIntensityBilinearUV
 * -------------------------
 * Muestrea la luminancia (Rec.709) de la imagen usando coordenadas
 * normalizadas (u,v) en [0..1] y filtrado bilineal.
 *
 * Parametros:
 *   img -> puntero a la imagen (formato RGBA32 en img->pixels).
 *   u   -> coordenada horizontal normalizada en [0..1].
 *   v   -> coordenada vertical   normalizada en [0..1].
 *
 * Retorna:
 *   Luminancia en [0..1] obtenida por interpolacion bilineal de los 4
 *   vecinos mas cercanos. Si la imagen no es valida, retorna 0.0f.
 *
 * Detalles:
 *   - Se clamp(e)a u y v a [0,1] para evitar accesos fuera de rango.
 *   - Se mapea (u,v) a coordenadas de pixel flotantes (x,y) en [0..w-1]x[0..h-1].
 *   - Se toman los cuatro vecinos p00, p10, p01, p11 y se combinan con pesos
 *     (tx,ty) para producir el valor interpolado.
 *   - En los bordes se evita salir de rango forzando x1==x0 o y1==y0 cuando
 *     corresponde.
 */
float sampleIntensityBilinearUV(const Image *img, float u, float v)
{
  if (!img || !img->pixels || img->w <= 0 || img->h <= 0)
    return 0.0f;

  // Clamp de coordenadas normalizadas a [0,1]
  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  // Convertir UV -> coordenadas de pixel flotantes
  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  // Indices de pixel inferior-izquierdo y sus vecinos
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  int x1 = (x0 + 1 < img->w) ? x0 + 1 : x0; // borde derecho: repetir x0
  int y1 = (y0 + 1 < img->h) ? y0 + 1 : y0; // borde inferior: repetir y0

  // Pesos de interpolacion en X e Y
  float tx = x - (float)x0;
  float ty = y - (float)y0;

  // Cargar los 4 pixeles vecinos
  Uint32 p00 = img->pixels[y0 * img->pitchPixels + x0];
  Uint32 p10 = img->pixels[y0 * img->pitchPixels + x1];
  Uint32 p01 = img->pixels[y1 * img->pitchPixels + x0];
  Uint32 p11 = img->pixels[y1 * img->pitchPixels + x1];

  // Convertir cada pixel a luminancia Rec.709
  float l00 = lumaFromPixel(p00);
  float l10 = lumaFromPixel(p10);
  float l01 = lumaFromPixel(p01);
  float l11 = lumaFromPixel(p11);

  // Interpolacion bilineal: primero en X (dos filas), luego en Y
  float l0 = l00 * (1.f - tx) + l10 * tx; // fila superior
  float l1 = l01 * (1.f - tx) + l11 * tx; // fila inferior
  return l0 * (1.f - ty) + l1 * ty;       // mezclar filas en Y
}
