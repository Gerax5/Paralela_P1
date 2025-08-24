# `lloyd.c` – Documentación técnica

Este módulo implementa **una iteración** del algoritmo de Lloyd para *stippling* ponderado por imagen. La idea: cada punto se mueve hacia el **centro de masa** de los píxeles que le “pertenecen” (su celda de Voronoi), usando como peso la oscuridad de la imagen.

## API pública

```c
bool lloydStep(const Image *img,
               Stippling *s,
               int W, int H,
               int step,
               float gamma);
```

- `img` -> campo de densidad (imagen en RGBA32) desde el cual se deriva el peso.
- `s`   -> nube de puntos a actualizar in-place.
- `W,H` -> dimensiones del canvas sobre el que viven los puntos.
- `step` -> stride de muestreo del canvas (>= 1). Un `step` mayor acelera a costa de precisión.
- `gamma` -> controla el peso de la oscuridad: `w = (1 - luminancia)^gamma`.

  - `gamma = 1.0` lineal
  - `gamma > 1` enfatiza zonas oscuras (más peso)
  - `gamma < 1` las atenúa

**Retorna**: `true` si la iteración se ejecuta (aunque ningún punto cambie), `false` ante parámetros inválidos o fallo de memoria.

## Resumen del flujo

1. **Validación** de entradas básicas.
2. **Índice espacial**: se construye una grilla uniforme (`voronoi.c`) para acelerar el “punto más cercano”.
3. **Buffers de acumulación** por punto:

   - `sumX[i]`, `sumY[i]` -> suma ponderada de coordenadas.
   - `sumW[i]` -> suma de pesos.
4. **Barrido del canvas** con paso `step`:

   - Convertir `(x,y)` a **UV** normalizados y muestrear luminancia **bilineal** (`image.c`).
   - Calcular `w = (1 - lum)^gamma`. Si `w <= 0`, saltar.
   - Hallar el **punto más cercano** con la grilla (y un fallback O(N) por si no hay candidatos).
   - Acumular en el punto ganador: `sumX`, `sumY`, `sumW`.
5. **Actualización**: para cada punto con `sumW[i] > 0`, colocar:

   ```bash
   x_i = sumX[i] / sumW[i]
   y_i = sumY[i] / sumW[i]
   ```

   Si `sumW[i] == 0`, el punto queda donde estaba.
6. **Limpieza**: liberar buffers y grilla.

## Muestreo de imagen

- El muestreo se hace en **UV** normalizados (centro de píxel: `+0.5`) para desacoplar la resolución del canvas (`W x H`) de la de la imagen.
- La luminancia se obtiene con **bilineal** para reducir aliasing.
- El peso es **oscuridad** elevada a `gamma`: más oscuro -> mayor contribución.

## Índice espacial: grilla uniforme

- Se usa una **grilla de celdas de 32 px** para bucketear puntos y consultar vecinos cercanos.
- La búsqueda se hace por **anillos** alrededor de la celda de `(x,y)`; con `ringMax = 2` suele bastar.
- Si no hay candidatos en la grilla, se aplica un **fallback O(N)** puntual.

**Tuning**:

- El tamaño de celda (`32`) y `ringMax` son parámetros de compromiso entre costo y calidad.
- Si `N` es muy grande, conviene revisar estos parámetros.

## Complejidad

Aproximada:

```bash
O( (W/step * H/step) * k )
```

donde `k` es la cantidad de candidatos visitados por la grilla (constante pequeña con `ringMax` bajo).
El **stride** `step` reduce proporcionalmente el costo del barrido.

## Elecciones numéricas

- Acumulación en `double` (más estable).
- Coordenadas de puntos en `float` (suficiente para pantalla).
- Si `sumW[i] == 0` no se mueve el punto (no recibió peso).

## Fallos y retornos

- Retorna `false` si:

  - `img` o `s` inválidos.
  - `W` o `H` no positivos.
  - Fallo al reservar `sumX/sumY/sumW`.
- Retorna `true` en ejecución normal, incluso si no se movieron puntos (p. ej., `gamma` pequeño y/o imagen clara).

## Interacción con otros módulos

- **`image.c`**: `sampleIntensityBilinearUV` para luminancia.
- **`voronoi.c`**: `gridBuild`, `gridNearest`, `gridFree` para consultas rápidas de vecino.
- **`stippling.c`**: estructura de puntos (`s->pts[i].x/y`) que se actualiza in-place.

## Consejos de uso y tuning

- Empezar con `step = 3..4` para iteraciones rápidas y luego bajar a `step = 1` para refinar.
- `gamma` mayor a 1 coloca más puntos en sombras; experimentar con `1.2..1.8`.
- Para imágenes muy pequeñas, el bilineal ayuda; para muy grandes, considerar subir `step` en etapas tempranas.

## Notas para la versión OpenMP

- Paralelizar el bucle de **barrido** por tiles o filas.
- Usar **buffers privados por hilo** (`sumX/Y/W` locales) y combinar al final, o `reduction` por índice si fuera viable.
- Mantener la construcción de grilla en un solo hilo o paralelizarla por buckets si es necesario.
- Evitar *false sharing* al combinar sumas.

## Pseudocódigo compacto

```c
// build grid
gridBuild(&g, W, H, 32, s);

// zero sums
sumX/Y/W = zeros(n)

for y in 0..H-1 step step:
  v = (y + 0.5) / H
  for x in 0..W-1 step step:
    u = (x + 0.5) / W
    lum = sampleIntensityBilinearUV(img, u, v)
    w   = pow(max(0, 1 - lum), gamma)
    if w == 0: continue

    best = gridNearest(&g, s, x, y, 2)
    if best < 0: best = argmin_i ||p_i - (x,y)||^2

    sumX[best] += x * w
    sumY[best] += y * w
    sumW[best] += w

for i in 0..n-1:
  if sumW[i] > 0:
    s->pts[i] = (sumX[i]/sumW[i], sumY[i]/sumW[i])

gridFree(&g)
```

## Comprobación rápida

- Con fondo visible, presionar SPACE varias veces debe “acomodar” los puntos hacia las zonas oscuras.
- Ajustar `gamma` en caliente:

  - `G` sube (más puntos en sombras).
  - `H` baja (distribución más uniforme).
- Aumentar `step` acelera la iteración pero hace la relajación más tosca. Bajarlo al final mejora el resultado.
