# `image.c` — carga y muestreo de imágenes

Módulo para cargar una imagen con SDL_image y muestrearla en escala de grises (luminancia Rec.709) **y en color sRGB**. Soporta muestreo puntual (nearest) y bilineal en coordenadas normalizadas.
**Novedad:** el camino bilineal de luminancia convierte de **sRGB a lineal** antes de calcular la luma, lo que mejora el contraste percibido.

## Dependencias

- SDL2_image para `IMG_Load` / `IMG_SavePNG` (si se usa).
- SDL2 para `SDL_Surface`, formatos de pixel y utilidades.

## Suposiciones de formato

- Tras cargar, la imagen se convierte a `SDL_PIXELFORMAT_RGBA32` para uniformidad.
- `img->pixels` apunta al buffer interno de `img->surface`.
- `img->pitchPixels == surface->pitch / 4` (RGBA8888 = 4 bytes por píxel).
- Para extraer canales se prioriza **`SDL_GetRGBA`** vía `unpackRGBA(...)`, que respeta masks/shifts del `SDL_PixelFormat`.
  Existe además el helper `rgbaToUint8(...)` (asume layout `0xRRGGBBAA`) útil en contextos controlados.

## Colorimetría y espacio de color

- **`srgbToLinear01`** implementa la curva IEC 61966-2-1 para pasar de sRGB → **lineal**.
- **`lumaFromPixel`** convierte R,G,B a lineal y calcula la luminancia **Rec.709**:

  ```bash
  Y = 0.2126*R + 0.7152*G + 0.0722*B
  ```

- **`sampleIntensityBilinearUV`** usa `lumaFromPixel`, por lo que su luminancia es **lineal y físicamente coherente**.
- **`sampleIntensity`** (nearest) usa bytes normalizados en sRGB (sin convertir a lineal). Es más rápido, pero puede diferir levemente del bilineal en zonas muy claras/oscuras.

## Tipos usados

```c
typedef struct {
  int w, h;          // dimensiones en píxeles
  int pitchPixels;   // pitch en píxeles (no bytes)
  Uint32 *pixels;    // buffer RGBA8888
  SDL_Surface *surface;
} Image;
```

## Helpers internos

- `static inline float srgbToLinear01(float c)`: sRGB → lineal en \[0,1].
- `static inline void unpackRGBA(const Image*, Uint32 px, Uint8*,Uint8*,Uint8*,Uint8*)`: extrae RGBA usando `SDL_GetRGBA` del formato real de la surface (portátil).
- `static inline void rgbaToUint8(Uint32 px, Uint8*,Uint8*,Uint8*,Uint8*)`: desempaque por shifts; **asume** layout `0xRRGGBBAA`.

## API

### `bool imageLoad(Image *img, const char *path)`

Carga PNG/JPG y convierte a `RGBA32`.

**Flujo:**

1. `IMG_Load(path)`
2. `SDL_ConvertSurfaceFormat(..., SDL_PIXELFORMAT_RGBA32, 0)`
3. Completa `img` (`surface`, `pixels`, `w`, `h`, `pitchPixels`)

**Retorno:** `true` en éxito; `false` en error (se loguea en `stderr`).
**Notas:** `img->pixels` no se libera aparte; pertenece a `img->surface`.

### `void imageFree(Image *img)`

Libera la `SDL_Surface` y deja `img` en estado neutro (`NULL`/`0`). Idempotente y segura con `img == NULL`.

### `float sampleIntensity(const Image *img, int x, int y)`

Muestreo **nearest** en coordenadas de píxel.
Devuelve luminancia Rec.709 **aproximada** en `[0,1]` (sin pasar a lineal).
Retorna `0.0f` si parámetros/rangos no son válidos.

### `float sampleIntensityBilinearUV(const Image *img, float u, float v)`

Muestreo **bilineal** de luminancia en `u,v ∈ [0,1]`.
Clampea `u,v`, toma `p00,p10,p01,p11`, convierte cada vecino a **lineal** y mezcla bilinealmente.
Retorna `0.0f` si la imagen no es válida.

### `void sampleRgbBilinearUV(const Image *img, float u, float v, Uint8 *r, Uint8 *g, Uint8 *b)`

Muestrea **color sRGB** por bilineal en `u,v ∈ [0,1]` y escribe `r,g,b ∈ [0..255]`.
Interpolación realizada en espacio sRGB (rápida; suficiente para “tintar” puntos).
Si la imagen/parámetros no son válidos, escribe `0,0,0`.

## Errores y retorno

- `imageLoad` retorna `false` si falla `IMG_Load` o la conversión; imprime motivo.
- Los muestreos retornan `0.0f` (o `0,0,0`) ante entradas inválidas.

## Rendimiento

- **Nearest (`sampleIntensity`)**: 1 lectura + aritmética mínima → **muy rápido**.
- **Bilineal de luma (`sampleIntensityBilinearUV`)**: 4 lecturas + sRGB→lineal + mezcla → **más costo, mejor fidelidad**.
- **Bilineal de color (`sampleRgbBilinearUV`)**: 4 lecturas + mezcla sRGB → costo similar al de luma bilineal, sin conversiones sRGB→lineal.

## Ejemplo

```c
Image img;
if (!imageLoad(&img, "images/input/twitch.png")) {
  // manejar error
}

// nearest en (100,50) (aprox en sRGB)
float y0 = sampleIntensity(&img, 100, 50);

// bilinear (correcto en lineal)
float y1 = sampleIntensityBilinearUV(&img, 0.33f, 0.75f);

// color sRGB bilineal
Uint8 r,g,b;
sampleRgbBilinearUV(&img, 0.33f, 0.75f, &r, &g, &b);

imageFree(&img);
```

## Invariantes

Tras `imageLoad == true`:

- `img->surface` y `img->pixels` no son `NULL`
- `img->w > 0`, `img->h > 0`
- `img->pitchPixels == img->surface->pitch / 4`

Tras `imageFree(&img)`:

- `img->surface == NULL`, `img->pixels == NULL`
- `img->w == img->h == img->pitchPixels == 0`
