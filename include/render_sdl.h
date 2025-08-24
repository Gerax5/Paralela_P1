#pragma once
#include <SDL2/SDL.h>

/*
 * render_sdl.h
 * ------------
 * Utilidades de dibujo puntuales para la demo (overlay simple de prueba).
 * No mantiene estado interno ni depende de otras partes del motor.
 *
 * Uso tipico desde el loop principal:
 *
 *   // t = segundos acumulados desde el arranque
 *   renderFrame(renderer, windowWidth, windowHeight, t);
 *
 * Notas:
 *   - Todo el dibujo ocurre sobre el render target actual de SDL.
 *   - Esta funcion es opcional; puedes comentar su llamada si no quieres
 *     el overlay de prueba.
 *   - Llamar siempre desde el hilo principal (SDL no es thread-safe).
 */

/*
 * renderFrame
 * -----------
 * Dibuja un overlay de prueba animado (figuras simples) para verificar que
 * el pipeline de render funciona y que el tiempo avanza.
 *
 * Params:
 *   ren -> renderer de destino (valido y creado con SDL_CreateRenderer).
 *   w   -> ancho del viewport en pixeles.
 *   h   -> alto  del viewport en pixeles.
 *   t   -> tiempo en segundos (monotono) usado para animacion.
 *
 * Efectos:
 *   - Cambia el color de dibujo y emite primitivas sobre `ren`.
 *   - No limpia la pantalla ni presenta; deja esas tareas al llamador.
 *
 * Garantias:
 *   - No asigna memoria ni conserva estado entre llamadas.
 */
void renderFrame(SDL_Renderer *ren, int w, int h, double t);
