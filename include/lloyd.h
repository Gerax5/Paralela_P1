#pragma once
#include <stdbool.h>
#include "image.h"
#include "stippling.h"

/*
 * lloyd.h
 * -------
 * API para ejecutar una iteracion del algoritmo de Lloyd (centroidal Voronoi)
 * sobre un conjunto de puntos (stippling) en un lienzo 2D.
 *
 * Idea general:
 *   - Para cada muestra del lienzo se busca el punto mas cercano.
 *   - Se acumula su centroide ponderado por la "oscuridad" de la imagen.
 *   - Cada punto se recoloca en el centro de masa de sus asignaciones.
 *
 * Notas de calidad y rendimiento:
 *   - El muestreo se hace sobre un lienzo de w x h en pasos de `pixelStride`.
 *     Un stride grande recorre menos pixeles (mas rapido) pero con mas error.
 *   - El peso de cada muestra es (1 - luminancia)^gamma en [0..1].
 *     gamma = 1.0  -> lineal
 *     gamma > 1.0  -> enfatiza zonas oscuras (mas puntos se mueven hacia sombras)
 *     gamma < 1.0  -> distribucion mas plana
 *   - La implementacion secuencial actual modifica `s` in-place.
 *   - No es thread-safe por si misma; si se paraleliza, coordinar afuera.
 */

/*
 * lloydStep
 * ---------
 * Ejecuta una iteracion de Lloyd sobre el conjunto de puntos `s`, usando la
 * imagen `img` como campo de pesos, y un lienzo virtual de dimensiones w x h.
 *
 * Params:
 *   img          -> imagen fuente (RGBA32); se usa su luminancia Rec.709.
 *   s            -> conjunto de puntos (modificado in-place).
 *   w, h         -> dimensiones del lienzo donde viven los puntos.
 *   pixelStride  -> salta cada k pixeles al muestrear (k >= 1).
 *   gamma        -> exponente del peso de oscuridad (ver notas arriba).
 *
 * Precondiciones:
 *   - img != NULL y contiene pixeles validos.
 *   - s != NULL, s->pts != NULL y s->count > 0.
 *   - w > 0, h > 0, pixelStride >= 1.
 *
 * Postcondiciones:
 *   - s->pts[i] pueden cambiar de posicion hacia su centro de masa ponderado.
 *
 * Return:
 *   true  si se realizo la iteracion correctamente.
 *   false en caso de parametros invalidos o error de memoria.
 */
bool lloydStep(const Image *img,
               Stippling *s,
               int w, int h,
               int pixelStride,
               float gamma);
