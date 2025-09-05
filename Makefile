# =========================
# Proyecto Voronoi Stippling
# Compila binarios: secuencial y paralelo (OpenMP)
# =========================

# --- Detectar hilos disponibles (para ejecución) ---
THREADS ?= $(shell \
  (getconf _NPROCESSORS_ONLN) 2>/dev/null || \
  (nproc) 2>/dev/null || \
  (sysctl -n hw.ncpu) 2>/dev/null || echo 8)

# --- Binarios ---
BIN_DIR      := build/bin
OBJ_DIR      := build/obj
SEQ_BIN      := $(BIN_DIR)/stippling_seq
OMP_BIN      := $(BIN_DIR)/stippling_par

# --- Fuentes comunes ---
COMMON_SRCS  := src/main.c src/app.c src/image.c src/utils.c

# --- Fuentes específicas ---
SEQ_SRCS     := src/lloyd.c src/voronoi.c src/stippling.c
OMP_SRCS     := src/lloyd_parallel.c src/voronoi_parallel.c src/stippling_parallel.c

# --- Objetos (separados por variante) ---
SEQ_OBJS     := $(patsubst src/%.c,$(OBJ_DIR)/seq/%.o,$(COMMON_SRCS) $(SEQ_SRCS))
OMP_OBJS     := $(patsubst src/%.c,$(OBJ_DIR)/omp/%.o,$(COMMON_SRCS) $(OMP_SRCS))

# --- Compilador y flags ---
CC           := gcc
CSTD         := -std=c11
WARN         := -Wall -Wextra
OPT          := -O2 -march=native
DEFS         := -D_POSIX_C_SOURCE=200809L
INCLUDES     := -Iinclude
DEPFLAGS     := -MMD -MP

# SDL / PNG / TTF
SDL2_CFLAGS  := $(shell sdl2-config --cflags)
SDL2_LIBS    := $(shell sdl2-config --libs)
IMG_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_image SDL2_ttf)
IMG_TTF_LIBS   := $(shell pkg-config --libs SDL2_image SDL2_ttf)

# Base (secuencial)
CFLAGS_BASE  := $(CSTD) $(WARN) $(OPT) $(DEFS) $(INCLUDES) $(SDL2_CFLAGS) $(IMG_TTF_CFLAGS) $(DEPFLAGS)
LDLIBS_BASE  := $(SDL2_LIBS) $(IMG_TTF_LIBS) -lm

# Paralelo (añade OpenMP)
OMPFLAGS     := -fopenmp
CFLAGS_OMP   := $(CFLAGS_BASE) $(OMPFLAGS)
LDLIBS_OMP   := $(LDLIBS_BASE) $(OMPFLAGS)

# --- Targets por defecto ---
.PHONY: all clean rebuild run-seq run-omp dirs
all: $(SEQ_BIN) $(OMP_BIN)

# --- Enlaces ---
$(SEQ_BIN): $(SEQ_OBJS) | dirs
	@mkdir -p $(dir $@)
	$(CC) $(SEQ_OBJS) -o $@ $(LDLIBS_BASE)

$(OMP_BIN): $(OMP_OBJS) | dirs
	@mkdir -p $(dir $@)
	$(CC) $(OMP_OBJS) -o $@ $(LDLIBS_OMP)

# --- Compilación (objetos separados por variante) ---
$(OBJ_DIR)/seq/%.o: src/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_BASE) -c $< -o $@

$(OBJ_DIR)/omp/%.o: src/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_OMP) -c $< -o $@

# --- Utilidades ---
dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)/seq $(OBJ_DIR)/omp

clean:
	@$(RM) -r build

rebuild: clean all

# --- Ejecución rápida ---
# Cambia hilos con: make run-omp THREADS=12
N ?= 5000
run-seq: $(SEQ_BIN)
	$(SEQ_BIN) -n $(N)

run-omp: $(OMP_BIN)
	OMP_NUM_THREADS=$(THREADS) $(OMP_BIN) -n $(N)

# --- Dependencias automáticas ---
-include $(SEQ_OBJS:.o=.d)
-include $(OMP_OBJS:.o=.d)
