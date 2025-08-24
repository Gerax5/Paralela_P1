# `stippling.c` — documentación del módulo

## Propósito

Mantener la nube de puntos del efecto *stippling* y dibujarla con un tamaño por-punto derivado del **brillo** de una imagen de referencia. El módulo administra memoria de puntos e implementa un render estilado (con radio variable y color opcional).

## Estructuras

```c
typedef struct {
  float x, y;
} Dot;

typedef struct {
  Dot *pts;     // arreglo de N puntos
  int  count;   // N
  int  width;   // ancho del canvas lógico
  int  height;  // alto del canvas lógico
} Stippling;
```

**Invariantes:**

- `pts` es un arreglo de `count` elementos o `NULL` si el estado está vacío.
- Las coordenadas están en espacio de ventana `[0..width) x [0..height)`.

## API pública

### `bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);`

Inicializa el estado con `n` puntos distribuidos aleatoriamente en `[0..w) x [0..h)`.

**Parámetros:**

- `s` → estado a inicializar (no nulo).
- `n` → número de puntos (`> 0`).
- `w, h` → dimensiones del canvas (`> 0`).
- `seed` → semilla LCG (si `0`, se usa una por defecto).

**Retorno:**

- `true` si se asignó memoria y se generó la nube.
- `false` si hay parámetros inválidos o falla `malloc`.

**Notas:**

- Complejidad O(n).
- En fallo, `s` queda sin inicializar (no se escriben campos).

### `void stipplingFree(Stippling *s);`

Libera la memoria del arreglo de puntos y resetea el estado.

**Parámetros:**

- `s` → estado a limpiar. Puede ser `NULL`.

**Notas:**

- Idempotente: segura frente a múltiples llamadas.
- No libera la estructura `Stippling` en sí, solo su contenido.

### `void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH, const Image *img, float minR, float maxR, bool useColor, bool invertTheme);`

Dibuja la nube con **radio por-punto** mapeado desde la **luminancia** de la imagen, y opcionalmente colorea cada punto con el **color bilineal** de la imagen.

**Parámetros:**

- `s` → puntos a dibujar (`s` y `s->pts` válidos).
- `ren` → `SDL_Renderer` (hilo principal).
- `canvasW, canvasH` → dimensiones del canvas donde viven los puntos.
- `img` → imagen de referencia; si es válida se usa para brillo y (si se pide) color.
- `minR` → radio mínimo. Se clampa internamente a `≥ 0.5`.
- `maxR` → radio máximo. Si `< minR`, se ajusta a `minR`.
- `useColor` → `true`: el color del punto se toma de la imagen; `false`: color base.
- `invertTheme` → `true`: color base negro; `false`: blanco (solo cuando `useColor == false`).

**Comportamiento:**

- Para cada punto `(x,y)`:

  - UV del canvas a la imagen: `u=(x+0.5)/canvasW`, `v=(y+0.5)/canvasH`.
  - Luminancia bilineal **en lineal** (Rec.709) con `sampleIntensityBilinearUV`.
  - Radio: `r = lerp(minR, maxR, 1 - lum)` → claro ⇒ pequeño; oscuro ⇒ grande.
  - Color:

    - `useColor == false` → usa color base (tema).
    - `useColor == true` → `sampleRgbBilinearUV` y alfa 230 para una leve suavización.
  - El disco se rellena por **scanlines** (`SDL_RenderDrawLine`).

**Consideraciones:**

- Si `img` es `NULL` o inválida, `lum = 0` ⇒ todos los radios \~`maxR` (tema base aplica).
- Complejidad de dibujo ≈ `O(Σ_i r_i)` (unas `2*r + 1` líneas por punto).
- Cambia el color del renderer varias veces; no modifica blending mode.
- No llama a `SDL_RenderPresent` ni limpia el fondo.

## Helpers internos (privados)

### `static float frand01(unsigned *st);`

Generador LCG simple. Devuelve un flotante en `[0,1]` y actualiza la semilla in-place. Se usa para inicializar posiciones.

## Integración

- `stipplingInit` se invoca en `appInit`.
- `stipplingRenderStyled` se usa en el bucle principal (`appRun`) después de dibujar el fondo.
- `stipplingFree` se llama en `appShutdown`.

## Seguridad y errores

- Validación básica de punteros: si `s == NULL` o `s->pts == NULL`, el render sale temprano.
- `stipplingInit` retorna `false` si `malloc` falla.
- No hay sincronización; no es thread-safe. Dibujar siempre desde el **hilo del renderer**.

## Ejemplo de uso

```c
// Init
Stippling s;
if (!stipplingInit(&s, 2000, winW, winH, 42u)) { /* manejar error */ }

// Draw dentro del frame:
stipplingRenderStyled(&s, ren, winW, winH,
                      &img,         // imagen de referencia
                      0.8f, 3.0f,   // minR, maxR
                      true,         // useColor
                      false);       // invertTheme

// Shutdown
stipplingFree(&s);
```
