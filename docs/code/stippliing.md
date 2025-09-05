# `stippling.c` — Documentación técnica

## Rol del módulo

Gestiona la **nube de puntos** del puntillismo: creación inicial (siembra pseudoaleatoria en el lienzo) y **render estilizado** (radio por brillo e, opcionalmente, color muestreado de la imagen). La siembra y el render están pensados para alimentar el **algoritmo de Lloyd** y mostrar el resultado en **SDL2**.

## Flujo (resumen)

1. **Siembra inicial** (`stipplingInit`): reserva memoria y ubica `n` puntos uniformes en `[0..W)×[0..H)`.
   - *Secuencial:* LCG 32-bit;
   - *Paralelo:* *xorshift32* con mezcla determinista por índice/hilo.
2. **Render** (`stipplingRenderStyled`): por cada punto, calcula UV -> muestrea **luminancia** y (si se pide) **color** de la imagen, calcula radio `r∈[minR,maxR]` y rasteriza un disco mediante scanlines. Debe ejecutarse en el **hilo principal** (SDL no es thread-safe para render).

## Convenciones

- **Rangos y contratos**

  - `n > 0`, `W > 0`, `H > 0`; en caso contrario, `stipplingInit` retorna `false`.
  - `minR` se clampa internamente a `>=0.5`; `maxR` se clampa a `>=minR`.
- **Determinismo**

  - Misma **semilla** + mismos **parámetros** ⇒ misma nube inicial.
  - En OMP, cada punto usa estado RNG **independiente** (mezcla de `seed` + `i`), evitando corridas no deterministas.
- **Paralelismo y sincronía**

  - *Init (OMP):* `#pragma omp parallel for schedule(static)`; no hay reducciones ni locks (cada hilo escribe índices disjuntos).
  - *Render:* llámese **solo** desde el hilo principal (SDL).
- **Errores/defensiva**

  - Si `malloc` falla, no se altera el estado previo; `stipplingFree` es **idempotente** (segura ante múltiples llamadas).

## Estructuras expuestas (resumen)

> Las definiciones completas viven en `stippling.h`. A partir de las *units* de implementación:
>
> - `Dot { float x, y; }` (posición)
> - `Stippling { Dot *pts; int count, width, height; }` (conjunto y metadatos)

## `bool stipplingInit(Stippling *s, int n, int w, int h, unsigned seed);`

**Entradas:**

- `s` (`Stippling*`): salida a inicializar (no `NULL`).
- `n` (`int`): cantidad de puntos (`>0`).
- `w, h` (`int`): dimensiones del canvas (`>0`).
- `seed` (`unsigned`): semilla RNG (si `0`, la impl. usa un default interno).

**Salidas:**

- `bool`: `true` si reserva e inicializa; `false` si parámetros inválidos o `malloc` falla.

**Descripción:**

- Reserva `n` puntos y los ubica **uniformemente** en el rectángulo `[0..w)×[0..h)`.
- **Secuencial:** usa **LCG** (Numerical Recipes: `state = state*1664525 + 1013904223`) y normaliza con 24 bits altos.
- **Paralelo (OMP):** `#pragma omp parallel for`, RNG **xorshift32** por punto con semilla mezclada por índice (`0x9E3779B9 ^ seed ^ i*0x85EBCA6B`). Sin sincronización.

## `void stipplingFree(Stippling *s);`

**Entradas:**

- `s` (`Stippling*`): estructura a limpiar (puede ser `NULL`).

**Salidas:**

**Descripción:**

- Libera `s->pts` (si existe) y pone `pts=NULL`, `count=width=height=0`.
- **Idempotente** y segura ante `NULL`.

## `void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int canvasW, int canvasH, const Image *img, float minR, float maxR, bool useColor, bool invertTheme);`

**Entradas:**

- `s` (`const Stippling*`): nube de puntos (no `NULL`, `s->pts` válido).
- `ren` (`SDL_Renderer*`): destino (hilo principal).
- `canvasW, canvasH` (`int`): dimensiones del canvas (>=1).
- `img` (`const Image*`): imagen de referencia (opcional).
- `minR, maxR` (`float`): radios mínimo/máximo (clamp interno).
- `useColor` (`bool`): si `true`, pinta con color de la imagen; si `false`, usa color base tema.
- `invertTheme` (`bool`): tema claro/oscuro para color base.

**Salidas:**

**Descripción:**

- Convierte `(x,y)` de cada punto a `uv` normalizado; muestrea **luminancia** (y **RGB** si `useColor`) por bilineal; calcula `radius = lerp(minR,maxR, 1 - lum)`; rasteriza el disco con **scanlines** (`SDL_RenderDrawLine`).
- Si no hay imagen, `lum=0` -> `radius≈maxR`. El alfa baja ligeramente si `useColor` para suavizar.

## Diferencias clave: secuencial vs paralelo

| Aspecto      | Secuencial                                                  | Paralelo (OpenMP)                                                                 |
| ------------ | ----------------------------------------------------------- | --------------------------------------------------------------------------------- |
| RNG          | **LCG** 32-bit (`1664525`, `1013904223`), normaliza 24 bits | **xorshift32** por punto, semilla mezclada con índice; `#pragma omp parallel for` |
| Sincronía    | No aplica                                                   | No se requiere (índices disjuntos)                                                |
| Determinismo | Sí (misma semilla -> misma nube)                             | Sí (mezcla determinista por `seed`+`i`)                                           |
