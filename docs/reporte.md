<!-- pandoc --from=markdown-implicit_figures   reporte.md -o reporte.pdf   --pdf-engine=xelatex   -V geometry:top=0.67in -V geometry:bottom=0.67in -V geometry:left=0.85in -V geometry:right=0.85in   -H header.tex   --resource-path=.:images:../images -->

# Índice - Catalogo

# Introducción

El objetivo fue medir el rendimiento de la versión **secuencial (SEQ)** versus la versión **paralela con OpenMP (OMP)** del algoritmo de Lloyd, usando **speedup = Tseq / Tomp** y CSVs con tiempos por iteración. En PC1 (escritorio) OMP tiene *overhead* para N pequeños y supera a SEQ a partir de \~N=6000; en PC2 (laptop) OMP es más rápido desde N bajos. Con N=50000 se observaron **speedups** de \~**11.5×** (PC1) y \~**14.0×** (PC2). Estos resultados confirman escalamiento con el tamaño del problema y dependencia del hardware.

## Enlaces

- [Repositorio GitHub](https://github.com/Gerax5/Paralela_P1/tree/vs)

# Antecedentes

## Documentación: Voronoi Stippling

### Descripción general

**Stippling** es una técnica de representación gráfica donde una imagen o figura se aproxima usando **puntos distribuidos en el plano**.
El objetivo es que la **densidad y posición de los puntos** transmitan la estructura, contraste y detalles de la imagen.

Para lograr esto de manera algorítmica, se combina:

- **Diagramas de Voronoi** -> dividen el plano en celdas según cercanía a cada punto semilla.
- **Algoritmo de Lloyd** -> ajusta iterativamente la posición de los puntos hacia el **centroide o centro de masa** de su celda Voronoi.
- **Pesos derivados de la imagen** -> las áreas oscuras atraen más puntos, generando mayor densidad (Weighted Voronoi Stippling).

### Parámetros principales del algoritmo

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

### Funcionamiento (flujo básico)

1. Inicializar **N puntos aleatorios** en el área de la imagen/canvas.
2. **Construir el diagrama de Voronoi** con esos puntos.
3. Para cada celda Voronoi:

   - Calcular el **centroide geométrico** (si uniforme).
   - Calcular el **centro de masa ponderado** (si depende de imagen).
4. Mover cada punto al centroide/centro de masa de su celda.
5. Repetir el proceso hasta convergencia o un número fijo de iteraciones.
6. Renderizar los puntos (círculos o dots).

### Retornos / Resultados

- **Distribución final de puntos** (coordenadas 2D).
- Imagen stippled que aproxima la forma o gradientes de la imagen de entrada.
- En versión animada (*screensaver*), se visualiza la evolución iterativa (los puntos "se deslizan" hacia posiciones más uniformes/densas según la imagen).

### Variantes importantes

- **Lloyd’s Algorithm (Centroidal Voronoi Tessellation)**: puntos uniformemente distribuidos.
- **Weighted Voronoi Stippling**: más puntos en áreas oscuras.
- **Multiplicatively Weighted Voronoi**: permite puntos de distinto tamaño.
- **Anisotropy**: genera patrones direccionales (líneas o texturas).

### Reto y propósito del algoritmo

El reto de **Voronoi Stippling** es convertir imágenes o áreas en representaciones basadas **únicamente en puntos**, distribuidos de manera **eficiente y visualmente coherente**.

Esto implica resolver problemas de:

- **Distribución espacial**: mantener puntos equidistantes evitando agrupamientos innecesarios.
- **Convergencia rápida**: el algoritmo puede ser costoso con miles de puntos.
- **Visualización estética**: los puntos deben transmitir la información visual de manera clara.

En el contexto de tu proyecto de **paralela con OpenMP**, el reto es:

- Computar las celdas Voronoi o aproximaciones para cada iteración.
- Recalcular centroides/centros de masa de manera eficiente.
- Paralelizar el procesamiento de celdas/puntos para acelerar la convergencia.

# Cuerpo

## Algerítmico Matemático: Voronoi Stippling

### 1. Diagramas de Voronoi (Región de Influencia)

Un diagrama de Voronoi divide el plano en regiones $C_i$, cada una asociada a un punto $p_i = (x_i, y_i)$, donde

El criterio de distancia $d$ puede ser:

- Euclidiana: $\sqrt{(x - x_i)^2 + (y - y_i)^2}$
- Chebyshev: $\max(|x - x_i|, |y - y_i|)$
- Manhattan: $|x - x_i| + |y - y_i|$
  ([Extra Polynymous][1])

### 2. Algoritmo de Lloyd (Centroidal Voronoi Tessellation)

Para uniformizar la distribución de puntos se aplica Lloyd iterativamente:

1. Construir el diagrama de Voronoi.
2. Calcular el centroide geométrico de cada región:

3. Reubicar $p_i$ en $(\bar{x}_i, \bar{y}_i)$.
   Se repite hasta convergencia.
   ([Extra Polynymous][1])

### 3. Weighted Voronoi Stippling (por imagen)

En lugar del centroide geométrico, se calcula el centro de masa ponderado según la imagen:

con $I(x, y)$ como brillo o intensidad.
([Extra Polynymous][1])

### 4. Multiplicatively Weighted Voronoi (tamaños variables)

Para dotar de distintos tamaños (radios $r_i$) a los puntos, el diagrama considera:

Así se logra una separación proporcional al tamaño.
([Extra Polynymous][1])

### 5. Anisotropía (distorsión direccional)

Introducción de anisotropía con razón $\alpha$ y ángulo $\theta$:

Esto genera regiones alargadas en dirección preferida.
([Extra Polynymous][1])

### 6. Implementación simple iterativa

Definir campos sobre la imagen (radio $R(x, y)$, ángulo $\theta(x, y)$, anisotropía $\alpha(x, y)$), luego:

1. Posicionar aleatoriamente $N$ puntos.
2. En cada iteración:

   - Obtener $R_i, \theta_i, \alpha_i$ según posición.
   - Construir Voronoi ponderado/anisotrópico.
   - Calcular centro de masa (o centroide).
   - Mover $p_i$ al nuevo punto.
3. Repetir hasta convergencia o límite de iteraciones.
   ([Extra Polynymous][1], [Extra Polynymous][1])

### 7. Ajuste de conservación de densidad

Para mantener la "oscuridad total" igual a la densidad visual, se escala uniformemente todo con factor $\rho$, asegurando:

Así la densidad de puntos representa la luminosidad de la imagen.
([Extra Polynymous][1])

### 8. Versión escalonada ("Better implementation")

1. Partir con campos iniciales $R_0(x,y), \theta_0(x,y), \alpha_0(x,y)$ y una imagen $I_0$ de área $A_0$.
2. Establecer un número objetivo de puntos $N_f$.
3. Iterativamente:

   - Generar $N_2$ temporales con escalamiento proporcional a imagen/área.
   - Calcular Voronoi y centro de masa, determinar oscuridad $D_i$ y radio medio $\bar{R}_i$.
   - Usar factor:

     $$
     c = \frac{\sum_{i=1}^{N_2} D_i}{\sum_{i=1}^{N_2} \pi \,\bar{R}_i^{\,2}}
     $$

     y asignar nuevos puntos por región:

     $$
     N_i = \operatorname{round}\!\left(\frac{D_i}{c \,\pi\, \bar{R}_i^{\,2}}\right),
     \qquad
     N = \sum_{i=1}^{N_2} N_i
     $$

   - Redistribuir aleatoriamente los $N_i$ dentro de cada región.
   - Aplicar iteraciones de Lloyd primero dentro de subregiones y luego en todo el campo.
4. Repetir hasta alcanzar $N \geq N_f$.
   Esta aproximación gradual mejora la eficiencia y la convergencia.
   ([Extra Polynymous][1])

### Resumen matemático clave

| Caso                   | Distancia modificada                  | Actualización de puntos     |
| ---------------------- | ------------------------------------- | --------------------------- |
| Lloyd clásico          | Euclidiana estándar                   | Centroide                   |
| Weighted stippling     | Euclidiana + ponderación de imagen    | Centro de masa              |
| Multiplicative weights | Euclidiana escalada por $s = 1 / r_i$ | Incluido en cálculo Voronoi |
| Anisotropic            | Distorsión por $\alpha, \theta$       | Incluido en cálculo Voronoi |

# Diagramas

## Diagrama de secuencia

![Secuencia](../images/diagrams/secuencia.png)

## Diagrama de flujo

![Flujo](../images/diagrams/flujo.png)

# Bitácora de pruebas

El benchmark utilizado considera las dos partes de los archivos compilados:

- **SEQ** -> versión secuencial.
- **OMP** -> versión paralela con OpenMP.

El objetivo es medir cuánto mejora (o empeora) el rendimiento al paralelizar; para las pruebas se calculó el **speedup**, esto se define como:

$$
\text{Speedup} = \frac{T_{seq}}{T_{par}}
$$

donde:

- $T_{seq}$ = tiempo promedio de la versión secuencial.
- $T_{par}$ = tiempo promedio de la versión paralela.

- **Speedup = 1.0** -> No hay ganancia, ambas versiones tardan lo mismo.
- **Speedup > 1.0** -> La versión paralela es más rápida (hay mejora).

  - Ej: Speedup = 2.0 -> OMP es el doble de rápido que SEQ.
  - Ej: Speedup = 5.0 -> OMP es 5 veces más rápido.
- **Speedup < 1.0** -> La versión paralela es más lenta (hay overhead de paralelización).

## Benchmark bucle

### PC1 - Escritorio

| N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|-------|-------------------|-------------------|-----------------|
| 2000  | 6.598020          | 8.775120          | 0.752           |
| 4000  | 11.783380         | 12.836300         | 0.919           |
| 6000  | 17.933300         | 15.059940         | 1.194           |
| 8000  | 28.949420         | 16.253840         | 1.781           |
| 10000 | 48.921300         | 22.612800         | 2.166           |
| 12000 | 68.567910         | 21.369830         | 3.209           |
| 14000 | 82.506950         | 20.197140         | 3.942           |
| 16000 | 105.261280        | 23.017500         | 4.573           |
| 18000 | 123.027270        | 23.712520         | 5.188           |
| 20000 | 140.503360        | 26.834800         | 5.236           |

### PC2 - Laptop

| N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|-------|-------------------|-------------------|-----------------|
| 2000  | 11.660620         | 3.189670          | 3.659           |
| 4000  | 22.163180         | 4.908770          | 4.515           |
| 6000  | 31.974470         | 5.495560          | 5.818           |
| 8000  | 51.081030         | 11.178510         | 4.571           |
| 10000 | 66.904320         | 13.873110         | 4.823           |
| 12000 | 89.123800         | 20.712340         | 4.303           |
| 14000 | 119.171460        | 33.654470         | 3.542           |
| 16000 | 142.775740        | 32.417460         | 4.406           |
| 18000 | 162.633430        | 27.016560         | 6.020           |
| 20000 | 174.230730        | 29.735440         | 5.856           |

## Benchmark unitario (N = 50000)

| PC            | N     | Promedio ms (SEQ) | Promedio ms (OMP) | Speedup SEQ/OMP |
|---------------|-------|-------------------|-------------------|-----------------|
| PC escritorio | 50000 | 443.005050        | 38.483720         | 11.511          |
| Laptop        | 50000 | 823.389900        | 58.601760         | 14.051          |

## Evidencia de pruebas

### PC1 (Escritorio)

![Test usando 2000 - 10000 puntos](../images/pruebas/pc1_test_2000-10000.png)

![Test usando 12000 - 20000 puntos](../images/pruebas/pc1_test_12000-20000.png)

![Test unitario con 50000 puntos](../images/pruebas/pc1_test_50000.png)

### PC2 (Laptop)

![Test usando 2000 - 8000 puntos](../images/pruebas/pc2_test_2000-8000.png)

![Test usando 10000 - 16000 puntos](../images/pruebas/pc2_test_10000-16000.png)

![Test usando 18000 - 20000 puntos](../images/pruebas/pc2_test_18000-20000.png)

![Test unitario con 50000 puntos](../images/pruebas/pc2_test_50000.png)

## Gráficas

### PC1 (Escritorio): Tiempos vs N

![PC1 Escritorio](../images/pruebas/pc1_tiempos.png)

### PC2 (Laptop): Tiempos vs N

![PC2 Laptop](../images/pruebas/pc2_tiempos.png)

### Comparación tiempos N=50000

![Comparación N50000](../images/pruebas/n50000_comparacion.png)

### Speedup vs N

![Speedup vs N](../images/pruebas/speedup_vs_n.png)

## Interpretación de resultados

- En **problemas pequeños** (N bajos), la versión **OMP** en PC1 llega a ser más lenta que SEQ, debido al overhead de gestión de threads.
- A partir de **N=6000** en PC1, OMP supera claramente a SEQ y escala hasta **5.2x** en N=20000.
- En PC2 (laptop), el **speedup es mayor desde el inicio**, llegando hasta **~6x** en N=18000–20000.  
- Para **N=50000**, ambas máquinas muestran una aceleración significativa:
  - PC1 (escritorio): ~11.5x  
  - PC2 (laptop): ~14.0x  
- Esto refleja que el **algoritmo paralelo escala bien con N**, y que las diferencias de hardware impactan tanto en los tiempos absolutos como en el speedup relativo.

# Recomendaciones

- **Tamaño del problema**: usar **N ≥ 6000** en PC1 para ver ganancias; en PC2 el paralelismo rinde incluso con N bajos.
- **Hilos**: fijar `OMP_NUM_THREADS` al # de núcleos físicos y mantenerlo constante entre corridas comparables.
- **Parámetros fijos para pruebas**: semilla (`STIPPLE_SEED`), `gamma` (sin *sweep*), color ON/OFF, radios, y desactivar rotación de fondos (`STIPPLE_BG_SECONDS=0`) para reducir ruido en medición.
- **Métricas**: recolectar CSV con `STIPPLE_AUTORUN=1` y `STIPPLE_MAX_ITERS=K`; repetir 3–5 veces y promediar.
- **Warm-up**: descartar las primeras 1–2 iteraciones si hay variabilidad inicial.
- **Afinidad/ruido**: cerrar apps pesadas, usar modo “alto rendimiento” del SO y, si es posible, fijar afinidad (e.g., `taskset`) para estabilidad.
- **Explorar escalado**: barrer N y graficar *ms vs N* y *speedup vs N* para identificar la zona eficiente.

# Conclusiones

- La paralelización **no trivial** (buffers por hilo + reducción) elimina contención y habilita **aceleraciones sostenidas** en tamaños grandes.
- Existe un **punto de cruce**: con N pequeño, el *overhead* de OMP puede superar el beneficio; con N medio/alto, OMP **supera claramente** a SEQ.
- El **hardware** influye: la laptop probada obtiene speedups altos desde N bajos, mientras que el escritorio necesita N mayores para despegar.
- Con N=50000 se alcanzan speedups **>10×**, validando que la estrategia paralela es efectiva y escalable para cargas intensivas.

# Referencias

- [Esteban Hufstedler: *Modified Voronoi Diagrams and Stippling*](https://estebanhufstedler.com/2020/01/11/modfied-voronoi-diagrams-and-stippling/) (conceptos de Lloyd, Weighted, Anisotropy)
- [Mike Bostock – ObservableHQ: *Voronoi Stippling*](https://observablehq.com/@mbostock/voronoi-stippling) (visualización interactiva paso a paso).
- [The Coding Train (Challenge #181 – Image Stippling)](https://thecodingtrain.com/challenges/181-image-stippling): explicación pedagógica y animada.
- [Repositorio de referencia en JS](https://github.com/smallwhale1/voronoi-stippling?tab=readme-ov-file)

[1]: https://estebanhufstedler.com/2020/01/11/modfied-voronoi-diagrams-and-stippling/ "Modfied Voronoi Diagrams and Stippling – Extra Polynymous"
