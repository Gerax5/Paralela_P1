#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "image.h"

/*
 * stippling.h
 * -----------
 * Estructuras y utilidades para manejar la nube de puntos del efecto
 * de stippling. Este modulo solo gestiona:
 *   - Almacenamiento de puntos (coordenadas en float).
 *   - Inicializacion aleatoria reproducible.
 *   - Renderizado simple de los puntos con SDL.
 *
 * Politicas y convenciones:
 *   - Sistema de coordenadas: origen en la esquina superior izquierda.
 *     x en [0, width), y en [0, height).
 *   - Las posiciones son floats porque Lloyd y las medias ponderadas
 *     no tienen por que caer en coordenadas enteras.
 *   - Este modulo no hace asignacion Voronoi ni Lloyd. Eso vive en lloyd.c.
 *   - Thread-safety: no es thread-safe; el acceso concurrente debe
 *     sincronizarse externamente.
 */

/* Punto individual del stippling. */
typedef struct
{
  float x, y; /* posicion en pixeles, espacio de la ventana */
} Dot;

/* Conjunto de puntos y metadatos de su canvas asociado. */
typedef struct
{
  Dot *pts;   /* arreglo de N puntos; propiedad del modulo */
  int count;  /* N actual de puntos en `pts` */
  int width;  /* ancho del canvas usado para normalizar/limitar */
  int height; /* alto  del canvas usado para normalizar/limitar */
} Stippling;

/*
 * stipplingInit
 * -------------
 * Reserva y rellena `s->pts` con `n` puntos distribuidos de forma
 * aleatoria uniforme dentro del rectangulo [0,width) x [0,height).
 *
 * Params:
 *   s    -> salida; debe ser un puntero valido.
 *   n    -> numero de puntos a crear (n > 0).
 *   w,h  -> dimensiones del canvas en pixeles (w > 0, h > 0).
 *   seed -> semilla de RNG para reproducibilidad. Si es 0 se usara
 *           un valor por defecto interno.
 *
 * Return:
 *   true  si asigno memoria y creo los puntos.
 *   false ante parametros invalidos o falta de memoria.
 *
 * Notas:
 *   - Si `s->pts` ya tenia memoria, el llamador debe llamar antes a
 *     `stipplingFree(s)` para evitar fugas.
 */
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);

/*
 * stipplingFree
 * -------------
 * Libera la memoria interna asociada a `s` y pone el estado en cero.
 * Es segura ante `s == NULL`.
 */
void stipplingFree(Stippling *s);

/*
 * stipplingRender
 * ---------------
 * Dibuja la nube de puntos como discos rellenos de radio `radius`
 * en el renderer SDL dado. Este es un efecto visual; no modifica
 * la simulacion.
 *
 * Params:
 *   s       -> conjunto de puntos a dibujar.
 *   ren     -> renderer de destino (valido).
 *   radius  -> radio del disco en pixeles. Si radius < 1 se usa 1.
 *
 * Efectos:
 *   - Cambia el color de dibujo actual del renderer.
 *   - Emite primitivas 2D (lineas) para rellenar cada disco.
 *
 * Requisitos:
 *   - Llamar desde el hilo principal. SDL no es thread-safe.
 *   - `s` y `s->pts` deben ser validos.
 */
void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius);

/**
 * stipplingRenderStyled
 * ---------------------
 * Dibuja puntos con radio por-punto en [minR,maxR] segun brillo de imagen
 * y con color opcional muestreado de la imagen.
 */
void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH,
                           const Image *img, float minR, float maxR,
                           bool useColor, bool invertTheme);
