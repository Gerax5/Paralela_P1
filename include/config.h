#pragma once

/*
 * config.h
 * --------
 * Valores por defecto para la aplicación. Estos se usan cuando el usuario no
 * provee parámetros por CLI o cuando se pasan valores <= 0.
 *
 * Notas:
 *  - Puedes sobreescribir cualquier macro en tiempo de compilación con -D.
 *      Ej: gcc ... -DdefaultWidth=1280 -DdefaultHeight=720
 *  - El path de imagen debe existir y ser legible por SDL2_image (PNG/JPG).
 *
 * Controles en tiempo de ejecución (resumen):
 *   SPACE: iteracion de Lloyd
 *   A:     auto ON/OFF
 *   -/+:   pixelStride (granularidad de muestreo)
 *   G/H:   gamma up/down
 *   B:     mostrar/ocultar fondo
 *   Z/X:   radio de puntos -/+
 *   R:     resembrar puntos
 *   P:     guardar captura PNG
 *   ESC:   salir
 */

/* Tamaño de ventana por defecto (px). Si appInit recibe <= 0, se usan estos. */
#define defaultWidth 800
#define defaultHeight 600

/* Titulo de la ventana. */
#define defaultTitle "Voronoi Stippling - bootstrap"

/* Imagen de entrada por defecto (ruta relativa al ejecutable). */
#define defaultImagePath "images/input/twitch.png"

/*
 * Parametros del algoritmo por defecto:
 *
 * defaultNPoints
 *   - Cantidad inicial de puntos para el stippling.
 *   - Rango tipico: 500..20000 segun resolucion y GPU/CPU.
 *
 * defaultLloydStep
 *   - Paso de muestreo espacial (pixelStride) para Lloyd.
 *   - 1 = muestreo denso y preciso (mas costo).
 *   - 2..6 = mas rapido, menos preciso por iteracion.
 *
 * defaultGamma
 *   - Peso de oscuridad al acumular: w = (1 - luminancia)^gamma.
 *   - 1.0 = lineal, >1 favorece zonas oscuras, <1 las atenua.
 */
#define defaultNPoints 1000
#define defaultLloydStep 3
#define defaultGamma 1.0f