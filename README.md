# Animación de círculos con SDL2

Este programa dibuja **N círculos en movimiento** usando la librería **SDL2**.  

## Requisitos

Antes de compilar, instala las librerías necesarias:

```bash
sudo apt-get install build-essential
sudo apt-get install libsdl2-dev
```

## Compilación

Puedes compilar y enlazar en **un solo comando**:

```bash
gcc main.c -o secuencial -lSDL2 -lm
```

- `-lSDL2` enlaza la librería SDL2.
- `-lm` enlaza la librería matemática.

## Ejecución

Ejecuta el programa indicando el número de partículas (círculos).
Por ejemplo, para **200 círculos**:

```bash
./secuencial 200
```

Para salir de la ventana, presiona **ESC** o cierra la ventana.

## Notas

- Si cambias el número de partículas, el tamaño y color de los círculos se ajusta automáticamente.
- En caso de error al compilar, asegúrate de que `libsdl2-dev` está instalado correctamente.
