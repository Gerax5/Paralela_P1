# Nube de puntos 3D + kNN (SDL2)

Visualiza una nube de **N** puntos 3D con conexiones **k-NN** y opciones de MST/Conexión por grilla en tiempo real.

## Requisitos (WSL/Ubuntu)

```bash
sudo apt-get install build-essential
sudo apt-get install libsdl2-dev
```

## Compilación

```bash
gcc -O2 -std=c11 main.c $(sdl2-config --cflags --libs) -lm -o secuencial
```

## Ejecución

```bash
./secuencial [N] [k]
```

* `N` = número de puntos (default: 2000)
* `k` = vecinos por punto para kNN (default: 8)

Ejemplos:

```bash
./secuencial
./secuencial 4000 12
```

## Controles

* `- / +`  → disminuir/aumentar **N** (±200)
* `[` / `]` → disminuir/aumentar **k**
* `m` → alterna **MST exacto**
* `c` → alterna **conectar componentes**
* `k` → alterna **kNN**
* `p` / `l` → mostrar/ocultar **puntos** / **líneas**
* `t` → activar **trails**
* `d` → activar **drift** (movimiento)
* `g` → cambiar tamaño de celda de grilla
* `SPACE` → re-generar nube
* `ESC` → salir

## Notas

* Si ves un error tipo `GLIBC_2.38 not found`, recompila **dentro de tu WSL** con el comando de arriba.
