#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "image.h"

/*
 * stippling.h
 * -----------
 * Manejo de la nube de puntos:
 *   - Almacenamiento (float x,y).
 *   - Inicializacion aleatoria reproducible.
 *   - Render basico y render “estilizado” (radio por brillo y color de imagen).
 *
 * Convenciones:
 *   - Sistema de coords: origen arriba-izquierda; x in [0,w), y in [0,h).
 *   - Este modulo no hace Lloyd ni Voronoi (vive en lloyd.c).
 *   - No thread-safe; usar solo desde el hilo principal (SDL).
 */

typedef struct
{
  float x, y; /* posicion en pixeles, espacio ventana */
} Dot;

typedef struct
{
  Dot *pts;   /* arreglo de N puntos; propiedad del modulo */
  int count;  /* N actual de puntos */
  int width;  /* ancho del canvas asociado */
  int height; /* alto  del canvas asociado */
} Stippling;

/**
 * stipplingInit
 * -------------
 * Reserva y genera `n` puntos uniformes en [0,w) x [0,h).
 *
 * Params:
 *   s    -> salida (no inicializado).
 *   n    -> numero de puntos (>0).
 *   w,h  -> dimensiones (>0).
 *   seed -> semilla reproducible (0 usa default interno).
 *
 * Return: true en exito; false si parametros invalidos o OOM.
 *
 * Nota: si `s->pts` ya tenia memoria, llamar antes a stipplingFree(s).
 */
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);

/**
 * stipplingFree
 * -------------
 * Libera `s->pts` y pone el estado en cero. Seguro con s==NULL.
 */
void stipplingFree(Stippling *s);

/**
 * stipplingRenderStyled
 * ---------------------
 * Dibuja con radio por-punto segun brillo y color opcional de la imagen.
 *
 * Params:
 *   s         -> nube a dibujar.
 *   ren       -> renderer SDL destino.
 *   canvasW,H -> dimensiones del canvas donde viven los puntos.
 *   img       -> imagen para muestreo (UV bilineal). Puede ser NULL (queda monocromo).
 *   minR,maxR -> radios en px por-punto (minR <= maxR; min clamp ~0.5).
 *   useColor  -> true: color por imagen; false: monocromo segun tema.
 *   invertTheme -> true: fondo claro/puntos oscuros; false: fondo oscuro/puntos claros.
 *
 * Notas:
 *   - El radio por-punto usa: inv = 1 - luma; radius = minR + inv*(maxR-minR).
 *   - Luma: Rec.709 en [0..1] (sRGB->lineal) via image.c.
 *   - Llamar desde el hilo principal (SDL no es thread-safe).
 */
void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH,
                           const Image *img, float minR, float maxR,
                           bool useColor, bool invertTheme);
