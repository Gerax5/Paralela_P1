#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
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
 * App
 * ---
 * Estado global de la aplicacion. Vive entre appInit(..) y appShutdown(..).
 * Propiedad/vida util:
 *   - `win`, `ren`, `imageTex` y `image.surface` se crean en appInit y se destruyen en appShutdown.
 *   - `stip` (nube de puntos) se inicializa/libera en appInit/appShutdown o reseed (R).
 */
struct App
{
  // --- Ventana y renderer (SDL)
  SDL_Window *win;   // ventana principal
  SDL_Renderer *ren; // renderer acelerado (idealmente con vsync)

  // --- Tamano actual del canvas (px)
  int w; // ancho de ventana
  int h; // alto  de ventana

  // --- Reloj / metricas
  Uint64 freq;    // SDL_GetPerformanceFrequency()
  Uint64 last;    // ultima lectura de contador (para dt)
  double accTime; // segundos acumulados desde appInit
  int frames;     // frames renderizados

  // Cache de UI (para no recalcular cada frame)
  double fpsAvg;       // FPS promedio desde el inicio, cacheado
  double lastUiUpdate; // último timestamp en el que se refrescó título/overlay

  // --- Recursos de imagen / puntos
  Image image;           // surface RGBA8888 + acceso a pixeles
  SDL_Texture *imageTex; // textura creada desde image.surface (puede ser NULL)
  Stippling stip;        // nube de puntos (modificada por Lloyd)
  bool colorPoints;      // ON: muestrea color de la imagen por punto
  bool invertTheme;      // ON: fondo claro + puntos oscuros; OFF: fondo oscuro + puntos claros
  float minRadius;       // radio minimo por punto (px)  [clamp >= ~0.5]
  float maxRadius;       // radio maximo por punto (px)  [>= minRadius]

  // --- Lloyd (parametros/estado)
  bool autoRun;    // ON: ejecuta un paso de Lloyd por frame
  int iters;       // iteraciones de Lloyd acumuladas
  int pixelStride; // stride de muestreo (>=1)  [mayor = mas rapido/menos preciso]
  float gammaW;    // exponente del peso; segun config:
                   //   STIPPLE_WEIGHT_BY_BRIGHTNESS==1 -> w = lum^gammaW
                   //   STIPPLE_WEIGHT_BY_BRIGHTNESS==0 -> w = (1 - lum)^gammaW
  unsigned seed;   // semilla para resembrar la nube (tecla R)

  // --- Opciones visuales / utilidades
  bool showBg;         // ON: dibuja la imagen de fondo
  int dotRadius;       // radio visual fijo (solo en stipplingRender)
  bool wantScreenshot; // marca para guardar PNG al final del frame actual

  // --- Modo batch / logging
  int maxIters;      // si >0, salir cuando iters >= maxIters
  char *metricsPath; // ruta CSV para log de metricas (propiedad de App; se libera)

  // --- Sweep de gamma (testing por entorno)
  bool sweepGamma;           // ON: barrido de gamma automatico
  float gStart, gEnd, gStep; // rango e incremento de gamma
  int gEvery;                // aplicar incremento cada N iteraciones

  // --- Overlay FPS
  TTF_Font *font;      // fuente para texto
  SDL_Texture *fpsTex; // textura cacheada del texto "FPS: ... "
  int fpsTexW;
  int fpsTexH;

  // --- Lista de fondos
  char **bgPaths;
  int bgCount;
  int bgIndex;

  double bgTimer;  // segundos acumulados desde el último cambio
  double bgPeriod; // cada cuántos segundos cambiar de imagen (>0 activa)
};

/**
 * refreshFpsOverlay
 * -----------------
 * Regenera la textura de texto del overlay de FPS/estado.
 *
 * Flujo:
 *   1) Construye el string con FPS (cacheado), iters, step y gamma.
 *   2) Renderiza texto con TTF_RenderText_Blended (color según tema).
 *   3) Crea SDL_Texture desde el surface y cachea en app->fpsTex (+ ancho/alto).
 *   4) Libera la textura previa si existía.
 *
 * Params:
 *   app -> contexto con renderer, fuente y estado (no NULL).
 *
 * Notas:
 *   - Se invoca cada ~0.25s desde appRun() para evitar costo por frame.
 *   - Si no hay fuente (app->font == NULL) no hace nada.
 */
static void refreshFpsOverlay(App *app)
{
  if (!app->font)
    return;

  char buf[96];
  snprintf(buf, sizeof(buf), "FPS: %.1f | it=%d | step=%d | gamma=%.2f",
           app->fpsAvg, app->iters, app->pixelStride, app->gammaW);

  SDL_Color color = app->invertTheme
                        ? (SDL_Color){10, 10, 10, 255}
                        : (SDL_Color){240, 240, 240, 255};

  SDL_Surface *surf = TTF_RenderText_Blended(app->font, buf, color);
  if (!surf)
  {
    fprintf(stderr, "TTF_RenderText_Blended: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *tex = SDL_CreateTextureFromSurface(app->ren, surf);
  if (!tex)
  {
    fprintf(stderr, "SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return;
  }

  if (app->fpsTex)
    SDL_DestroyTexture(app->fpsTex);
  app->fpsTex = tex;
  app->fpsTexW = surf->w;
  app->fpsTexH = surf->h;

  SDL_FreeSurface(surf);
}

/**
 * drawFpsOverlay
 * --------------
 * Dibuja un recuadro semitransparente y encima el texto cacheado de FPS.
 *
 * Params:
 *   app -> contexto con renderer y app->fpsTex válido (opcional).
 *
 * Notas:
 *   - No re-renderiza el texto; solo usa la textura ya generada.
 *   - Coordenadas fijas (x=10,y=10) con padding para mejorar legibilidad.
 *   - Requiere SDL_BLENDMODE_BLEND para el rect de fondo.
 */
static void drawFpsOverlay(App *app)
{
  if (!app->fpsTex)
    return;

  const int pad = 6;
  const int x = 10;
  const int y = 10;

  SDL_SetRenderDrawBlendMode(app->ren, SDL_BLENDMODE_BLEND);
  SDL_Color bg = app->invertTheme
                     ? (SDL_Color){255, 255, 255, 160}
                     : (SDL_Color){0, 0, 0, 160};

  SDL_SetRenderDrawColor(app->ren, bg.r, bg.g, bg.b, bg.a);
  SDL_Rect box = {x - pad, y - pad, app->fpsTexW + pad * 2, app->fpsTexH + pad * 2};
  SDL_RenderFillRect(app->ren, &box);

  SDL_Rect dst = {x, y, app->fpsTexW, app->fpsTexH};
  SDL_RenderCopy(app->ren, app->fpsTex, NULL, &dst);
}

/**
 * ensureDir
 * ---------
 * Crea un directorio si no existe.
 *
 * Params:
 *   path -> ruta del directorio (p.ej. "images/output").
 */
static void ensureDir(const char *path)
{
  struct stat st;
  if (stat(path, &st) != 0)
  {
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
    {
      perror("mkdir");
    }
  }
}

/**
 * loadBackground
 * --------------
 * Carga una imagen desde disco, la convierte a SDL_Surface RGBA32 (via Image)
 * y crea una SDL_Texture asociada para usar como fondo.
 *
 * Params:
 *   app  -> contexto con renderer y estado. Se limpia image/texture previos.
 *   path -> ruta a PNG/JPG válido.
 *
 * Return:
 *   true  en éxito (app->imageTex listo para render),
 *   false en error (se loguea motivo).
 *
 * Notas:
 *   - Libera la textura anterior y el Image previo antes de cargar.
 *   - No cambia app->showBg; solo actualiza el recurso de imagen.
 */
static bool loadBackground(App *app, const char *path)
{
  if (app->imageTex)
  {
    SDL_DestroyTexture(app->imageTex);
    app->imageTex = NULL;
  }
  imageFree(&app->image);

  if (!imageLoad(&app->image, path))
  {
    fprintf(stderr, "No se pudo cargar fondo: %s\n", path);
    return false;
  }

  app->imageTex = SDL_CreateTextureFromSurface(app->ren, app->image.surface);
  if (!app->imageTex)
  {
    fprintf(stderr, "SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    return false;
  }
  return true;
}

/**
 * appSetBackgroundList
 * --------------------
 * Define/actualiza la lista de rutas de fondos rotables y carga el primero.
 *
 * Params:
 *   app   -> contexto (no NULL).
 *   count -> cantidad de rutas en 'paths' (debe ser > 0).
 *   paths -> arreglo de C-strings con rutas válidas a imágenes.
 *
 * Return:
 *   true  si pudo copiar la lista y cargar el primer fondo,
 *   false si count<=0, paths==NULL o falla memoria/carga.
 *
 * Efectos:
 *   - Libera la lista anterior (si existía).
 *   - Duplica cada string (propiedad pasa a App).
 *   - Inicializa bgIndex=0 y carga ese fondo (loadBackground).
 */
bool appSetBackgroundList(App *app, int count, const char *const *paths)
{
  if (app->bgPaths)
  {
    for (int i = 0; i < app->bgCount; ++i)
      free(app->bgPaths[i]);
    free(app->bgPaths);
    app->bgPaths = NULL;
  }
  app->bgCount = 0;
  app->bgIndex = 0;

  if (count <= 0 || !paths)
    return false;

  app->bgPaths = (char **)calloc(count, sizeof(char *));
  if (!app->bgPaths)
    return false;

  for (int i = 0; i < count; ++i)
  {
    app->bgPaths[i] = strdup(paths[i]);
  }
  app->bgCount = count;

  return loadBackground(app, app->bgPaths[app->bgIndex]);
}

/**
 * appNextBackground
 * -----------------
 * Avanza al siguiente fondo en la lista circular y lo carga.
 *
 * Params:
 *   app -> contexto con lista de fondos ya establecida.
 *
 * Notas:
 *   - No hace reseed de puntos ni cambia flags visuales.
 *   - No hace nada si bgCount <= 0.
 */
void appNextBackground(App *app)
{
  if (app->bgCount <= 0)
    return;
  app->bgIndex = (app->bgIndex + 1) % app->bgCount;
  (void)loadBackground(app, app->bgPaths[app->bgIndex]);
}

/**
 * appPrevBackground
 * -----------------
 * Retrocede al fondo anterior en la lista circular y lo carga.
 *
 * Params:
 *   app -> contexto con lista de fondos ya establecida.
 *
 * Notas:
 *   - No hace reseed de puntos ni cambia flags visuales.
 *   - No hace nada si bgCount <= 0.
 */
void appPrevBackground(App *app)
{
  if (app->bgCount <= 0)
    return;
  app->bgIndex = (app->bgIndex - 1 + app->bgCount) % app->bgCount;
  (void)loadBackground(app, app->bgPaths[app->bgIndex]);
}

/**
 * updateFpsTitle
 * --------------
 * Actualiza el titulo de la ventana con metricas de ejecucion.
 * Muestra:
 *   - FPS: promedio desde el arranque (cacheado en app->fpsAvg)
 *   - it:  iteraciones de Lloyd
 *   - step: pixelStride de muestreo
 *   - gamma: exponente del peso (segun build)
 *            STIPPLE_WEIGHT_BY_BRIGHTNESS==1 -> w = lum^gamma
 *            STIPPLE_WEIGHT_BY_BRIGHTNESS==0 -> w = (1 - lum)^gamma
 *   - r:  radio visual fijo de puntos
 *   - minR/maxR: radios por-punto usados en el render “styled”
 * Flags visuales:
 *   - [COLOR]  cuando los puntos muestrean color de la imagen
 *   - [INVERT] cuando el tema esta invertido (fondo claro / puntos oscuros)
 *
 * Notas:
 *   - El throttling se hace en appRun() (cada ~0.25s).
 */
static void updateFpsTitle(App *app)
{
  char title[160];
  snprintf(title, sizeof(title),
           "%s — FPS: %.1f | it=%d step=%d gamma=%.2f | r=%d | minR=%.1f maxR=%.1f%s%s",
           defaultTitle, app->fpsAvg, app->iters, app->pixelStride, app->gammaW,
           app->dotRadius, app->minRadius, app->maxRadius,
           app->colorPoints ? " [COLOR]" : "",
           app->invertTheme ? " [INVERT]" : "");

  SDL_SetWindowTitle(app->win, title);
}

/**
 * sweepGammaTick
 * ---------------
 * Actualiza gammaW automaticamente cuando el barrido de gamma esta activo.
 *
 * Comportamiento:
 *   - Si sweepGamma == false -> no hace nada.
 *   - Asegura gEvery >= 1.
 *   - Cada gEvery iteraciones (iters % gEvery == 0) propone next = gammaW + gStep.
 *     * Si gStep > 0.f: avanza hacia gEnd sin pasarse.
 *     * Si gStep < 0.f: retrocede hacia gEnd sin pasarse.
 *     * Si gStep == 0.f: no cambia gammaW.
 *   - Llama updateFpsTitle(app) para reflejar el nuevo gamma en el titulo.
 *
 * Notas:
 *   - Los limites gStart/gEnd/gStep se configuran en appInit (p. ej. por vars de entorno).
 *   - Este helper debe llamarse tras incrementar app->iters.
 */
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
 * Captura el contenido visual actual del renderer y lo guarda como PNG.
 *
 * Flujo:
 *   1) Asegura el directorio "images/output" (lo crea si falta).
 *   2) Crea un SDL_Surface RGBA32 del tamano de la ventana.
 *   3) Copia los pixeles del render target con SDL_RenderReadPixels.
 *   4) Compone el nombre: images/output/stipple_%05d.png (iters actual).
 *   5) Escribe el PNG via IMG_SavePNG.
 *
 * Parametros:
 *   app -> contexto con ventana/renderer y contador de iteraciones.
 *
 * Return:
 *   true  si se guardo el PNG correctamente.
 *   false si fallo la creacion del surface, la lectura del backbuffer
 *         o la escritura del archivo.
 *
 * Notas:
 *   - Llamar al FINAL del frame (tras dibujar y antes de SDL_RenderPresent).
 *   - IMG_SavePNG devuelve 0 en exito.
 *   - SDL_RenderReadPixels puede ser costoso; usar para capturas puntuales.
 */
static bool saveScreenshot(App *app)
{
  struct stat st;
  if (stat("images/output", &st) != 0)
  {
    if (mkdir("images/output", 0755) != 0 && errno != EEXIST)
    {
      perror("mkdir images/output");
      return false;
    }
  }

  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
      0, app->w, app->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surf)
  {
    fprintf(stderr, "CreateRGBSurface: %s\n", SDL_GetError());
    return false;
  }

  if (SDL_RenderReadPixels(app->ren, NULL, SDL_PIXELFORMAT_RGBA32,
                           surf->pixels, surf->pitch) != 0)
  {
    fprintf(stderr, "RenderReadPixels: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  char path[256];
  snprintf(path, sizeof(path), "images/output/stipple_%05d.png", app->iters);

  if (IMG_SavePNG(surf, path) != 0)
  {
    fprintf(stderr, "IMG_SavePNG(%s): %s\n", path, IMG_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  printf("Saved %s\n", path);
  SDL_FreeSurface(surf);
  return true;
}

/**
 * appInit
 * -------
 * Inicializa la aplicacion y sus subsistemas (SDL, SDL_image), crea ventana/
 * renderer, carga la imagen (opcional), inicializa la nube de puntos y deja
 * listo el estado para `appRun`.
 *
 * Params:
 *   outApp    -> salida; recibe puntero valido a App en exito (no NULL).
 *   width     -> ancho de ventana; si <= 0 usa defaultWidth.
 *   height    -> alto  de ventana; si <= 0 usa defaultHeight.
 *   title     -> titulo de la ventana; si NULL usa defaultTitle.
 *   imagePath -> ruta de imagen; si NULL usa defaultImagePath.
 *   npoints   -> cantidad inicial de puntos; si <= 0 usa defaultNPoints.
 *
 * Return:
 *   true  en exito; false si falla alguna etapa (no quedan recursos colgados).
 *
 * Variables de entorno (opcional):
 *   STIPPLE_AUTORUN=1           -> autoRun ON
 *   STIPPLE_MAX_ITERS=<N>       -> modo batch: salir al llegar a N iteraciones
 *   STIPPLE_METRICS=<ruta.csv>  -> log de métricas por iteración
 *   STIPPLE_GAMMA_START=<g0>    -> barrido: gamma inicial
 *   STIPPLE_GAMMA_END=<g1>      -> barrido: gamma objetivo (activa sweep)
 *   STIPPLE_GAMMA_STEP=<dg>     -> barrido: incremento por gEvery iteraciones
 *   STIPPLE_GAMMA_EVERY=<k>     -> barrido: aplicar cada k iteraciones (default=1)
 *   STIPPLE_BG_SECONDS=<s>      -> cambiar fondo automáticamente cada s segundos (>0 activa)
 */
bool appInit(App **outApp, int width, int height, const char *title,
             const char *imagePath, int npoints)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2"); // "1" bilinear, "2" mejor disponible

  App *app = (App *)calloc(1, sizeof(App));
  if (!app)
  {
    SDL_Quit();
    return false;
  }

  app->w = (width > 0) ? width : defaultWidth;
  app->h = (height > 0) ? height : defaultHeight;

  // --- Defaults de ejecución/visualización (arranque pausado y reproducible)
  app->autoRun = false; // inicia PAUSADO (no se auto-ordena)
  app->iters = 0;
  app->pixelStride = defaultLloydStep;
  app->gammaW = defaultGamma;
  app->seed = 12345; // semilla fija por defecto para pruebas

  app->colorPoints = true;  // arranca con color
  app->invertTheme = false; // fondo oscuro por defecto
  app->minRadius = 0.8f;    // radios usados en render "styled"
  app->maxRadius = 3.0f;

  app->showBg = false; // mostrar imagen de fondo (toggle con 'B')
  app->dotRadius = 2;  // (ya no se usa para Z/X; puedes eliminarlo luego)
  app->wantScreenshot = false;

  // Modo batch + logging
  app->maxIters = 0;
  app->metricsPath = NULL;

  // --- Rotación automática de fondos (DESACTIVADA por defecto)
  app->bgTimer = 0.0;
  app->bgPeriod = 0.0; // 0 => sin rotación
  const char *env_bg = getenv("STIPPLE_BG_SECONDS");
  if (env_bg)
  {
    double v = atof(env_bg);
    app->bgPeriod = (v > 0.0) ? v : 0.0; // 0 o menor => desactiva
  }

  // Flags de entorno opcionales
  const char *env_auto = getenv("STIPPLE_AUTORUN");   // "1" para auto
  const char *env_maxi = getenv("STIPPLE_MAX_ITERS"); // "300", etc.
  const char *env_csv = getenv("STIPPLE_METRICS");    // ruta CSV

  // --- Barrido de gamma (opcional, NO reactiva autorun)
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
      app->gammaW = app->gStart; // arrancar desde START si se da
    }
    else
    {
      app->gStart = app->gammaW; // si no hay START, parte del gamma actual
    }
    // IMPORTANTE: NO encender autorun aquí.
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

  app->win = SDL_CreateWindow(title ? title : defaultTitle,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              app->w, app->h, 0);
  app->ren = SDL_CreateRenderer(app->win, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

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

  // Inicializar contadores de tiempo/FPS + cache UI
  app->freq = SDL_GetPerformanceFrequency();
  app->last = SDL_GetPerformanceCounter();
  app->accTime = 0.0;
  app->frames = 0;
  app->fpsAvg = 0.0;
  app->lastUiUpdate = 0.0;

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

  ensureDir("images");
  ensureDir("images/input");
  ensureDir("images/output");

  app->bgPaths = NULL;
  app->bgCount = 0;
  app->bgIndex = 0;

  if (imagePath && *imagePath)
  {
    const char *one[] = {imagePath};
    appSetBackgroundList(app, 1, one);
  }
  else
  {
    appSetBackgroundList(app, defaultBgCount, defaultBgPaths);
  }

  // Estado de la nube de puntos (stippling)
  int n0 = (npoints > 0) ? npoints : defaultNPoints;
  if (!stipplingInit(&app->stip, n0, app->w, app->h, app->seed))
  {
    fprintf(stderr, "stipplingInit fallo\n");
    // Permitimos continuar para ver solo el fondo si hay imagen
  }

  if (TTF_Init() != 0)
  {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
  }

  app->font = TTF_OpenFont("/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf", 16);
  if (!app->font)
  {
    fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
  }

  app->fpsTex = NULL;
  app->fpsTexW = app->fpsTexH = 0;

  // Ayuda rápida en consola
  printf(
      "Controles:\n"
      "  SPACE  una iteracion de Lloyd\n"
      "  A      auto ON/OFF\n"
      "  G/H    gamma up/down\n"
      "  B      fondo ON/OFF\n"
      "  Z/X    escalar min/max radios ↓/↑\n"
      "  N/M    minRadius -/+\n"
      "  ,/.    maxRadius -/+\n"
      "  C      color ON/OFF\n"
      "  I      tema\n"
      "  R      reseed (misma N)\n"
      "  P      screenshot\n"
      "  O/U    fondo siguiente/anterior\n"
      "  ESC    salir\n");
  printf("Autorun inicial: %s | Rotacion fondo: %s\n",
         app->autoRun ? "ON" : "OFF",
         (app->bgPeriod > 0.0) ? "ON" : "OFF");
  fflush(stdout);

  *outApp = app;
  return true;
}

/**
 * appRun
 * ------
 * Bucle principal: procesa eventos, avanza la simulacion (Lloyd) y renderiza.
 *
 * Fases por frame:
 *   1) Entrada: eventos de ventana/teclado.
 *   2) Simulacion: paso de Lloyd manual (SPACE) o automatico (autoRun).
 *      - Mide tiempo por iteracion y, si corresponde, escribe CSV de metricas.
 *      - Aplica sweep de gamma segun configuracion.
 *   3) Render: fondo (imagen u oscuro/claro) + nube de puntos estilizada.
 *      - Captura PNG al final del frame si fue solicitada.
 *
 * Controles:
 *   ESC       -> salir
 *   SPACE     -> una iteracion de Lloyd
 *   A         -> auto ON/OFF
 *   - / +     -> pixelStride -/+
 *   G / H     -> gamma up/down
 *   B         -> mostrar/ocultar fondo
 *   Z / X     -> radio visual de puntos -/+
 *   R         -> resembrar puntos (misma N, nueva semilla)
 *   P         -> screenshot PNG (deferida al final del frame)
 *   C         -> color de puntos ON/OFF (muestreo desde la imagen)
 *   I         -> invertir tema (fondo claro/oscuro y base de puntos)
 *   N / M     -> minRadius -/+
 *   , / .     -> maxRadius -/+
 *
 * Detalles:
 *   - Se actualiza título y overlay cada ~0.25 s, no en cada frame.
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

        if (sym == SDLK_ESCAPE)
          running = false;

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

            if (app->metricsPath)
            {
              double ms = util_ns_to_ms(t1 - t0);
              util_csv_append(app->metricsPath,
                              "iter,ms,step,gamma,npoints",
                              "%d,%.3f,%d,%.3f,%d\n",
                              app->iters, ms, app->pixelStride, app->gammaW, app->stip.count);
            }

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
          updateFpsTitle(app); // feedback inmediato
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

        // Radios (Z/X ahora escalan min/max en bloque)
        if (sym == SDLK_z)
        {
          app->minRadius = fmaxf(0.5f, app->minRadius - 0.1f);
          app->maxRadius = fmaxf(app->minRadius, app->maxRadius - 0.1f);
          updateFpsTitle(app);
        }
        if (sym == SDLK_x)
        {
          app->maxRadius = fminf(20.f, app->maxRadius + 0.1f);
          app->minRadius = fminf(app->maxRadius, app->minRadius + 0.1f);
          updateFpsTitle(app);
        }

        // Siguiente fondo: 'o' (con reseed y reset iter)
        if (sym == SDLK_o)
        {
          int n = app->stip.count; // conservar N actual
          stipplingFree(&app->stip);
          stipplingInit(&app->stip, n, app->w, app->h, ++app->seed);
          app->iters = 0;
          appNextBackground(app);
          updateFpsTitle(app);
        }

        // Fondo anterior: 'u' (sin reseed)
        if (sym == SDLK_u)
        {
          appPrevBackground(app);
          updateFpsTitle(app);
        }

        // Captura: marcar para el final del frame actual
        if (sym == SDLK_p)
          app->wantScreenshot = true;

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
        {
          app->maxRadius -= 0.1f;
          if (app->maxRadius < app->minRadius)
            app->maxRadius = app->minRadius;
          updateFpsTitle(app);
        }
        if (sym == SDLK_PERIOD)
        {
          app->maxRadius += 0.1f;
          if (app->maxRadius > 20.f)
            app->maxRadius = 20.f;
          updateFpsTitle(app);
        }
      }
    }

    // 2) SIMULACIÓN / TIEMPO
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - app->last) / (double)app->freq; // segundos reales
    app->last = now;
    app->accTime += dt;
    app->frames++;

    // Rotación automática de fondo + reseed (solo si está activada)
    if (app->bgPeriod > 0.0)
    {
      app->bgTimer += dt;
      if (app->bgTimer >= app->bgPeriod)
      {
        app->bgTimer = 0.0;
        appNextBackground(app);

        int n = app->stip.count;
        stipplingFree(&app->stip);
        stipplingInit(&app->stip, n, app->w, app->h, ++app->seed);

        app->iters = 0;
        updateFpsTitle(app);
      }
    }

    // Modo automático: un paso de Lloyd por frame (solo si autoRun = true)
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

    // Refrescar título y overlay cada ~0.25 s
    if (app->accTime - app->lastUiUpdate >= 0.25)
    {
      app->fpsAvg = (app->accTime > 0.0) ? (app->frames / app->accTime) : 0.0;
      updateFpsTitle(app);
      refreshFpsOverlay(app);
      app->lastUiUpdate = app->accTime;
    }

    // 3) RENDER
    if (app->showBg && app->imageTex)
    {
      SDL_Rect dst = {0, 0, app->w, app->h};
      SDL_RenderCopy(app->ren, app->imageTex, NULL, &dst);
    }
    else
    {
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

    // Captura al final del frame
    if (app->wantScreenshot)
    {
      (void)saveScreenshot(app);
      app->wantScreenshot = false;
    }

    // Overlay
    drawFpsOverlay(app);

    SDL_RenderPresent(app->ren);
  }
}

/**
 * appShutdown
 * -----------
 * Libera todos los recursos creados por la aplicación y cierra SDL/SDL_image.
 *
 * Propósito:
 *   Deja el proceso en un estado limpio después de usar `App`. Es segura ante
 *   inicializaciones parciales (p. ej., si `appInit` falló a mitad) y ante
 *   punteros NULL internos.
 */
void appShutdown(App *app)
{
  if (!app)
    return;

  // 1) Recursos propios
  stipplingFree(&app->stip); // nube de puntos

  // 2) Gráficos
  if (app->fpsTex)
    SDL_DestroyTexture(app->fpsTex);
  if (app->imageTex)
    SDL_DestroyTexture(app->imageTex);
  imageFree(&app->image); // liberar surface + metadatos

  // Métricas (si se usó logging a CSV)
  if (app->metricsPath)
    free(app->metricsPath);

  if (app->ren)
    SDL_DestroyRenderer(app->ren);
  if (app->win)
    SDL_DestroyWindow(app->win);

  if (app->font)
    TTF_CloseFont(app->font);
  TTF_Quit();

  IMG_Quit();
  SDL_Quit();

  if (app->bgPaths)
  {
    for (int i = 0; i < app->bgCount; ++i)
      free(app->bgPaths[i]);
    free(app->bgPaths);
  }

  free(app);
}
