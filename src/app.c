#include "render_sdl.h"
#include "app.h"
#include "config.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include "image.h"

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
};

/*
 * Actualiza el título de la ventana con los FPS actuales.
 */
static void updateFpsTitle(App *app)
{
  double fps = (app->accTime > 0.0) ? (app->frames / app->accTime) : 0.0;
  char title[128];
  snprintf(title, sizeof(title), "%s — FPS: %.1f", defaultTitle, fps);
  SDL_SetWindowTitle(app->win, title);
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
  if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0)
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
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        running = false;
    }

    // Actualización de tiempo
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - app->last) / (double)app->freq;
    app->last = now;
    app->accTime += dt;
    app->frames++;

    // Actualización periódica del título con FPS
    if (app->accTime - app->lastFpsUpdate >= 0.25)
    {
      updateFpsTitle(app);
      app->lastFpsUpdate = app->accTime;
    }

    SDL_SetRenderDrawColor(app->ren, 12, 16, 28, 255);
    SDL_RenderClear(app->ren);

    // Dibujar la imagen si existe
    if (app->imageTex)
    {
      SDL_Rect dst = {0, 0, app->w, app->h}; // estirar la imagen a la ventana
      SDL_RenderCopy(app->ren, app->imageTex, NULL, &dst);
    }

    // Render principal (cuadrado y círculo pulsante)
    renderFrame(app->ren, app->w, app->h, app->accTime);
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
  if (app->imageTex)
    SDL_DestroyTexture(app->imageTex);
  imageFree(&app->image);
  if (app->ren)
    SDL_DestroyRenderer(app->ren);
  if (app->win)
    SDL_DestroyWindow(app->win);
  IMG_Quit();
  free(app);
  SDL_Quit();
}
