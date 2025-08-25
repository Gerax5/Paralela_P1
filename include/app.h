#pragma once
#include <stdbool.h>

/*
 * Modulo: app
 * -----------
 * Orquesta la vida de la aplicacion:
 *   - Inicializa SDL, SDL_image, ventana y renderer.
 *   - Carga la imagen de referencia (opcional).
 *   - Crea y mantiene el estado de stippling.
 *   - Ejecuta el bucle principal (eventos, simulacion, render).
 *   - Libera todos los recursos al finalizar.
 *
 * Notas:
 *   - Todas las funciones de este modulo deben invocarse desde el hilo principal.
 *   - El tipo App es opaco fuera de app.c.
 *   - El peso de Lloyd se define en config.h con STIPPLE_WEIGHT_BY_BRIGHTNESS
 *     (1 -> w = luminancia^gamma, 0 -> w = (1 - luminancia)^gamma).
 *
 * Controles de teclado:
 *   SPACE -> una iteracion de Lloyd
 *   A     -> auto ON/OFF
 *   -/+   -> pixelStride (granularidad de muestreo)
 *   G/H   -> gamma up/down
 *   B     -> mostrar/ocultar fondo (imagen)
 *   Z/X   -> radio visual de puntos -/+
 *   R     -> resembrar puntos
 *   P     -> guardar captura PNG
 *   C     -> puntos con color de la imagen ON/OFF
 *   I     -> invertir tema (fondo claro/oscuro)
 *   N/M   -> minRadius -/+
 *   ,/.   -> maxRadius -/+
 *   ESC   -> salir
 */

/* Adelanto de tipo opaco. La definicion completa vive en app.c. */
typedef struct App App;

/**
 * appInit
 * -------
 * Crea e inicializa la aplicacion.
 *
 * Params:
 *   outApp    -> salida; puntero a la instancia creada si retorna true.
 *   width     -> ancho de ventana en pixeles (si <= 0 usa default).
 *   height    -> alto  de ventana en pixeles (si <= 0 usa default).
 *   title     -> titulo de la ventana (NULL para usar default).
 *   imagePath -> ruta a imagen PNG/JPG (NULL para usar default).
 *   npoints   -> cantidad inicial de puntos (si <= 0 usa default).
 *
 * Return:
 *   true en exito, false en error (no quedan recursos activos).
 *
 * Efectos:
 *   - Llama SDL_Init e IMG_Init.
 *   - Crea ventana y renderer.
 *   - Carga imagen y crea textura (si hay).
 *   - Inicializa la nube de puntos.
 */
bool appInit(App **outApp, int width, int height, const char *title,
             const char *imagePath, int npoints);

/**
 * appRun
 * ------
 * Ejecuta el bucle principal: procesa eventos, avanza la simulacion y renderiza.
 * Bloquea hasta que el usuario cierre la ventana o presione ESC.
 */
void appRun(App *app);

/**
 * appShutdown
 * -----------
 * Libera todos los recursos asociados a la aplicacion.
 *
 * Params:
 *   app -> instancia a destruir (se permite NULL).
 *
 * Efectos:
 *   - Destruye textura, renderer y ventana.
 *   - Libera la nube de puntos y la imagen.
 *   - Cierra SDL_image y SDL.
 */
void appShutdown(App *app);

/**
 * appSetBackgroundList
 * --------------------
 * Define la lista de imagenes de fondo que usara la aplicacion (y carga la primera).
 *
 * Params:
 *   app    -> instancia valida de App.
 *   count  -> cantidad de rutas en 'paths' (debe ser > 0).
 *   paths  -> arreglo de 'count' punteros a cadenas C terminadas en '\0'.
 *             Las cadenas se copian internamente; el caller conserva su propiedad.
 *             Firma: const char* const* para permitir pasar arreglos de literales.
 *
 * Return:
 *   true si se cargo correctamente la primera imagen; false en error.
 *
 * Notas:
 *   - Reemplaza cualquier lista previa y libera su memoria.
 *   - Tras el set, la imagen en indice 0 queda activa.
 *   - Esta lista se recorre circularmente con appNextBackground/appPrevBackground
 *     y por la rotacion automatica (si esta activada).
 */
bool appSetBackgroundList(App* app, int count, const char* const* paths);

/**
 * appNextBackground
 * -----------------
 * Avanza al siguiente fondo de la lista (recorrido circular) y lo carga.
 * No hace nada si no hay lista cargada.
 */
void appNextBackground(App* app);

/**
 * appPrevBackground
 * -----------------
 * Retrocede al fondo anterior de la lista (recorrido circular) y lo carga.
 * No hace nada si no hay lista cargada.
 */
void appPrevBackground(App* app);