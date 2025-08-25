#include "app.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * print_usage
 * -----------
 * Imprime el mensaje de ayuda en stderr con las opciones de línea de comandos
 * y los atajos de teclado disponibles durante la ejecución.
 *
 * Parametros:
 *   prog -> nombre del ejecutable (argv[0]).
 *
 * Notas:
 *   - Muestra los valores por defecto definidos en config.h para N y la imagen.
 *   - Mantener este texto sincronizado con los controles reales en app.c.
 */
static void print_usage(const char *prog)
{
  fprintf(stderr,
          "Uso: %s [-n PUNTOS] [imagen.png]\n"
          "  -n PUNTOS   Numero de puntos iniciales (default=%d)\n"
          "  imagen.png  Ruta opcional a la imagen (default=%s)\n"
          "\nControles:\n"
          "  SPACE  una iteracion de Lloyd\n"
          "  A      auto ON/OFF\n"
          "  -/+    step (pixelStride)\n"
          "  G/H    gamma up/down\n"
          "  B      fondo ON/OFF\n"
          "  Z/X    radio de punto -/+\n"
          "  R      re-seed puntos\n"
          "  P      screenshot PNG\n"
          "  ESC    salir\n",
          prog, defaultNPoints, defaultImagePath);
}

/*
 * main
 * ----
 * Punto de entrada del programa. Parsea argumentos, inicializa la App
 * (ventana, renderer, imagen y nube de puntos), ejecuta el loop principal
 * y realiza la limpieza de recursos al salir.
 *
 * Flujo:
 *   1) Parseo de CLI: -n PUNTOS y una ruta de imagen opcional.
 *   2) appInit(...) con los valores resueltos (o defaults).
 *   3) appRun(...) para el bucle de eventos, simulacion y render.
 *   4) appShutdown(...) para liberar recursos y cerrar SDL/SDL_image.
 *
 * Retorno:
 *   0 en salida normal o al mostrar ayuda (-h/--help).
 *   1 si falla la inicializacion (appInit retorna false) o hay errores de CLI.
 */
int main(int argc, char **argv)
{
  int npoints = defaultNPoints; // numero de puntos iniciales (fallback a config.h)
  const char *imgPath = NULL;   // ruta a imagen (si no se especifica, usa defaultImagePath)

  // Parseo simple de argumentos: -h/--help, -n PUNTOS y una imagen opcional
  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      print_usage(argv[0]);
      return 0; // mostrar ayuda y salir sin error
    }
    else if (strcmp(argv[i], "-n") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "Error: -n requiere un valor.\n");
        print_usage(argv[0]);
        return 1;
      }
      npoints = atoi(argv[++i]); // consumir el valor de -n
      if (npoints <= 0)
      {
        fprintf(stderr, "Error: PUNTOS debe ser > 0.\n");
        return 1;
      }
    }
    else
    {
      // Primer argumento no-opcional se interpreta como ruta de imagen
      imgPath = argv[i];
    }
  }

  // Mensaje de arranque con los parametros efectivos
  fprintf(stdout, "Inicializando con N=%d  Imagen=%s\n",
          npoints, imgPath);

  // Inicializacion de la aplicacion (SDL, imagen, puntos, etc.)
  App *app = NULL;
  if (!appInit(&app, defaultWidth, defaultHeight, defaultTitle, imgPath, npoints))
  {
    return 1; // fallo de inicializacion
  }

  // Bucle principal: eventos, simulacion (Lloyd) y render
  appRun(app);

  // Liberacion ordenada de recursos
  appShutdown(app);
  return 0;
}
