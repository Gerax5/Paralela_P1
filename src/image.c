#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>
#include "image.h"

static inline float srgbToLinear01(float c)
{
  // c en [0,1] sRGB -> lineal (IEC 61966-2-1)
  if (c <= 0.04045f)
    return c / 12.92f;
  return powf((c + 0.055f) / 1.055f, 2.4f);
}

/** Extrae R,G,B,A según el formato real de la surface. */
static inline void unpackRGBA(const Image *img, Uint32 px,
                              Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
  SDL_GetRGBA(px, img->surface->format, r, g, b, a);
}

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
  if (!img || !img->pixels || x < 0 || y < 0 || x >= img->w || y >= img->h)
    return 0.0f;

  Uint32 px = img->pixels[y * img->pitchPixels + x];
  Uint8 R, G, B, A;
  (void)A;
  unpackRGBA(img, px, &R, &G, &B, &A);

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
static inline float lumaFromPixel(const Image *img, Uint32 px)
{
  Uint8 R8, G8, B8, A;
  (void)A;
  unpackRGBA(img, px, &R8, &G8, &B8, &A);

  float Rs = R8 / 255.0f, Gs = G8 / 255.0f, Bs = B8 / 255.0f;
  float R = srgbToLinear01(Rs);
  float G = srgbToLinear01(Gs);
  float B = srgbToLinear01(Bs);
  return 0.2126f * R + 0.7152f * G + 0.0722f * B;
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

  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  int x0 = (int)floorf(x), y0 = (int)floorf(y);
  int x1 = (x0 + 1 < img->w) ? x0 + 1 : x0;
  int y1 = (y0 + 1 < img->h) ? y0 + 1 : y0;

  float tx = x - (float)x0, ty = y - (float)y0;

  Uint32 p00 = img->pixels[y0 * img->pitchPixels + x0];
  Uint32 p10 = img->pixels[y0 * img->pitchPixels + x1];
  Uint32 p01 = img->pixels[y1 * img->pitchPixels + x0];
  Uint32 p11 = img->pixels[y1 * img->pitchPixels + x1];

  float y00 = lumaFromPixel(img, p00);
  float y10 = lumaFromPixel(img, p10);
  float y01 = lumaFromPixel(img, p01);
  float y11 = lumaFromPixel(img, p11);

  float yx0 = y00 + tx * (y10 - y00);
  float yx1 = y01 + tx * (y11 - y01);
  return yx0 + ty * (yx1 - yx0);
}

void sampleRgbBilinearUV(const Image *img, float u, float v,
                         Uint8 *r, Uint8 *g, Uint8 *b)
{
  if (!img || !img->pixels || img->w <= 0 || img->h <= 0 || !r || !g || !b)
  {
    if (r)
      *r = 0;
    if (g)
      *g = 0;
    if (b)
      *b = 0;
    return;
  }

  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  int x0 = (int)floorf(x), y0 = (int)floorf(y);
  int x1 = (x0 + 1 < img->w) ? x0 + 1 : x0;
  int y1 = (y0 + 1 < img->h) ? y0 + 1 : y0;

  float tx = x - (float)x0, ty = y - (float)y0;

  Uint32 p00 = img->pixels[y0 * img->pitchPixels + x0];
  Uint32 p10 = img->pixels[y0 * img->pitchPixels + x1];
  Uint32 p01 = img->pixels[y1 * img->pitchPixels + x0];
  Uint32 p11 = img->pixels[y1 * img->pitchPixels + x1];

  Uint8 r00, g00, b00, a;
  Uint8 r10, g10, b10;
  Uint8 r01, g01, b01;
  Uint8 r11, g11, b11;
  unpackRGBA(img, p00, &r00, &g00, &b00, &a);
  unpackRGBA(img, p10, &r10, &g10, &b10, &a);
  unpackRGBA(img, p01, &r01, &g01, &b01, &a);
  unpackRGBA(img, p11, &r11, &g11, &b11, &a);

  float rx0 = r00 + tx * (r10 - r00);
  float gx0 = g00 + tx * (g10 - g00);
  float bx0 = b00 + tx * (b10 - b00);

  float rx1 = r01 + tx * (r11 - r01);
  float gx1 = g01 + tx * (g11 - g01);
  float bx1 = b01 + tx * (b11 - b01);

  float rf = rx0 + ty * (rx1 - rx0);
  float gf = gx0 + ty * (gx1 - gx0);
  float bf = bx0 + ty * (bx1 - bx0);

  *r = (Uint8)(rf + 0.5f);
  *g = (Uint8)(gf + 0.5f);
  *b = (Uint8)(bf + 0.5f);
}
