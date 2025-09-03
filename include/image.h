#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

/*
 * image.h
 * -------
 * Utilidades para trabajar con imagenes en memoria.
 *
 * Convenciones:
 *   - Las imagenes se cargan y convierten a SDL_PIXELFORMAT_RGBA32.
 *   - `pixels` alias de surface->pixels (Uint32*); `pitchPixels = pitch/4`.
 *   - La intensidad devuelta por sample* es luminancia Rec.709 en [0..1],
 *     calculada sobre RGB lineal (sRGB -> lineal).
 *
 * Propiedad y ciclo de vida:
 *   - imageLoad crea y asigna el SDL_Surface; imageFree lo libera.
 *
 * Seguridad:
 *   - Las funciones validan limites y toleran punteros nulos devolviendo 0.0f.
 *   - Extraccion de canales via SDL_GetRGBA (independiente de endian/format).
 *   - No hay sincronizacion interna.
 */

typedef struct
{
  /* Dimensiones en pixeles. */
  int w, h;

  /* Pitch en unidades de pixel (surface->pitch / 4 en RGBA32). */
  int pitchPixels;

  /* Buffer RGBA8888 (SDL_PIXELFORMAT_RGBA32). */
  Uint32 *pixels;

  /* Superficie SDL propietaria del buffer. */
  SDL_Surface *surface;

  /* Luminancia lineal precomputada (size = w*h, fila mayor)*/
  float *luma;
} Image;

/**
 * imageLoad
 * ---------
 * Carga una imagen (PNG/JPG) y la convierte a RGBA32.
 *
 * Params:
 *   img  -> salida (no inicializado).
 *   path -> ruta a archivo.
 *
 * Return:
 *   true si cargo/convertio; false y log a stderr en error.
 *
 * Post:
 *   En exito, `surface` y `pixels` quedan listos para muestreo.
 */
bool imageLoad(Image *img, const char *path);

/**
 * imageFree
 * ---------
 * Libera el SDL_Surface y deja `img` en estado nulo.
 * Es segura con `img == NULL`.
 */
void imageFree(Image *img);

/**
 * sampleIntensity
 * ---------------
 * Muestreo NN en coords de pixel.
 *
 * Params:
 *   img -> imagen fuente.
 *   x,y -> coordenadas enteras (0..w-1, 0..h-1).
 *
 * Return:
 *   Luma Rec.709 en [0..1] (sRGB->lineal). 0.0f si invalido.
 */
float sampleIntensity(const Image *img, int x, int y);

/**
 * sampleIntensityBilinearUV
 * -------------------------
 * Muestreo bilineal en coords normalizadas.
 *
 * Params:
 *   img -> imagen fuente.
 *   u,v -> [0,1]; se hace clamp interno.
 *
 * Return:
 *   Luma Rec.709 en [0..1] (sRGB->lineal). 0.0f si invalido.
 */
float sampleIntensityBilinearUV(const Image *img, float u, float v);

/**
 * sampleRgbBilinearUV
 * -------------------
 * Muestrea color sRGB por bilineal en (u,v) in [0,1].
 *
 * Params:
 *   img -> imagen fuente.
 *   u,v -> [0,1]; clamp interno.
 *   r,g,b -> salida en 0..255 (alfa ignorado).
 */
void sampleRgbBilinearUV(const Image *img, float u, float v,
                         Uint8 *r, Uint8 *g, Uint8 *b);


/**
 * imageBuildLuma
 * -------------------
 * Construye o actualiza el buffer de luminancia lineal precomputada `img->luma`.
 * Params:
 *    img -> imagen fuente
 * Return:
 *    apunta a un buffer w*h listo para muestreo rapido en `sampleIntensityBilinearUV`.
 * 
*/
void imageBuildLuma(Image *img);