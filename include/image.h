#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

/*
 * image.h
 * -------
 * Estructuras y funciones utilitarias para trabajar con imagenes en memoria.
 *
 * Convenciones:
 *   - Todas las imagenes se almacenan como SDL_Surface en formato SDL_PIXELFORMAT_RGBA32.
 *   - El puntero `pixels` es un alias directo de `surface->pixels` casteado a Uint32*.
 *   - La intensidad devuelta por las funciones sample* es luminancia Rec.709 normalizada [0..1].
 *
 * Propiedad y ciclo de vida:
 *   - imageLoad inicializa un Image y toma propiedad del SDL_Surface creado.
 *   - imageFree libera el SDL_Surface y deja el Image en estado nulo.
 *
 * Seguridad:
 *   - Las funciones de muestreo validan limites y toleran punteros nulos devolviendo 0.0f.
 *   - No hay sincronizacion interna; si accedes desde varios hilos, coordina externamente.
 */

typedef struct
{
  /* Dimensiones de la imagen en pixeles. */
  int w, h;

  /* Pitch expresado en pixeles (no en bytes). Equivale a surface->pitch / 4 en RGBA32. */
  int pitchPixels;

  /* Buffer de pixeles en formato RGBA8888 (SDL_PIXELFORMAT_RGBA32). */
  Uint32 *pixels;

  /* Superficie SDL propietaria del buffer. Se libera en imageFree. */
  SDL_Surface *surface;
} Image;

/*
 * Carga una imagen desde disco con SDL2_image y la convierte a RGBA32.
 *
 * Params:
 *   img   -> salida; debe ser un puntero valido a Image (no inicializado).
 *   path  -> ruta al archivo (PNG/JPG soportados por SDL2_image).
 *
 * Return:
 *   true  si la carga y conversion fueron exitosas.
 *   false en error (tambien imprime diagnostico por stderr).
 *
 * Post:
 *   En exito, `img->surface` y `img->pixels` quedan inicializados.
 *   En error, `img` queda en estado nulo.
 */
bool imageLoad(Image *img, const char *path);

/*
 * Libera los recursos asociados a `img` y lo deja en estado nulo.
 * Es seguro llamar con `img == NULL`.
 */
void imageFree(Image *img);

/*
 * sampleIntensity
 * ---------------
 * Muestreo por vecino mas cercano en coordenadas de pixel.
 *
 * Params:
 *   img -> imagen fuente.
 *   x,y -> coordenadas enteras en espacio de pixel.
 *
 * Return:
 *   Luminancia Rec.709 normalizada en [0..1].
 *   Devuelve 0.0f si (img == NULL) o fuera de rango.
 */
float sampleIntensity(const Image *img, int x, int y);

/*
 * sampleIntensityBilinearUV
 * -------------------------
 * Muestreo bilineal en coordenadas normalizadas.
 *
 * Params:
 *   img -> imagen fuente.
 *   u,v -> coordenadas en [0,1]; se hace clamp interno al rango valido.
 *
 * Return:
 *   Luminancia Rec.709 normalizada en [0..1], interpolada bilinealmente.
 *   Devuelve 0.0f si (img == NULL) o si la imagen no esta inicializada.
 */
float sampleIntensityBilinearUV(const Image *img, float u, float v);
