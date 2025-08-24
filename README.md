Instalar:

sudo apt-get install build-essential libsdl2-dev libsdl2-image-dev

### Recompilar/Ejecutar

```bash
mkdir -p build/bin
gcc src/main.c src/app.c src/render_sdl.c src/image.c src/stippling.c src/lloyd.c src/utils.c \
  -Iinclude -o build/bin/stippling_demo \
  $(sdl2-config --cflags --libs) $(pkg-config --cflags --libs SDL2_image) -lm

./build/bin/stippling_demo images/input/cabra.png
```

### Controles actuales

* `SPACE`: una iteración de Lloyd.
* `A`: auto-run ON/OFF.
* `-` y `+` (también keypad): disminuir/aumentar `step`.
* `G` / `H`: subir/bajar `gamma`.
* `R`: resembrar puntos.
* `ESC`: salir.
