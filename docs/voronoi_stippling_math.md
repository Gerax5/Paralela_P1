# Algerítmico Matemático: Voronoi Stippling

## 1. Diagramas de Voronoi (Región de Influencia)

Un diagrama de Voronoi divide el plano en regiones $C_i$, cada una asociada a un punto $p_i = (x_i, y_i)$, donde

El criterio de distancia $d$ puede ser:

* Euclidiana: $\sqrt{(x - x_i)^2 + (y - y_i)^2}$
* Chebyshev: $\max(|x - x_i|, |y - y_i|)$
* Manhattan: $|x - x_i| + |y - y_i|$
  ([Extra Polynymous][1])

## 2. Algoritmo de Lloyd (Centroidal Voronoi Tessellation)

Para uniformizar la distribución de puntos se aplica Lloyd iterativamente:

1. Construir el diagrama de Voronoi.
2. Calcular el centroide geométrico de cada región:

3. Reubicar $p_i$ en $(\bar{x}_i, \bar{y}_i)$.
   Se repite hasta convergencia.
   ([Extra Polynymous][1])

## 3. Weighted Voronoi Stippling (por imagen)

En lugar del centroide geométrico, se calcula el centro de masa ponderado según la imagen:

con $I(x, y)$ como brillo o intensidad.
([Extra Polynymous][1])

## 4. Multiplicatively Weighted Voronoi (tamaños variables)

Para dotar de distintos tamaños (radios $r_i$) a los puntos, el diagrama considera:

Así se logra una separación proporcional al tamaño.
([Extra Polynymous][1])

## 5. Anisotropía (distorsión direccional)

Introducción de anisotropía con razón $\alpha$ y ángulo $\theta$:

Esto genera regiones alargadas en dirección preferida.
([Extra Polynymous][1])

## 6. Implementación simple iterativa

Definir campos sobre la imagen (radio $R(x, y)$, ángulo $\theta(x, y)$, anisotropía $\alpha(x, y)$), luego:

1. Posicionar aleatoriamente $N$ puntos.
2. En cada iteración:

   * Obtener $R_i, \theta_i, \alpha_i$ según posición.
   * Construir Voronoi ponderado/anisotrópico.
   * Calcular centro de masa (o centroide).
   * Mover $p_i$ al nuevo punto.
3. Repetir hasta convergencia o límite de iteraciones.
   ([Extra Polynymous][1], [Extra Polynymous][1])

## 7. Ajuste de conservación de densidad

Para mantener la "oscuridad total" igual a la densidad visual, se escala uniformemente todo con factor $\rho$, asegurando:

Así la densidad de puntos representa la luminosidad de la imagen.
([Extra Polynymous][1])

## 8. Versión escalonada (“Better implementation”)

1. Partir con campos iniciales $R_0(x,y), \theta_0(x,y), \alpha_0(x,y)$ y una imagen $I_0$ de área $A_0$.
2. Establecer un número objetivo de puntos $N_f$.
3. Iterativamente:

   * Generar $N_2$ temporales con escalamiento proporcional a imagen/área.
   * Calcular Voronoi y centro de masa, determinar oscuridad $D_i$ y radio medio $\bar{R}_i$.
   * Usar factor:

     $$
     c = \frac{\sum_{i=1}^{N_2} D_i}{\sum_{i=1}^{N_2} \pi \,\bar{R}_i^{\,2}}
     $$

     y asignar nuevos puntos por región:

     $$
     N_i = \operatorname{round}\!\left(\frac{D_i}{c \,\pi\, \bar{R}_i^{\,2}}\right),
     \qquad
     N = \sum_{i=1}^{N_2} N_i
     $$

   * Redistribuir aleatoriamente los $N_i$ dentro de cada región.
   * Aplicar iteraciones de Lloyd primero dentro de subregiones y luego en todo el campo.
4. Repetir hasta alcanzar $N \geq N_f$.
   Esta aproximación gradual mejora la eficiencia y la convergencia.
   ([Extra Polynymous][1])

## Resumen matemático clave

| Caso                   | Distancia modificada                  | Actualización de puntos     |
| ---------------------- | ------------------------------------- | --------------------------- |
| Lloyd clásico          | Euclidiana estándar                   | Centroide                   |
| Weighted stippling     | Euclidiana + ponderación de imagen    | Centro de masa              |
| Multiplicative weights | Euclidiana escalada por $s = 1 / r_i$ | Incluido en cálculo Voronoi |
| Anisotropic            | Distorsión por $\alpha, \theta$       | Incluido en cálculo Voronoi |

[1]: https://estebanhufstedler.com/2020/01/11/modfied-voronoi-diagrams-and-stippling/?utm_source=chatgpt.com "Modfied Voronoi Diagrams and Stippling – Extra Polynymous"
