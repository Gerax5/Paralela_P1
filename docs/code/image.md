# `image.c` — carga y muestreo de imágenes

Módulo para cargar una imagen con SDL_image y muestrearla en escala de grises (luminancia Rec.709). Soporta muestreo puntual (nearest) y bilineal en coordenadas normalizadas.
**Novedad:** el camino bilineal convierte de **sRGB a lineal** antes de calcular la luma, lo que mejora mucho el contraste real.

## Dependencias

- SDL2_image para `IMG_Load`.
- SDL2 para `SDL_Surface`, formatos de pixel y utilidades.

## Suposiciones de formato

- Al cargar, la imagen se convierte a `SDL_PIXELFORMAT_RGBA32` para tener un layout homogéneo.
- `img->pixels` apunta al buffer interno de `img->surface`.
- `img->pitchPixels == surface->pitch / 4` (4 bytes por pixel en RGBA8888).
- Los helpers internos que desempaquetan canales por shifts asumen layout `0xRRGGBBAA`. Para portabilidad total, usar `SDL_GetRGBA(px, surface->format, ...)`.

## Colorimetría y espacio de color

- **`srgbToLinear01`** aplica la curva IEC 61966-2-1 (sRGB) para pasar a **espacio lineal**.
- **`lumaFromPixel`** usa esa conversión y luego aplica **coeficientes Rec.709** en lineal:

  ```bash
  Y = 0.2126*R + 0.7152*G + 0.0722*B
  ```

- **`sampleIntensityBilinearUV`** llama a `lumaFromPixel`, así que su luminancia es **lineal y físicamente coherente**.
- **`sampleIntensity` (nearest)** mantiene un atajo rápido usando bytes normalizados sin pasar por sRGB->lineal. Es más barato, pero puede diferir levemente del bilineal en zonas claras/oscuras. Si se desea máxima consistencia, se puede actualizar para que también use la conversión (a costa de unas pocas operaciones extra).

## Tipos usados

```c
typedef struct {
  int w, h;          // dimensiones en pixeles
  int pitchPixels;   // pitch en pixeles (no bytes)
  Uint32 *pixels;    // buffer RGBA8888
  SDL_Surface *surface;
} Image;
```

## API

### `bool imageLoad(Image *img, const char *path)`

Carga una imagen desde disco y la convierte a `RGBA32`.

**Flujo:**

1. `IMG_Load(path)`
2. `SDL_ConvertSurfaceFormat(..., SDL_PIXELFORMAT_RGBA32, 0)`
3. Rellena `img` con `surface`, `pixels`, `w`, `h`, `pitchPixels`.

**Retorno:**

- `true` si la carga y conversión fue exitosa.
- `false` si falla (se loguea en `stderr`).

**Notas:**

- `img->pixels` no se libera por separado; vive dentro de `img->surface`.
- En caso de fallo no quedan recursos colgados.

### `void imageFree(Image *img)`

Libera la `SDL_Surface` y deja `img` en estado neutro.

**Efectos:**

- Si `img` es `NULL` no hace nada.
- Si `img->surface` existe, la libera con `SDL_FreeSurface`.
- Setea punteros a `NULL` y escalares a 0.

### `static inline void rgbaToUint8(Uint32 px, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)` *(interno)*

Desempaqueta un pixel de 32 bits a canales de 8 bits mediante shifts y máscaras.
**Asume** layout `0xRRGGBBAA`.

### `float sampleIntensity(const Image *img, int x, int y)`

Muestreo **nearest** en coordenadas de pixel. Retorna luminancia Rec.709 en `[0,1]` usando bytes normalizados (sin sRGB->lineal).
Devuelve `0.0f` si parámetros o rangos no son válidos.

### `static inline float lumaFromPixel(Uint32 px)` *(interno)*

Convierte canales sRGB -> lineal y calcula luma Rec.709 en `[0,1]`.

### `float sampleIntensityBilinearUV(const Image *img, float u, float v)`

Muestreo **bilineal** en `u,v` en `[0,1]`.
Clampea `u,v`, convierte a `(x,y)`, toma `p00,p10,p01,p11`, evalúa luma **en lineal** y mezcla bilinealmente.
Devuelve `0.0f` si la imagen no es válida.

## Errores y retorno

- `imageLoad` retorna `false` y explica el motivo si falla `IMG_Load` o la conversión de formato.
- Los muestreos retornan `0.0f` si la imagen o parámetros no son válidos.

## Rendimiento

- `sampleIntensity` hace 1 lectura de pixel y operaciones mínimas; es el camino más barato.
- `sampleIntensityBilinearUV` hace 4 lecturas, sRGB->lineal y mezclas; mayor costo pero **mejor fidelidad**.
- Para Lloyd en tiempo real, bilineal lineal ofrece mejor mapeo de densidad a un costo aceptable.

## Ejemplo

```c
Image img;
if (!imageLoad(&img, "images/input/twitch.png")) {
  // manejar error
}

// nearest en pixel (100, 50) (aprox en sRGB)
float y0 = sampleIntensity(&img, 100, 50);

// bilinear en UV (correcto en lineal)
float y1 = sampleIntensityBilinearUV(&img, 0.33f, 0.75f);

imageFree(&img);
```

## Invariantes y contrato

- Tras `imageLoad == true`:

  - `img->surface != NULL`, `img->pixels != NULL`
  - `img->w > 0`, `img->h > 0`
  - `img->pitchPixels == img->surface->pitch / 4`
- Tras `imageFree(&img)`:

  - `img->surface == NULL`, `img->pixels == NULL`
  - `img->w == img->h == img->pitchPixels == 0`
