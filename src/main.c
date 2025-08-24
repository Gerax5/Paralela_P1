#include "app.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog)
{
  fprintf(stderr,
          "Uso: %s [-n PUNTOS] [imagen.png]\n"
          "  -n PUNTOS   Número de puntos iniciales (default=%d)\n"
          "  imagen.png  Ruta opcional a la imagen (default=%s)\n"
          "\nControles:\n"
          "  SPACE  una iteración de Lloyd\n"
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

int main(int argc, char **argv)
{
  int npoints = defaultNPoints;
  const char *imgPath = NULL;

  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      print_usage(argv[0]);
      return 0;
    }
    else if (strcmp(argv[i], "-n") == 0)
    {
      if (i + 1 >= argc)
      {
        fprintf(stderr, "Error: -n requiere un valor.\n");
        print_usage(argv[0]);
        return 1;
      }
      npoints = atoi(argv[++i]);
      if (npoints <= 0)
      {
        fprintf(stderr, "Error: PUNTOS debe ser > 0.\n");
        return 1;
      }
    }
    else
    {
      // Primer argumento no-opcional: imagen
      imgPath = argv[i];
    }
  }

  // Mensaje de arranque
  fprintf(stdout, "Inicializando con N=%d  Imagen=%s\n",
          npoints, imgPath ? imgPath : defaultImagePath);

  App *app = NULL;
  if (!appInit(&app, defaultWidth, defaultHeight, defaultTitle, imgPath, npoints))
  {
    return 1;
  }
  appRun(app);
  appShutdown(app);
  return 0;
}
