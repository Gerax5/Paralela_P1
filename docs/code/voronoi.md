# `voronoi.c` — Índice espacial y consulta de vecinos más cercanos

Este módulo implementa un **índice de grilla uniforme** para acelerar la búsqueda del punto más cercano durante el paso de Lloyd. La imagen de entrada no participa aquí; solo se indexa y consulta la **nube de puntos** (`Stippling`).

## Estructura de datos: `UniformGrid`

La grilla divide el lienzo `w x h` en celdas cuadradas de tamaño `cell` (en pixeles).
Cada celda mantiene una lista enlazada (implícita) de índices de puntos.

```c
typedef struct {
  int w, h;      // tamaño del canvas en pixeles
  int cell;      // tamaño de celda (px), p.ej. 32
  int cols, rows; // número de columnas y filas

  int *head;     // head[cy*cols + cx] -> índice del primer punto o -1
  int *next;     // next[i] -> siguiente punto en el bucket de su celda, o -1
} UniformGrid;
```

### Invariantes

- `0 <= cx < cols`, `0 <= cy < rows`.
- Cada punto `i` aparece exactamente una vez en la lista de su celda.
- `head` tiene tamaño `cols*rows`; `next` tiene tamaño `s->count`.

## Construcción de la grilla

```c
bool gridBuild(UniformGrid *g, int w, int h, int cell, const Stippling *s);
```

**Qué hace:**

Inicializa `g` para cubrir el canvas `w x h` con celdas de lado `cell`, y **bucketiza** todos los puntos de `s` en la celda que les corresponde.

**Parámetros:**

- `g` -> salida. Debe apuntar a una estructura válida (vacía).
- `w`, `h` -> tamaño del canvas en pixeles.
- `cell` -> tamaño de celda (px). Valores típicos: 16..64.
- `s` -> nube de puntos (`s->pts[i].x,y` en coordenadas de canvas).

**Retorno:**

`true` si la construcción fue exitosa, `false` en caso de parámetros inválidos o fallo de memoria.

**Notas:**

- Debe volver a llamarse **luego de mover los puntos** (p. ej., al final de cada iteración de Lloyd), porque los buckets dependen de la posición actual.

## Búsqueda de vecino más cercano

```c
int gridNearest(const UniformGrid *g,
                const Stippling *s,
                float x, float y,
                int ringMax);
```

**Qué hace**
Encuentra el índice del punto más cercano a `(x, y)` usando la grilla como índice espacial. Recorre celdas en **anillos** alrededor de la celda base de `(x, y)` y examina solo el **borde** de cada anillo. Mide distancia **al cuadrado** (evita `sqrt`).

**Parámetros:**

- `g` -> grilla ya construida con `gridBuild`.
- `s` -> nube de puntos (posiciones válidas).
- `x, y` -> consulta en pixeles (espacio de canvas).
- `ringMax` -> anillos máximos a expandir (0 = solo celda base, 1 = base + vecinos inmediatos, etc.). Típico: 1 o 2.

**Retorno:**

Índice del punto más cercano en `s->pts`, o `-1` si `g/s` no son válidos o la grilla está vacía.

**Complejidad:**

Promedio `O(k)`, donde `k` es el número de puntos contenidos en las celdas visitadas hasta `ringMax`. En distribuciones razonables, `ringMax=1..2` suele ser suficiente.

**Heurística de corte:**

Si en un anillo `r >= 1` ya se encontraron candidatos, se corta la expansión (suele bastar con vecinos cercanos).

## Liberación de recursos

```c
void gridFree(UniformGrid *g);
```

Libera `head` y `next` y deja los punteros en `NULL`. Es **segura** si `g` es `NULL`.
Debe llamarse **después de usar** la grilla y **antes** de reconstruirla para otro estado de puntos o al salir.

## Recomendaciones de uso

- **Tamaño de celda** (`cell`): valores en `[16..64]` suelen funcionar bien.

  - Celdas muy pequeñas -> más celdas, menos puntos por celda (más overhead).
  - Celdas muy grandes -> menos celdas, más puntos por celda (menos filtrado).
- **ringMax**: empieza con `1`. Si tu distribución es muy dispersa, sube a `2`.
- **Reconstrucción**: reconstruye la grilla **después** de mover puntos (p. ej., al final de cada `lloydStep`), si necesitas consultas en el siguiente paso.
- **Distancia**: usa siempre **distancia al cuadrado** para evitar `sqrt`.

## Ejemplo de integración (fragmento)

```c
// 1) Construir grilla para el estado actual de los puntos
UniformGrid g = {0};
if (!gridBuild(&g, canvasW, canvasH, 32, &stip)) {
  // fallback opcional: búsqueda O(N)
}

// 2) Consultar vecino más cercano al recorrer el lienzo
for (int y = 0; y < canvasH; y += step) {
  for (int x = 0; x < canvasW; x += step) {
    int idx = gridNearest(&g, &stip, (float)x, (float)y, 2);
    // ... acumular contribuciones en sumX/sumY/sumW[idx]
  }
}

// 3) Liberar grilla cuando ya no se necesite
gridFree(&g);
```

## Casos límite

- Consultas **fuera del canvas**: se mapean a celdas fuera de rango y se ignoran; no hay crash.
- Puntos **en el borde** exacto de celdas: la división entera los reparte de manera consistente.
- Nube **vacía** (`s->count == 0`): `gridBuild` falla y `gridNearest` retorna `-1`.

## Relación con Lloyd

En `lloyd.c`, el índice de grilla reemplaza la búsqueda `O(N)` por consulta **local**.
Esto reduce el costo de la fase "asignación" (píxel -> punto más cercano), que es la parte dominante del algoritmo, especialmente cuando `N` es grande.
