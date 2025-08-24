Instalar:

sudo apt-get install build-essential libsdl2-dev libsdl2-image-dev

### Recompilar/Ejecutar

```bash
mkdir -p build/bin
gcc -std=c11 -O2 -Wall -Wextra \
  $(sdl2-config --cflags) $(pkg-config --cflags SDL2_image) -Iinclude \
  src/main.c src/app.c src/image.c src/stippling.c src/lloyd.c src/voronoi.c src/utils.c \
  $(sdl2-config --libs) $(pkg-config --libs SDL2_image) -lm \
  -o build/bin/stippling_demo
```

### Controles actuales

* `SPACE`: una iteración de Lloyd.
* `A`: auto-run ON/OFF.
* `-` y `+` (también keypad): disminuir/aumentar `step`.
* `G` / `H`: subir/bajar `gamma`.
* `R`: resembrar puntos.
* `ESC`: salir.
