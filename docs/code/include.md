# API de headers – Documentación técnica

## Rol por módulo

- **app**: orquesta el ciclo de vida, entrada, timing, logging y render.
- **image**: I/O de imagen y muestreo bilineal (color/intensidad).
- **stippling**: almacenamiento de la nube y render estilizado.
- **lloyd**: núcleo de Lloyd (CVD); calcula centroides ponderados y actualiza puntos.
- **voronoi**: índice espacial (grilla) para NN rápido; fallback a O(N).
- **utils**: tiempos de alta resolución y utilitario CSV.

## app.h — Ciclo de vida y fondos

### `typedef struct App App;`

- **Entradas/Salidas**:
- **Descripción**: tipo opaco que encapsula el estado global de la app (ventana, renderer, imagen, nube de puntos, UI, etc.).

### `bool appInit(App **outApp, int width, int height, const char *title, const char *imagePath, int npoints);`

- **Entradas**:

  - `outApp` (`App**`): salida por referencia.
  - `width, height` (`int`): tamaño ventana (usa defaults si <=0).
  - `title` (`const char*`): título ventana (usa default si `NULL`).
  - `imagePath` (`const char*`): ruta de imagen (usa default si `NULL`).
  - `npoints` (`int`): número inicial de puntos (usa default si <=0).
- **Salidas**: `true/false` éxito.
- **Descripción**: inicializa SDL/SDL_image/TTF, crea ventana/renderer, carga imagen y siembra la nube de puntos.

### `void appRun(App *app);`

- **Entradas**: `app` (`App*`).
- **Salidas**:
- **Descripción**: bucle principal; procesa eventos, ejecuta Lloyd (manual/auto), renderiza y registra métricas si está habilitado.

### `void appShutdown(App *app);`

- **Entradas**: `app` (`App*`).
- **Salidas**:
- **Descripción**: libera recursos (texturas, fuentes, imagen, nube de puntos) y cierra subsistemas.

### Fondos (rotación)

- `bool appSetBackgroundList(App *app, int count, const char *const *paths);`
  - **In**: `app`, `count`, `paths[]`.
  - **Out**: `true/false`.
  - **Desc.**: define lista de fondos y carga el primero.
- `void appNextBackground(App *app);` / `void appPrevBackground(App *app);`
  - **In**: `app`.
  - **Out**:
  - **Desc.**: avanza/retrocede en la lista circular y carga.

## config.h — Defaults de ejecución

### Constantes principales

- `defaultWidth`, `defaultHeight` (`int`): tamaño inicial ventana.
- `defaultTitle` (`const char[]`): título por defecto.
- `defaultImagePath` (`const char[]`) y `defaultBgPaths[]` + `defaultBgCount`: imagen por defecto y lista rotatoria.
- `defaultNPoints` (`int`): número inicial de puntos.
- `defaultGamma` (`float`): gamma del peso.
- `defaultLloydStep` (`int`): stride de muestreo del lienzo.
  **Uso**: proveen valores por defecto si no se especifican por CLI/entorno.

## image.h — Carga y muestreo de imagen

### `typedef struct Image { SDL_Surface *surface; int width, height; uint32_t *pixels; } Image;`

- **Descripción**: envoltorio para `SDL_Surface` + acceso directo a pixeles RGBA.

### `bool imageLoad(Image *img, const char *path);`

- **In**: `img`, `path`.
- **Out**: `true/false`.
- **Desc.**: carga PNG/JPG, garantiza formato RGBA y rellena metadatos.

### `void imageFree(Image *img);`

- **In**: `img`.
- **Out**:
- **Desc.**: libera `SDL_Surface` y limpia campos.

### `uint32_t imageSampleBilinear(const Image *img, float u, float v);`

- **In**: `img`, coords normalizadas `u,v` \[0..1].
- **Out**: `RGBA` empaquetado.
- **Desc.**: muestreo bilineal de color.

### `float sampleIntensityBilinearUV(const Image *img, float u, float v);`

- **In**: `img`, `u,v`.
-**Out**: luminancia \[0..1].
- **Desc.**: muestreo bilineal de intensidad (rec.709).

### `SDL_Texture *imageCreateTexture(SDL_Renderer *ren, const Image *img);`

- **In**: `ren`, `img`.
- **Out**: `SDL_Texture*` (o `NULL`).
- **Desc.**: crea textura renderizable desde `Image`.

## stippling.h — Nube de puntos y render

### Estructuras

- `struct StipplePoint { float x,y; uint8_t r,g,b,a; };`
  
  **Desc.**: punto con posición y color/alpha.

- `typedef struct { struct StipplePoint *pts; int count; float *accumW; } Stippling;`
  
  **Desc.**: contenedor de puntos; `accumW` acumula pesos por punto.

### Funciones

- `bool stipplingInit(Stippling *s, int count, int W, int H, unsigned seed);`
  - **In**: `s`, `count`, `W,H`, `seed`.
  - **Out**: `true/false`.
  - **Desc.**: reserva y siembra puntos iniciales en el canvas.
- `void stipplingFree(Stippling *s);`
  - **In**: `s`.
  - **Out**:
  - **Desc.**: libera buffers asociados.
- `void stipplingRenderStyled(const Stippling *s, SDL_Renderer *ren, int W, int H, const Image *img, float minRadius, float maxRadius, bool colorPoints, bool invertTheme);`
  - **In**: `s`, `ren`, `W,H`, `img`, `minRadius`, `maxRadius`, `colorPoints`, `invertTheme`.
  - **Out**:
  - **Desc.**: dibuja la nube (puntillismo) con radio por intensidad/color y tema visual.

## lloyd.h — Iteración de Lloyd + utilidades RNG

### Utilidades RNG

- `static inline unsigned lcg(unsigned *st);`
  - **In**: `st` (estado).
  - **Out**: nuevo estado (32b).
  - **Desc.**: generador congruencial lineal rápido (no cripto).
- `static inline int irand_range(unsigned *st, int hi);`
  - **In**: `st`, `hi>0`.
  - **Out**: entero `[0,hi)`.
  - **Desc.**: pseudo-aleatorio uniforme approx. por módulo.

### Lloyd

- `bool lloydStep(const Image *img, Stippling *s, int W, int H, int step, float gamma);`
  - **In**: `img` (campo de densidad), `s` (puntos), `W,H` (canvas), `step>=1` (stride), `gamma` (exponente).
  - **Out**: `true/false` éxito.
  - **Desc.**: una iteración de Lloyd (Centroidal Voronoi); para cada muestra (cada `step` px) acumula centroide ponderado por luminancia y actualiza cada punto al centro de masa; maneja huérfanos con re-seed.

## voronoi.h — Índice espacial (grid uniforme)

### Tipos

- `typedef struct UniformGrid { ... } UniformGrid;` – grilla 2D con celdas y listas de índices.
- `typedef struct GridEntry { int pindex; int pad; } GridEntry;` – elemento por celda.

### Funciones

- `bool gridBuild(UniformGrid *g, int W, int H, int cell, const Stippling *s);`
  - **In**: `g`, dimensiones `W,H`, tamaño de celda `cell`, puntos `s`.
  - **Out**: `true/false`.
  - **Desc.**: construye índice uniforme para acelerar NN.
- `int gridNearest(const UniformGrid *g, const Stippling *s, float x, float y, int maxRing);`
  - **In**: `g`, `s`, posición `(x,y)`, anillos `maxRing`.
  - **Out**: índice de punto más cercano o `<0` si no hay.
  - **Desc.**: búsqueda de vecino más cercano explorando celdas por anillos.
- `void gridFree(UniformGrid *g);`
  - **In**: `g`.
  - **Out**:
  - **Desc.**: libera memoria del índice.

## utils.h — Tiempos y CSV

### `uint64_t util_now_ns(void);`

- **Entradas**:
- **Salidas**: `nanosegundos` desde reloj de alto-resolución.
- **Descripción**: timestamp monotónico para medir iteraciones.

### `double util_ns_to_ms(uint64_t ns);`

- **Entradas**: `ns` (nanosegundos).
- **Salidas**: `ms` (double).
- **Descripción**: conversión de unidades.

### `void util_csv_append(const char *path, const char *header, const char *rowfmt, ...);`

- **Entradas**: `path` (CSV), `header` (línea de encabezado), `rowfmt` (formato de fila) + varargs.
- **Salidas**:
- **Descripción**: crea/abre CSV y **apendea** una fila; escribe `header` solo si el archivo no existe o está vacío.
