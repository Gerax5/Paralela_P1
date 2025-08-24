# `stippling.c` — documentación del módulo

## Propósito

Mantener y dibujar la nube de puntos del efecto de *stippling*. Este módulo no conoce nada de Lloyd ni de la imagen fuente; solo administra memoria de los puntos y los dibuja en pantalla.

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

Invariantes:

- `pts` es un arreglo de `count` elementos o `NULL` si el estado está vacío.
- Las coordenadas se expresan en espacio de ventana `[0..width) x [0..height)`.

## API pública

### `bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);`

Inicializa el estado con `n` puntos distribuidos aleatoriamente en `[0..w) x [0..h)`.

Parámetros:

- `s` -> estado a inicializar (no nulo).
- `n` -> número de puntos, debe ser `> 0`.
- `w, h` -> dimensiones lógicas del canvas, ambos `> 0`.
- `seed` -> semilla para el generador (si `seed == 0` se usa un valor por defecto).

Retorno:

- `true` si se asignó memoria y se generó la nube.
- `false` si hubo parámetros inválidos o fallo de `malloc`.

Notas:

- Complejidad O(n).
- Si falla, deja `s` en estado no inicializado.

---

### `void stipplingFree(Stippling *s);`

Libera la memoria del arreglo de puntos y resetea el estado.

Parámetros:

- `s` -> estado a limpiar. Puede ser `NULL`.

Notas:

- Idempotente: se puede llamar múltiples veces sin provocar error.
- No libera la propia estructura `Stippling`, solo su contenido.

---

### `void stipplingRender(const Stippling *s, SDL_Renderer *ren, int radius);`

Dibuja todos los puntos como discos sólidos en el `SDL_Renderer`.

Parámetros:

- `s` -> estado con los puntos a dibujar.
- `ren` -> renderer válido.
- `radius` -> radio visual en píxeles. Si `< 1`, se fuerza a `1`.

Comportamiento:

- Color fijo `RGBA(250,250,250,255)` en esta implementación.
- Para cada punto, redondea `(x,y)` y traza un círculo lleno por scanlines.

Complejidad:

- Aproximadamente O(N \* r), r es el radio.

Notas:

- No hace `SDL_RenderPresent` ni limpia el fondo.
- Si se requiere otro color/estilo, ajustar antes el `SDL_SetRenderDrawColor` o exponer color como parámetro.

## Helpers internos (privados)

### `static float frand01(unsigned *st);`

Generador LCG simple. Devuelve un flotante en `[0,1)` y actualiza la semilla en sitio. Se usa para inicializar posiciones.

### `static void drawFilledCircle(SDL_Renderer *r, int cx, int cy, int radius);`

Dibuja un círculo sólido centrado en `(cx, cy)` mediante líneas horizontales. Evita el coste de llamar píxel a píxel. Complejidad O(r).

## Sistema de coordenadas y dibujo

- Origen en la esquina superior izquierda.
- Eje X hacia la derecha, eje Y hacia abajo.
- Las posiciones de `Dot` son flotantes; el render redondea a entero.

## Integración

- `stipplingInit` se invoca durante `appInit`.
- `stipplingRender` se usa en el bucle principal, después del fondo y antes de `SDL_RenderPresent`.
- `stipplingFree` se llama en `appShutdown`.

## Consideraciones de rendimiento

- Con radios grandes o N alto, `drawFilledCircle` puede dominar el tiempo. Posibles mejoras:

  - Cambiar a texturas de punto (sprite) y dibujar con `SDL_RenderCopy` para batches.
  - Reducir el radio durante la fase de convergencia y aumentarlo al final.
  - Agrupar por tiles y recortar cuando el disco queda fuera de pantalla.

## Seguridad y errores

- Las funciones validan punteros básicos; si `s == NULL` o `s->pts == NULL`, el render sale temprano.
- `stipplingInit` retorna `false` si `malloc` falla.
- No hay sincronización; este módulo no es thread-safe por sí mismo. Dibujar siempre desde el hilo del renderer.
