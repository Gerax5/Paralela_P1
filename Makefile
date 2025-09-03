TARGET := build/bin/stippling_demo
SRC    := src/main.c src/app.c src/image.c src/stippling.c src/lloyd.c src/voronoi.c src/utils.c
OBJ    := $(patsubst src/%.c,build/obj/%.o,$(SRC))
N      ?= 5000

# --- compi y OMP ---
CC        := gcc

# Núcleos disponibles por defecto
THREADS ?= $(shell \
  (getconf _NPROCESSORS_ONLN) 2>/dev/null || \
  (nproc) 2>/dev/null || \
  (sysctl -n hw.ncpu) 2>/dev/null || echo 8)

OMP_FLAGS := -fopenmp

CFLAGS  := -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude -fopenmp
LDFLAGS :=
LDLIBS  := -fopenmp

SDL2_CFLAGS     := $(shell sdl2-config --cflags)
SDL2_LIBS       := $(shell sdl2-config --libs)
IMG_TTF_CFLAGS  := $(shell pkg-config --cflags SDL2_image SDL2_ttf)
IMG_TTF_LIBS    := $(shell pkg-config --libs SDL2_image SDL2_ttf)

CFLAGS += $(SDL2_CFLAGS) $(IMG_TTF_CFLAGS) $(OMP_FLAGS) -MMD -MP
LDLIBS += $(SDL2_LIBS) $(IMG_TTF_LIBS) -lm $(OMP_FLAGS)

.PHONY: all run run-par start clean rebuild dirs

# Por defecto: solo compila
all: $(TARGET)

run: all
	@echo "Build listo: $(TARGET)"

# corre en paralelo (OMP); cambia hilos con: make run-par THREADS=12
run-par: all
	STIPPLE_PARALLEL=1 OMP_NUM_THREADS=$(THREADS) $(TARGET) -n ${N}

start: $(TARGET)
	@echo "Ejecutando $(TARGET)"
	$(TARGET)

$(TARGET): | dirs $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJ) -o $@ $(LDLIBS)

build/obj/%.o: src/%.c | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@mkdir -p build/obj

clean:
	@$(RM) -r build

rebuild: clean all

# incluir dependencias automáticas
-include $(OBJ:.o=.d)
