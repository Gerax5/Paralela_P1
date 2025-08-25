# Voronoi Stippling (Secuencial)

Pequeña demo interactiva de **stipple** usando **Algoritmo de Lloyd** sobre una imagen.
La densidad de puntos se guía por la **luminancia** (zonas oscuras atraen más puntos).
Render y entrada con **SDL2**, carga de imágenes con **SDL2_image**.

## Requisitos

- Ubuntu / WSL (o Linux con GCC)
- Paquetes de desarrollo de SDL2

### Instalar dependencias

```bash
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev libsdl2-image-dev fonts-jetbrains-mono libsdl2-ttf-dev
```

## Compilar

### Línea directa (sin Makefile)

```bash
mkdir -p build/bin
gcc -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L src/main.c src/app.c src/image.c src/stippling.c src/lloyd.c src/voronoi.c src/utils.c -Iinclude $(sdl2-config --cflags) $(pkg-config --cflags SDL2_image SDL2_ttf) -o build/bin/stippling_demo $(sdl2-config --libs) $(pkg-config --libs SDL2_image SDL2_ttf) -lm
```

> Nota: `-D_POSIX_C_SOURCE=200809L` habilita `strdup` en GCC/GLIBC.

### Línea directa (con Makefile)

```bash
mkdir -p build/bin
make run
```

## Ejecutar

### Modo normal (interactivo)

```bash
./build/bin/stippling_demo -n 3000
```

- `-n 3000` → número de puntos iniciales (opcional).
- Si omites la imagen, usa la ruta por defecto de `include/config.h` (`defaultImagePath`).

**Controles (teclado):**

- `SPACE` → una iteración de Lloyd
- `A` → auto-run ON/OFF
- `-` / `+` (también keypad) → disminuir/aumentar `step` (pixelStride)
- `G` / `H` → subir/bajar `gamma`
- `B` → mostrar/ocultar fondo (imagen)
- `Z` / `X` → radio de punto -/+
- `R` → resembrar puntos (misma N, nueva semilla)
- `P` → guardar screenshot PNG del frame actual (en `images/output/`)
- `U` / `O` → anterior/siguiente `fondo`
- `ESC` → salir

**Salidas:**

- **Capturas**: `images/output/stipple_XXXXX.png` (cuando presionas `P`)

## Modo test (batch) y recolección de métricas

Permite correr iteraciones automáticamente y volcar **tiempos por iteración** a CSV.

> Si **no** defines `STIPPLE_METRICS`, **no** se guarda ningún CSV (modo silencioso).

### Ejecución básica (auto-run + CSV)

```bash
mkdir -p images/output/seq
STIPPLE_AUTORUN=1 \
STIPPLE_MAX_ITERS=1000 \
STIPPLE_METRICS=images/output/seq/metrics.csv \
./build/bin/stippling_demo -n 3000 images/input/twitch.png
```

**Variables de entorno soportadas:**

| Variable            | Ejemplo                         | Descripción                                            |
| ------------------- | ------------------------------- | ------------------------------------------------------ |
| `STIPPLE_AUTORUN`   | `1`                             | Ejecuta una iteración de Lloyd en cada frame.          |
| `STIPPLE_MAX_ITERS` | `1000`                          | Finaliza tras `K` iteraciones (ideal para benchmarks). |
| `STIPPLE_METRICS`   | `images/output/seq/metrics.csv` | Activa logging de métricas por iteración.              |

**Formato del CSV**
Encabezado + filas por iteración:

```csv
iter,ms,step,gamma,npoints
1,12.444,3,1.000,3000
2,12.887,3,1.000,3000
...
```

- `iter`: número de iteración (1..K)
- `ms`: tiempo de esa iteración (milisegundos)
- `step`: `pixelStride` usado
- `gamma`: gamma en ese instante
- `npoints`: cantidad de puntos

## Modo test avanzado: **sweep de gamma** automático

Puedes variar `gamma` automáticamente cada N iteraciones usando variables de entorno.

> Esto modifica **gamma durante la ejecución** y el valor queda **registrado** en el CSV.

```bash
mkdir -p images/output/seq
STIPPLE_AUTORUN=1 \
STIPPLE_MAX_ITERS=120 \
STIPPLE_METRICS=images/output/seq/metrics.csv \
STIPPLE_GAMMA_START=1.0 \
STIPPLE_GAMMA_END=1.8 \
STIPPLE_GAMMA_STEP=0.05 \
STIPPLE_GAMMA_EVERY=10 \
./build/bin/stippling_demo -n 3000 images/input/twitch.png
```

**Variables del sweep de gamma:**

| Variable              | Requerido | Ejemplo | Significado                                                   |
| --------------------- | --------- | ------- | ------------------------------------------------------------- |
| `STIPPLE_GAMMA_END`   | ✔         | `1.8`   | Gamma objetivo final del sweep.                               |
| `STIPPLE_GAMMA_STEP`  | ✔         | `0.05`  | Incremento (o decremento, si negativo) por salto.             |
| `STIPPLE_GAMMA_START` | ✖         | `1.0`   | Valor inicial (si no se define, parte del gamma actual).      |
| `STIPPLE_GAMMA_EVERY` | ✖         | `10`    | Aplica el cambio de gamma cada `N` iteraciones (default = 1). |

**Ejemplo explicado:**

Con los valores de arriba, gamma avanza:

`1.00, 1.05, 1.10, ...` **cada 10 iteraciones**, hasta `1.80`.

En el CSV verás cómo `gamma` cambia en las filas correspondientes.

> Nota: también puedes usar valores grandes (p. ej. `STIPPLE_GAMMA_END=5.0`), pero no suele ser útil visualmente.

## Notas técnicas

- Muestreo de luminancia **bilineal** en UV + conversión **sRGB → lineal** para ponderar correctamente.
- Búsqueda de vecino más cercano acelerada con **grilla uniforme**.
- `step` controla el stride de muestreo (mayor = más rápido, menos preciso).
- `gamma > 1` acentúa sombras (más puntos en zonas oscuras).
