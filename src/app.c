#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "app.h"
#include "config.h"
#include "image.h"
#include "stippling.h"
#include "lloyd.h"
#include "utils.h"

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
  bool colorPoints;      // alterna color por imagen
  bool invertTheme;      // fondo oscuro+puntos claros <-> fondo claro+puntos negros
  float minRadius;       // radio minimo por punto
  float maxRadius;       // radio maximo por punto

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

  int maxIters;      // si >0, salir cuando iters >= maxIters (modo batch)
  char *metricsPath; // si no NULL, log de métricas por iteración (CSV)

  // --- Sweep de gamma (modo batch/testing)
  bool sweepGamma;
  float gStart, gEnd, gStep;
  int gEvery; // incrementar cada N iteraciones
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
           "%s — FPS: %.1f | it=%d step=%d gamma=%.2f | r=%d | minR=%.1f maxR=%.1f%s%s",
           defaultTitle, fps, app->iters, app->pixelStride, app->gammaW,
           app->dotRadius, app->minRadius, app->maxRadius,
           app->colorPoints ? " [COLOR]" : "",
           app->invertTheme ? " [INVERT]" : "");

  // Aplicar el nuevo título a la ventana
  SDL_SetWindowTitle(app->win, title);
}

static void sweepGammaTick(App *app)
{
  if (!app->sweepGamma)
    return;
  if (app->gEvery < 1)
    app->gEvery = 1;

  if (app->iters > 0 && (app->iters % app->gEvery) == 0)
  {
    float next = app->gammaW + app->gStep;
    if (app->gStep > 0.f)
    {
      app->gammaW = (next > app->gEnd) ? app->gEnd : next;
    }
    else if (app->gStep < 0.f)
    {
      app->gammaW = (next < app->gEnd) ? app->gEnd : next;
    }
    else
    {
      // gStep == 0 -> no cambiar nada
    }
    updateFpsTitle(app);
  }
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

  // Defaults de ejecución/visualización
  app->autoRun = false;
  app->iters = 0;
  app->pixelStride = defaultLloydStep;
  app->gammaW = defaultGamma;
  app->seed = 42u;

  app->colorPoints = false; // arranque en monocromo
  app->invertTheme = false; // fondo oscuro por defecto (ya lo usas)
  app->minRadius = 0.8f;    // ajustable en runtime
  app->maxRadius = 3.0f;

  app->showBg = true;          // mostrar imagen de fondo
  app->dotRadius = 2;          // radio de los puntos (solo visual)
  app->wantScreenshot = false; // sin captura pendiente

  // Modo batch + logging (variables de entorno)
  app->maxIters = 0;
  app->metricsPath = NULL;

  const char *env_auto = getenv("STIPPLE_AUTORUN");   // "1" para auto
  const char *env_maxi = getenv("STIPPLE_MAX_ITERS"); // p.ej. "300"
  const char *env_csv = getenv("STIPPLE_METRICS");    // ruta CSV

  // Barrido de gamma (opcional)
  app->sweepGamma = false;
  app->gStart = app->gEnd = 0.f;
  app->gStep = 0.f;
  app->gEvery = 1;

  const char *env_gstart = getenv("STIPPLE_GAMMA_START"); // opcional
  const char *env_gend = getenv("STIPPLE_GAMMA_END");     // requerido para activar
  const char *env_gstep = getenv("STIPPLE_GAMMA_STEP");   // requerido para activar
  const char *env_gevery = getenv("STIPPLE_GAMMA_EVERY"); // opcional, default=1

  if (env_gend && env_gstep)
  {
    app->sweepGamma = true;
    app->gEnd = (float)atof(env_gend);
    app->gStep = (float)atof(env_gstep);
    app->gEvery = (env_gevery && atoi(env_gevery) > 0) ? atoi(env_gevery) : 1;

    if (env_gstart)
    {
      app->gStart = (float)atof(env_gstart);
      app->gammaW = app->gStart; // arrancar desde START
    }
    else
    {
      app->gStart = app->gammaW; // si no hay START, parte del gamma actual
    }

    // Si además quieres auto-run sin tocar el CLI:
    if (!app->autoRun)
      app->autoRun = true;
  }

  if (env_auto && *env_auto == '1')
    app->autoRun = true;
  if (env_maxi)
  {
    int v = atoi(env_maxi);
    if (v > 0)
      app->maxIters = v;
  }
  if (env_csv && *env_csv)
  {
    app->metricsPath = strdup(env_csv); // se libera en appShutdown
  }

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

  // Ayuda rapida en consola
  printf("[SPACE] paso Lloyd | [A] auto | [-]/[+] step | [G]/[H] gamma | [B] fondo | "
         "[Z]/[X] radio | [R] reseed | [P] screenshot | "
         "[C] color ON/OFF | [I] tema | [N]/[M] minR -/+ | [,]/[.] maxR -/+\n");
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
        // SDL_Scancode sc = e.key.keysym.scancode;
        // Uint16      mods = e.key.keysym.mod;

        // Salir
        if (sym == SDLK_ESCAPE)
        {
          running = false;
        }

        // Paso manual de Lloyd (con timing + CSV opcional)
        if (sym == SDLK_SPACE)
        {
          uint64_t t0 = util_now_ns();
          bool ok = lloydStep(&app->image, &app->stip, app->w, app->h,
                              app->pixelStride, app->gammaW);
          uint64_t t1 = util_now_ns();

          if (ok)
          {
            app->iters++;

            // Log a CSV si está habilitado
            if (app->metricsPath)
            {
              double ms = util_ns_to_ms(t1 - t0);
              util_csv_append(app->metricsPath,
                              "iter,ms,step,gamma,npoints",
                              "%d,%.3f,%d,%.3f,%d\n",
                              app->iters, ms, app->pixelStride, app->gammaW, app->stip.count);
            }

            // Salir si alcanzó tope de iteraciones en modo batch
            if (app->maxIters > 0 && app->iters >= app->maxIters)
              running = false;

            sweepGammaTick(app);
          }
        }

        // Auto ON/OFF
        if (sym == SDLK_a)
        {
          app->autoRun = !app->autoRun;
          sweepGammaTick(app);
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
          int n = app->stip.count; // conservar N actual
          stipplingFree(&app->stip);
          stipplingInit(&app->stip, n, app->w, app->h, ++app->seed);
          app->iters = 0;
          updateFpsTitle(app);
        }

        // Color ON/OFF (C)
        if (sym == SDLK_c)
        {
          app->colorPoints = !app->colorPoints;
          updateFpsTitle(app);
        }

        // Invertir tema (I)
        if (sym == SDLK_i)
        {
          app->invertTheme = !app->invertTheme;
          updateFpsTitle(app);
        }

        // Min radius (N/M)
        if (sym == SDLK_n)
        {
          app->minRadius -= 0.1f;
          if (app->minRadius < 0.5f)
            app->minRadius = 0.5f;
          if (app->minRadius > app->maxRadius)
            app->minRadius = app->maxRadius;
          updateFpsTitle(app);
        }
        if (sym == SDLK_m)
        {
          app->minRadius += 0.1f;
          if (app->minRadius > app->maxRadius)
            app->minRadius = app->maxRadius;
          updateFpsTitle(app);
        }

        // Max radius (, .)
        if (sym == SDLK_COMMA)
        { // ','
          app->maxRadius -= 0.1f;
          if (app->maxRadius < app->minRadius)
            app->maxRadius = app->minRadius;
          updateFpsTitle(app);
        }
        if (sym == SDLK_PERIOD)
        { // '.'
          app->maxRadius += 0.1f;
          if (app->maxRadius > 20.f)
            app->maxRadius = 20.f;
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

    // Modo automatico: avanza Lloyd cada frame (con timing + CSV opcional)
    if (app->autoRun)
    {
      uint64_t t0 = util_now_ns();
      bool ok = lloydStep(&app->image, &app->stip, app->w, app->h,
                          app->pixelStride, app->gammaW);
      uint64_t t1 = util_now_ns();

      if (ok)
      {
        app->iters++;

        if (app->metricsPath)
        {
          double ms = util_ns_to_ms(t1 - t0);
          util_csv_append(app->metricsPath,
                          "iter,ms,step,gamma,npoints",
                          "%d,%.3f,%d,%.3f,%d\n",
                          app->iters, ms, app->pixelStride, app->gammaW, app->stip.count);
        }

        sweepGammaTick(app);

        if (app->maxIters > 0 && app->iters >= app->maxIters)
          running = false;
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
    else
    {
      // fondo sólido según tema
      if (app->invertTheme)
        SDL_SetRenderDrawColor(app->ren, 245, 245, 245, 255);
      else
        SDL_SetRenderDrawColor(app->ren, 12, 16, 28, 255);
      SDL_RenderClear(app->ren);
    }

    // Nube de puntos (stippling)
    stipplingRenderStyled(&app->stip, app->ren, app->w, app->h,
                          &app->image,
                          app->minRadius, app->maxRadius,
                          app->colorPoints, app->invertTheme);

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

  // 3) Liberar métricas
  if (app->metricsPath)
    free(app->metricsPath);

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
