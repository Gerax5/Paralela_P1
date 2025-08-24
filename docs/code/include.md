# API de headers – Voronoi Stippling

> Convenciones
>
> - Coordenadas en píxeles, origen arriba-izquierda.
> - Rangos válidos: `x ∈ [0,w)`, `y ∈ [0,h)`.
> - Todas las funciones `bool` retornan `true` en éxito, `false` en error.
> - Punteros de salida: no deben ser `NULL`.

## `include/app.h`

**Rol:** ciclo de vida (init → loop → shutdown).

### Tipos

```c
typedef struct App App;  /* tipo opaco */
````

### Funciones

```c
bool appInit(App **outApp,
             int width, int height,
             const char *title,
             const char *imagePath,
             int npoints);
```

- Crea ventana/renderer (SDL2), inicializa SDL_image, carga imagen (`imagePath` o `defaultImagePath`) e inicializa la nube con `npoints` (o `defaultNPoints`).

```c
void appRun(App *app);
```

- Bucle principal: eventos → paso(s) de Lloyd (manual/auto) → render.

```c
void appShutdown(App *app);
```

- Libera puntos, texturas/superficies, renderer/ventana y cierra SDL/SDL_image.

**Controles (runtime):**

- `SPACE` paso Lloyd
- `A` auto
- `-`/`+` step
- `G`/`H` gamma
- `B` fondo
- `Z`/`X` radio
- `R` reseed
- `P` screenshot
- `C` color ON/OFF
- `I` tema
- `N`/`M` minR -/+
- `,`/`.` maxR -/+
- `ESC` salir.

## `include/config.h`

**Rol:** defaults (fallback si no se pasan por CLI).

```c
#define defaultWidth     800
#define defaultHeight    600
#define defaultTitle     "Voronoi Stippling - bootstrap"
#define defaultImagePath "images/input/twitch.png"

#define defaultNPoints   1000
#define defaultLloydStep 3     /* k de muestreo; 1 preciso, 3–4 rápido */
#define defaultGamma     1.0f  /* exp. del peso */
```

**Peso nativo de Lloyd (build-time):**

```c
/* 1 -> w = luminancia^gamma (favorece claros)
 * 0 -> w = (1 - luminancia)^gamma (favorece oscuros) */
#ifndef STIPPLE_WEIGHT_BY_BRIGHTNESS
#define STIPPLE_WEIGHT_BY_BRIGHTNESS 1
#endif
```

Notas:

- `defaultLloydStep` controla costo/calidad por iteración.
- Con `STIPPLE_WEIGHT_BY_BRIGHTNESS==1`: `gamma>1` enfatiza zonas **claras**; con `0`: `gamma>1` enfatiza **oscuras**.

## `include/image.h`

**Rol:** carga y muestreo de imagen.

### Tipo

```c
typedef struct {
  int w, h;           /* dimensiones px */
  int pitchPixels;    /* pitch/4 en RGBA32 */
  Uint32 *pixels;     /* buffer RGBA8888 */
  SDL_Surface *surface;
} Image;
```

### Funciones

```c
bool  imageLoad(Image *img, const char *path);
void  imageFree(Image *img);
float sampleIntensity(const Image *img, int x, int y);
float sampleIntensityBilinearUV(const Image *img, float u, float v);
void  sampleRgbBilinearUV(const Image *img, float u, float v,
                          Uint8 *r, Uint8 *g, Uint8 *b);
```

Notas:

- `imageLoad` convierte a `SDL_PIXELFORMAT_RGBA32`.
- Luma = Rec.709 en `[0..1]` sobre **RGB lineal** (se hace sRGB→lineal internamente).
- `sample*UV` usa bilineal en `u,v ∈ [0,1]` (evita aliasing al mapear canvas↔imagen).

## `include/stippling.h`

**Rol:** estado/render de la nube de puntos.

### Tipos

```c
typedef struct { float x, y; } Dot;

typedef struct {
  Dot *pts;
  int  count, width, height;
} Stippling;
```

### Funciones

```c
bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);
void stipplingFree(Stippling *s);

/* Render “estilizado”: radio por brillo y color opcional de la imagen */
void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH,
                           const Image *img, float minR, float maxR,
                           bool useColor, bool invertTheme);
```

Notas:

- `stipplingRenderStyled`: radio por punto `radius = minR + (1 - luma)*(maxR - minR)`; si `useColor==true`, toma `r,g,b` bilineal de la imagen; `invertTheme` alterna base claro/oscuro en monocromo.

## `include/lloyd.h`

**Rol:** una iteración de Lloyd ponderada por imagen.

```c
bool lloydStep(const Image *img,
               Stippling *s,
               int w, int h,
               int pixelStride,
               float gamma);
```

- Muestrea el lienzo `(w×h)` cada `pixelStride` px; asigna cada muestra a su punto más cercano (acelerado por grilla) y acumula centroide con peso:

  - si `STIPPLE_WEIGHT_BY_BRIGHTNESS==1`: `w = luminancia^gamma`;
  - si `==0`: `w = (1 - luminancia)^gamma`.
- Actualiza cada punto al centro de masa ponderado.

**Coste aprox.:** `O((w/stride * h/stride) * k)`, `k` = puntos en celdas vecinas (grilla uniforme).

## `include/voronoi.h`

**Rol:** índice espacial (grilla uniforme) para vecino más cercano.

### Tipo

```c
typedef struct {
  int w,h, cell, cols, rows;
  int *head; /* cols*rows, -1 si vacia */
  int *next; /* s->count, enlaza puntos por celda */
} UniformGrid;
```

### Funciones

```c
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);
void gridFree(UniformGrid *g);
int  gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);
```

Notas:

- `gridBuild`: O(N). Reconstruir si los puntos se mueven mucho (cada iteración de Lloyd).
- `gridNearest`: explora anillos de celdas hasta `ringMax`; retornar `-1` implica ampliar `ringMax` o fallback O(N).
