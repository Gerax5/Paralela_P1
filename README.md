# Voronoi Stippling — Secuencial & Paralelo

Demo de **Voronoi Stippling** usando el **algoritmo de Lloyd** para distribuir puntos según la luminancia de una imagen.
Incluye versión **secuencial** y **paralela (OpenMP)** para pruebas de rendimiento.

## 📂 Estructura del proyecto

```bash
.
├── Makefile
├── README.md
├── build/              # binarios compilados
├── docs/               # documentación y reportes
├── images/             # imágenes de entrada/salida
├── include/            # headers (.h)
├── src/                # código fuente (.c)
└── tests/              # scripts y benchmarks
```

## 📦 Requisitos e Instalación

### Requisitos

* Ubuntu / WSL (o Linux con GCC)
* Paquetes de desarrollo de **SDL2**
* Dependencias para compilación y pruebas

### Instalación

1. Asegúrate de tener las utilidades necesarias:

```bash
sudo apt-get install dos2unix
dos2unix tests/*.sh
```

2. Instala dependencias:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev libsdl2-image-dev fonts-jetbrains-mono libsdl2-ttf-dev
```

3. Da permisos de ejecución a los scripts de prueba:

```bash
chmod +x tests/*.sh
```

## ⚙️ Compilación

Compilar con:

```bash
make clean && make
```

Esto genera:

* `build/bin/stippling_seq` -> versión secuencial
* `build/bin/stippling_par` -> versión paralela (OpenMP)

## ▶️ Ejecución interactiva

Ejemplo secuencial:

```bash
./build/bin/stippling_seq -n 5000 images/input/doge.png
```

Ejemplo paralelo (8 hilos):

```bash
OMP_NUM_THREADS=8 ./build/bin/stippling_omp -n 5000 images/input/doge.png
```

### Controles de teclado

* `SPACE` -> una iteración de Lloyd
* `A` -> auto-run ON/OFF
* `- / + / =` -> step (pixelStride) -/+
* `G / H` -> gamma -/+
* `B` -> mostrar/ocultar fondo
* `Z / X` -> radio visual fijo -/+
* `N / M` -> minRadius -/+
* `, / .` -> maxRadius -/+
* `C` -> color ON/OFF
* `I` -> invertir tema (claro/oscuro)
* `R` -> resembrar puntos (misma N, nueva semilla)
* `O / U` -> siguiente / anterior fondo
* `P` -> guardar screenshot en `images/output/`
* `ESC` -> salir

## 🌐 Variables de entorno

### Ejecución general

* `STIPPLE_AUTORUN=1` -> auto-run ON (default = 0)
* `STIPPLE_MAX_ITERS=N` -> parar tras N iteraciones
* `STIPPLE_METRICS=out.csv` -> guardar CSV con métricas

### Puntos y semilla

* `STIPPLE_NPOINTS=N` -> número inicial de puntos
* `STIPPLE_SEED=N` -> semilla para reseed (default: 12345)

### Gamma

* `STIPPLE_GAMMA_START=g0` -> gamma inicial al arrancar (si no se define, usa `defaultGamma`).
* `STIPPLE_GAMMA_END=g1` -> gamma objetivo para "sweep"; **junto con** `STIPPLE_GAMMA_STEP` activa el barrido.
* `STIPPLE_GAMMA_STEP=dg` -> incremento/decremento por salto del sweep (positivo sube, negativo baja).
* `STIPPLE_GAMMA_EVERY=k` -> aplica el cambio de gamma cada `k` iteraciones (default: `1`).

### Fondo / imágenes

* `STIPPLE_BG_SECONDS=s` -> cambiar fondo cada s segundos (0 = desactivar)

### Color / tema

* `STIPPLE_COLOR=1|0` -> con/sin color (default: 1)
* `STIPPLE_INVERT=1|0` -> invertir tema (default: 0)

### Paralelismo (solo versión omp)

* `OMP_NUM_THREADS=N` -> número de hilos

## 📊 Benchmarks

En `tests/` tienes scripts para automatizar pruebas:

### 🔁 Loop (`bench.sh`)

Ejecuta secuencial y paralelo para distintos valores de **N**.

```bash
./tests/bench.sh --n-start 2000 --n-end 6000 --n-step 2000 --iters 100 --img images/input/doge.png
```

👉 CSV generados en:

* `tests/bench/seq/run_NXXXX.csv`
* `tests/bench/par/run_NXXXX.csv`

Incluyen:

```csv
iter,ms,step,gamma,npoints
1,6.540,3,1.000,3000
...
```

Parámetros editables vía entorno en el script:
`gamma`, `color`, `minR`, `maxR`, `seed`, `iters`.

### 🔹 Ejecución única (`bench_once.sh`)

Ejecuta solo un valor de **N**, con timestamp en el nombre del CSV.

```bash
./tests/bench_once.sh --n 4000 --iters 50 --img images/input/doge.png
```

👉 Resultados en `tests/bench_once/{seq,par}/run_N4000_<fecha>.csv`

### 📐 Comparación (`compare.sh`)

Compara dos CSV y muestra speedup:

```bash
./tests/compare.sh tests/bench/seq/run_N2000.csv tests/bench/par/run_N2000.csv
```

Salida con colores:

```bash
Promedio ms (SEQ): 6.550
Promedio ms (OMP): 4.230
Speedup SEQ/OMP  : 1.55x
```
