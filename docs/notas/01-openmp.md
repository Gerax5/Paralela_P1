# Introducción OpenMP

## ¿Qué es OpenMP?

OpenMP es una herramienta que ayuda a los programadores a hacer que un programa trabaje más rápido dividiendo el trabajo entre varios núcleos o procesadores al mismo tiempo. Es útil cuando una computadora tiene varios "cerebros" (núcleos) y queremos usarlos todos al mismo tiempo. Se usa principalmente en lenguajes como C, C++ y Fortran.

## Origen

OpenMP fue creado a finales de los años 90 por un grupo de empresas y organizaciones interesadas en facilitar la programación paralela, es decir, dividir tareas para que se ejecuten al mismo tiempo.

## Historia y evolución

Desde su primera versión, OpenMP ha sido mejorado con nuevas funciones. Al principio solo permitía tareas básicas en paralelo, pero con el tiempo se añadieron capacidades más avanzadas, como hacer tareas dentro de otras tareas (paralelismo anidado) y trabajar con distintos tipos de computadoras o procesadores (arquitecturas heterogéneas).

## Aplicaciones típicas

OpenMP se usa en muchos campos donde se necesita mucho poder de cálculo. Algunos ejemplos son:

* Simulaciones científicas (como predecir el clima o estudiar el universo)
* Análisis de grandes cantidades de datos
* Procesamiento de imágenes y videos

Aquí tienes la documentación explicada de forma clara, simple y sin tecnicismos complejos:

## Directivas

Son instrucciones especiales que se escriben en el código fuente y que el compilador entiende como órdenes para manejar la ejecución paralela. No afectan si el compilador no tiene soporte para OpenMP, simplemente se ignoran.

### Directiva `#pragma omp parallel`

**¿Qué hace?**

Esta directiva le dice al programa:
*"Desde este punto, ejecuta el siguiente bloque de código con varios hilos (procesos pequeños que trabajan en paralelo)."*

**¿Qué significa eso?**
Imagina que tienes una tarea que puedes dividir en varias partes iguales (como lavar platos en equipo). Cada hilo será una persona lavando una parte al mismo tiempo.

#### Código de ejemplo

```c
#include <stdio.h>
#include <omp.h>

int main() {
    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        printf("Hello from thread %d\n", id);
    }
    return 0;
}
```

* **`#include <omp.h>`**: Importa la biblioteca de OpenMP.
* **`#pragma omp parallel`**: Crea una región paralela. Aquí, varios hilos ejecutarán el mismo bloque de código.
* **`omp_get_thread_num()`**: Esta función obtiene el número del hilo que está ejecutando esa parte del código. Por ejemplo, si hay 4 hilos, los números serán 0, 1, 2 y 3.
* **`printf`**: Imprime un mensaje indicando qué hilo lo está diciendo.

#### ¿Cómo se vería la salida?

Algo como esto (el orden puede variar porque todos los hilos trabajan al mismo tiempo):

```bash
Hello from thread 0
Hello from thread 2
Hello from thread 1
Hello from thread 3
```

#### ¿Cuándo usar `#pragma omp parallel`?

* Cuando quieras que varios núcleos del procesador hagan lo mismo pero más rápido.
* Por ejemplo: recorrer un arreglo, realizar cálculos repetitivos, simular procesos que se pueden dividir.

**Ejemplo en palabras:**
Si tienes que sumar los números de una lista muy grande, puedes dividir la lista entre varios hilos para que cada uno sume una parte.

#### ¿Cuándo *no* usarlo?

* Si el código depende de que las cosas se hagan en orden exacto (porque los hilos trabajan al mismo tiempo, puede haber confusión si no se controla bien).
* Si lo que vas a hacer no se puede dividir o es muy pequeño (porque puede ser más lento usar varios hilos que hacerlo directo con uno).

### Control de Hilos

**¿Qué significa controlar los hilos?**

Por defecto, OpenMP puede decidir cuántos hilos usar, dependiendo del sistema. Pero a veces, el programador quiere tener el control y decir exactamente cuántos hilos se deben usar. Esto se puede hacer de dos formas:

1. Usando la función `omp_set_num_threads(n)`
2. Usando la cláusula `num_threads(n)` dentro de la directiva

#### Ejemplo usando `omp_set_num_threads`

```c
#include <stdio.h>
#include <omp.h>

int main() {
    omp_set_num_threads(4); // Le decimos a OpenMP que use 4 hilos

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        printf("Thread %d out of %d\n", id, omp_get_num_threads());
    }

    return 0;
}
```

**¿Qué hace este código?**

* **`omp_set_num_threads(4)`**: Indica que queremos usar exactamente 4 hilos en la región paralela.
* **`omp_get_thread_num()`**: Dice qué número tiene el hilo que está corriendo.
* **`omp_get_num_threads()`**: Dice cuántos hilos hay en total en esa región.

**¿Cuándo usar esto?**

* Cuando necesitas controlar cuántos núcleos usar, por ejemplo, para no saturar la computadora.
* Cuando estás haciendo pruebas y quieres comparar el rendimiento con diferente cantidad de hilos.

**Ejemplo en palabras:**
Imagina que tienes una cocina con 4 personas disponibles, pero solo quieres que 2 cocinen al mismo tiempo para no chocar entre ellas. Esta función sería como dar esa instrucción: “solo 2 personas a la vez”.

**¿Cuándo *no* usarlo?**

* Si quieres dejar que el sistema decida automáticamente según la cantidad de núcleos disponibles.
* Si no estás seguro cuántos núcleos tiene la computadora que va a ejecutar el programa.

Debes agregar esta parte como una nueva subsección dentro de **"Directivas"**, justo **después de "Control de Hilos"**.

### Directiva `#pragma omp for`

**¿Qué significa esta directiva?**

La directiva `#pragma omp for` permite que **las repeticiones (iteraciones) de un bucle se repartan entre varios hilos**. Así, cada hilo hace una parte del trabajo en lugar de que uno solo lo haga todo.

**¿Para qué sirve esto?**
Sirve para mejorar el rendimiento de tareas repetitivas como recorrer arreglos, hacer sumas o aplicar fórmulas a muchos datos.

#### Ejemplo de uso

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int n = 10;
    int sum = 0;
    int arr[n];

    for (int i = 0; i < n; i++) arr[i] = i + 1;

    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }

    printf("Sum is %d\n", sum);
    return 0;
}
```

**¿Qué hace este código?**

* Llena un arreglo con los números del 1 al 10.
* Usa la directiva `#pragma omp parallel for` para **repartir el bucle `for` entre varios hilos**.
* Cada hilo suma una parte del arreglo.
* La cláusula `reduction(+:sum)` asegura que las sumas de todos los hilos se combinen correctamente al final.

**¿Qué significa `reduction`?**
Cuando varios hilos modifican una misma variable (como `sum`), puede haber errores si todos escriben al mismo tiempo.
La cláusula `reduction` crea una copia temporal para cada hilo y luego las une correctamente al final.

**¿Cuándo usar `#pragma omp for`?**

* Cuando tienes un bucle con muchas repeticiones que se pueden hacer en paralelo.
* Cuando cada repetición es independiente de las otras (por ejemplo, sumar los valores de un arreglo, multiplicar, copiar, etc.).

**Ejemplo en palabras:**
Si tienes 1000 cajas con frutas y necesitas contar cuántas manzanas hay en total, puedes dividir el trabajo entre 4 personas. Cada una contará 250 cajas y luego suman los resultados. Eso es lo que hace esta directiva.

**¿Cuándo *no* usarlo?**

* Cuando las repeticiones dependen unas de otras (por ejemplo, si el cálculo en la vuelta 2 depende del resultado de la vuelta 1).
* Si el bucle es muy pequeño y dividirlo toma más tiempo del que se gana.

### Cláusulas `private` y `shared`

**¿Qué son?**

Cuando usamos varios hilos, hay variables que pueden ser **compartidas entre todos** (como si todos usaran la misma hoja de papel), y otras que deben ser **privadas para cada hilo** (cada quien con su propia copia).

OpenMP permite controlar eso con las cláusulas `private` (privada por hilo) y `shared` (compartida por todos los hilos).

#### Cláusula `private`

**¿Qué hace?**
Crea una **copia separada** de la variable para cada hilo. Así, cada uno la puede modificar sin afectar a los demás.

##### Ejemplo

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int n = 10, i;
    int private_var = 5;

    #pragma omp parallel for private(private_var)
    for (i = 0; i < n; i++) {
        private_var = i;
        printf("Thread %d has private_var = %d\n", omp_get_thread_num(), private_var);
    }

    return 0;
}
```

**¿Qué hace este código?**

* `private_var` se declara fuera del bucle, pero con la cláusula `private(private_var)`, OpenMP le da a **cada hilo su propia copia** de esa variable.
* Dentro del bucle, cada hilo asigna un valor a su copia de `private_var`.
* Como cada hilo tiene su propia versión, no interfieren entre sí.

**¿Por qué es útil?**

Imagina que estás en una clase y todos los estudiantes usan una misma hoja para escribir (variable compartida): el resultado sería un desastre. En cambio, si cada quien tiene su cuaderno (variable privada), pueden trabajar sin molestar a los demás.

#### Cláusula `shared`

Es lo contrario de `private`. La variable es **compartida por todos los hilos**, y **cualquier cambio que haga un hilo afecta a todos**.
Esto se usa cuando los hilos deben colaborar sobre un mismo dato (por ejemplo, llenar un arreglo entre todos).

#### Otros comportamientos comunes con variables

* **`firstprivate`**: Cada hilo obtiene una copia privada, **inicializada con el valor del hilo principal**.
* **`lastprivate`**: Al final del bloque paralelo, **el valor que tenga la variable en el último hilo se copia afuera**, en la variable original.

**¿Cuándo usar `private`?**

* Cuando no quieres que los hilos interfieran entre sí al modificar una variable.
* Ejemplo en palabras: Cada trabajador lleva su propio reloj y mide su propio tiempo, sin depender del reloj central.

**¿Cuándo *no* usar `private`?**

* Si los hilos necesitan comunicarse usando una misma variable.
* Si deseas conservar el valor final después del trabajo paralelo (en ese caso, se puede usar `lastprivate`).

### Cláusula `reduction`

**¿Qué es `reduction`?**

Cuando varios hilos quieren **modificar la misma variable al mismo tiempo**, puede haber errores porque todos están escribiendo a la vez. La cláusula `reduction` **permite hacer operaciones acumulativas de forma segura en paralelo**.

**¿Qué significa acumulativas?**
Suma, producto, máximo, mínimo, etc. Es decir, operaciones donde se combinan muchos valores en un solo resultado.

**¿Por qué es segura?**
Porque OpenMP crea una **copia temporal de la variable para cada hilo**, hace su parte del cálculo, y al final combina los resultados correctamente.

#### Ejemplo de uso con multiplicación

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int n = 10;
    int product = 1;
    int arr[n];

    for (int i = 0; i < n; i++) arr[i] = i + 1;

    #pragma omp parallel for reduction(*:product)
    for (int i = 0; i < n; i++) {
        product *= arr[i];
    }

    printf("Product is %d\n", product);
    return 0;
}
```

**¿Qué hace este código?**

* Llena un arreglo con los números del 1 al 10.
* Usa `reduction(*:product)` para multiplicar todos los números del arreglo en paralelo.
* Cada hilo hace parte de la multiplicación y OpenMP junta los resultados sin errores.

#### ¿Cuándo usar `reduction`?

* Cuando se quiere sumar, multiplicar o aplicar cualquier operación acumulativa entre muchos datos, y se quiere hacer en paralelo sin problemas.
* Funciona con: `+`, `*`, `-`, `&`, `|`, `^`, `&&`, `||`.

**Ejemplo en palabras:**
Si cada persona multiplica un grupo de números por separado y luego todos juntan sus resultados, eso es lo que hace `reduction`.

#### ¿Cuándo *no* usarlo?

* Si la operación no es acumulativa o no puede combinarse fácilmente (por ejemplo, escribir texto o modificar estructuras complejas).
* Si necesitas conservar todos los pasos intermedios, no solo el resultado final.

### Secciones (`sections`) y Trabajo en Equipo

**¿Qué es `sections`?**

La directiva `#pragma omp sections` permite que **cada hilo realice una tarea diferente**. A diferencia de `for`, donde todos los hilos repiten la misma acción con distintos datos, aquí **cada sección hace algo distinto**.

Es útil cuando tienes varias tareas independientes y quieres que se ejecuten al mismo tiempo, como si cada hilo tomara un trabajo distinto de una lista.

#### Ejemplo de uso

```c
#include <stdio.h>
#include <omp.h>

int main() {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            printf("Section 1 executed by thread %d\n", omp_get_thread_num());
        }

        #pragma omp section
        {
            printf("Section 2 executed by thread %d\n", omp_get_thread_num());
        }
    }

    return 0;
}
```

**¿Qué hace este código?**

* Crea dos secciones.
* Cada sección será ejecutada por **un hilo diferente**.
* Los hilos trabajan en paralelo, pero **haciendo cosas distintas**.

#### ¿En qué se diferencia de `for`?

* **`for`** se usa cuando **todos los hilos hacen lo mismo** con diferentes partes de un arreglo o bucle.
* **`sections`** se usa cuando **cada hilo hace algo distinto**, como ejecutar una función diferente o trabajar en otra parte del problema.

**Ejemplo en palabras:**

Imagina que estás en una cocina. Si usas `for`, todos pelan papas al mismo tiempo (la misma tarea dividida). Si usas `sections`, uno pela papas, otro corta carne, y otro lava platos (tareas diferentes, al mismo tiempo).

#### ¿Cuándo usar `sections`?

* Cuando tienes varias tareas distintas pero independientes, y quieres que se ejecuten en paralelo.
* Por ejemplo, si un hilo debe cargar datos, otro debe procesarlos, y otro debe escribirlos en disco.
* Úsalo si tienes pocas tareas distintas, como máximo una por hilo.

#### ¿Cuándo *no* usar `sections`?

* Si todas las tareas son iguales o puedes dividirlas fácilmente con `for`.
* Si las tareas dependen unas de otras en orden, porque se ejecutan en paralelo y eso puede causar errores si no se sincronizan.

**¿Es equivalente a `parallel for`?**

No exactamente. Aunque ambos crean trabajo paralelo, su uso es distinto:

* `parallel for`: divide un bucle en partes iguales.
* `sections`: divide el código en **bloques diferentes de trabajo**.
* No lo uses si quieres repartir muchas repeticiones o balancear carga entre hilos.

## Sincronización en OpenMP

Cuando se trabaja con múltiples hilos, es importante controlar **cuándo y cómo acceden a datos compartidos**. OpenMP ofrece varias herramientas para asegurar que los resultados sean correctos y que no ocurran errores por conflictos de acceso.

### `critical`: Sección exclusiva para un solo hilo

**¿Qué hace?**
Permite que **solo un hilo a la vez** ejecute una parte del código. Es útil cuando varios hilos necesitan modificar una variable compartida.

```c
#pragma omp parallel
{
    #pragma omp critical
    {
        // Código que debe ser ejecutado por un solo hilo a la vez
    }
}
```

**Ejemplo en palabras:**
Como si todos los hilos quisieran escribir en una misma hoja, pero se turnan para evitar escribir encima de otros.

**¿Cuándo usarlo?**
Cuando se modifica una variable compartida y no se puede usar `reduction` o `atomic`.

**¿Cuándo no usarlo?**
Si el bloque es muy corto o muy usado, puede crear cuellos de botella (ralentiza el programa).

### `atomic`: Operación segura y más rápida sobre una variable

**¿Qué hace?**
Asegura que una **operación simple sobre una variable** (como `sum++`) sea ejecutada de forma segura por un solo hilo a la vez.

```c
int sum = 0;

#pragma omp parallel
{
    #pragma omp atomic
    sum++;
}
```

**¿Diferencia con `critical`?**
`atomic` es más **rápido**, pero solo funciona para operaciones simples como suma, resta, multiplicación.

### `barrier`: Esperar a que todos lleguen

**¿Qué hace?**
Hace que todos los hilos se **detengan y esperen** hasta que todos hayan llegado a ese punto.

```c
#pragma omp parallel
{
    // Código paralelo

    #pragma omp barrier

    // Código que se ejecuta después de que todos los hilos llegan
}
```

**Ejemplo en palabras:**
Como en una carrera donde nadie puede avanzar a la siguiente etapa hasta que todos los corredores lleguen a la meta anterior.

### `flush`: Coherencia entre hilos

**¿Qué hace?**
Fuerza a que los valores compartidos entre hilos estén **actualizados y visibles para todos**.

```c
int flag = 0;

#pragma omp parallel
{
    if (omp_get_thread_num() == 0) {
        flag = 1;
        #pragma omp flush(flag)
    } else {
        int local_flag;
        #pragma omp flush(flag)
        local_flag = flag;
    }
}
```

**¿Cuándo usarlo?**
Cuando un hilo cambia una variable y otros deben ver ese cambio inmediatamente.

**Ejemplo en palabras:**
Como si un jugador activa un interruptor y los demás deben ver la luz encendida; `flush` asegura que todos vean el cambio.

Este contenido debe agregarse como una **nueva sección al mismo nivel que “Directivas” y “Sincronización”**, con el título:

## Variables de Entorno en OpenMP

OpenMP permite controlar varios aspectos del comportamiento del programa mediante **variables de entorno**. Estas variables se configuran antes de ejecutar el programa, y afectan cómo se usa el paralelismo, cuántos hilos se crean, cómo se distribuyen las tareas, entre otros.

### Ejemplo básico: `OMP_NUM_THREADS`

```c
#include <stdio.h>
#include <omp.h>

int main() {
    // Obtener el número de hilos disponibles
    int num_threads = omp_get_max_threads();
    printf("Número de hilos disponibles: %d\n", num_threads);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        printf("Hola desde el hilo %d\n", thread_id);
    }

    return 0;
}
```

#### Cómo compilar y ejecutar

```bash
gcc -fopenmp omp_env_example.c -o omp_env_example
export OMP_NUM_THREADS=4
./omp_env_example
```

### ¿Qué hace `OMP_NUM_THREADS`?

Controla **cuántos hilos** se usarán en las regiones paralelas.

**Ejemplo en palabras:**
Es como decirle al equipo cuántas personas deben trabajar al mismo tiempo. Si tienes 8 disponibles pero dices `4`, solo trabajarán 4.

### Otras variables útiles

#### `OMP_SCHEDULE`

Controla **cómo se reparten las iteraciones de un bucle** en paralelo.
Por ejemplo:

```bash
export OMP_SCHEDULE="dynamic,2"
```

* `"static"`: las tareas se asignan de forma fija.
* `"dynamic"`: las tareas se asignan a medida que los hilos están disponibles.
* `"guided"`: parecido al dinámico, pero empieza con tareas grandes que se van reduciendo.

#### `OMP_DYNAMIC`

Permite que OpenMP **ajuste automáticamente el número de hilos** dependiendo de la carga del sistema.

```bash
export OMP_DYNAMIC=true
```

* Puede ser útil si no quieres saturar la computadora.

#### `OMP_NESTED`

Activa el **paralelismo anidado**, es decir, permitir que dentro de una región paralela, haya otra región paralela.

```bash
export OMP_NESTED=true
```

* Solo tiene sentido si tu código crea regiones paralelas dentro de otras.

#### `OMP_MAX_ACTIVE_LEVELS`

Define **cuántos niveles de regiones paralelas** puedes tener activas al mismo tiempo.

```bash
export OMP_MAX_ACTIVE_LEVELS=3
```

* Se usa en programas complejos con muchas capas de paralelismo.

#### `OMP_THREAD_LIMIT`

Establece el **número máximo de hilos totales** que puede usar la aplicación, sin importar cuántos se pidan en una región.

```bash
export OMP_THREAD_LIMIT=16
```

#### `OMP_STACKSIZE`

Define el **tamaño de pila para cada hilo**, en bytes.

```bash
export OMP_STACKSIZE=4M
```

* Es útil cuando se crean muchas variables dentro de los hilos y necesitas más memoria por hilo.

#### `OMP_WAIT_POLICY`

Controla cómo esperan los hilos cuando no están activos:

```bash
export OMP_WAIT_POLICY=passive
```

* `"active"`: los hilos siguen activos, consumen más CPU.
* `"passive"`: los hilos descansan mientras esperan, usan menos energía.

#### `OMP_PROC_BIND`

Define **cómo se asignan los hilos a los núcleos del procesador**.

```bash
export OMP_PROC_BIND=spread
```

* `"true"`: los hilos se mantienen en un mismo procesador.
* `"spread"`: se distribuyen en diferentes núcleos para mejor rendimiento.

**¿Cuándo usar variables de entorno?**

* Cuando quieres **controlar el comportamiento global** de OpenMP sin modificar el código fuente.
* Para **probar diferentes configuraciones de rendimiento** fácilmente.

Puedes dividir este contenido en dos nuevas **secciones al mismo nivel que “Directivas”, “Sincronización”, y “Variables de Entorno”**, con los títulos:

---

## Programación Anidada y Dinámica

OpenMP permite crear **regiones paralelas dentro de otras regiones paralelas**, lo que se conoce como **paralelismo anidado**. Esto es útil cuando cada tarea principal también puede dividirse en subtareas que pueden ejecutarse en paralelo.

---

### Ejemplo: Paralelismo Anidado

```c
#include <stdio.h>
#include <omp.h>

int main() {
    omp_set_nested(1);     // Activar paralelismo anidado
    omp_set_dynamic(1);    // Permitir número de hilos dinámico

    #pragma omp parallel num_threads(2)
    {
        printf("Outer thread %d\n", omp_get_thread_num());

        #pragma omp parallel num_threads(2)
        {
            printf("Inner thread %d, outer thread %d\n",
                   omp_get_thread_num(), omp_get_ancestor_thread_num(1));
        }
    }
    return 0;
}
```

**¿Qué hace este código?**

* Crea una región paralela con 2 hilos (“outer”).
* Cada hilo de esa región crea otra región paralela con 2 hilos más (“inner”).
* En total se ejecutan 4 hilos internos, 2 por cada hilo externo.

**¿Cuándo usarlo?**

* Cuando cada hilo principal necesita trabajar con más subtareas en paralelo.
* Ejemplo en palabras: si tienes dos equipos de personas, y dentro de cada equipo se divide el trabajo otra vez.

## Estrategias de Balanceo de Carga (`schedule`)

Cuando usas `parallel for`, OpenMP permite definir cómo se reparten las iteraciones del bucle entre los hilos. Esto se controla con la cláusula `schedule`.

### Sintaxis general

```c
#pragma omp parallel for schedule(tipo, tamaño)
```

Donde `tipo` puede ser `static`, `dynamic`, `guided` o `auto`.

### `static`

Divide el trabajo **equitativamente desde el inicio**.

```c
#pragma omp parallel for schedule(static, 4)
for (int i = 0; i < N; i++) {
    // Código del bucle
}
```

* Cada hilo recibe **bloques fijos** de iteraciones.
* Bueno si el trabajo por iteración es uniforme.

### `dynamic`

Asigna el trabajo **en bloques conforme los hilos están disponibles**.

```c
#pragma omp parallel for schedule(dynamic, 4)
for (int i = 0; i < N; i++) {
    // Código del bucle
}
```

* Un hilo toma un bloque, y cuando termina, toma otro.
* Útil si las iteraciones **tienen tiempos variables**.

### `guided`

Empieza asignando **bloques grandes**, luego los reduce poco a poco.

```c
#pragma omp parallel for schedule(guided, 4)
for (int i = 0; i < N; i++) {
    // Código del bucle
}
```

* Buen balance entre sobrecarga y rendimiento.
* Ideal cuando hay muchas iteraciones pesadas al inicio.

### `auto`

Deja que OpenMP **elija automáticamente** la mejor estrategia según el sistema.

```c
#pragma omp parallel for schedule(auto)
for (int i = 0; i < N; i++) {
    // Código del bucle
}
```

* No necesitas preocuparte por el tipo de reparto.
* Útil cuando no conoces bien el comportamiento de tu código.

### Ejemplo con `static`

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int n = 1000;
    int arr[n];
    int sum = 0;

    for (int i = 0; i < n; i++) arr[i] = i + 1;

    #pragma omp parallel for schedule(static, 50)
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }

    printf("Sum with static schedule is %d\n", sum);
    return 0;
}
```

**¿Cuándo usar cada uno?**

| Estrategia | Cuándo usarla                                |
| ---------- | -------------------------------------------- |
| `static`   | Iteraciones similares y predecibles          |
| `dynamic`  | Iteraciones variables o impredecibles        |
| `guided`   | Mezcla de tareas pesadas y ligeras           |
| `auto`     | No sabes cuál usar o dejas que OpenMP decida |

## Consideraciones y Buenas Prácticas en OpenMP

La programación paralela con OpenMP permite mejorar el rendimiento, pero también introduce riesgos que deben manejarse correctamente. A continuación, se presentan consejos clave para escribir código paralelo confiable y eficiente.

### Comprensión de las Dependencias de Datos

Antes de paralelizar un bucle o una sección de código, es necesario identificar si existen **dependencias de datos**.

**¿Qué es una dependencia de datos?**  
Es cuando una instrucción **necesita el resultado de otra instrucción anterior**.

```c
for (int i = 1; i < N; i++) {
    arr[i] = arr[i - 1] + 1;  // No se puede paralelizar
}
```

Aquí, cada paso depende del valor anterior, así que **no se puede ejecutar en paralelo sin modificar la lógica**.

**¿Qué pasa si ignoro estas dependencias?**

Puedes provocar **condiciones de carrera**, errores donde los resultados varían cada vez que se ejecuta el programa, y que son difíciles de detectar y depurar.

### Uso Eficiente de Recursos

**Más hilos no siempre es mejor.** Usar demasiados puede causar:

* Contención de memoria
* Mayor uso del sistema
* Pérdida de rendimiento

**Recomendación:** Usa solo los hilos necesarios y prueba distintas configuraciones con `OMP_NUM_THREADS`.

**Ejemplo con afinidad:**

```bash
export OMP_PROC_BIND=true
```

Esto puede ayudar a que los hilos **se mantengan en el mismo núcleo**, reduciendo el tiempo de espera.

### Manejo Adecuado de la Sincronización

Herramientas como `critical`, `atomic` y `barrier` **deben usarse con precaución**:

* ✅ Úsalas para **proteger datos compartidos**.
* ❌ No las uses en exceso, porque **ralentizan el programa** y reducen el paralelismo.

**Ejemplo malo (innecesario):**

```c
#pragma omp parallel
{
    #pragma omp critical
    printf("Hola\n");  // No necesita critical, ya que no hay conflicto real
}
```

### Pruebas y Depuración Exhaustiva

Los errores en programas paralelos pueden ser difíciles de detectar. Por eso, debes hacer pruebas desde el principio y con distintas configuraciones de hilos.

## Depuración de Programas con OpenMP

La depuración de código paralelo presenta desafíos únicos debido a la **concurrencia y sincronización**. Algunos errores comunes:

### Condiciones de Carrera

Ocurren cuando varios hilos **modifican o leen una misma variable sin control**. El resultado es impredecible.

### Bloqueos Mutuos (Deadlocks)

Suceden cuando dos o más hilos se **esperan entre sí** y ninguno avanza.

### Interbloqueos (Livelocks)

Los hilos están activos pero **no progresan**, reaccionando continuamente al estado de otros hilos.

## Herramientas y Técnicas de Depuración

### `GDB`

Puede inspeccionar el estado de hilos y variables. No está optimizado para paralelismo, pero es útil.

```bash
gdb ./mi_programa
```

### `Intel Inspector`

Herramienta gráfica que detecta **condiciones de carrera y errores de memoria** en código OpenMP.

### `Valgrind`

Útil para detectar:

* Fugas de memoria
* Lecturas o escrituras fuera de los límites
* Problemas de acceso concurrente

## Estrategias para Detectar y Resolver Problemas

* **Aislar el problema**: Trabaja sobre una parte pequeña del código.
* **Revisar la sincronización**: Asegúrate de proteger bien todas las variables compartidas.
* **Condiciones controladas**: Limita el número de hilos y controla el flujo para facilitar la depuración.
