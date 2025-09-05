# `lloyd.c` — Documentación técnica

## Rol del módulo

Ejecuta **una iteración** del algoritmo de Lloyd (Centroidal Voronoi) para *Voronoi Stippling*: mueve cada punto al **centro de masa ponderado por la imagen** (luminancia). La API pública expuesta en `lloyd.h` es `lloydStep(...)`.

**Modo de peso (compile-time):**

- `STIPPLE_WEIGHT_BY_BRIGHTNESS=1` -> `w = (lum)^gamma` (favorece zonas **claras**).
- `STIPPLE_WEIGHT_BY_BRIGHTNESS=0` -> `w = (1 - lum)^gamma` (favorece **oscuras**).&#x20;

La luminancia proviene de muestreo **bilineal** en UV con conversión sRGB->lineal usando Rec.709.&#x20;

## Convenciones del módulo

- Coordenadas en píxel (origen arriba-izquierda), rangos válidos `x∈[0,w)`, `y∈[0,h)`.&#x20;
- `pixelStride >= 1` controla granularidad/costo de muestreo; mayor = más rápido/menos preciso.&#x20;
- La implementación **modifica** `Stippling` *in-place* y por sí sola no es thread-safe; la versión paralela usa OpenMP con acumuladores por hilo y reducción.

## Flujo (secuencial)

1. **Validación** de entradas (`img`, `s`, dimensiones, `step`).&#x20;
2. **Construcción de grilla uniforme** (`UniformGrid`, `cell=32`) para acelerar *nearest neighbor*.&#x20;
3. **Acumuladores** por punto `sumX/sumY/sumW` en `double`.&#x20;
4. **Barrido** del lienzo cada `step`:

   - `(x,y)->(u,v)` centrado del píxel, muestrear `lum` (bilineal).
   - Calcular `w` con el modo activo (`lum^gamma` o `(1-lum)^gamma`).
   - NN con grilla; si falla, **fallback O(N)**.
   - Acumular `(x*w, y*w, w)` en el punto ganador.&#x20;
5. **Actualizar** puntos con su centroide si `sumW>0`.&#x20;
6. **Reseed de huérfanos** (`sumW==0`): hasta 64 intentos en zonas con `w>1e-6`.&#x20;

**Complejidad aprox.**
`O((W/step * H/step) * k)`; `k` = puntos inspeccionados por la grilla.&#x20;

## Flujo (paralelo con OpenMP)

La API es la **misma** (`lloydStep`); la implementación usa:

- **Buffers locales por hilo** `sumXlocal/sumYlocal/sumWlocal` (tamaño `T×N`) para evitar carreras.
- Un `#pragma omp for collapse(2)` sobre el doble bucle `y/x`.
- **Reducción manual**: sumar locales -> globales.

Búsqueda NN con grilla (`ringMax=2`), con fallback O(N) si no hay candidatos, igual que en la versión secuencial.&#x20;

Luego:

- **Actualización** de puntos para `sumW>0`.
- **Reseed** determinista de huérfanos (semilla derivada de `n^W^H + 0x9E3779B9`, hasta 64 intentos).
- **Liberación** de buffers y grilla.

## Mecanismos de sincronía

- Paralelismo de *loop-level* (OpenMP).
- Evita *atomics* en el inner loop: **acumulación privada** por hilo y **reducción** posterior -> sin contención.&#x20;

## Programación defensiva / robustez

- Chequeos de punteros, dimensiones y `step`.
- Fallback a O(N) si la grilla no devuelve candidato.
- Re-sembrado de huérfanos para evitar colapsos de celdas vacías.

## Despliegue de resultados

Este módulo **no renderiza**; solo actualiza `s->pts[i].(x,y)`. El render ocurre en `stipplingRenderStyled(...)` desde `stippling.c`.&#x20;

### `bool lloydStep(const Image *img, Stippling *s, int w, int h, int pixelStride, float gamma);`

- **Entradas**

  - `img` (`const Image*`): fuente de luminancia Rec.709 en `[0..1]` (UV bilineal, sRGB->lineal).
  - `s` (`Stippling*`): nube de puntos (modificada in-place).
  - `w,h` (`int`): dimensiones del lienzo (px).
  - `pixelStride` (`int`): salto de muestreo (`k>=1`).
  - `gamma` (`float`): exponente del peso (`lum^gamma` o `(1-lum)^gamma` según macro).&#x20;
- **Salidas**

  - `bool`: `true` si la iteración completó; `false` si entradas inválidas o error.&#x20;
- **Descripción (funcionamiento)**
  Muestrea el lienzo cada `pixelStride`, asigna cada muestra a su punto más cercano (grilla uniforme + fallback), acumula centroide ponderado por `w` y actualiza cada punto a su centro de masa. Maneja puntos **huérfanos** mediante re-sembrado probabilístico en zonas con peso. Coste aprox. `O((W/stride * H/stride) * k)`.

**Pre/Postcondiciones resumidas (del header):**
`img!=NULL`, `s!=NULL`, `s->count>0`, `w,h>0`, `pixelStride>=1`; al finalizar, los puntos se reubican hacia sus centroides ponderados.&#x20;

## Notas de implementación

- RNG ligero (LCG) + `irand_range` para re-seed; determinista a igualdad de semilla. *(Detalles en `lloyd.c`/`lloyd_parallel.c`)*.
