# `voronoi.c` — Índice espacial y consulta de vecino más cercano

Módulo que implementa una **grilla uniforme** para acelerar la búsqueda del punto más cercano (NN) durante Lloyd. No toca la imagen; solo indexa y consulta la **nube de puntos** (`Stippling`).

## Estructura de datos: `UniformGrid`

La grilla divide el canvas `w × h` en celdas cuadradas de lado `cell` (px).
Cada celda guarda una lista enlazada (implícita) con los índices de puntos que caen en ella.

```c
typedef struct {
  int w, h;       // tamaño del canvas (px)
  int cell;       // lado de celda (px), p.ej. 32
  int cols, rows; // número de columnas y filas

  int *head;      // head[cy*cols + cx] -> primer punto o -1 si vacío
  int *next;      // next[i] -> siguiente punto en la celda, o -1
} UniformGrid;
```

**Invariantes:**

- `head` tiene tamaño `cols*rows`.
- `next` tiene tamaño `s->count` (un enlace por punto).
- Cada punto `i` aparece **a lo sumo una vez** en su lista de celda.

**Notas:**

- No es thread-safe.
- Las coordenadas de puntos están en el mismo espacio de pantalla que la grilla.

## Construcción de la grilla

```c
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);
```

**Qué hace:**

Inicializa `g` para cubrir `w × h` con celdas de lado `cell` y **bucketiza** todos los puntos de `s`.

**Flujo:**

1. `cols = ceil(w/cell)`, `rows = ceil(h/cell)`
2. Reserva `head[cols*rows]` y `next[s->count]`
3. Inicializa `head` con `-1`
4. Para cada punto `i`: calcula celda `(cx, cy)` y hace *push-front* en O(1).

**Clamping:**

Si algún punto cae fuera de `[0..w)×[0..h)`, se **clampa** a la celda válida más cercana (usa `clampi`).

**Complejidad:**

- Tiempo: `O(cols*rows + N)`
- Memoria: `O(cols*rows + N)`

**Retorno:**

`true` en éxito; `false` si hay parámetros inválidos o falla de memoria (libera lo parcialmente asignado).

## Búsqueda de vecino más cercano

```c
int gridNearest(const UniformGrid *g,
                const Stippling *s,
                float x, float y,
                int ringMax);
```

**Qué hace:**

Devuelve el índice del punto más cercano a `(x, y)` recorriendo celdas en **anillos** alrededor de la celda base de `(x, y)` y examinando **solo el borde** de cada anillo (evita visitas redundantes).
Se compara **distancia al cuadrado** (sin `sqrt`).

**Parámetros:**

- `g` -> grilla construida con `gridBuild` para el mismo `s`
- `s` -> nube de puntos (posiciones válidas)
- `x, y` -> consulta en píxeles (canvas)
- `ringMax` -> anillos máximos (0 = solo base, 1 = +vecinas, 2 = +siguientes, …)

**Heurística de corte:**

Si en un anillo `r ≥ 1` ya se encontró al menos un candidato, se **corta** la expansión (suele bastar con `ringMax` 1–2).

**Complejidad esperada:**

Promedio `O(k)`, donde `k` es el número de puntos en las pocas celdas visitadas.

**Retorno:**

Índice del punto más cercano, o `-1` si no hay candidatos / entradas inválidas.
Si `ringMax < 0`, no se itera y el resultado es `-1`.

## Liberación

```c
void gridFree(UniformGrid *g);
```

Libera `head` y `next` y deja sus punteros en `NULL`. Es segura si `g == NULL` y es idempotente.
No libera la estructura `UniformGrid` (solo sus buffers internos).

## Recomendaciones

- **Tamaño de celda (`cell`)**: 16–64 px suele funcionar bien.

  - Muy pequeña → muchas celdas, más overhead.
  - Muy grande  → menos celdas, más puntos por celda (filtra peor).
- **`ringMax`**: empieza con `1`; si tu distribución es muy dispersa, sube a `2`.
- **Reconstrucción**: si los puntos se mueven (Lloyd), reconstruye la grilla para el **nuevo** estado antes de volver a consultarla.
- **Métrica**: compara **distancia al cuadrado**; evita `sqrt` innecesarios.

## Ejemplo de integración

```c
UniformGrid g = (UniformGrid){0};
if (!gridBuild(&g, canvasW, canvasH, 32, &stip)) {
  // fallback opcional: NN O(N)
}

for (int y = 0; y < canvasH; y += step) {
  for (int x = 0; x < canvasW; x += step) {
    int idx = gridNearest(&g, &stip, (float)x, (float)y, 2);
    if (idx >= 0) {
      // acumular en sumX/sumY/sumW[idx] ...
    }
  }
}

gridFree(&g);
```

## Casos límite

- **Consultas fuera del canvas**: su celda resulta fuera de rango y se ignoran (no crash).
- **Puntos exactamente en bordes de celda**: la división entera los asigna de forma consistente.
- **Nube vacía** (`s->count == 0`): `gridBuild` falla; `gridNearest` retorna `-1`.
