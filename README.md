# Campo “magnético” con triángulos + partículas (SDL2)

Renderiza un **campo vectorial** (dipolo/monopolo/swirls) con flechas como **triángulos** y **partículas** advectadas en tiempo real.

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
./secuencial [P]
```

- `P` = número de partículas (default: 3000).

Ejemplos:

```bash
./secuencial
./secuencial 8000
```

## Controles

- **Mouse**: mueve el dipolo; **Wheel**: ± fuerza.
- **Q / E**: rota el dipolo del mouse.
- **V**: mostrar/ocultar flechas (vector field).
- **O**: mostrar/ocultar partículas.
- **T**: activar/desactivar trails.
- **M**: activar/desactivar dipolo del mouse.
- **F**: cambiar tipo de campo (Dipolo → Monopolo → Swirl).
- **R**: reubicar partículas aleatoriamente.
- **+ / =**: +1000 partículas.
- **-**: −1000 partículas.
- **G**: cambia densidad de malla (64/48/32/24).
- **Click Izq.**: añade imán fijo.
- **Click Der.**: elimina último imán fijo.
- **Click Medio**: limpia imanes fijos.
- **ESC**: salir.

## Notas

- Si aparece `GLIBC_2.38 not found`, recompila **dentro de tu WSL** con el comando de arriba.
