# `lloyd.c` — Iteración de Lloyd ponderada por imagen

Este módulo ejecuta **una iteración** del algoritmo de Lloyd para *Voronoi Stippling* usando la imagen como campo de densidad. Cada punto se mueve al **centro de masa** de su región Voronoi, ponderando por **oscuridad** de la imagen.

## API

```c
bool lloydStep(const Image *img,
               Stippling *s,
               int W, int H,
               int step,
               float gamma);
```

- `img`  -> imagen fuente (RGBA32). El muestreo de luminancia se hace en UV con **bilineal**.
- `s`    -> nube de puntos a actualizar in-place.
- `W,H`  -> dimensiones del canvas.
- `step` -> stride de muestreo del canvas (>= 1). Mayor = más rápido, menos fino.
- `gamma`-> peso de oscuridad: `w = dark^gamma`, con `dark = 1 - luminancia`.

**Retorna** `true` si la iteración se ejecuta; `false` ante parámetros inválidos o fallo de memoria.

## Flujo resumido

1. **Validación** de entradas.
2. **Índice espacial**: se construye una **grilla uniforme** para acelerar el “punto más cercano”.
3. **Acumuladores** por punto: `sumX`, `sumY`, `sumW` (en `double`).
4. **Barrido** del canvas cada `step` píxeles:

   - Convertir `(x,y)` a **UV** centrados en el píxel.
   - Muestrear luminancia con **bilineal** (usa la ruta sRGB->lineal definida en `image.c`).
   - Aplicar **remapeo de blancos** (ver más abajo) y peso `w = dark^gamma`.
   - Encontrar **nearest** con la grilla (fallback O(N) si no hay candidatos).
   - Acumular `(x*w, y*w, w)` en el punto ganador.
5. **Actualización** de puntos: si `sumW[i] > 0`, mover a `(sumX/sumW, sumY/sumW)`.
6. **Reseed de huérfanos**: puntos con `sumW == 0` se reubican aleatoriamente en píxeles con **peso > eps** (intenta hasta 64 veces).
7. **Limpieza** de buffers y grilla.

## Remapeo de luminancia (desaturar blancos)

Para evitar que **pequeñas texturas claras** sigan atrayendo puntos, se aplica un umbral y reescalado:

- `t = 0.08`  (umbral de blancos; probar 0.05..0.15)
- `c = 1.0 / (1.0 - t)` (reescala el rango útil a \[0,1])
- `dark = max(0, (1 - lum) - t) * c`
- `w = dark^gamma`

Efecto: las zonas muy claras aportan `w = 0`. El contraste del resto se estira para conservar rango dinámico.

## Reseed de puntos huérfanos

Después de actualizar centroides, cualquier punto con `sumW == 0` se **reubica** en un píxel **oscuro** elegido al azar:

- Semilla reproducible derivada de `(n ^ W ^ H) + 0x9E3779B9`.
- Hasta `64` intentos por punto: se acepta el primer `(rx,ry)` con `w > 1e-6`.
- Evita que queden puntos atrapados en regiones completamente blancas.

## Índice espacial (grilla uniforme)

- Tamaño de celda: `32` px (compromiso razonable; se puede tunear).
- Búsqueda por **anillos** alrededor de la celda de `(x,y)`; `ringMax = 2` suele bastar.
- Fallback O(N) solo si no hay candidatos en la grilla.

## Complejidad aproximada

```bash
O( (W/step * H/step) * k )
```

donde `k` es el número de candidatos visitados por la grilla (constante pequeña con `ringMax` bajo).
`step` reduce linealmente el coste del barrido.

## Detalles numéricos

- Acumulación en `double`, posiciones de puntos en `float`.
- Muestreo bilineal con **luminancia en lineal** (ver `image.c` y `srgbToLinear01`).
- `gamma` recomienda iniciar en `1.0..1.6` y ajustar con teclas `G/H`.

## Interacción con otros módulos

- `image.c`:

  - `sampleIntensityBilinearUV` con conversión sRGB->lineal dentro de `lumaFromPixel`.
- `voronoi.c`:

  - `gridBuild`, `gridNearest`, `gridFree` para *nearest neighbor* acelerado.
- `stippling.c`:

  - Arreglo de puntos `s->pts` que se actualiza in-place.

## Pseudocódigo (con remapeo y reseed)

```c
gridBuild(&g, W, H, 32, s);
sumX/Y/W = zeros(n);

for y = 0; y < H; y += step:
  v = (y + 0.5) / H
  for x = 0; x < W; x += step:
    u = (x + 0.5) / W
    lum  = sampleIntensityBilinearUV(img, u, v)  // luminancia en lineal
    dark = max(0, (1 - lum) - t) * (1/(1 - t))   // remapeo de blancos
    w    = pow(dark, gamma)
    if (w <= 0) continue

    best = gridNearest(&g, s, x, y, 2)
    if (best < 0): best = argmin_i ||p_i - (x,y)||^2

    sumX[best] += x * w
    sumY[best] += y * w
    sumW[best] += w

for i in 0..n-1:
  if (sumW[i] > 0):
    s->pts[i] = (sumX[i]/sumW[i], sumY[i]/sumW[i])

// reseed de huérfanos
seed = (n ^ W ^ H) + 0x9E3779B9
for i in 0..n-1:
  if (sumW[i] == 0):
    try 64 times:
      rx,ry random
      u = (rx + 0.5)/W; v = (ry + 0.5)/H
      lum = sampleIntensityBilinearUV(...)
      w   = pow(max(0, 1 - lum), gamma)
      if (w > 1e-6): s->pts[i] = (rx, ry); break

gridFree(&g);
```

## Sugerencias de uso

- Iteraciones tempranas: `step = 3..4` para acelerar; al final, bajar a `step = 1` para refinar.
- Ajustar `t` si aún ves “fugas” de puntos en blancos: 0.05..0.12 suele funcionar bien.
- `gamma` mayor desplaza más masa a sombras.

## Notas para la versión OpenMP

- Paralelizar el doble bucle `(y,x)` por tiles o filas.
- Acumulación: buffers privados por hilo y combinar al final (o esquema de *reduction* seguro por índice).
- Construcción de grilla en un hilo; si se paraleliza, hacerlo por buckets y luego enlazar.
- Cuidar *false sharing* en `sumX/sumY/sumW` si se opta por reducir manualmente.
