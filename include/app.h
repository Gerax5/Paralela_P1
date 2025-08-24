#pragma once
#include <stdbool.h>

/*
 * Módulo: app
 * ------------
 * Orquesta la vida de la aplicación:
 *   - Inicializa SDL, SDL_image, ventana y renderer.
 *   - Carga la imagen de referencia (opcional).
 *   - Crea y mantiene el estado de stippling.
 *   - Ejecuta el bucle principal (eventos, simulación, render).
 *   - Libera todos los recursos al finalizar.
 *
 * Notas:
 *   - Todas las funciones de este módulo deben ser invocadas desde el hilo principal.
 *   - El objeto App es opaco fuera de app.c; usa los punteros y funciones públicas.
 */

/* Adelanto de tipo opaco. La definición completa vive en app.c. */
typedef struct App App;

/*
 * appInit
 * -------
 * Crea e inicializa la aplicación.
 *
 * Parámetros:
 *   outApp     -> salida, instancia creada si la función retorna true.
 *   width      -> ancho de ventana en píxeles (si <= 0 se usa default).
 *   height     -> alto  de ventana en píxeles (si <= 0 se usa default).
 *   title      -> título de la ventana (puede ser NULL para usar default).
 *   imagePath  -> ruta a imagen PNG/JPG (puede ser NULL para usar default).
 *   npoints    -> cantidad inicial de puntos para el stippling (si <= 0 se usa default).
 *
 * Retorno:
 *   true  si toda la inicialización fue exitosa.
 *   false si falló alguna etapa; en ese caso no queda estado activo.
 *
 * Efectos:
 *   - Llama SDL_Init y IMG_Init.
 *   - Crea ventana y renderer.
 *   - Carga imagen y prepara textura (si hay).
 *   - Inicializa la nube de puntos con npoints.
 */
bool appInit(App **outApp, int width, int height, const char *title,
             const char *imagePath, int npoints);

/*
 * appRun
 * ------
 * Ejecuta el bucle principal: procesa eventos, avanza la simulación y renderiza.
 * Bloquea hasta que el usuario cierra la ventana o presiona ESC.
 *
 * Controles de teclado (resumen):
 *   SPACE -> una iteración de Lloyd
 *   A     -> auto ON/OFF
 *   -/+   -> pixelStride (granularidad de muestreo)
 *   G/H   -> gamma up/down
 *   B     -> mostrar/ocultar fondo (imagen)
 *   Z/X   -> radio de los puntos -/+
 *   R     -> resembrar puntos
 *   P     -> guardar captura PNG
 *   ESC   -> salir
 */
void appRun(App *app);

/*
 * appShutdown
 * -----------
 * Libera todos los recursos asociados a la aplicación.
 *
 * Parámetros:
 *   app -> instancia a destruir (se permite NULL, en cuyo caso no hace nada).
 *
 * Efectos:
 *   - Destruye textura, renderer y ventana.
 *   - Libera memoria de la nube de puntos y la imagen.
 *   - Cierra subsistemas de SDL_image y SDL.
 */
void appShutdown(App *app);
