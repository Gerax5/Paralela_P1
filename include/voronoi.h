#pragma once
#include <stdbool.h>
#include "stippling.h"

/*
 * voronoi.h
 * ---------
 * Acelerador de consultas de vecino mas cercano mediante una grilla uniforme.
 *
 * Idea:
 *   - Se divide el canvas en celdas cuadradas de lado `cell` pixeles.
 *   - Cada celda guarda una lista (implicita) de indices de puntos.
 *   - Para (x,y) se inspecciona su celda y anillos vecinos hasta `ringMax`.
 *
 * Convenciones:
 *   - Origen (0,0) arriba-izquierda; x in [0,w), y in [0,h).
 *   - La grilla indexa posiciones actuales; si los puntos se mueven, reconstruir.
 *
 * Complejidad esperada:
 *   - Construccion: O(N).
 *   - Consulta: O(k) promedio (k = puntos en pocas celdas vecinas).
 *   - Peor caso degenerado: O(N).
 *
 * Thread-safety:
 *   - No thread-safe. Usar una grilla por hilo o sincronizar externamente.
 */

/* Grilla uniforme para indexar puntos de `Stippling`. */
typedef struct
{
  int w, h; /* dimensiones del canvas en pixeles */
  int cell; /* lado de celda en pixeles (p.ej. 16..64) */
  int cols; /* ceil(w / cell) */
  int rows; /* ceil(h / cell) */

  /*
   * Listas por celda:
   *   head[c] -> indice del primer punto o -1 si vacia
   *   next[i] -> siguiente punto en la misma celda que contiene a i
   *
   * Tamano:
   *   head: cols*rows
   *   next: s->count
   */
  int *head;
  int *next;
} UniformGrid;

/**
 * gridBuild
 * ---------
 * Construye la grilla para los puntos `s` en un canvas w x h.
 *
 * Params:
 *   g     -> salida (no inicializada).
 *   w,h   -> dimensiones del canvas (>0).
 *   cell  -> lado de celda en px (>0).
 *   s     -> nube de puntos valida.
 *
 * Return:
 *   true si asigno y lleno estructuras; false en parametros invalidos u OOM.
 *
 * Notas:
 *   - Si `g` ya tenia memoria, llamar antes a gridFree(g).
 *   - Coordenadas fuera de rango se clamp-ean a la celda valida mas cercana.
 */
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);

/**
 * gridFree
 * --------
 * Libera la memoria interna y deja punteros en NULL. Seguro con g==NULL.
 */
void gridFree(UniformGrid *g);

/**
 * gridNearest
 * -----------
 * Devuelve el indice del punto mas cercano a (x,y) explorando celdas en
 * anillos crecientes hasta `ringMax`.
 *
 * Params:
 *   g       -> grilla valida (de gridBuild).
 *   s       -> nube de puntos asociada.
 *   x,y     -> posicion de consulta en pixeles.
 *   ringMax -> 0 solo celda actual; 1 vecinas inmediatas; 2 suele bastar.
 *
 * Return:
 *   indice en `s->pts` o -1 si no se encontro en el rango explorado.
 *
 * Notas:
 *   - Si retorna -1, el llamador puede aumentar `ringMax` o hacer fallback O(N).
 */
int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);
