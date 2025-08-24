#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "render_sdl.h"
#include "app.h"
#include "config.h"
#include "image.h"
#include "stippling.h"
#include "lloyd.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/*
 * Estructura principal de la aplicación.
 * Mantiene handlers de SDL, estado de tiempo/FPS y recursos para imagen y stippling.
 * Nota: la vida útil de todos los miembros está acotada a appInit/appShutdown.
 */
struct App
{
  // --- Ventana y renderer (creados en appInit, destruidos en appShutdown)
  SDL_Window *win;   // ventana principal de SDL
  SDL_Renderer *ren; // renderer acelerado (vsync si está disponible)

  // --- Tamaño actual del canvas en píxeles
  int w; // ancho de la ventana
  int h; // alto de la ventana

  // --- Reloj y métricas de rendimiento
  Uint64 freq;          // frecuencia del contador de alto rendimiento (SDL_GetPerformanceFrequency)
  Uint64 last;          // último valor leído del contador (para calcular dt)
  double accTime;       // tiempo acumulado desde appInit (segundos)
  int frames;           // total de frames renderizados desde appInit
  double lastFpsUpdate; // última vez que se actualizó el título con FPS (segundos)

  // --- Recursos de imagen
  Image image;           // imagen cargada (surface RGBA8888 + acceso a píxeles)
  SDL_Texture *imageTex; // textura creada a partir de image.surface (puede ser NULL si no hay imagen)
  Stippling stip;        // nube de puntos que se renderiza y actualiza con Lloyd

  // --- Parámetros y estado de ejecución de Lloyd
  bool autoRun;    // si es true, ejecuta un paso de Lloyd en cada frame
  int iters;       // contador de iteraciones de Lloyd realizadas
  int pixelStride; // muestreo del canvas en píxeles (>=1; mayor -> más rápido, menos preciso)
  float gammaW;    // gamma del peso (1 - luminancia)^gamma
  unsigned seed;   // semilla para re-inicializar la nube de puntos (tecla R)

  // --- Opciones visuales y utilidades
  bool showBg;         // si es true, dibuja la imagen de fondo
  int dotRadius;       // radio visual de los puntos (solo afecta el render)
  bool wantScreenshot; // marcador para guardar captura al final del frame actual
};

/*
 * updateFpsTitle
 * --------------
 * Actualiza el título de la ventana con métricas de ejecución.
 * Muestra:
 *   - FPS: promedio desde el arranque (frames / accTime)
 *   - it:  iteraciones de Lloyd realizadas
 *   - step: pixelStride usado para el muestreo
 *   - gamma: gamma del peso (1 - luminancia)^gamma
 *   - r:  radio visual de los puntos
 * Además añade las banderas "[AUTO]" cuando autoRun está activo
 * y "[BG OFF]" cuando el fondo (imagen) está oculto.
 *
 * Notas:
 *   - Este helper no limita su frecuencia de uso; el throttling se hace en appRun.
 *   - snprintf se usa para evitar desbordes del buffer de título.
 */
static void updateFpsTitle(App *app)
{
  // FPS promedio desde el inicio; si accTime es 0, evitar división
  double fps = (app->accTime > 0.0) ? (app->frames / app->accTime) : 0.0;

  // Buffer temporal para formatear el título
  char title[160];

  // Construir el string con las métricas y banderas visibles
  snprintf(title, sizeof(title),
           "%s — FPS: %.1f | it=%d step=%d gamma=%.2f | r=%d%s%s",
           defaultTitle, fps, app->iters, app->pixelStride, app->gammaW,
           app->dotRadius,
           app->autoRun ? " [AUTO]" : "",
           app->showBg ? "" : " [BG OFF]");

  // Aplicar el nuevo título a la ventana
  SDL_SetWindowTitle(app->win, title);
}

/*
 * saveScreenshot
 * --------------
 * Captura el contenido del renderer actual y lo guarda como PNG.
 *
 * Uso y flujo:
 *   1) Asegura que exista el directorio "images/output" (lo crea si falta).
 *   2) Crea un SDL_Surface RGBA32 del tamaño de la ventana.
 *   3) Copia los píxeles del render target con SDL_RenderReadPixels.
 *   4) Genera un nombre de archivo basado en el contador de iteraciones.
 *   5) Escribe un PNG con IMG_SavePNG.
 *
 * Notas:
 *   - Debe llamarse al final del frame, despues de dibujar todo y antes
 *     (o justo en el mismo tick) de SDL_RenderPresent, para capturar el frame actual.
 *   - IMG_SavePNG retorna 0 en exito. Cualquier valor distinto indica error.
 *   - RenderReadPixels puede ser costoso segun el driver, pero es correcto
 *     para capturas puntuales.
 */
static bool saveScreenshot(App *app)
{
  // Asegurar que exista "images/output"
  struct stat st;
  if (stat("images/output", &st) != 0)
  {
    if (mkdir("images/output", 0755) != 0 && errno != EEXIST)
    {
      perror("mkdir images/output");
      return false;
    }
  }

  // Crear un surface RGBA32 donde volcar los pixeles del renderer
  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
      0, app->w, app->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surf)
  {
    fprintf(stderr, "CreateRGBSurface: %s\n", SDL_GetError());
    return false;
  }

  // Leer los pixeles del render target actual al surface
  if (SDL_RenderReadPixels(app->ren, NULL, SDL_PIXELFORMAT_RGBA32,
                           surf->pixels, surf->pitch) != 0)
  {
    fprintf(stderr, "RenderReadPixels: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  // Construir el path de salida usando el contador de iteraciones
  char path[256];
  snprintf(path, sizeof(path), "images/output/stipple_%05d.png", app->iters);

  // Guardar PNG (0 = OK segun SDL_image)
  if (IMG_SavePNG(surf, path) != 0)
  {
    fprintf(stderr, "IMG_SavePNG(%s): %s\n", path, IMG_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  // Log de exito y liberacion de recursos
  printf("Saved %s\n", path);
  SDL_FreeSurface(surf);
  return true;
}

/*
 * appInit
 * -------
 * Inicializa todos los subsistemas necesarios para ejecutar la demo:
 *   - SDL (video + timer)
 *   - SDL_image (PNG y JPG)
 *   - Ventana y renderer acelerado
 *   - Carga de imagen base y creación de textura
 *   - Estado de stippling (nube de puntos)
 *   - Variables de tiempo/FPS y ajustes visuales
 *
 * Parámetros:
 *   outApp    -> salida. Recibe la instancia inicializada (propiedad del caller; liberar con appShutdown).
 *   width     -> ancho deseado de ventana. Si es <= 0 se usa defaultWidth.
 *   height    -> alto deseado de ventana.  Si es <= 0 se usa defaultHeight.
 *   title     -> titulo de la ventana. Si es NULL se usa defaultTitle.
 *   imagePath -> ruta de la imagen a cargar. Si es NULL se usa defaultImagePath.
 *   npoints   -> cantidad de puntos iniciales. Si es <= 0 se usa defaultNPoints.
 *
 * Retorna:
 *   true  si todo se inicializa correctamente.
 *   false en caso de error. En ese caso, no se deja memoria/recursos colgados.
 *
 * Notas:
 *   - SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"2") pide el mejor filtrado al escalar la textura.
 *   - En cada error se imprime un mensaje a stderr y se limpian recursos antes de regresar false.
 *   - El mensaje de ayuda de teclas se imprime una sola vez al finalizar la init.
 */
bool appInit(App **outApp, int width, int height, const char *title,
             const char *imagePath, int npoints)
{
  // Inicializar SDL (video + timer)
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  // Pedir buen filtrado al escalar texturas (si el backend lo soporta)
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2"); // "1" bilinear, "2" mejor disponible

  // Reservar la estructura principal de la app
  App *app = (App *)calloc(1, sizeof(App));
  if (!app)
  {
    SDL_Quit();
    return false;
  }

  // Dimensiones de ventana (fallback a defaults)
  app->w = (width > 0) ? width : defaultWidth;
  app->h = (height > 0) ? height : defaultHeight;

  // Crear ventana y renderer acelerado con vsync
  app->win = SDL_CreateWindow(title ? title : defaultTitle,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              app->w, app->h, 0);
  app->ren = SDL_CreateRenderer(app->win, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  // Validar creacion de ventana/renderer
  if (!app->win || !app->ren)
  {
    fprintf(stderr, "SDL_Create: %s\n", SDL_GetError());
    if (app->ren)
      SDL_DestroyRenderer(app->ren);
    if (app->win)
      SDL_DestroyWindow(app->win);
    free(app);
    SDL_Quit();
    return false;
  }

  // Inicializar contadores de tiempo/FPS
  app->freq = SDL_GetPerformanceFrequency();
  app->last = SDL_GetPerformanceCounter();
  app->accTime = 0.0;
  app->frames = 0;
  app->lastFpsUpdate = 0.0;

  // Inicializar SDL_image con soporte PNG y JPG
  int flags = IMG_INIT_PNG | IMG_INIT_JPG;
  if ((IMG_Init(flags) & flags) != flags)
  {
    fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
    SDL_DestroyRenderer(app->ren);
    SDL_DestroyWindow(app->win);
    free(app);
    SDL_Quit();
    return false;
  }

  // Cargar imagen base y crear textura (opcional si la carga falla)
  const char *path = (imagePath && *imagePath) ? imagePath : defaultImagePath;
  if (!imageLoad(&app->image, path))
  {
    fprintf(stderr, "No se pudo cargar %s, continuo sin imagen.\n", path);
    app->imageTex = NULL; // seguimos, el fondo es opcional
  }
  else
  {
    app->imageTex = SDL_CreateTextureFromSurface(app->ren, app->image.surface);
    if (!app->imageTex)
      fprintf(stderr, "SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
  }

  // Estado de la nube de puntos (stippling)
  int n0 = (npoints > 0) ? npoints : defaultNPoints;
  if (!stipplingInit(&app->stip, n0, app->w, app->h, 42u))
  {
    fprintf(stderr, "stipplingInit fallo\n");
    // No abortamos: la app podria seguir mostrando fondo, pero lo normal es salir.
    // Si prefieres abortar estrictamente, descomenta el bloque de abajo.
    // SDL_DestroyTexture(app->imageTex);
    // imageFree(&app->image);
    // SDL_DestroyRenderer(app->ren);
    // SDL_DestroyWindow(app->win);
    // IMG_Quit();
    // SDL_Quit();
    // free(app);
    // return false;
  }

  // Parametros de ejecucion/visualizacion por defecto
  app->autoRun = false;
  app->iters = 0;
  app->pixelStride = defaultLloydStep;
  app->gammaW = defaultGamma;
  app->seed = 42u;

  app->showBg = true;          // mostrar imagen de fondo
  app->dotRadius = 2;          // radio de los puntos (solo visual)
  app->wantScreenshot = false; // sin captura pendiente

  // Ayuda rapida en consola
  printf("[SPACE] paso Lloyd | [A] auto | [-]/[+] step | [G]/[H] gamma | [B] fondo | [Z]/[X] radio | [R] reseed | [P] screenshot\n");
  fflush(stdout);

  // Entregar la instancia al caller
  *outApp = app;
  return true;
}

/*
 * appRun
 * -------
 * Bucle principal de la aplicacion. Ciclo por frame con tres etapas:
 *   1) Entrada: procesa eventos de ventana y teclado.
 *   2) Simulacion: aplica paso de Lloyd (manual o automatico) y actualiza metricas.
 *   3) Render: dibuja fondo opcional e imprime la nube de puntos en pantalla.
 *
 * Atajos de teclado (keydown):
 *   ESC        -> salir
 *   SPACE      -> una iteracion de Lloyd
 *   A          -> alterna ejecucion automatica (autoRun)
 *   - / +      -> pixelStride (mayor = mas rapido, menor precision)
 *   G / H      -> gamma (peso de zonas oscuras)
 *   B          -> alterna fondo (imagen)
 *   Z / X      -> radio visual de los puntos (solo afecta el dibujo)
 *   R          -> resembrar la nube de puntos (mismo N, nueva semilla)
 *   P          -> programar captura PNG del frame actual (se dispara al final del draw)
 *
 * Detalles:
 *   - El titulo de la ventana se refresca cada ~0.25 s para evitar sobrecarga.
 *   - La captura se hace despues del render para incluir exactamente lo que se ve.
 *   - pixelStride controla la granularidad del muestreo; gamma > 1 enfatiza zonas oscuras.
 */
void appRun(App *app)
{
  if (!app)
    return;

  bool running = true;

  while (running)
  {
    // 1) ENTRADA / EVENTOS
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_QUIT)
      {
        running = false;
        continue;
      }

      if (e.type == SDL_KEYDOWN)
      {
        SDL_Keycode sym = e.key.keysym.sym;
        // SDL_Scancode sc = e.key.keysym.scancode; // reservado por si mapeamos layout
        // Uint16      mods = e.key.keysym.mod;      // o combinaciones con Shift/Ctrl
        // printf("KEYDOWN sym=%s (%d)\n", SDL_GetKeyName(sym), sym); // debug opcional

        // Salir
        if (sym == SDLK_ESCAPE)
        {
          running = false;
        }

        // Paso manual de Lloyd
        if (sym == SDLK_SPACE)
        {
          if (lloydStep(&app->image, &app->stip, app->w, app->h,
                        app->pixelStride, app->gammaW))
          {
            app->iters++;
          }
        }

        // Auto ON/OFF
        if (sym == SDLK_a)
        {
          app->autoRun = !app->autoRun;
          updateFpsTitle(app); // feedback inmediato en el titulo
        }

        // Granularidad de muestreo: step-- / step++
        if (sym == SDLK_MINUS || sym == SDLK_KP_MINUS)
        {
          if (app->pixelStride > 1)
            app->pixelStride--;
          updateFpsTitle(app);
        }
        if (sym == SDLK_PLUS || sym == SDLK_KP_PLUS || sym == SDLK_EQUALS)
        {
          if (app->pixelStride < 64)
            app->pixelStride++;
          updateFpsTitle(app);
        }

        // Gamma (peso de oscuridad)
        if (sym == SDLK_g)
        {
          app->gammaW *= 1.10f;
          updateFpsTitle(app);
        }
        if (sym == SDLK_h)
        {
          app->gammaW /= 1.10f;
          updateFpsTitle(app);
        }

        // Fondo ON/OFF (imagen de referencia)
        if (sym == SDLK_b)
        {
          app->showBg = !app->showBg;
          updateFpsTitle(app);
        }

        // Radio visual de puntos (solo render, no afecta simulacion)
        if (sym == SDLK_z)
        {
          if (app->dotRadius > 1)
            app->dotRadius--;
          updateFpsTitle(app);
        }
        if (sym == SDLK_x)
        {
          if (app->dotRadius < 20)
            app->dotRadius++;
          updateFpsTitle(app);
        }

        // Captura: marcar para el final del frame actual
        if (sym == SDLK_p)
        {
          app->wantScreenshot = true;
        }

        // Re-seed: reinicia nube de puntos con nueva semilla (misma N)
        if (sym == SDLK_r)
        {
          stipplingFree(&app->stip);
          stipplingInit(&app->stip, defaultNPoints, app->w, app->h, ++app->seed);
          app->iters = 0;
          updateFpsTitle(app);
        }
      }
    }

    // 2) SIMULACION / TIEMPO
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - app->last) / (double)app->freq; // segundos reales
    app->last = now;
    app->accTime += dt;
    app->frames++;

    // Modo automatico: avanza Lloyd cada frame
    if (app->autoRun)
    {
      if (lloydStep(&app->image, &app->stip, app->w, app->h,
                    app->pixelStride, app->gammaW))
      {
        app->iters++;
      }
    }

    // Refrescar titulo de ventana cada ~0.25 s
    if (app->accTime - app->lastFpsUpdate >= 0.25)
    {
      updateFpsTitle(app);
      app->lastFpsUpdate = app->accTime;
    }

    // 3) RENDER
    SDL_SetRenderDrawColor(app->ren, 12, 16, 28, 255); // fondo solido cuando no hay imagen
    SDL_RenderClear(app->ren);

    // Fondo (imagen), si esta habilitado
    if (app->showBg && app->imageTex)
    {
      SDL_Rect dst = {0, 0, app->w, app->h};
      SDL_RenderCopy(app->ren, app->imageTex, NULL, &dst);
    }

    // Nube de puntos (stippling)
    stipplingRender(&app->stip, app->ren, app->dotRadius);

    // Captura al final del frame para incluir todo lo dibujado
    if (app->wantScreenshot)
    {
      (void)saveScreenshot(app); // si falla, ya loguea el motivo
      app->wantScreenshot = false;
    }

    SDL_RenderPresent(app->ren);
  }
}

/*
 * appShutdown
 * -----------
 * Libera todos los recursos creados por la aplicacion y cierra los
 * subsistemas de SDL/SDL_image. Debe llamarse exactamente una vez
 * al final del programa, cuando ya no se vaya a renderizar ni a
 * usar recursos de imagen.
 *
 * Orden recomendado de liberacion:
 *   1) Recursos propios (memoria del estado de puntos).
 *   2) Recursos graficos dependientes del renderer/ventana
 *      (texturas, superficies de imagen, renderer, ventana).
 *   3) Subsistemas globales (SDL_image y SDL).
 *
 * Notas:
 *   - Todas las llamadas son seguras si los punteros son NULL.
 *   - La funcion no invalida punteros externos; solo libera y
 *     destruye lo que vive dentro de 'app' y luego libera 'app'.
 */
void appShutdown(App *app)
{
  if (!app)
    return;

  // 1) Recursos propios
  //    Memoria de la nube de puntos (arreglo de Dot).
  stipplingFree(&app->stip);

  // 2) Graficos
  //    Destruye la textura creada a partir de la imagen cargada.
  if (app->imageTex)
    SDL_DestroyTexture(app->imageTex);

  //    Libera la superficie y metadatos de la imagen (pixels, pitch, etc.).
  imageFree(&app->image);

  //    Destruye el renderer antes que la ventana para respetar dependencias.
  if (app->ren)
    SDL_DestroyRenderer(app->ren);

  //    Destruye la ventana SDL.
  if (app->win)
    SDL_DestroyWindow(app->win);

  // 3) Subsistemas
  //    Cierra SDL_image y SDL. Debe ocurrir al final.
  IMG_Quit();
  SDL_Quit();

  // Libera la estructura principal de la aplicacion.
  free(app);
}
