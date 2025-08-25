# `app.c` — Documentación técnica

## Resumen

`app.c` orquesta la aplicación gráfica _Voronoi Stippling_. Se encarga de:

- Inicializar/cerrar SDL y SDL_image.
- Crear ventana, renderer y recursos de imagen.
- Mantener el estado de la simulación (puntos, iteraciones, parámetros).
- Bucle por frame:
  - entrada (teclado/ventana)
  - simulación (Lloyd manual/auto)
  - render (fondo + puntos) y captura opcional.
- Mostrar métricas (FPS, iteraciones, parámetros) en el título.

## Flujo de alto nivel

```text
appInit(...) -> appRun(app) -> appShutdown(app)
```

## Estructura `App`

```c
struct App {
  // SDL
  SDL_Window   *win;
  SDL_Renderer *ren;

  // Canvas
  int w, h;

  // Tiempo/FPS
  Uint64 freq, last;
  double accTime, lastFpsUpdate;
  int    frames;

  // Imagen + puntos
  Image     image;
  SDL_Texture *imageTex;
  Stippling stip;

  // Visual
  bool  colorPoints;   // colorear cada punto desde la imagen
  bool  invertTheme;   // alterna fondo claro/oscuro y base de puntos
  float minRadius;     // radio mínimo por punto (estilizado)
  float maxRadius;     // radio máximo por punto (estilizado)

  // Lloyd
  bool  autoRun;
  int   iters;
  int   pixelStride;   // k >= 1
  float gammaW;        // (1 - luminancia)^gamma
  unsigned seed;       // para reseed (R)

  // Render/utilidades
  bool showBg;
  int  dotRadius;      // radio fijo (si no se usa el estilizado)
  bool wantScreenshot;

  // Batch/medición
  int   maxIters;      // 0 -> sin tope
  char *metricsPath;   // CSV (si no NULL)

  // Sweep de gamma (batch/testing)
  bool  sweepGamma;
  float gStart, gEnd, gStep;
  int   gEvery;        // aplicar cada N iteraciones

  // --- Overlay FPS
  TTF_Font    *font;         // fuente para texto
  SDL_Texture *fpsTex;       // textura cacheada del texto "FPS: ... "
  int          fpsTexW;
  int          fpsTexH;
  double       lastFpsOverlayUpdate; // última vez que refrescamos el texto

  // --- Lista de fondos
  char  **bgPaths;
  int     bgCount;
  int     bgIndex;

  double bgTimer;    // segundos acumulados desde el último cambio
  double bgPeriod;   // cada cuántos segundos cambiar de imagen (>0 activa)
};
```

**Notas:**

- Los recursos viven entre `appInit` y `appShutdown`.
- `minRadius/maxRadius`, `colorPoints`, `invertTheme` afectan el render “estilizado”.
- `metricsPath`, `maxIters` y el _sweep_ de gamma permiten ejecuciones batch con logging.

## Inicialización — `appInit`

Hace:

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)` y `IMG_Init(PNG | JPG)`.
- Crea ventana y renderer acelerado (con VSYNC si hay).
- Carga imagen (si falla, se puede continuar sin fondo) y crea textura.
- Inicializa nube de puntos (`stipplingInit`).
- Define defaults: `autoRun=false`, `pixelStride=defaultLloydStep`, `gammaW=defaultGamma`, `minRadius=0.8`, `maxRadius=3.0`, etc.
- Imprime ayuda de teclas en consola.

**Variables de entorno soportadas:**

- `STIPPLE_AUTORUN=1` -> arranca en auto.
- `STIPPLE_MAX_ITERS=N` -> tope de iteraciones (batch).
- `STIPPLE_METRICS=path.csv` -> log por iteración (se crea encabezado si no existe).
- Sweep de gamma:

  - `STIPPLE_GAMMA_START=<f>` (opcional),
  - `STIPPLE_GAMMA_END=<f>` (requiere),
  - `STIPPLE_GAMMA_STEP=<f>` (requiere),
  - `STIPPLE_GAMMA_EVERY=<int>` (opcional; default=1).

## Título y FPS — `updateFpsTitle`

- Formatea: `FPS`, `it`, `step`, `gamma`, `r` (radio fijo), `minR/maxR`, y flags `[COLOR]` / `[INVERT]`.
- Se actualiza aprox. cada 0.25 s desde `appRun`.

## Capturas — `saveScreenshot`

- Asegura `images/output/`.
- Copia el _framebuffer_ a un `SDL_Surface` RGBA32 y guarda `PNG`.
- Nombre: `images/output/stipple_XXXXX.png` (`XXXXX = iters`).
- Debe llamarse al final del frame para capturar lo que se ve.

## Fondos rotables

- `appSetBackgroundList`
  - Copia y guarda la lista y carga el primer fondo
- `appNextBackground`, `appPrevBackground`
  - Avanza/retrocede circularmente y carga el fondo.

## Bucle principal — `appRun`

**Por frame:**

1. **Entrada**

   - `SDL_PollEvent`: `SDL_QUIT` y `SDL_KEYDOWN`.
   - **Atajos de teclado:**
     - `ESC` -> salir.
     - `SPACE` -> 1 paso de Lloyd (con timing + CSV si activo).
     - `A` -> auto ON/OFF.
     - `-` / `+` (incluye keypad y `=`) -> `pixelStride` down/up.
     - `G` / `H` -> `gammaW` up/down.
     - `B` -> alterna `showBg`.
     - `Z` / `X` -> `dotRadius` down/up.
     - `R` -> reseed con nueva `seed`.
     - `P` -> marcar captura del frame.
     - `C` -> alterna `colorPoints` (color real de la imagen).
     - `I` -> alterna `invertTheme` (tema claro/oscuro).
     - `N` / `M` -> `minRadius` -/+ (clamp `[0.5, maxRadius]`).
     - `,` / `.` -> `maxRadius` -/+ (clamp `[minRadius, 20]`).

2. **Simulación:**

   - Calcula `dt`, acumula `accTime` y `frames`.
   - Si `autoRun`, ejecuta `lloydStep` (mide tiempo, loguea si `metricsPath`).
   - Aplica sweep de gamma si está activo (cada `gEvery` iters).

3. **Render:**

   - Fondo sólido o imagen (según `showBg` e `imageTex`), respetando `invertTheme`.
   - Dibuja puntos con `stipplingRenderStyled(...)` usando:

     - `minRadius/maxRadius`, `colorPoints`, `invertTheme`, y brillo local de la imagen.

   - Si `wantScreenshot`, guarda PNG y limpia el flag.
   - `SDL_RenderPresent()`.

**Notas:**

- `lloydStep` usa muestreo bilineal UV y _UniformGrid_ para _nearest neighbor_.
- `pixelStride` balancea costo/calidad (3–4 suele ir bien).

## Cierre — `appShutdown`

Orden:

1. `stipplingFree`
2. `SDL_DestroyTexture`, `imageFree`, `SDL_DestroyRenderer`, `SDL_DestroyWindow`
3. `IMG_Quit`, `SDL_Quit`
4. `free(app)`

Idempotente a nivel de punteros internos; no-op si `app == NULL`.

## Interacción con otros módulos

- `image.c`: carga y muestreo (luminancia/ RGB bilineal).
- `lloyd.c`: paso de Lloyd con ponderación por oscuridad y reseed de huérfanos.
- `voronoi.c`: _UniformGrid_ para acelerar NN.
- `stippling.c`: estado y render (incluye versión “estilizada” por brillo/color).

## Parámetros clave en runtime

- `pixelStride` (>=1): más alto -> más rápido/menos preciso por paso.
- `gammaW`: >1 concentra en zonas oscuras; <1 aplanado.
- `minRadius/maxRadius`: controlan el rango de radios por punto (estilizado).
- `colorPoints`: ON -> usa color real; OFF -> monocromo según tema.
- `invertTheme`: alterna fondo claro/oscuro y base de puntos.
- `autoRun`: ejecuta Lloyd en cada frame.

## Registro y errores

- Errores a `stderr`.
- Info/ayuda y confirmaciones a `stdout` (p. ej., captura guardada).
- CSV (si `metricsPath`) con encabezado auto y filas `iter,ms,step,gamma,npoints`.
