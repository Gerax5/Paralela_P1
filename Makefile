TARGET := build/bin/stippling_demo
SRC    := src/main.c src/app.c src/image.c src/stippling.c src/lloyd.c src/voronoi.c src/utils.c
OBJ    := $(patsubst src/%.c,build/obj/%.o,$(SRC))

CFLAGS  := -std=c11 -O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L -Iinclude
LDFLAGS :=
LDLIBS  :=

SDL2_CFLAGS     := $(shell sdl2-config --cflags)
SDL2_LIBS       := $(shell sdl2-config --libs)
IMG_TTF_CFLAGS  := $(shell pkg-config --cflags SDL2_image SDL2_ttf)
IMG_TTF_LIBS    := $(shell pkg-config --libs SDL2_image SDL2_ttf)

CFLAGS += $(SDL2_CFLAGS) $(IMG_TTF_CFLAGS)
LDLIBS += $(SDL2_LIBS) $(IMG_TTF_LIBS) -lm

.PHONY: all run start clean rebuild dirs

# Por defecto: solo compila
all: $(TARGET)

run: all
	@echo "Build listo: $(TARGET)"

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
