# `voronoi.c` — Documentación técnica

## Rol del módulo

Este módulo provee un **índice espacial 2D** liviano para acelerar el vecino más cercano (NN) durante una iteración de **Lloyd**. Divide el canvas en **celdas fijas** y, por celda, mantiene los índices de puntos. Hay **dos backends**:

1. **Secuencial (lista por celda)**: `head[]` y `next[]` (lista enlazada implícita). Flujo simple y cache-friendly para N y W×H moderados.
2. **Paralelo (CSR + OpenMP)**: `cellCount`, `cellOffset`, `cellPoints` con compactación **contigua por celda**; bucles `omp parallel for` y vectorización `omp simd` en el recorrido de candidatos. Ideal para **muchos puntos** y **muchas celdas**.

## Flujo interno

### A. Secuencial (listas por celda)

1. **Construcción (`gridBuild`)**

   - Calcula `cols`, `rows` y reserva `head[cols*rows] = -1` y `next[N]`.
   - Para cada punto *i*, obtiene `(cx,cy)`, calcula `hidx` y hace **push-front** en `head[hidx]` con `next[i]=head[hidx]`.
2. **Búsqueda (`gridNearest`)**

   - Toma la **celda base** de `(x,y)` y explora por **anillos** `r=0..ringMax`.
   - Solo recorre el **borde** del anillo; compara distancias **al cuadrado** y aplica **corte temprano** si ya hubo candidato con `r>=1`.
3. **Liberación (`gridFree`)**: suelta `head/next`, pone estado neutro.

### B. Paralelo (CSR)

1. **Construcción (`gridBuildCSR_omp` vía `gridBuild`)**

   - **Conteo por celda** en paralelo con `#pragma omp parallel for` + `omp atomic`.
   - **Prefix-sum** en `cellOffset` para obtener rangos por celda.
   - **Dispersión paralela** de los índices en `cellPoints` usando contador **atómico** por celda.
2. **Búsqueda (`gridNearestCSR`)**

   - Misma estrategia de **anillos**, pero cada celda se recorre como **rango contiguo** `[offset[h]..offset[h+1])`.
   - Bucle interno con `#pragma omp simd` para **vectorización** (CPU SIMD).
3. **Liberación (`gridFreeCSR` / `gridFree`)**: libera `cellCount/cellOffset/cellPoints`.

## Convenciones

- **Coordenadas de canvas**: flotantes `(x,y)` en `[0..W)×[0..H)`.
- **Celda**: tamaño entero `cell>0`.
- **Truncamiento/clamp**: `cx=(int)x/cell`, `cy=(int)y/cell`, con **clamp** a bordes.
- **Distancias**: se compara **d²** (sin `sqrt`) para rendimiento.
- **ringMax**: `0` = solo la celda base; `1` = vecinas inmediatas; etc.
- **Defensiva**: valida punteros, tamaños >0, y **estado neutro** tras `free`.

> Las firmas públicas están en `voronoi.h`. Aquí se documenta su comportamiento según backend.

## `bool gridBuild(UniformGrid *g, int W, int H, int cell, const Stippling *s);`

- **Entradas**

  - `g` (`UniformGrid*`): salida a inicializar.
  - `W,H` (`int`): dimensiones del canvas.
  - `cell` (`int`): tamaño de celda (>0).
  - `s` (`const Stippling*`): nube de puntos válida.
- **Salidas**: `true/false`.
- **Descripción**: Construye la grilla.

  - **Secuencial**: rellena `head`/`next` con listas por celda.
  - **Paralelo**: ruta CSR (`cellCount/Offset/Points`) con OpenMP.

## `int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int ringMax);`

- **Entradas**

  - `g` (`const UniformGrid*`): grilla construida (listas o CSR).
  - `s` (`const Stippling*`): puntos (se leen `pts[i].x/y`).
  - `x,y` (`float`): consulta.
  - `ringMax` (`int`): radio máximo de anillos.
- **Salidas**: `índice` del punto más cercano o `-1`.
- **Descripción**: Búsqueda por **anillos** con **corte temprano**.

  - **Secuencial**: recorre listas por celda.
  - **Paralelo/CSR**: recorre **rangos contiguos** y usa `omp simd`.

## `void gridFree(UniformGrid *g);`

- **Entradas**: `g` (`UniformGrid*`).
- **Salidas**:
- **Descripción**: Libera buffers internos y lleva `g` a **estado neutro**.

  - **Secuencial**: suelta `head/next`.
  - **Paralelo**: suelta `cellCount/Offset/Points`.

## Secciones paralelas y mecanismos de sincronía

- **Conteo por celda**: `#pragma omp parallel for` + `#pragma omp atomic update` al acumular `cellCount[hidx]`. Evita colisiones entre hilos cuando múltiples puntos caen en la misma celda.
- **Dispersión**: `#pragma omp atomic capture` para obtener un **slot único** por celda al escribir en `cellPoints[offset[h]+slot]`.
- **Recorrido de candidatos**: `#pragma omp simd` en el bucle interno para vectorizar el cálculo de distancias (SIMD **intra-hilo**).

## Programación defensiva

- Valida **punteros** (`g`, `s`, `s->pts`) y **rangos** (`W,H,cell>0`, `count>0`).
- En fallos de reserva, libera parcial y retorna `false`.
- `gridFree` es **idempotente** (seguro llamar varias veces).

## Despliegue de resultados

Este módulo **no dibuja**; devuelve **índices** para que el paso de Lloyd acumule centroides o el renderer pinte. Se integra en `lloyd.c / lloyd_parallel.c` como **acelerador NN**.

## Ejemplo de uso (pseudo)

```c
UniformGrid g = {0};
if (gridBuild(&g, W, H, 32, &stip)) {
    int idx = gridNearest(&g, &stip, qx, qy, 1);
    // usar idx...
}
gridFree(&g);
```

## Rendimiento y elección de backend

- **Listas por celda**: simple y rápido para tamaños modestos.
- **CSR + OpenMP**: mejor **localidad** y **paralelismo** cuando N es grande o al hacer muchas consultas NN por iteración.
