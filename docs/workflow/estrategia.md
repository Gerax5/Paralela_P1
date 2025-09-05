# Estrategia de paralelización

## 1. Siembra inicial de puntos (paralelo por datos)

**Qué**: paralelizamos la inicialización de la nube de puntos `N` (cada iteración escribe en `pts[i]`, sin dependencias).

**Dónde**: `stippling_parallel.c`

- **Bucle paralelo**: `#pragma omp parallel for schedule(static)` sobre `i = 0..n-1`.

  - **Técnica**: *parallel for* con *schedule(static)* (trabajo uniforme); no se requieren atómicos/barreras porque cada hilo escribe un índice distinto.

## 2. Construcción de grilla uniforme (CSR) para NN — paralelizada + atomics + SIMD

**Qué**: construir el índice (CSR) para acelerar la búsqueda de vecino más cercano.

**Dónde**: `voronoi_parallel.c`

1. **Conteo por celda** (evitar colisiones con `atomic update`)

   - Bucle paralelo sobre puntos (calcula celda y **suma 1** con atómico).
   - Líneas: `#pragma omp parallel for` + `#pragma omp atomic update`.

2. **Reinicialización en paralelo** (reusar `cellCount` como contadores)

   - Líneas: `#pragma omp parallel for`.

3. **Dispersion de índices en rangos CSR** (reserva *slot* con `atomic capture`)

   - Cada hilo obtiene un **índice único** dentro del rango de su celda y escribe el id de punto.
   - Líneas: `#pragma omp parallel for` + `#pragma omp atomic capture`.

4. **Nearest neighbor vectorizado** (SIMD dentro de una celda)

   - Se usa `#pragma omp simd` para recorrer candidatos contiguos en memoria (**no** crea hilos nuevos; vectoriza registros en un hilo).
   - Líneas: `#pragma omp simd` y la reducción de mínimo con índice.

**Técnicas usadas**: *parallel for* + **atomic update/capture** (consistencia sin *critical*), **SIMD** (vectorización), *schedule(static)* (trabajo uniforme por punto/celda).

**Sincronía**: las regiones `parallel for` tienen barrera implícita al final; los atómicos garantizan coherencia en los contadores.

## 3. Iteración de Lloyd (paralela con acumuladores por hilo + reducción T$\times$N->N)

**Qué**: barrido del canvas (2D), acumulando centroide ponderado para el **punto Voronoi ganador**.

**Dónde**: `lloyd_parallel.c` (versión paralela pura)

1. **Buffers locales por hilo** (evitar *false sharing* y atómicos por píxel)

   - Se reserva un bloque **T$\times$N** y cada hilo usa su *slice* `tid*n .. (tid+1)*n`.
   - Líneas: reserva de *locales* y chequeos.

2. **Región paralela + bucle 2D colapsado**

   - *Thread-private* para `sx/sy/sw` (punteros que apuntan al *slice* del hilo).
   - `#pragma omp parallel` + `#pragma omp for collapse(2) schedule(static)` para recorrer `(y,x)` con stride `step`.
   - Líneas: *parallel region*, *scoping por hilo* y *for collapse(2)*.
   - La parte de **muestreo** y **NN** ocurre dentro del bucle 2D; la **acumulación** va solo al *slice* local del hilo (no hay *atomic*).

3. **Reducción T$\times$N -> N** (serial, fuera de la región)

   - Un único hilo combina los *slices* (sumas) en `sumX/sumY/sumW`.
   - Líneas: lazo externo en `t` y lazo interno en `i`.

4. **Actualizar centroides**

   - Divide (sum / peso) por punto; solo para aquellos con `sumW>0`.
   - Líneas: actualización de `pts[i]`.

5. **Re-seed de huérfanos** (determinista; no paralelo)

   - Si `sumW[i]==0`, se prueba aleatorio en zonas con peso > 0.
   - Líneas: *fallback* de re-siembra controlada.

**Técnicas usadas**: *parallel region* + *for collapse(2)* + *schedule(static)*, **buffers privados por hilo** (evita atómicos/critical), **reducción manual**.

**Sincronía**: barrera **implícita** al salir de la región paralela; **no** hay `atomic/critical` en la acumulación por pixel.

**Scoping**: `sx/sy/sw` son **privados** por estar definidos dentro de la región (`tid` privado); el arreglo global `sumX/Y/W` solo se toca en la **reducción**.

## 4. Resumen — Mapa "técnica -> archivo -> líneas"

| Técnica                                                            | Archivo                | Líneas |
| ------------------------------------------------------------------ | ---------------------- | ------ |
| Siembra paralela de `N` puntos (`parallel for`)                    | `stippling_parallel.c` |        |
| CSR: conteo por celda (`parallel for` + `atomic update`)           | `voronoi_parallel.c`   |        |
| CSR: reinicio contadores (`parallel for`)                          | `voronoi_parallel.c`   |        |
| CSR: dispersión a `cellPoints` (`parallel for` + `atomic capture`) | `voronoi_parallel.c`   |        |
| NN por celda: vectorización                                        | `voronoi_parallel.c`   |        |
| Lloyd: buffers por hilo (T$\times$N)                                      | `lloyd_parallel.c`     |        |
| Lloyd: región paralela + `for collapse(2)`                         | `lloyd_parallel.c`     |        |
| Lloyd: acumulación local sin atómicos                              | `lloyd_parallel.c`     |        |
| Lloyd: reducción T$\times$N->N (serial)                                    | `lloyd_parallel.c`     |        |
| Lloyd: actualizar centroides                                       | `lloyd_parallel.c`     |        |
| Lloyd: re-seed huérfanos                                           | `lloyd_parallel.c`     |        |

## 5. Qué NO usamos y por qué

- **`critical`/`atomic` en el barrido de píxeles**: evitados a propósito para no crear contención; en su lugar, **acumuladores por hilo** (T$\times$N) + **reducción** posterior. (Véase disposición por *slices* en T$\times$N).
- **Barrera explícita**: no es necesaria; la **salida** de la región `parallel` ya hace de sincronía antes de la reducción.
- **`reduction(...)` directo sobre arreglos**: no se usa porque la reducción estándar de OpenMP no reduce **arreglos largos** sin *declare reduction* personalizado; la reducción manual en C es clara, portable y evita overhead.
