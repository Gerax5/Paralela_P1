# `lloyd.c` — Iteración de Lloyd ponderada por imagen

Este módulo ejecuta **una iteración** del algoritmo de Lloyd (Centroidal Voronoi) para *Voronoi Stippling* usando la imagen como campo de densidad. Cada punto se desplaza al **centro de masa** de su región, ponderando por luminancia.

## Firma

```c
bool lloydStep(const Image *img,
               Stippling *s,
               int W, int H,
               int step,
               float gamma);
```

- `img`  → imagen fuente (RGBA32). La luminancia se muestrea en UV con **bilineal** (en lineal, ver `image.c`).
- `s`    → nube de puntos a modificar in-place.
- `W,H`  → dimensiones del canvas (en píxeles).
- `step` → stride de muestreo del canvas (≥ 1). Mayor = más rápido, menos fino.
- `gamma`→ exponente del peso (ver más abajo).

**Retorna** `true` si se ejecutó la iteración; `false` ante parámetros inválidos o fallo de memoria temporal.

## Modo de ponderación (compile-time)

La función soporta dos esquemas, seleccionables en compilación:

```c
// 1 = por brillo (modo “debug/nativo”), 0 = por oscuridad (modo clásico)
#ifndef STIPPLE_WEIGHT_BY_BRIGHTNESS
#define STIPPLE_WEIGHT_BY_BRIGHTNESS 1
#endif
```

- **Brillo (nativo/debug):** `w = (lum)^gamma`
  Favorece zonas **claras**.
- **Oscuridad (clásico):** `w = (1 - lum)^gamma`
  Favorece zonas **oscuras**.

`gamma > 1` enfatiza el modo elegido; `gamma < 1` lo atenúa.

> Nota: la luminancia `lum` proviene de `sampleIntensityBilinearUV`, que convierte sRGB→lineal y aplica Rec.709 antes de interpolar.

## Flujo interno

1. **Validación** de entradas.
2. **Grilla uniforme** (`UniformGrid`) para acelerar *nearest neighbor*; cell=32 px.
3. **Acumuladores** por punto: `sumX`, `sumY`, `sumW` en `double`.
4. **Barrido** del canvas cada `step`:

   - Convertir `(x,y)` → `(u,v)` centrado en el píxel.
   - Muestrear `lum` con **bilineal**.
   - Calcular `w` según macro (brillo u oscuridad) y `gamma`.
   - Buscar **punto más cercano** con la grilla (`ringMax=2`); si no hay candidatos, fallback O(N).
   - Acumular `(x*w, y*w, w)` en el punto ganador.
5. **Actualizar** cada punto con su centroide si `sumW[i] > 0`.
6. **Reseed** de huérfanos (`sumW == 0`): elegir hasta 64 posiciones aleatorias con `w > 1e-6`.
7. **Liberar** buffers y grilla.

## Complejidad

```bash
O( (W/step * H/step) * k )
```

donde `k` es la cantidad de puntos inspeccionados por la grilla (pequeña con `ringMax` bajo).
`step` reduce linealmente el coste del barrido.

## Detalles numéricos

- Acumulación en `double`; posiciones de puntos en `float`.
- Muestreo de luminancia en **lineal** (sRGB→lineal + Rec.709) dentro de `image.c`.
- La grilla se reconstruye en cada iteración (los puntos se mueven).

## Reseed de huérfanos

Para puntos sin asignaciones (`sumW==0`):

- Semilla reproducible: `(n ^ W ^ H) + 0x9E3779B9`.
- Hasta 64 intentos; se acepta el primer `(rx,ry)` con `w > 1e-6`.

## Consideraciones prácticas

- Iteraciones tempranas: `step = 3..4` acelera; al final, refinar con `step = 1`.
- `gamma` típico: `1.0..1.6` (ajustar según el modo de peso deseado).
- Si compilas con `STIPPLE_WEIGHT_BY_BRIGHTNESS=1`, el resultado tenderá a **zonas claras**; con `0`, a **zonas oscuras**.

## Interacción con otros módulos

- `image.c` → `sampleIntensityBilinearUV` (bilineal en UV, lineal/Rec.709).
- `voronoi.c` → `gridBuild`, `gridNearest`, `gridFree`.
- `stippling.c` → estructura y almacenamiento de puntos.
