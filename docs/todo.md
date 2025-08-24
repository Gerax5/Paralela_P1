# Plan integral del proyecto — *Voronoi Stippling* (Secuencial + OpenMP)

## 1. Estructura del repositorio (árbol)

```bash
.
├── Makefile
├── README.md
├── docs/
│   ├── guia.pdf
│   ├── voronoi_stippling.md
│   ├── voronoi_stippling_explication.md
│   └── ...
├── include/
│   ├── app.h
│   ├── config.h            # DEFAULT_THREADS, DEFAULT_SCHEDULE, etc.
│   ├── image.h
│   ├── lloyd.h
│   ├── render_sdl.h
│   ├── stippling.h
│   └── voronoi.h
├── src/
│   ├── main.c              # parse args (N, T, S, IMG), loop, FPS
│   ├── app.c               # orquestación por iteración
│   ├── image.c             # carga PNG/JPG (SDL2_image) y muestreo
│   ├── lloyd.c             # centros de masa ponderados
│   ├── stippling.c         # estado de puntos, radios, init
│   ├── voronoi.c           # asignación píxel→punto (tileado)
│   ├── render_sdl.c        # dibujo puntos/overlay FPS con SDL2
│   └── util.c              # timers, clamps, RNG
├── images/
│   ├── input/
│   │   ├── lena.png
│   │   └── uvg.png
│   └── output/
│       ├── seq/
│       └── omp/
├── tests/
│   ├── run_grid.sh         # barrido N×hilos, guarda CSV
│   ├── compare.sh          # speedup/eficiencia
│   ├── perf.sh             # 10+ mediciones repetibles
│   └── data/
│       └── baseline.csv
└── build/
    ├── obj/                # .o
    └── bin/                # ejecutables (seq/omp)
```

## 2. Descripción de carpetas/archivos

- **Makefile**: reglas de compilación (seq/omp), `clean`, `run`, variables `OMP_NUM_THREADS`.
- **README.md**: qué hace, cómo compilar/ejecutar (WSL), controles, ejemplos.
- **docs/**: guía del curso y documentación técnica (stipple/Voronoi/OpenMP).
- **include/**: headers públicos (APIs de cada módulo).
- **src/**:

  - `main.c`: parsea `N` (y opciones), inicia SDL, bucle, muestra FPS.
  - `app.c`: ejecuta *una iteración* (asignar→acumular→actualizar→render).
  - `image.c`: carga PNG/JPG (o genera campo sintético), provee `sample(x,y)`.
  - `lloyd.c`: centros de masa ponderados por intensidad.
  - `stippling.c`: init y actualización de puntos (pos/radio).
  - `voronoi.c`: asignación píxel→stipple (dist mínima; versión tileada).
  - `render_sdl.c`: dibuja puntos/overlay de métricas; color/gradientes.
  - `util.c`: cronómetro de alta resolución, helpers.
- **images/**:

  - `input/`: imágenes de prueba.
  - `output/seq|omp`: capturas PNG, CSV de FPS y tiempo.
- **tests/**:

  - `run_grid.sh`: corre (N={500..10000}) × (hilos={1,2,4,8}) → `perf.csv`.
  - `compare.sh`: calcula speedup, eficiencia, promedia 10 corridas.
  - `perf.sh`: lanza baterías y ordena resultados.
- **build/**:

  - `obj/`: objetos por módulo.
  - `bin/`: `stippling_seq`, `stippling_omp`.

## 3. Dependencias y entorno (WSL/Ubuntu)

Instalar compilador + SDL2:

```bash
sudo apt-get install build-essential
sudo apt-get install libsdl2-dev
sudo apt-get install libsdl2-image-dev
```

Uso:

- **GCC/OpenMP**: se activa con `-fopenmp`.
- **SDL2**: se enlaza con `sdl2-config --cflags --libs`.
- **SDL2\_image**: necesaria para cargar PNG/JPG.

  - Se añade con `pkg-config --cflags --libs SDL2_image`.
  - Enlace final incluye: `-lSDL2_image -lm`.

- Definir constantes por defecto en `include/config.h`:

  ```c
  #define DEFAULT_THREADS 4
  #define DEFAULT_SCHEDULE "dynamic"
  ```

- Y sobreescribirlas con argumentos en tiempo de ejecución (`./stippling_omp 3000 8 dynamic`).

## 4. Makefile mínimo (OpenMP + SDL2 + SDL2\_image)

```make
# --- Config ---
CC        := gcc
CSTD      := -std=c11
WARN      := -Wall -Wextra -Wshadow -Wconversion
OPT       := -O2
SDL_CFLAGS:= $(shell sdl2-config --cflags) $(shell pkg-config --cflags SDL2_image)
SDL_LIBS  := $(shell sdl2-config --libs) $(shell pkg-config --libs SDL2_image) -lm
INC       := -Iinclude

SRC_DIR   := src
OBJ_DIR   := build/obj
BIN_DIR   := build/bin

SRC_COMMON:= main.c app.c image.c lloyd.c stippling.c voronoi.c render_sdl.c util.c
SRC_PATHS := $(addprefix $(SRC_DIR)/,$(SRC_COMMON))
OBJ_SEQ   := $(addprefix $(OBJ_DIR)/,$(SRC_COMMON:.c=.seq.o))
OBJ_OMP   := $(addprefix $(OBJ_DIR)/,$(SRC_COMMON:.c=.omp.o))

BIN_SEQ   := $(BIN_DIR)/stippling_seq
BIN_OMP   := $(BIN_DIR)/stippling_omp

CFLAGS_SEQ:= $(CSTD) $(WARN) $(OPT) $(INC) $(SDL_CFLAGS)
CFLAGS_OMP:= $(CFLAGS_SEQ) -fopenmp

# --- Targets ---
.PHONY: all seq omp run clean dirs

all: dirs $(BIN_SEQ) $(BIN_OMP)

seq: dirs $(BIN_SEQ)
omp: dirs $(BIN_OMP)

dirs:
 @mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Objetos
$(OBJ_DIR)/%.seq.o: $(SRC_DIR)/%.c
 $(CC) $(CFLAGS_SEQ) -c $< -o $@

$(OBJ_DIR)/%.omp.o: $(SRC_DIR)/%.c
 $(CC) $(CFLAGS_OMP) -c $< -o $@

# Enlace
$(BIN_SEQ): $(OBJ_SEQ)
 $(CC) $^ $(SDL_LIBS) -o $@

$(BIN_OMP): $(OBJ_OMP)
 $(CC) $^ $(SDL_LIBS) -fopenmp -o $@

# Ejecutar: make run N=3000 T=8 S=guided
N ?= 3000
T ?= 4
S ?= dynamic

run: omp
 @echo "Ejecutando con N=$(N), Threads=$(T), Schedule=$(S)"
 @$(BIN_OMP) $(N) $(T) $(S)

clean:
 @rm -rf build
```

## 5. Pipeline de trabajo (TODO)

### A. Fase Secuencial (baseline)

1. **Entrada**: `N` (puntos), imagen opcional (`PNG/JPG`).
2. **Inicialización**: puntos aleatorios y carga de imagen (`SDL2_image`).
3. **Iteración Lloyd**: asignación → acumulación → actualización.
4. **Render**: puntos y FPS en pantalla.
5. **Medición**: CSV y capturas en `images/output/seq`.

### B. Fase Paralela (OpenMP)

1. **Asignación paralela**: `#pragma omp parallel for schedule(dynamic)` (píxeles o tiles).
2. **Reducción segura**: `reduction(+:sumX,sumY,sumW)` o buffers privados por hilo.
3. **Actualización**: `parallel for` por punto.
4. **Optimización**: tiling, `schedule(guided)`, evitar *false sharing*.
5. **Render**: siempre en hilo principal.
6. **Validación**: comparar con baseline secuencial.

### C. Pruebas

- `tests/run_grid.sh`: barrido N × threads, 10 repeticiones → CSV.
- `tests/compare.sh`: calcula speedup y eficiencia.
- Resultados guardados en `images/output/`.

### D. Validación

- Correctitud: error medio < ε px entre seq y omp.
- Rendimiento: FPS, ms/frame, speedup.
- Requisitos: N param, FPS en pantalla, ≥640×480, Lloyd’s + ponderación.

## 6. Checklist final

- [x] **Estructura de repo creada.**

- [ ] **Docs colocados en `docs/`.**

- [x] **Dependencias instaladas:**

  - [x] `build-essential`
  - [x] `libsdl2-dev`
  - [x] `libsdl2-image-dev`

- [ ] **Baseline secuencial:**

  - [ ] Args (`N`, `IMG`, `T`, `S`). *(solo `IMG` por argv\[1])*
  - [x] Imagen cargada con `SDL2_image`.
  - [x] Iteración Lloyd implementada.
  - [x] FPS en pantalla.

- [ ] **Versión OpenMP:**

  - [ ] Paralelización asignación/centroides.
  - [ ] Reducciones o buffers privados.
  - [ ] Schedules probados (`dynamic`, `guided`).

- [ ] **Pruebas:**

  - [ ] Scripts de performance. *(\*archivos existen pero sin código)*
  - [ ] CSV con ≥10 mediciones. *(\*no generado)*
  - [ ] Capturas seq vs omp. *(\*solo tienes secuencial)*

- [ ] **Documentación:**

  - [ ] README actualizado. *(\*falta detallar compilación/controles/CLI)*
  - [ ] Diagrama de flujo + catálogo funciones.
  - [ ] Bitácora de pruebas.
