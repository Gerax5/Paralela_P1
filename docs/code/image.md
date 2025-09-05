# `image.c` — Documentación técnica

## Rol del módulo

Proveer una **capa delgada y segura** para: (1) cargar una imagen desde disco, (2) convertirla a un formato homogéneo `RGBA32`, (3) **precomputar luminancia lineal** (`img->luma`) y (4) exponer **muestras** de intensidad/color con versiones **nearest** y **bilineal**.

## Flujo interno

1. **Carga**

   - `imageLoad(img, path)` valida punteros, hace `IMG_Load`, convierte a `SDL_PIXELFORMAT_RGBA32`, rellena campos (`w,h,pixels,pitchPixels`) y dispara `imageBuildLuma(img)`. En error, limpia y **loggea** en `stderr`.

2. **Precálculo de luma**

   - `imageBuildLuma(img)` recorre filas/columnas, extrae `R,G,B` (vía `SDL_GetRGBA`), convierte **sRGB->lineal** y calcula **luma Rec.709** por píxel; guarda en `img->luma`. (Escalable por filas; sin sincronización compartida).

3. **Muestreo**

   - **Nearest:** `sampleIntensity(img,x,y)` usa `pitchPixels`, desempaqueta canales con `SDL_GetRGBA` y calcula luma **en sRGB normalizado** (rápido).
   - **Bilineal (luma):** `sampleIntensityBilinearUV(img,u,v)` **clamp**, mapea `(u,v)->(x,y)`, toma 4 vecinos y **mezcla en dominio lineal** (si hay `img->luma`, usa la tabla).
   - **Bilineal (color):** `sampleRgbBilinearUV(img,u,v, r,g,b)` interpola canales **sRGB** para tintar puntos.

4. **Liberación**

   - `imageFree(img)` libera la `SDL_Surface` y deja la estructura en estado neutro (**idempotente**).

## Convenciones

- **Formato**: siempre `SDL_PIXELFORMAT_RGBA32`; `pixels = (Uint32*)surface->pixels`; `pitchPixels = surface->pitch / 4`.
- **Coordenadas**: `(x,y)` en píxel entero; `(u,v)` **normalizados** \[0..1] (se hace **clamp** interno).
- **Luma**: **Rec.709** en \[0..1]; **nearest** la computa en sRGB, **bilineal** la interpola en **lineal** (evita artefactos por gamma).
- **Portabilidad**: extracción de canales con `SDL_GetRGBA` (independiente de endian/layout).
- **Defensivo**: validaciones de punteros/rangos y `clamp` a \[0..1] en bilineal. Retornos nulos ante entradas inválidas.

**Paralelo/sincronía**: `imageBuildLuma` es **embarrassingly parallel** por filas; no requiere sincronización si se paraleliza externamente (cada hilo escribe su fila). El módulo no introduce primitivas de sincronía propias.

## Tipo principal

```c
typedef struct {
  int w, h;            // dimensiones px
  int pitchPixels;     // pitch/4 en RGBA32
  Uint32 *pixels;      // buffer RGBA8888 (alias de surface->pixels)
  SDL_Surface *surface;
  float *luma;         // luminancia lineal precomputada (w*h)
} Image;
```

## `bool imageLoad(Image *img, const char *path);`

- **Entradas**: `img` (salida no inicializada), `path` (ruta).
- **Salidas**: `true/false`. En éxito, llena `w,h,pixels,pitchPixels,surface` y **construye `luma`**.
- **Descripción**: carga vía `SDL_image`, convierte a `RGBA32`, deja el objeto listo para muestreos; limpia y reporta en error.

## `void imageBuildLuma(Image *img);`

- **Entradas**: `img` válido con `pixels`/`surface`.
- **Salidas**: — (efecto: `img->luma = w*h` en **lineal**).
- **Descripción**: recorre la imagen, hace `sRGB->lineal` canal por canal y calcula luma Rec.709 en un buffer contiguo para **muestreo bilineal rápido/fiel**. Escalable por filas.

## `void imageFree(Image *img);`

- **Entradas**: `img` (se permite `NULL`).
- **Salidas**: —.
- **Descripción**: libera la `SDL_Surface` y pone `w=h=pitchPixels=0`, `pixels=surface=luma=NULL`. **Segura e idempotente**.

## `float sampleIntensity(const Image *img, int x, int y);`

- **Entradas**: `img`, `x,y` en rango.
- **Salidas**: `luma_srgb` en \[0..1] (0.0 si inválido).
- **Descripción**: muestreo **nearest** de luma Rec.709 a partir de `RGBA32` usando `SDL_GetRGBA` y pesos Rec.709 **en sRGB**. Muy rápido; para **interpolación fiel** usa la versión bilineal.

## `float sampleIntensityBilinearUV(const Image *img, float u, float v);`

- **Entradas**: `img`, `u,v` normalizados (se hace **clamp** interno).
- **Salidas**: `luma_lineal` en \[0..1] (0.0 si inválido).
- **Descripción**: mapea `(u,v)` -> `(x,y)`, toma los 4 vecinos y **mezcla en lineal**. Si existe `img->luma`, la usa directamente (menos conversiones).

## `void sampleRgbBilinearUV(const Image *img, float u, float v, Uint8 *r, Uint8 *g, Uint8 *b);`

- **Entradas**: `img`, `u,v`, punteros `r,g,b`.
- **Salidas**: `r,g,b ∈ [0..255]` (0s si inválido).
- **Descripción**: muestreo **bilineal** por canal en **sRGB** (rápido y suficiente para colorear puntos).

## Notas de programación defensiva

- Validación de **punteros/rangos** en `imageLoad`, `sampleIntensity`, `sampleIntensityBilinearUV`.  
- **Clamp** de `(u,v)` a \[0..1] en bilineal.
- Extracción de canales con `SDL_GetRGBA` (portátil, evita asumir desplazamientos).
