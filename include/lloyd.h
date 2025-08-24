#pragma once
#include <stdbool.h>
#include "image.h"
#include "stippling.h"

/*
 * lloyd.h
 * -------
 * API para ejecutar una iteracion de Lloyd (centroidal Voronoi) sobre un conjunto
 * de puntos (stippling) en un lienzo 2D, usando la imagen como campo de pesos.
 *
 * Pesos (ver config.h):
 *   STIPPLE_WEIGHT_BY_BRIGHTNESS = 1 -> w = luminancia^gamma   (favorece zonas claras)
 *   STIPPLE_WEIGHT_BY_BRIGHTNESS = 0 -> w = (1 - luminancia)^gamma (favorece zonas oscuras)
 *
 * Notas:
 *   - La luminancia es Rec.709 en [0..1] sobre RGB lineal (sRGB -> lineal).
 *   - El muestreo se hace en UV con filtrado bilineal.
 *   - pixelStride controla la granularidad del muestreo del lienzo (>=1).
 *   - La implementacion modifica `s` in-place y no es thread-safe por si sola.
 */

/**
 * lloydStep
 * ---------
 * Ejecuta una iteracion de Lloyd sobre `s`, ponderando por la imagen `img`
 * en un lienzo virtual de w x h.
 *
 * Params:
 *   img         -> imagen fuente (RGBA32); se usa su luminancia Rec.709.
 *   s           -> conjunto de puntos (modificado in-place).
 *   w, h        -> dimensiones del lienzo.
 *   pixelStride -> salto de muestreo espacial (k >= 1).
 *   gamma       -> exponente del peso (segun macro en config.h).
 *                  (1) w = luminancia^gamma
 *                  (0) w = (1 - luminancia)^gamma
 *
 * Pre:
 *   img != NULL con pixeles validos; s != NULL y s->pts != NULL; s->count > 0;
 *   w > 0, h > 0, pixelStride >= 1.
 *
 * Post:
 *   Reubica cada punto hacia el centro de masa ponderado de su celda.
 *
 * Return:
 *   true si la iteracion se completo; false en parametros invalidos o error.
 */
bool lloydStep(const Image *img,
               Stippling *s,
               int w, int h,
               int pixelStride,
               float gamma);
