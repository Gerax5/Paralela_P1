# Patrones 3D con SDL2

Este programa dibuja **puntos en 3D conectados con líneas**, generando distintos patrones interactivos en tiempo real usando **SDL2**.  

## Requisitos

En WSL/Ubuntu asegúrate de instalar:

```bash
sudo apt-get install build-essential
sudo apt-get install libsdl2-dev
```

## Compilación

Compila el archivo `main.c` con:

```bash
gcc -O2 -std=c11 main.c $(sdl2-config --cflags --libs) -lm -o secuencial
```

- `$(sdl2-config --cflags --libs)` agrega las rutas correctas de SDL2.
- `-lm` enlaza la librería matemática.
- El ejecutable final será **`secuencial`**.

## Ejecución

Ejecuta el programa con los parámetros opcionales:

```bash
./secuencial [N] [mult]
```

- `N` = número de puntos (default: 300, rango 20–3000).
- `mult` = multiplicador para el modo 1 (default: 2).

Ejemplo:

```bash
./secuencial 500 7
```

## Controles

- `1 / 2 / 3` → cambiar modo (Multiplicativa / Estrella / Tejido).
- `← / →` → disminuir/aumentar `mult` (modo 1).
- `A / Z` → aumentar/disminuir `skip1` (modo 2/3).
- `S / X` → aumentar/disminuir `skip2` (modo 3).
- `+ / -` → aumentar/disminuir número de puntos.
- `P` → mostrar/ocultar puntos.
- `L` → mostrar/ocultar líneas.
- `T` → activar/desactivar trails (persistencia).
- `R` → resetear parámetros del modo actual.
- `ESC` → salir.

## Notas

- Si ves un error como:

  ```bash
  ./secuencial: /lib/x86_64-linux-gnu/libm.so.6: version `GLIBC_2.38' not found
  ```

  Significa que el ejecutable fue compilado en otro sistema, lo mejor es compilar nuevamente.
