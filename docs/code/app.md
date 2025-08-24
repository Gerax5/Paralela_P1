# `app.c` — Documentación técnica

## Resumen

`app.c` orquesta la aplicación gráfica del proyecto *Voronoi Stippling*. Se encarga de:

- Inicializar y cerrar SDL/SDL_image.
- Crear ventana, renderer y recursos de imagen.
- Mantener el estado de la simulación (puntos, iteraciones, parámetros).
- Ejecutar el bucle principal por frame:

  1. Entrada de usuario (teclado/ventana).
  2. Simulación (paso de Lloyd manual o automatico).
  3. Render (fondo + puntos) y captura opcional.
- Mostrar FPS y estado en el titulo de la ventana.

## Flujo de alto nivel

```bash
appInit(...) -> appRun(app) -> appShutdown(app)
```

- `appInit` prepara todo lo necesario (SDL, imagen, puntos).
- `appRun` procesa eventos, ejecuta Lloyd y dibuja la escena.
- `appShutdown` libera recursos en orden correcto.

## Estructura `App`

```c
struct App {
  // SDL
  SDL_Window   *win;     // ventana
  SDL_Renderer *ren;     // renderer acelerado (con vsync si hay soporte)

  // Canvas
  int w, h;              // dimensiones actuales de la ventana

  // Tiempo y FPS
  Uint64 freq;           // frecuencia del contador de alto rendimiento
  Uint64 last;           // ultimo tick medido
  double accTime;        // tiempo acumulado (s)
  int frames;            // frames renderizados desde init
  double lastFpsUpdate;  // ultima vez que se actualizo el titulo (s)

  // Imagen y stippling
  Image image;           // surface RGBA8888 + acceso a pixeles
  SDL_Texture *imageTex; // textura para dibujar la imagen de fondo
  Stippling stip;        // nube de puntos

  // Parametros de Lloyd
  bool  autoRun;         // si es true, ejecuta Lloyd cada frame
  int   iters;           // iteraciones acumuladas
  int   pixelStride;     // muestreo espacial (>=1; grande = mas rapido, menos preciso)
  float gammaW;          // peso (1 - luminancia)^gamma
  unsigned seed;         // semilla para resembrar puntos (R)

  // Opciones visuales
  bool showBg;           // dibujar fondo si hay imagen cargada
  int  dotRadius;        // radio visual de los puntos
  bool wantScreenshot;   // marcar captura para el final del frame actual
};
```

Pautas:

- Los recursos de SDL viven entre `appInit` y `appShutdown`.
- `iters`, `pixelStride`, `gammaW` y `seed` controlan la simulacion.
- `showBg` y `dotRadius` solo afectan el render.

## Inicializacion: `appInit`

Responsable de:

- `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)`.
- `IMG_Init` con `PNG` y `JPG`.
- Creacion de `SDL_Window` y `SDL_Renderer` (acelerado + vsync).
- Carga de imagen (si falla, la app continua sin fondo).
- Creacion de textura a partir de la superficie cargada.
- Inicializacion de la nube de puntos (`stipplingInit`).
- Seteo de parametros por defecto (`autoRun=false`, `pixelStride`, `gammaW`, etc.).
- Mensaje de ayuda en consola con los atajos.

Errores:

- Cada fallo imprime motivo en `stderr` y limpia lo ya creado antes de devolver `false`.

## Titulo y FPS: `updateFpsTitle`

- Formatea el titulo con:

  - `FPS` promedio desde el arranque.
  - `it`, `step`, `gamma`, `r`.
  - Indicadores `"[AUTO]"` y `"[BG OFF]"` segun el estado.
- Llamado cada \~0.25 s desde `appRun` para evitar sobrecarga.

## Capturas: `saveScreenshot`

- Crea `images/output` si no existe.
- Lee los pixeles del render target a un `SDL_Surface` RGBA32.
- Guarda `PNG` con nombre `stipple_XXXXX.png` (XXXXX = `iters`).
- Debe invocarse al final del frame para capturar exactamente lo dibujado.
- Devuelve `true` en exito, `false` en caso de error (y loguea el motivo).

## Bucle principal: `appRun`

Estructura por frame:

1. **Entrada**

   - `SDL_PollEvent` procesa `SDL_QUIT` y `SDL_KEYDOWN`.
   - Atajos de teclado:

     - `ESC` -> salir.
     - `SPACE` -> 1 paso de Lloyd.
     - `A` -> alterna `autoRun`.
     - `-` / `+` (incluye keypad y `=`) -> `pixelStride` down/up.
     - `G` / `H` -> `gammaW` up/down.
     - `B` -> alterna `showBg`.
     - `Z` / `X` -> `dotRadius` down/up.
     - `R` -> resembrar nube con nueva `seed`.
     - `P` -> marcar captura del frame.

2. **Simulacion**

   - Calcula `dt` con `SDL_GetPerformanceCounter`.
   - Acumula `accTime` y `frames`.
   - Si `autoRun == true`, ejecuta `lloydStep` y aumenta `iters`.
   - Actualiza el titulo cada \~0.25 s.

3. **Render**

   - Limpia el fondo con color solido.
   - Dibuja la imagen de fondo si `showBg` y hay `imageTex`.
   - Dibuja los puntos (`stipplingRender`).
   - Si `wantScreenshot == true`, llama `saveScreenshot` y limpia el flag.
   - `SDL_RenderPresent`.

Notas:

- `lloydStep` usa el muestreo bilinear de la imagen y una grilla uniforme para vecino mas cercano (ver `lloyd.c` y `voronoi.c`).
- `pixelStride` controla la densidad de muestreo por iteracion (3-4 suele ser buen compromiso).

## Cierre: `appShutdown`

Orden de liberacion:

1. `stipplingFree`.
2. `SDL_DestroyTexture`, `imageFree`, `SDL_DestroyRenderer`, `SDL_DestroyWindow`.
3. `IMG_Quit`, `SDL_Quit`.
4. `free(app)`.

Llamar exactamente una vez cuando ya no se necesite renderizar ni acceder a la imagen.

## Interaccion con otros modulos

- `image.c`: carga la imagen y expone muestreo por luminancia (`sampleIntensity*`).
- `lloyd.c`: implementa el paso de Lloyd con ponderacion por oscuridad y nearest usando `UniformGrid`.
- `voronoi.c`: indice espacial (grilla) para acelerar la busqueda del punto mas cercano.
- `stippling.c`: estado y renderizado de la nube de puntos.
- `render_sdl.c`: utilidades de dibujo (no critica aqui).

## Parametros importantes en runtime

- `pixelStride` (step >= 1): subirlo acelera el barrido de pixeles, pero reduce precision por paso.
- `gammaW`:

  - `> 1.0` -> da mas peso a zonas oscuras (mas puntos ahi).
  - `< 1.0` -> lo contrario.
- `autoRun`: si esta activo, se itera Lloyd en cada frame.
- `dotRadius`: puramente visual; no cambia la simulacion.
- `showBg`: alterna la imagen de fondo.

## Errores y registro

- Se usa `fprintf(stderr, ...)` para errores de SDL/SDL_image y utilidades del sistema.
- Mensajes de ayuda y exito (`printf`) para capturas y controles.

## Extensiones sugeridas

- Parseo de argumentos de linea de comandos:
  - `N` (numero de puntos), `IMG` (ruta), y, pensando en OMP, `T` (threads) y `S` (schedule).
- Medicion de rendimiento por iteracion y export a CSV.
- Modo paso a paso con limites de velocidad (p. ej., dormir si FPS > X).
- Reescalado de ventana con ajuste de puntos (si se desea).
