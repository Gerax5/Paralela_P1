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
 * Contiene ventana, renderer, tiempo, FPS y recursos de imagen.
 */
struct App
{
  SDL_Window *win;      // Ventana SDL
  SDL_Renderer *ren;    // Renderer acelerado
  int w, h;             // Ancho y alto de la ventana
  Uint64 freq, last;    // Contadores de tiempo de SDL
  double accTime;       // Tiempo acumulado desde el inicio
  int frames;           // Contador de frames
  double lastFpsUpdate; // Momento de última actualización de FPS

  // Recursos de imagen
  Image image;           // Imagen cargada con SDL_image
  SDL_Texture *imageTex; // Textura para dibujar la imagen en pantalla
  Stippling stip;

  // Lloyd runtime
  bool autoRun;
  int iters;
  int pixelStride;
  float gammaW;
  unsigned seed;

  bool showBg;
  int dotRadius;
  bool wantScreenshot;
};

/*
 * Actualiza el título de la ventana con los FPS actuales.
 */
static void updateFpsTitle(App *app)
{
  double fps = (app->accTime > 0.0) ? (app->frames / app->accTime) : 0.0;
  char title[160];
  snprintf(title, sizeof(title),
           "%s — FPS: %.1f | it=%d step=%d gamma=%.2f | r=%d%s%s",
           defaultTitle, fps, app->iters, app->pixelStride, app->gammaW,
           app->dotRadius,
           app->autoRun ? " [AUTO]" : "",
           app->showBg ? "" : " [BG OFF]");
  SDL_SetWindowTitle(app->win, title);
}

static bool saveScreenshot(App *app)
{
  // Asegura que exista images/output
  struct stat st;
  if (stat("images/output", &st) != 0)
  {
    if (mkdir("images/output", 0755) != 0 && errno != EEXIST)
    {
      perror("mkdir images/output");
      return false;
    }
  }

  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, app->w, app->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surf)
  {
    fprintf(stderr, "CreateRGBSurface: %s\n", SDL_GetError());
    return false;
  }

  if (SDL_RenderReadPixels(app->ren, NULL, SDL_PIXELFORMAT_RGBA32, surf->pixels, surf->pitch) != 0)
  {
    fprintf(stderr, "RenderReadPixels: %s\n", SDL_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  char path[256];
  snprintf(path, sizeof(path), "images/output/stipple_%05d.png", app->iters);

  if (IMG_SavePNG(surf, path) != 0)
  { // 0 = OK
    fprintf(stderr, "IMG_SavePNG(%s): %s\n", path, IMG_GetError());
    SDL_FreeSurface(surf);
    return false;
  }

  printf("Saved %s\n", path);
  SDL_FreeSurface(surf);
  return true;
}

/*
 * Inicializa SDL, SDL_image, la ventana, renderer y carga la imagen por defecto.
 * Retorna true si todo se inicializa correctamente.
 */
bool appInit(App **outApp, int width, int height, const char *title, const char *imagePath)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
  {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2"); // "1" = bilinear, "2" = mejor disponible

  App *app = (App *)calloc(1, sizeof(App));

  if (!app)
  {
    SDL_Quit();
    return false;
  }

  // Configuración de la ventana
  app->w = (width > 0) ? width : defaultWidth;
  app->h = (height > 0) ? height : defaultHeight;

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

  // Inicialización de tiempo
  app->freq = SDL_GetPerformanceFrequency();
  app->last = SDL_GetPerformanceCounter();
  app->accTime = 0.0;
  app->frames = 0;
  app->lastFpsUpdate = 0.0;

  /*
   * Inicializa SDL_image para PNG y JPG
   * Si falla, libera recursos y retorna false
   */
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

  /*
   * Carga la imagen por defecto
   * Si falla, continúa sin textura o por CLI
   */
  const char *path = (imagePath && *imagePath) ? imagePath : defaultImagePath;
  if (!imageLoad(&app->image, path))
  {
    fprintf(stderr, "No se pudo cargar %s, continúo sin imagen.\n", path);
    app->imageTex = NULL;
  }
  else
  {
    app->imageTex = SDL_CreateTextureFromSurface(app->ren, app->image.surface);
    if (!app->imageTex)
    {
      fprintf(stderr, "SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    }
  }
  // Inicializa el conjunto de puntos
  if (!stipplingInit(&app->stip, defaultNPoints, app->w, app->h, 42u))
  {
    fprintf(stderr, "stipplingInit fallo\n");
  }

  app->autoRun = false;
  app->iters = 0;
  app->pixelStride = defaultLloydStep;
  app->gammaW = defaultGamma;
  app->seed = 42u;

  app->showBg = true;          // fondo visible por defecto
  app->dotRadius = 2;          // radio inicial de puntos
  app->wantScreenshot = false; // sin captura pendiente

  printf("[SPACE] paso Lloyd | [A] auto | [-]/[+] step | [G]/[H] gamma | [B] fondo | [Z]/[X] radio | [R] reseed | [P] screenshot\n");
  fflush(stdout);

  *outApp = app;
  return true;
}

/*
 * Loop principal de la aplicación.
 * Maneja eventos, actualiza FPS, dibuja la imagen y el render principal.
 */
void appRun(App *app)
{
  if (!app)
    return;
  bool running = true;

  while (running)
  {
    // Manejo de eventos
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_QUIT)
        running = false;

      if (e.type == SDL_KEYDOWN)
      {
        SDL_Keycode sym = e.key.keysym.sym;
        SDL_Scancode sc = e.key.keysym.scancode;
        Uint16 mods = e.key.keysym.mod;

        // debug
        // printf("KEYDOWN  sym=%s (%d)  scancode=%d  mods=0x%04x\n",
        //        SDL_GetKeyName(sym), sym, sc, mods);
        // fflush(stdout);

        if (sym == SDLK_ESCAPE)
          running = false;

        // 1) Iteración manual
        if (sym == SDLK_SPACE)
        {
          if (lloydStep(&app->image, &app->stip, app->w, app->h,
                        app->pixelStride, app->gammaW))
          {
            app->iters++;
          }
        }

        // 2) Auto ON/OFF
        if (sym == SDLK_a)
        {
          app->autoRun = !app->autoRun;
          updateFpsTitle(app);
        }

        // 3) step --  (SOLO '-' y keypad '-')
        if (sym == SDLK_MINUS || sym == SDLK_KP_MINUS)
        {
          if (app->pixelStride > 1)
            app->pixelStride--;
          updateFpsTitle(app);
        }

        // 4) step ++  (SOLO '+' y variantes)
        if (sym == SDLK_PLUS || sym == SDLK_KP_PLUS ||
            sym == SDLK_EQUALS /* cubre SHIFT+'=' = '+' en varios teclados */)
        {
          if (app->pixelStride < 64)
            app->pixelStride++;
          updateFpsTitle(app);
        }

        // 5) gamma up / down
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

        // Toggle fondo
        if (sym == SDLK_b)
        {
          app->showBg = !app->showBg;
          updateFpsTitle(app);
        }
        // Radio de puntos: Z (-) / X (+)
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

        // Marcar captura para el final del frame actual
        if (sym == SDLK_p)
        {
          app->wantScreenshot = true;
        }

        // 6) re-seed puntos
        if (sym == SDLK_r)
        {
          stipplingFree(&app->stip);
          stipplingInit(&app->stip, defaultNPoints, app->w, app->h, ++app->seed);
          app->iters = 0;
          updateFpsTitle(app);
        }
      }
    }

    // Actualización de tiempo
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - app->last) / (double)app->freq;
    app->last = now;
    app->accTime += dt;
    app->frames++;

    if (app->autoRun)
    {
      if (lloydStep(&app->image, &app->stip, app->w, app->h,
                    app->pixelStride, app->gammaW))
      {
        app->iters++;
      }
    }

    // Actualización periódica del título con FPS
    if (app->accTime - app->lastFpsUpdate >= 0.25)
    {
      updateFpsTitle(app);
      app->lastFpsUpdate = app->accTime;
    }

    SDL_SetRenderDrawColor(app->ren, 12, 16, 28, 255);
    SDL_RenderClear(app->ren);

    // Dibujar la imagen si existe (y si showBg == true)
    if (app->showBg && app->imageTex)
    {
      SDL_Rect dst = {0, 0, app->w, app->h};
      SDL_RenderCopy(app->ren, app->imageTex, NULL, &dst);
    }

    stipplingRender(&app->stip, app->ren, app->dotRadius);

    if (app->wantScreenshot)
    {
      saveScreenshot(app); // lee los píxeles ya renderizados
      app->wantScreenshot = false;
    }
    SDL_RenderPresent(app->ren);
  }
}

/*
 * Libera todos los recursos y cierra SDL y SDL_image
 */
void appShutdown(App *app)
{
  if (!app)
    return;

  // 1) Recursos propios
  stipplingFree(&app->stip);

  // 2) Gráficos
  if (app->imageTex)
    SDL_DestroyTexture(app->imageTex);
  imageFree(&app->image);
  if (app->ren)
    SDL_DestroyRenderer(app->ren);
  if (app->win)
    SDL_DestroyWindow(app->win);

  // 3) Subsistemas
  IMG_Quit();
  SDL_Quit();

  free(app);
}
