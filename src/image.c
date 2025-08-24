#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>
#include "image.h"

/**
 * srgbToLinear01
 * ---------------
 * Convierte un componente en espacio sRGB (normalizado a [0,1]) a su
 * equivalente lineal siguiendo la curva oficial IEC 61966-2-1.
 *
 * Params:
 *   c -> componente sRGB en [0,1].
 *
 * Return:
 *   Componente lineal en [0,1].
 *
 * Notas:
 *   - Implementa la función de transferencia piecewise:
 *       c <= 0.04045 -> c / 12.92
 *       c  > 0.04045 -> ((c + 0.055) / 1.055)^2.4
 *   - Útil para calcular luminancia física (Rec.709) a partir de sRGB.
 */
static inline float srgbToLinear01(float c)
{
  // c en [0,1] sRGB -> lineal (IEC 61966-2-1)
  if (c <= 0.04045f)
    return c / 12.92f;
  return powf((c + 0.055f) / 1.055f, 2.4f);
}

/**
 * unpackRGBA
 * ----------
 * Extrae los canales R, G, B y A desde un píxel empaquetado (Uint32)
 * usando el formato real de la surface asociada a la imagen.
 *
 * Params:
 *   img -> imagen que contiene la SDL_PixelFormat a usar.
 *   px  -> valor de 32 bits con el píxel empaquetado.
 *   r,g,b,a -> punteros de salida (no deben ser NULL).
 *
 * Notas:
 *   - Usa SDL_GetRGBA con img->surface->format, por lo que es robusto a
 *     endianness y variaciones de layout interno (RGBA8888, ABGR8888, etc.).
 *   - Este helper evita asumir desplazamientos/máscaras fijos.
 */
static inline void unpackRGBA(const Image *img, Uint32 px,
                              Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
  SDL_GetRGBA(px, img->surface->format, r, g, b, a);
}

/**
 * imageLoad
 * ----------
 * Carga una imagen desde disco vía SDL_image y la convierte a un formato
 * uniforme RGBA8888 (SDL_PIXELFORMAT_RGBA32) para facilitar el muestreo.
 *
 * Params:
 *   img  -> salida; puntero válido a Image (no inicializado).
 *   path -> ruta al archivo (formatos soportados por SDL2_image: PNG/JPG, etc.).
 *
 * Return:
 *   true  si carga y conversión fueron exitosas.
 *   false si ocurre un error (se imprime diagnóstico en stderr).
 *
 * Efectos:
 *   - img->surface apunta a una SDL_Surface propiedad de Image (liberar con imageFree).
 *   - img->pixels alias de surface->pixels (no liberar por separado).
 *   - img->pitchPixels = surface->pitch / 4 (pitch expresado en píxeles).
 *
 * Garantías/errores:
 *   - En error no quedan recursos colgados (se libera la surface intermedia).
 *   - Requiere IMG_Init(...) previo en el proceso.
 */
bool imageLoad(Image *img, const char *path)
{
  // Validación básica
  if (!img || !path)
    return false;

  // Dejar salida en estado neutro
  img->w = img->h = img->pitchPixels = 0;
  img->pixels = NULL;
  img->surface = NULL;

  // 1) Cargar archivo en su formato nativo
  SDL_Surface *loaded = IMG_Load(path);
  if (!loaded)
  {
    fprintf(stderr, "IMG_Load(%s): %s\n", path, IMG_GetError());
    return false;
  }

  // 2) Convertir a RGBA32 homogéneo
  SDL_Surface *conv = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(loaded); // descartar surface original
  if (!conv)
  {
    fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
    return false;
  }

  // 3) Completar estructura de salida
  img->surface = conv;
  img->w = conv->w;
  img->h = conv->h;
  img->pitchPixels = conv->pitch / 4;   // bytes/row -> píxeles/row
  img->pixels = (Uint32 *)conv->pixels; // acceso directo RGBA8888

  return true;
}

/**
 * imageFree
 * ----------
 * Libera la SDL_Surface asociada a la imagen y deja la estructura en estado neutro.
 *
 * Params:
 *   img -> puntero a Image; se permite NULL.
 *
 * Comportamiento:
 *   - Si img es NULL, no hace nada.
 *   - Si img->surface existe, la libera con SDL_FreeSurface.
 *   - Pone punteros en NULL y escalares en 0 (estado seguro).
 *
 * Notas:
 *   - No libera la propia estructura Image (solo su contenido).
 *   - img->pixels aliasa surface->pixels; tras liberar la surface queda inválido,
 *     por eso se setea a NULL.
 *   - Idempotente: es seguro llamarla múltiples veces.
 */
void imageFree(Image *img)
{
  if (!img) // nada que hacer
    return;

  if (img->surface) // liberar surface si existe
    SDL_FreeSurface(img->surface);

  // dejar en estado neutro
  img->surface = NULL;
  img->pixels = NULL;
  img->w = 0;
  img->h = 0;
  img->pitchPixels = 0;
}

/**
 * rgbaToUint8
 * -----------
 * Desempaqueta un píxel de 32 bits en componentes R, G, B, A de 8 bits
 * mediante corrimientos y máscaras.
 *
 * Advertencia de formato/endianness:
 *   - Esta rutina asume el layout 0xRRGGBBAA al interpretar el Uint32.
 *   - En sistemas little-endian, SDL puede mapear SDL_PIXELFORMAT_RGBA32 a
 *     ABGR8888; en ese caso estos shifts no corresponden a los canales reales.
 *   - Si necesitas portabilidad respecto del formato de la surface, prefiere
 *     SDL_GetRGBA(px, surface->format, &R, &G, &B, &A).
 *
 * Params:
 *   px        -> píxel empaquetado (32 bits).
 *   r, g, b, a -> punteros de salida a 8 bits (no deben ser NULL).
 */
static inline void rgbaToUint8(Uint32 px, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
  *r = (px >> 24) & 0xFF;
  *g = (px >> 16) & 0xFF;
  *b = (px >> 8) & 0xFF;
  *a = (px >> 0) & 0xFF;
}

/**
 * sampleIntensity
 * ---------------
 * Devuelve la luminancia (luma) Rec.709 del píxel (x, y) en el rango [0..1].
 *
 * Descripción:
 *   - Accede al píxel en (x, y) dentro del buffer RGBA32 de `img` usando
 *     `pitchPixels` (pitch en unidades de píxel).
 *   - Extrae R, G, B con `SDL_GetRGBA` (a través de `unpackRGBA`) para ser
 *     independiente del layout real de la surface.
 *   - Calcula la luminancia Rec.709 en espacio sRGB normalizado:
 *       Y = 0.2126 * (R/255) + 0.7152 * (G/255) + 0.0722 * (B/255)
 *     (Sin conversión a lineal; para muestreo bilineal usa `sampleIntensityBilinearUV`).
 *
 * Parámetros:
 *   img -> imagen convertida a SDL_PIXELFORMAT_RGBA32 con campos válidos.
 *   x,y -> coordenadas enteras del píxel (origen en la esquina superior izquierda).
 *
 * Comportamiento:
 *   - Si `img` es NULL, `img->pixels` es NULL o (x,y) está fuera de rango, retorna 0.0f.
 *   - Ignora el canal alfa.
 *
 * Notas:
 *   - `pitchPixels` es el pitch en píxeles (no en bytes): `surface->pitch / 4`.
 *   - Esta variante es muestreo puntual (nearest). Para evitar aliasing al
 *     mapear tamaños distintos de canvas/imagen, usar la versión bilineal.
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

/**
 * lumaFromPixel
 * --------------
 * Calcula la luminancia Rec.709 en [0..1] a partir de un píxel empaquetado,
 * haciendo **linealización sRGB -> lineal** antes de aplicar los coeficientes.
 *
 * Descripción:
 *   - Extrae R, G, B con `unpackRGBA` (internamente `SDL_GetRGBA`) para ser
 *     independiente del layout/endian real de la surface.
 *   - Convierte cada canal de sRGB a lineal con `srgbToLinear01`.
 *   - Aplica la luma Rec.709 en dominio lineal:
 *       Y = 0.2126 * R + 0.7152 * G + 0.0722 * B
 *   - Devolver la luma en lineal es útil cuando luego se realiza interpolación
 *     (p. ej., en `sampleIntensityBilinearUV`), evitando errores por gamma.
 *
 * Parámetros:
 *   img -> imagen fuente; se usa su `surface->format` vía `SDL_GetRGBA`.
 *   px  -> valor de 32 bits del píxel (formato real resuelto por SDL).
 *
 * Retorno:
 *   Luminancia Rec.709 en [0..1] calculada en dominio lineal.
 *
 * Notas:
 *   - Se ignora el canal alfa.
 *   - Si deseas un muestreo puntual rápido en sRGB (sin linealizar), usa
 *     `sampleIntensity`; para interpolación fiel, usa esta ruta lineal.
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
 * Muestrea la **luminancia Rec.709** en coordenadas normalizadas `u,v ∈ [0,1]`
 * usando **interpolación bilineal**. La luminancia de cada vecino se calcula
 * en **dominio lineal** (sRGB → lineal con `srgbToLinear01`) vía `lumaFromPixel`,
 * y recién luego se interpola (evita artefactos por interpolar en gamma).
 *
 * Mapeo y bordes:
 *   - Se clamp(e)a u y v a [0,1].
 *   - Se mapea (u,v) → (x,y) flotantes sobre el grid de la imagen:
 *       x = u * (w-1),  y = v * (h-1)
 *     de modo que u=0/1 y v=0/1 correspondan al primer/último píxel.
 *   - Se toman los 4 vecinos (x0,y0), (x1,y0), (x0,y1), (x1,y1) y se evita
 *     salir de rango forzando x1==x0 o y1==y0 en los bordes.
 *
 * Parámetros:
 *   img -> imagen válida con formato RGBA32 (convertida por imageLoad).
 *   u   -> abscisa normalizada en [0,1] (se hace clamp interno).
 *   v   -> ordenada normalizada en [0,1] (se hace clamp interno).
 *
 * Retorno:
 *   Luminancia lineal Rec.709 en [0,1] de la posición (u,v) tras bilineal.
 *   Devuelve 0.0f si `img` no es válida.
 *
 * Complejidad:
 *   O(1) por muestra (4 accesos + mezcla bilineal).
 *
 * Notas:
 *   - Este muestreo es apropiado para mapear un canvas W×H a una imagen w×h
 *     sin aliasing duro (se usa en Lloyd para ponderar por oscuridad).
 *   - Si necesitas color, usa `sampleRgbBilinearUV` (interpolación por canal).
 */
float sampleIntensityBilinearUV(const Image *img, float u, float v)
{
  if (!img || !img->pixels || img->w <= 0 || img->h <= 0)
    return 0.0f;

  // Clamp a [0,1]
  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  // Coordenadas flotantes dentro del grid de píxeles
  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  // Vecinos bilineales
  int x0 = (int)floorf(x), y0 = (int)floorf(y);
  int x1 = (x0 + 1 < img->w) ? x0 + 1 : x0;
  int y1 = (y0 + 1 < img->h) ? y0 + 1 : y0;

  float tx = x - (float)x0, ty = y - (float)y0;

  Uint32 p00 = img->pixels[y0 * img->pitchPixels + x0];
  Uint32 p10 = img->pixels[y0 * img->pitchPixels + x1];
  Uint32 p01 = img->pixels[y1 * img->pitchPixels + x0];
  Uint32 p11 = img->pixels[y1 * img->pitchPixels + x1];

  // Luma lineal de los 4 vecinos (usar lumaFromPixel garantiza sRGB->lineal)
  float y00 = lumaFromPixel(img, p00);
  float y10 = lumaFromPixel(img, p10);
  float y01 = lumaFromPixel(img, p01);
  float y11 = lumaFromPixel(img, p11);

  // Bilineal: primero en X, luego en Y
  float yx0 = y00 + tx * (y10 - y00);
  float yx1 = y01 + tx * (y11 - y01);
  return yx0 + ty * (yx1 - yx0);
}

/**
 * sampleRgbBilinearUV
 * -------------------
 * Muestrea **color sRGB** (canales R,G,B en [0..255]) mediante **interpolación
 * bilineal** en coordenadas normalizadas `u,v ∈ [0,1]`.
 *
 * Comportamiento:
 *   - Hace clamp de `u` y `v` a [0,1].
 *   - Mapea (u,v) → (x,y) flotantes en el grid de la imagen:
 *       x = u * (w-1), y = v * (h-1),
 *     y toma los 4 vecinos (x0,y0), (x1,y0), (x0,y1), (x1,y1).
 *   - Interpola **linealmente en espacio sRGB** por canal (R,G,B) y redondea.
 *
 * Parámetros:
 *   img -> imagen válida en formato RGBA32 (preconvertida por imageLoad).
 *   u,v -> coordenadas normalizadas (se clamp-ean internamente).
 *   r,g,b -> punteros de salida (no NULL). Se escriben en [0..255].
 *
 * Retorno:
 *   - No retorna valor; escribe r,g,b. Si hay parámetros inválidos,
 *     coloca r=g=b=0.
 *
 * Notas:
 *   - La interpolación es en sRGB por simplicidad (rápida y suficiente para
 *     “pintar” puntos). Para máxima fidelidad, podría hacerse en lineal con
 *     conversión sRGB↔lineal por canal (coste adicional).
 *   - El canal alfa se ignora.
 *   - O(1) por muestra (4 accesos + mezcla bilineal).
 */
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

  // Clamp a [0,1]
  if (u < 0.f)
    u = 0.f;
  else if (u > 1.f)
    u = 1.f;
  if (v < 0.f)
    v = 0.f;
  else if (v > 1.f)
    v = 1.f;

  // Coordenadas flotantes dentro del grid de píxeles
  float x = u * (img->w - 1);
  float y = v * (img->h - 1);

  // Vecinos bilineales
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

  // Interpolación bilineal por canal (en espacio sRGB)
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
