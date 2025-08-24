# `image.c` — carga y muestreo de imágenes

Pequeño módulo para cargar una imagen con SDL_image y muestrearla en escala de grises (luminancia Rec.709), con soporte de muestreo puntual (nearest) y bilineal en coordenadas normalizadas.

## Dependencias

- SDL2_image para `IMG_Load` y `IMG_Save*` si se necesita.
- SDL2 para tipos y utilidades (`SDL_Surface`, formatos de pixel, etc.).

## Suposiciones de formato

- Al cargar, la imagen se convierte a `SDL_PIXELFORMAT_RGBA32` para tener un layout homogéneo.
- `img->pixels` apunta al buffer interno de `img->surface`.
- `img->pitchPixels == surface->pitch / 4` (4 bytes por pixel en RGBA8888).
- Las funciones internas que desempaquetan canales por shifts asumen layout 0xRRGGBBAA.
  Si necesitas independencia total del formato, usa `SDL_GetRGBA(px, surface->format, ...)`.

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

Carga una imagen desde disco usando SDL_image y la convierte a `RGBA32`.

**Flujo:**

1. `IMG_Load(path)`
2. `SDL_ConvertSurfaceFormat(..., SDL_PIXELFORMAT_RGBA32, 0)`
3. Rellena `img` con `surface`, `pixels`, `w`, `h`, `pitchPixels`.

**Retorna**
`true` si la carga + conversión fue exitosa, `false` en caso de error (se loguea en `stderr`).

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

Desempaqueta un pixel de 32 bits a canales de 8 bits con corrimientos de bits.

**Importante:**

Asume layout 0xRRGGBBAA. Para total portabilidad, preferir `SDL_GetRGBA`.

### `float sampleIntensity(const Image *img, int x, int y)`

Muestreo puntual (nearest) en coordenadas de pixel. Retorna luminancia Rec.709 en `[0,1]`.

**Comportamiento:**

- Si `img`/`pixels` es `NULL` o `(x,y)` está fuera de rango -> retorna `0.0f`.
- Lee el pixel, desempaqueta `R,G,B` e integra:

  ```bash
  Y = 0.2126*R + 0.7152*G + 0.0722*B   (con R,G,B normalizados a [0..1])
  ```

**Uso:**

- Rápido y suficiente cuando no se requiere filtrado.

### `static inline float lumaFromPixel(Uint32 px)` *(interno)*

Luminancia Rec.709 en `[0,1]` a partir de un `Uint32` empaquetado.

**Notas**
Mismos supuestos de layout que `rgbaToUint8`.

### `float sampleIntensityBilinearUV(const Image *img, float u, float v)`

Muestreo bilineal en coordenadas normalizadas `u,v` en `[0,1]`.

**Flujo:**

1. Clampea `u` y `v` a `[0,1]`.
2. Convierte a coords flotantes `(x,y)` en `[0..w-1] x [0..h-1]`.
3. Toma `p00, p10, p01, p11` y hace interpolación:

   - Interpola en X en la fila superior e inferior.
   - Interpola en Y entre los dos resultados.

**Bordes:**

Evita salir de rango forzando `x1 == x0` o `y1 == y0` cuando corresponde.

**Uso:**

- Útil para muestrear la imagen al tamaño del canvas o cuando se necesita suavizado.

## Errores y retorno

- `imageLoad` retorna `false` y loguea el motivo si falla `IMG_Load` o la conversión de formato.
- Los muestreos retornan `0.0f` si los parámetros o la imagen no son válidos.

## Rendimiento

- `sampleIntensity` hace 1 lectura de pixel.
- `sampleIntensityBilinearUV` hace 4 lecturas y algunas operaciones de mezcla.
- Para Lloyd con canvas grande, bilinear mejora calidad perceptual a cambio de un costo mínimo.

## Ejemplos de uso

```c
Image img;
if (!imageLoad(&img, "images/input/twitch.png")) {
  // manejar error
}

// nearest en pixel (100, 50)
float y0 = sampleIntensity(&img, 100, 50);

// bilinear en UV
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
