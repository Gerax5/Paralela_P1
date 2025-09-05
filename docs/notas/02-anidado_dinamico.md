# Documentación: Paralelismo Anidado y Paralelismo Dinámico en OpenMP

## 1. Paralelismo Anidado

### ¿Qué es?

El **paralelismo anidado** en OpenMP permite crear **regiones paralelas dentro de otras regiones paralelas**.
Cada hilo en una región paralela puede crear su propia región paralela interna.

**Ejemplo mental:**
Imagina que tienes **4 equipos** trabajando en una tarea principal, y cada equipo divide su trabajo entre **2 subequipos** que trabajan en paralelo.

### Ejemplo de código (imagen 1)

```c
#pragma omp parallel num_threads(4)
{
    int thread_id = omp_get_thread_num();
    printf("Nivel 1 - Hilo %d\n", thread_id);

    #pragma omp parallel num_threads(2)
    {
        int nested_thread_id = omp_get_thread_num();
        printf("Nivel 2 - Hilo %d.%d\n", thread_id, nested_thread_id);
    }
}
```

#### Explicación paso a paso

1. `#pragma omp parallel num_threads(4)`
   Crea **4 hilos** para el primer nivel (Nivel 1).
2. Cada hilo imprime su ID (`thread_id`).
3. Dentro de cada hilo de Nivel 1, se crea **otra región paralela** con `num_threads(2)` para el **Nivel 2**.
4. `nested_thread_id` es el ID del hilo **dentro del segundo nivel**.
5. El formato `"Nivel 2 - Hilo %d.%d"` muestra `ID_del_nivel1.ID_del_nivel2`.

### Cuándo usarlo

- ✅ Cuando una tarea principal **puede dividirse en sub-tareas**, y estas a su vez en **sub-sub-tareas**.
- ✅ Útil en arquitecturas **multi-core y multi-threading** que tienen varios niveles de paralelismo.

### Cuándo NO usarlo

- ❌ Si el sistema **no soporta bien** el paralelismo anidado (puede degradar el rendimiento).
- ❌ Si la tarea es pequeña, porque el **overhead** (sobrecarga) de crear hilos adicionales puede ser mayor que la ganancia.

### Ventajas

- Mejor utilización de **recursos de hardware** con varios niveles de paralelismo.
- Puede mejorar el rendimiento en problemas con estructura **jerárquica**.

### Desventajas

- No todos los sistemas lo implementan eficientemente.
- Requiere **controlar bien** la cantidad de hilos para evitar sobrecarga.
- Código más complejo de entender y depurar.

## 2. Paralelismo Dinámico

### ¿Qué es?

El **paralelismo dinámico** permite que **el número de hilos se ajuste automáticamente** durante la ejecución, dependiendo de la carga de trabajo y los recursos disponibles.

**Ejemplo mental:**
Si una fila de trabajo crece, el sistema asigna más personas; si la fila disminuye, asigna menos.

### Ejemplo de código (imagen 2)

```c
#include <omp.h>
#include <stdio.h>

// Función que realiza una carga de trabajo con ajuste dinámico de hilos
void dynamic_workload(int iterations) {
    omp_set_dynamic(1);   // Permitir ajuste dinámico
    int num_threads = 4;  // Número inicial de hilos
    omp_set_num_threads(num_threads);

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < iterations; i++) {
        int thread_id = omp_get_thread_num();
        printf("Iteración %d ejecutada por hilo %d\n", i, thread_id);
    }
}

int main() {
    int iterations = 20;
    dynamic_workload(iterations);
    return 0;
}
```

#### Explicación paso a paso

1. `omp_set_dynamic(1)` → Permite que OpenMP ajuste el número de hilos automáticamente.
2. `omp_set_num_threads(4)` → Define el número inicial de hilos.
3. `#pragma omp parallel for schedule(dynamic)`

   - Reparte las iteraciones dinámicamente entre los hilos.
   - Cuando un hilo termina su bloque, toma otro automáticamente.
4. Se imprimen las iteraciones con el ID del hilo que las ejecuta.

### Cuándo usarlo

- ✅ Cuando la **carga de trabajo varía** entre iteraciones.
- ✅ En sistemas compartidos para **evitar saturar recursos**.
- ✅ Cuando se busca **balancear la carga** sin planificar manualmente.

### Cuándo NO usarlo

- ❌ Si todas las iteraciones tardan lo mismo (es mejor `schedule(static)`).
- ❌ Si el ajuste dinámico introduce **sobrecarga innecesaria** en tareas muy pequeñas.

### Ventajas

- Se adapta a cargas de trabajo variables.
- Puede reducir tiempos de espera y mejorar el rendimiento.
- Evita la **contención de recursos**.

### Desventajas

- Requiere buena comprensión de la carga para implementarlo bien.
- Introduce **complejidad extra** en el manejo del código.
- No siempre es más rápido que el reparto estático.

**Resumen comparativo:**

| Característica    | Paralelismo Anidado                  | Paralelismo Dinámico                  |
| ----------------- | ------------------------------------ | ------------------------------------- |
| Ajuste de hilos   | Fijo en cada nivel                   | Automático según carga                |
| Complejidad       | Alta                                 | Media                                 |
| Uso típico        | Subtareas jerárquicas                | Tareas con tiempos variables          |
| Riesgos           | Sobrecarga por demasiados hilos      | Sobrecarga por reasignación constante |
| Ventaja principal | Aprovecha varios niveles de hardware | Adaptación a cambios en la carga      |
