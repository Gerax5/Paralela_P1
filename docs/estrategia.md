# Propuesta Técnica de Paralelización para *Voronoi Stippling*

## 1. Secuencial vs. Paralelizable

### Secuencial (no paralelizable)

- **Lectura de la imagen de entrada** y conversión a escala de grises → paso único, depende de librerías externas.
- **Inicialización de puntos aleatorios** (semilla inicial). Necesita un orden controlado para reproducibilidad.
- **Actualización global de parámetros** (convergencia, control de iteraciones, ajuste de radios).
- **Sincronización final de cada iteración** → debe esperar a que todos los hilos terminen antes de avanzar.

### Paralelizable (candidatos fuertes)

- **Cálculo del diagrama de Voronoi:**
  Evaluar para cada píxel qué punto (stipple) le corresponde → altamente paralelizable (SIMD/MIMD).
- **Cálculo de centroides ponderados:**
  Sumar coordenadas de píxeles en cada celda, aplicando peso según intensidad de la imagen. Cada celda puede acumularse en paralelo.
- **Reubicación de puntos (Lloyd’s algorithm):**
  Puede hacerse en paralelo por cada punto.
- **Escalamiento de radios y anisotropía:**
  Cada stipple aplica su propio factor → paralelismo por dato.

## 2. Estrategias de paralelización

- **Paralelismo de datos (SIMD + MIMD)**:
  Cada píxel del área de la imagen puede evaluarse en paralelo para asignarse a un punto Voronoi.
  → Justificación: el cálculo es independiente por píxel.

- **Reducciones en paralelo:**
  Para calcular centroides se necesitan sumatorias → usar `reduction(+:var)` de OpenMP.
  → Justificación: evita condiciones de carrera y es más eficiente que `critical`.

- **Paralelismo por tareas (OpenMP sections):**
  Diferentes fases (dibujar puntos, dibujar líneas, guardar resultados) pueden dividirse en secciones si se procesan en la misma iteración.
  → Justificación: balancea cómputo heterogéneo.

## 3. Directivas de OpenMP y estructuras de datos

- **Regiones paralelas por bucle:**

  ```c
  #pragma omp parallel for schedule(dynamic) reduction(+:sumX, sumY, count)
  for (int i = 0; i < pixels; i++) { ... }
  ```

  - `parallel for`: distribuir iteraciones de cálculo de celdas.
  - `reduction`: acumular coordenadas sin race conditions.
  - `schedule(dynamic)`: balancea carga porque algunas celdas tendrán más píxeles que otras.

- **Secciones para tareas diferenciadas:**

  ```c
  #pragma omp parallel sections
  {
    #pragma omp section
    render_points();
    #pragma omp section
    render_voronoi();
  }
  ```

  - Justificación: separa lógica de renderizado y cálculo.

- **Estructuras de datos recomendadas:**

  - Arreglos contiguos (`float*`, `int*`) para centroides y acumuladores → favorece vectorización SIMD.
  - Buffers temporales por hilo (`private`) para evitar bloqueos.
  - Uso de `reduction` en vez de `critical` o `atomic` para sumar intensidades.

## 4. Posibles mejoras de diseño

- **Bloques de imagen (tiling):** dividir la imagen en regiones cuadradas → cada hilo procesa un bloque completo de píxeles. Reduce cache misses.
- **Vectorización:** aprovechar SIMD (SSE/AVX) para calcular distancias cuadradas `(dx*dx + dy*dy)` en batch.
- **Incremental refinement:** iniciar con menos puntos y aumentarlos gradualmente (como sugiere la doc de Hufstedler). Esto reduce el coste inicial y escala mejor en paralelo.
- **Uso de `guided schedule`:** en fases iniciales donde hay muchos puntos pesados, reduce overhead dinámico.

## 5. Justificación técnica

- **Por qué `parallel for` y no `sections` para el Voronoi:**
  El cálculo de distancias y asignación es homogéneo y masivo, ideal para dividir iteraciones.
- **Por qué `reduction` y no `critical`:**
  `critical` genera un cuello de botella en acumulaciones. `reduction` escala mucho mejor porque combina resultados al final.
- **Por qué datos contiguos y no listas enlazadas:**
  El acceso aleatorio penaliza el rendimiento en paralelo. Arreglos lineales favorecen cache y SIMD.
- **Por qué iniciar con tiling:**
  Permite aprovechar coherencia espacial, especialmente en imágenes grandes.

## Ejemplo práctico de paralelización

```c
#pragma omp parallel for schedule(dynamic) reduction(+:cx, cy, w)
for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
        int nearest = find_nearest_stipple(x,y,stipples);
        float weight = 1.0f - image[y][x]; // oscuridad
        cx[nearest] += x * weight;
        cy[nearest] += y * weight;
        w[nearest]  += weight;
    }
}
```

- Cada hilo calcula asignaciones independientes.
- Reducciones acumulan resultados por celda.
- Posteriormente, los puntos se actualizan con:

  ```c
  px[i] = cx[i]/w[i];
  py[i] = cy[i]/w[i];
  ```
