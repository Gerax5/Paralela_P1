# API de headers – Voronoi Stippling

> Convención general
>
> - Coordenadas en píxeles, origen en la esquina superior-izquierda.
> - Rangos válidos: `x in [0, w)`, `y in [0, h)`.
> - Todos los `bool` devuelven `true` en éxito y `false` en error.
> - Los punteros de salida nunca deben ser NULL de entrada.

## `include/app.h`

**Responsabilidad:** ciclo de vida de la aplicación (inicialización, loop, shutdown).

### Tipos

```c
typedef struct App App;  /* tipo opaco */
```

### Funciones

```c
bool appInit(App **outApp,
             int width, int height,
             const char *title,
             const char *imagePath,
             int npoints);
```

- **Crea** ventana y renderer SDL2, inicializa SDL\_image, carga la imagen (si `imagePath != NULL`), y si no, usa `defaultImagePath`.

- Inicializa el conjunto de puntos de stippling con `npoints` (si `npoints <= 0`, se usa `defaultNPoints` de `config.h`).
- **Precondiciones:** `outApp != NULL`.
- **Postcondiciones:** `*outApp` apunta a un `App` listo para `appRun`, o `NULL` en error.

```c
void appRun(App *app);
```

- Bucle principal: procesa eventos, avanza iteraciones de Lloyd (manual/auto), dibuja y presenta el frame.

```c
void appShutdown(App *app);
```

- Libera todos los recursos (renderer, ventana, texturas, superficies, memoria de puntos) y cierra SDL/SDL\_image.

**Notas:**

- Render y captura de pantalla se hacen en el hilo principal (requisito de SDL).
- Las teclas de control están documentadas en los comentarios del `app.c` (SPACE, A, -, +, G, H, B, Z, X, R, P).

## `include/config.h`

**Responsabilidad:** constantes por defecto de la aplicación.
Se usan como fallback cuando no se pasa algo por CLI o al construir `App`.

```c
#define defaultWidth       800
#define defaultHeight      600
#define defaultTitle       "Voronoi Stippling — bootstrap"
#define defaultImagePath   "images/input/twitch.png"

#define defaultNPoints     1000
#define defaultLloydStep   3       /* muestreo espacial k; 3..4 rapido, 1 exacto */
#define defaultGamma       1.0f    /* >1 enfatiza zonas oscuras */
```

**Notas:**

- Ajustar `defaultLloydStep` impacta rendimiento vs. calidad en la iteración de Lloyd.
- `defaultGamma` controla el peso de oscuridad (1.0 es lineal).

## `include/image.h`

**Responsabilidad:** carga de imagen y muestreo de intensidad.

### Tipo

```c
typedef struct {
  int w, h;           /* dimensiones en pixeles */
  int pitchPixels;    /* pitch en pixeles (no bytes) para RGBA8888 */
  Uint32 *pixels;     /* buffer de pixeles (RGBA8888) */
  SDL_Surface *surface;
} Image;
```

### Funciones

```c
bool imageLoad(Image *img, const char *path);
```

- Carga PNG/JPG con SDL2\_image y convierte a `SDL_PIXELFORMAT_RGBA32`.
- Deja `img->pixels` apuntando a los datos de la superficie convertida.

```c
void imageFree(Image *img);
```

- Libera la `SDL_Surface` y limpia campos.

```c
float sampleIntensity(const Image *img, int x, int y);
```

- Retorna luminancia Rec.709 en \[0..1] del píxel `(x, y)`.
- Si cae fuera de rango o `img` no es válida, retorna 0.0f.

```c
float sampleIntensityBilinearUV(const Image *img, float u, float v);
```

- Muestreo bilineal de luminancia Rec.709 en coordenadas normalizadas `u, v in [0, 1]`.
- Útil para mapear canvas (W x H) a imagen (w x h) sin aliasing fuerte.

**Notas**

- Todos los muestreos asumen formato `RGBA8888` ya convertido en `imageLoad`.
- La luminancia se calcula con Rec.709: `0.2126 R + 0.7152 G + 0.0722 B`.

## `include/stippling.h`

**Responsabilidad:** estado y render del conjunto de puntos.

### Tipos

```c
typedef struct { float x, y; } Dot;

typedef struct {
  Dot *pts;     /* arreglo de puntos (tam: count) */
  int count;    /* numero de puntos */
  int width;    /* ancho del canvas (para init) */
  int height;   /* alto del canvas (para init) */
} Stippling;
```

### Funciones

```c
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);
```

- Reserva `n` puntos y los inicializa aleatoriamente en `[0..w) x [0..h)` con una LCG simple.

```c
void stipplingFree(Stippling *s);
```

- Libera `pts` y limpia campos.

```c
void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius);
```

- Dibuja los puntos como discos rellenos de radio `radius` (solo visual; no afecta la simulación).

**Notas:**

- `width` y `height` están para inicialización y consistencia con Lloyd.
- No hay ownership compartido: quien crea, libera.

## `include/lloyd.h`

**Responsabilidad:** una iteración de Lloyd ponderada por intensidad.

### Función

```c
bool lloydStep(const Image *img,
               Stippling *s,
               int w, int h,
               int pixelStride,
               float gamma);
```

- Reasigna cada muestra del canvas (muestreado cada `pixelStride` pixeles) al **punto mas cercano** y acumula centroides ponderados con `(1 - luminancia)^gamma`. Luego actualiza cada punto con su centro de masa.
- `w, h` son las dimensiones del canvas donde viven los puntos (normalmente iguales a la ventana).
- `pixelStride >= 1`. Mayor -> mas rapido y menos preciso.
- `gamma > 0`. `gamma > 1` concentra puntos en zonas oscuras.

**Complejidad:**

- Con busqueda acelerada por grilla uniforme (ver `voronoi.h`), el costo por muestra se reduce al vecindario cercano. En practica, `O(samples * k)` con `k` pequeno.
- `samples ~ (w/stride) * (h/stride)`.

**Notas:**

- Internamente usa muestreo bilineal UV (`image.h`) para mapear canvas a imagen de referencia sin aliasing fuerte.

## `include/voronoi.h`

**Responsabilidad:** indice espacial para consultas de vecino mas cercano (NN) usando grilla uniforme.

### Tipo

```c
typedef struct {
  int w, h;      /* canvas */
  int cell;      /* lado de celda en pixeles */
  int cols, rows;
  int *head;     /* size: cols*rows, inicializado a -1 */
  int *next;     /* size: s->count, enlaza puntos por celda */
} UniformGrid;
```

### Funciones

```c
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);
void gridFree(UniformGrid *g);
int  gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);
```

- `gridBuild`: O(N). Llena listas por celda con los puntos actuales de `s`.
- `gridNearest`: busca NN de `(x,y)` explorando anillos de celdas hasta `ringMax`.
  Si no encuentra, retorna -1 (el llamador puede hacer fallback O(N) o aumentar `ringMax`).
- `gridFree`: libera `head/next` y deja punteros en NULL.

**Notas:**

- Si los puntos cambian mucho de posicion (por Lloyd), conviene reconstruir la grilla por iteracion.
- No es thread-safe.

## `include/render_sdl.h`

**Responsabilidad:** helpers de dibujo opcionales.

```c
void renderFrame(SDL_Renderer *ren, int w, int h, double t);
```

- Ejemplo simple de animacion; no es parte del pipeline de Lloyd.
- Se puede usar como overlay o demo visual independiente.

## Relaciones y flujo

1. `app.c` orquesta: init SDL + imagen, crea `Stippling`, loop de eventos, invoca `lloydStep`, dibuja puntos, opcionalmente dibuja fondo y guarda captura.
2. `lloyd.c` hace el trabajo pesado:

   - Construye `UniformGrid` (`voronoi.c`)
   - Recorre el canvas muestreado (stride), consulta NN por grilla, acumula centroides, actualiza puntos.
   - Muestrea la imagen `Image` con bilinear UV (`image.c`).
3. `stippling.c` maneja estado y render de puntos.
4. `render_sdl.c` puede aportar utilidades de dibujo adicionales.
