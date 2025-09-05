# `app.c` — Documentación técnica

## Rol del módulo

`app.c` orquesta la vida de la app **Voronoi Stippling**: inicializa subsistemas (SDL/SDL_image/TTF), crea ventana/renderer, carga fondos, prepara la nube de puntos, ejecuta el bucle principal (eventos -> simulación -> render), muestra métricas y libera recursos al salir. El tipo `App` es **opaco** fuera de `app.c` y concentra todo el estado de ejecución (ventana, renderer, imagen, puntos, timers, UI, logging, etc.).

> Vida útil: los recursos viven entre `appInit(..)` y `appShutdown(..)`; el `struct App` contiene ventana/renderer, imagen/texture, nube de puntos, timers/FPS, opciones visuales, batch/logging y rotación de fondos.

## Flujo de alto nivel

- **appInit** -> **appRun** -> **appShutdown**.
   En `appRun`: por frame hace **entrada** (SDL events), **simulación** (Lloyd manual o auto, con timing/CSV y sweep de gamma si procede) y **render** (fondo + puntos + overlay + captura).

**Teclas** (resumen): `ESC`, `SPACE`, `A`, `G/H`, `B`, `Z/X`, `N/M`, `,/.`, `C`, `I`, `R`, `P`, `O/U`. El módulo imprime esta ayuda al iniciar.

## Convenciones

- **Hilo principal**: todas las funciones de `app` se invocan desde el **main thread**.
- **Defensivo**: comprobaciones de error ante fallos de creación de ventana/renderer, `IMG_Init`, etc., con liberación ordenada antes de devolver `false`.
- **Idempotencia de cierre**: `appShutdown(NULL)` no falla; además hace `free/destroy` condicionales y cierra subsistemas en orden.
- **Batch/Métricas**: si `metricsPath` está definido, cada iteración registra `iter,ms,step,gamma,npoints` (CSV, con encabezado).
- **Overlay y título**: título/overlay se refrescan \~cada 0.25s con `FPS`, `iters`, `step`, `gamma`, radios, y flags `[COLOR]/[INVERT]`.
- **Screenshots**: la captura se realiza **al final del frame** si `wantScreenshot` está activo.
- **Fondos**: se mantiene una lista circular de rutas y se cargan con `appSetBackgroundList`/`appNextBackground`/`appPrevBackground`.

## Funciones públicas (expuestas en `app.h`)

### `bool appInit(App **outApp, int width, int height, const char *title, const char *imagePath, int npoints);`

- **Entradas:**
  `outApp: App**` (salida por referencia) - `width,height: int` (<=0 usa defaults) + `title: const char*` (NULL -> default) + `imagePath: const char*` (NULL -> default) + `npoints: int` (<=0 -> default).
- **Salidas:**
  `bool` – `true` si inicializó todo correctamente.
- **Descripción:**
  Inicializa `SDL`/`SDL_image`/`TTF`, crea ventana/renderer VSYNC, prepara directorios, carga lista de fondos (o la imagen pasada), inicializa la nube de puntos y la UI (fuente/overlay). Imprime ayuda de teclas. Maneja errores liberando recursos y retornando `false`.

### `void appRun(App *app);`

- **Entradas:**
  `app: App*` (instancia válida).
- **Salidas:** (bloquea hasta salir).
- **Descripción:**
  Bucle principal: procesa eventos (`SDL_QUIT`, `SDL_KEYDOWN`), ejecuta **Lloyd** en modo **manual** (`SPACE`) o **auto** (si `autoRun`), mide tiempos y registra CSV si está activo, aplica **sweep de gamma** según configuración y renderiza (fondo sólido/imagen, puntos estilizados, overlay y captura diferida). Refresca título/overlay cada \~0.25s.

### `void appShutdown(App *app);`

- **Entradas:**
  `app: App*` (se permite `NULL`).
- **Salidas:**
- **Descripción:**
  Libera nube de puntos y texturas, destruye renderer/ventana, cierra `TTF`, `IMG_Quit`, `SDL_Quit`, libera lista de fondos y el `App`. Idempotente ante punteros internos `NULL`.

### **Fondos**

- `bool appSetBackgroundList(App *app, int count, const char *const *paths);`
  - **In**: `app`, `count>0`, `paths[]`
  - **Out**: `bool`
  - **Desc.** Duplica y guarda la lista (propiedad pasa a `App`) y carga el primer fondo.
- `void appNextBackground(App *app);` - `void appPrevBackground(App *app);`
  - **In**: `app`
  - **Out**:
  - **Desc.** Avanza/retrocede circularmente y **carga** el fondo.

## Helpers **internos** (estáticos en `app.c`)

- `static void refreshFpsOverlay(App *app);`
  - **In**: `app`
  - **Out**:
  - **Desc.** Regenera el texto del overlay (FPS/it/step/gamma) y cachea textura; llamado \~cada 0.25 s.

- `static void drawFpsOverlay(App *app);`
  - **In**: `app`
  - **Out**:
  - **Desc.** Dibuja un recuadro semitransparente y la textura cacheada del overlay.

- `static void ensureDir(const char *path);`
  - **In**: `path`
  - **Out**:
  - **Desc.** Crea el directorio si no existe (defensivo).

- `static bool loadBackground(App *app, const char *path);`
  - **In**: `app`, `path`
  - **Out**: `bool`
  - **Desc.** Carga imagen (PNG/JPG), crea `SDL_Texture` y reemplaza la anterior.

- `static void updateFpsTitle(App *app);`
  - **In**: `app`
  - **Out**:
  - **Desc.** Formatea el **título** de la ventana con `FPS`, `it`, `step`, `gamma`, radio y flags de visual.

- `static void sweepGammaTick(App *app);`
  - **In**: `app`
  - **Out**:
  - **Desc.** Si el barrido está activo, ajusta `gammaW` cada `gEvery` iteraciones en dirección a `gEnd`.

- `static bool saveScreenshot(App *app);`
  - **In**: `app`
  - **Out**: `bool`
  - **Desc.** Lee el backbuffer y guarda `PNG` con nombre `images/output/stipple_%05d.png` (iteración actual). Se invoca al **final** del frame.

## Interacción con otros módulos

- **`image.*`**: carga PNG/JPG y muestreo bilineal de color/ luminancia.
- **`stippling.*`**: estructura de puntos y render "estilizado" (radios por intensidad/color).
- **`lloyd.*` / `voronoi.*`**: paso de Lloyd (centroidal Voronoi) y búsqueda de vecino más cercano con **UniformGrid**. En builds paralelos, la paralelización vive aquí; `app.c` permanece en el hilo principal.

## Despliegue de resultados

- **Overlay** (FPS/estado) y **título** informativo, actualizados \~cada 0.25 s.
- **Screenshots** en `images/output/` cuando se pulsa `P` (captura diferida al final del frame).

## Variables de entorno relevantes (soportadas por `app.c`)

- `STIPPLE_AUTORUN=1|0` -> auto-iterar Lloyd por frame.
- `STIPPLE_MAX_ITERS=K` -> modo batch: salir al llegar a `K` iteraciones (log si `STIPPLE_METRICS`).
- `STIPPLE_METRICS=path.csv` -> CSV por iteración: `iter,ms,step,gamma,npoints`.
- **Barrido de gamma**: `STIPPLE_GAMMA_START`, `STIPPLE_GAMMA_END`, `STIPPLE_GAMMA_STEP`, `STIPPLE_GAMMA_EVERY` (activa cuando hay `END` y `STEP`).

## Glosario mínimo del estado `App`

- **Tiempo/FPS**: `freq`, `last`, `accTime`, `frames`, `fpsAvg`, `lastUiUpdate`.
- **Recursos**: `image`, `imageTex`, `stip`.
- **Visual**: `showBg`, `colorPoints`, `invertTheme`, `minRadius`, `maxRadius`, `dotRadius`.
- **Lloyd**: `autoRun`, `iters`, `pixelStride`, `gammaW`, `seed`.
- **Batch**: `maxIters`, `metricsPath`, `sweepGamma{gStart,gEnd,gStep,gEvery}`.
- **UI**: `font`, `fpsTex`, `fpsTexW/H`.
- **Fondos**: `bgPaths`, `bgCount`, `bgIndex`, `bgTimer`, `bgPeriod`.
