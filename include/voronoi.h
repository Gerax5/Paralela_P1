#pragma once
#include <stdbool.h>
#include "stippling.h"

/*
 * voronoi.h
 * ---------
 * Estructuras y utilidades para acelerar consultas de vecino mas cercano
 * usando una grilla uniforme (espacial) sobre el canvas 2D.
 *
 * Idea:
 *   - Se divide el canvas en celdas cuadradas de lado `cell` pixeles.
 *   - Cada celda mantiene una lista enlazada (implicita) de indices de puntos.
 *   - Para consultar el punto mas cercano a (x,y), se exploran primero
 *     los puntos en la celda de (x,y), luego el anillo de celdas vecinas
 *     a distancia 1, luego el anillo 2, etc., hasta `ringMax`.
 *
 * Convenciones:
 *   - Sistema de coordenadas: origen (0,0) en la esquina superior izquierda.
 *   - Rango valido: x en [0, w), y en [0, h).
 *   - La grilla solo indexa la posicion actual de los puntos. Si los puntos
 *     se mueven significativamente, se debe reconstruir la grilla.
 *
 * Complejidad esperada:
 *   - Construccion: O(N) en numero de puntos.
 *   - Consulta: O(k) promedio si los puntos estan mas o menos uniformes,
 *     donde k es el numero de puntos en las pocas celdas inspeccionadas.
 *   - En el peor caso degenerado puede acercarse a O(N).
 *
 * Thread-safety:
 *   - No es thread-safe. Si se usa desde varios hilos, se debe proveer
 *     sincronizacion externa o estructuras separadas por hilo.
 */

/* Grilla uniforme para indexar puntos de `Stippling`. */
typedef struct
{
  int w, h; /* dimensiones del canvas en pixeles */
  int cell; /* tamano del lado de la celda en pixeles (por ejemplo 32) */
  int cols; /* numero de columnas de celdas = ceil(w / cell) */
  int rows; /* numero de filas    de celdas = ceil(h / cell) */

  /*
   * Listas enlazadas implicitas por celda:
   *   - `head[c]` guarda el indice del primer punto en la celda c, o -1 si vacia.
   *   - `next[i]` enlaza al siguiente punto en la misma celda que contiene al punto i.
   *
   * Tamano de arreglos:
   *   head -> cols * rows
   *   next -> s->count  (uno por punto)
   */
  int *head;
  int *next;
} UniformGrid;

/*
 * gridBuild
 * ---------
 * Construye la grilla uniforme para el conjunto de puntos `s` y el canvas w x h.
 *
 * Params:
 *   g     -> salida; estructura a rellenar. Se asume no inicializada.
 *   w,h   -> dimensiones del canvas en pixeles (w > 0, h > 0).
 *   cell  -> lado de la celda en pixeles (cell > 0). Valores tipicos: 16..64.
 *   s     -> nube de puntos. Debe ser valida y con `s->pts` no nulo.
 *
 * Return:
 *   true  si se asigno memoria y se construyeron las listas por celda.
 *   false ante parametros invalidos o falta de memoria.
 *
 * Notas:
 *   - Si `g` ya contenia memoria, el llamador debe llamar antes a `gridFree(g)`.
 *   - Los indices fuera de [0, w) o [0, h) se clamp-ean a la celda mas cercana valida.
 */
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);

/*
 * gridFree
 * --------
 * Libera la memoria interna de la grilla y deja punteros en NULL.
 * Es segura ante `g == NULL`.
 */
void gridFree(UniformGrid *g);

/*
 * gridNearest
 * -----------
 * Busca el indice del punto mas cercano a la posicion (x, y) usando
 * la grilla `g`. Explora celdas en anillos concenctricos desde la celda
 * que contiene (x,y) hasta `ringMax`.
 *
 * Params:
 *   g        -> grilla valida y construida con `gridBuild`.
 *   s        -> nube de puntos asociada a la grilla.
 *   x,y      -> posicion de consulta en pixeles.
 *   ringMax  -> cuantos anillos de celdas examinar como maximo.
 *               0 examina solo la celda actual; 1 incluye vecinas inmediatas;
 *               2 suele ser suficiente en muchos casos.
 *
 * Return:
 *   indice del punto mas cercano en `s->pts`, o -1 si no se encontro ninguno
 *   dentro del rango explorado (por ejemplo, si `ringMax` fue muy pequeno
 *   y la celda inicial estaba vacia).
 *
 * Notas:
 *   - Si no se encuentra en los anillos inspeccionados, el llamador puede
 *     hacer un fallback O(N) o aumentar `ringMax`.
 *   - Las coordenadas (x,y) se interpretan en el mismo espacio que `s->pts`.
 */
int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);
