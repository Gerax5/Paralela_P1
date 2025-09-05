# Documentación: Voronoi Stippling

## Descripción general

**Stippling** es una técnica de representación gráfica donde una imagen o figura se aproxima usando **puntos distribuidos en el plano**.
El objetivo es que la **densidad y posición de los puntos** transmitan la estructura, contraste y detalles de la imagen.

Para lograr esto de manera algorítmica, se combina:

- **Diagramas de Voronoi** -> dividen el plano en celdas según cercanía a cada punto semilla.
- **Algoritmo de Lloyd** -> ajusta iterativamente la posición de los puntos hacia el **centroide o centro de masa** de su celda Voronoi.
- **Pesos derivados de la imagen** -> las áreas oscuras atraen más puntos, generando mayor densidad (Weighted Voronoi Stippling).

## Parámetros principales del algoritmo

1. **N**: número de puntos iniciales (parámetro clave del proyecto).
2. **Imagen de entrada (opcional)**: define la densidad de puntos por oscuridad/brillo. Si no hay imagen, se puede trabajar con distribuciones uniformes o funciones matemáticas.
3. **Iteraciones (I)**: número de repeticiones de Lloyd’s Algorithm hasta converger.
4. **Distancia usada**:

   - Euclidiana (clásico).
   - Chebyshev o Manhattan (variantes).
5. **Pesos/funciones**:

   - **Weighted**: ponderación según intensidad (1 − I(x,y)).
   - **Multiplicatively weighted**: cada punto tiene un factor de escala -> permite simular diferentes tamaños de puntos.
   - **Anisotrópico**: distancias escaladas por un ángulo θ y razón α, para estiramiento direccional.

## Funcionamiento (flujo básico)

1. Inicializar **N puntos aleatorios** en el área de la imagen/canvas.
2. **Construir el diagrama de Voronoi** con esos puntos.
3. Para cada celda Voronoi:

   - Calcular el **centroide geométrico** (si uniforme).
   - Calcular el **centro de masa ponderado** (si depende de imagen).
4. Mover cada punto al centroide/centro de masa de su celda.
5. Repetir el proceso hasta convergencia o un número fijo de iteraciones.
6. Renderizar los puntos (círculos o dots).

## Retornos / Resultados

- **Distribución final de puntos** (coordenadas 2D).
- Imagen stippled que aproxima la forma o gradientes de la imagen de entrada.
- En versión animada (*screensaver*), se visualiza la evolución iterativa (los puntos "se deslizan" hacia posiciones más uniformes/densas según la imagen).

## Variantes importantes

- **Lloyd’s Algorithm (Centroidal Voronoi Tessellation)**: puntos uniformemente distribuidos.
- **Weighted Voronoi Stippling**: más puntos en áreas oscuras.
- **Multiplicatively Weighted Voronoi**: permite puntos de distinto tamaño.
- **Anisotropy**: genera patrones direccionales (líneas o texturas).

## Reto y propósito del algoritmo

El reto de **Voronoi Stippling** es convertir imágenes o áreas en representaciones basadas **únicamente en puntos**, distribuidos de manera **eficiente y visualmente coherente**.

Esto implica resolver problemas de:

- **Distribución espacial**: mantener puntos equidistantes evitando agrupamientos innecesarios.
- **Convergencia rápida**: el algoritmo puede ser costoso con miles de puntos.
- **Visualización estética**: los puntos deben transmitir la información visual de manera clara.

En el contexto de tu proyecto de **paralela con OpenMP**, el reto es:

- Computar las celdas Voronoi o aproximaciones para cada iteración.
- Recalcular centroides/centros de masa de manera eficiente.
- Paralelizar el procesamiento de celdas/puntos para acelerar la convergencia.

## Referencias útiles

- [Esteban Hufstedler: *Modified Voronoi Diagrams and Stippling*](https://estebanhufstedler.com/2020/01/11/modfied-voronoi-diagrams-and-stippling/) (conceptos de Lloyd, Weighted, Anisotropy)
- [Mike Bostock – ObservableHQ: *Voronoi Stippling*](https://observablehq.com/@mbostock/voronoi-stippling) (visualización interactiva paso a paso).
- [The Coding Train (Challenge #181 – Image Stippling)](https://thecodingtrain.com/challenges/181-image-stippling): explicación pedagógica y animada.
- [Repositorio de referencia en JS](https://github.com/smallwhale1/voronoi-stippling?tab=readme-ov-file)
