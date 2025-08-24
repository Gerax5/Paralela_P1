# `utils.c` — utilidades de FS, tiempo y CSV

Pequeño módulo de utilidades para:

- asegurar directorios (`mkdir -p` estilo POSIX),
- medir tiempo monótono en nanosegundos y convertir a ms,
- anexar filas a un CSV con cabecera automática.

## Dependencias

- POSIX: `sys/stat.h`, `errno.h`, `time.h` (`clock_gettime`, `CLOCK_MONOTONIC`)
- C estándar: `stdio.h`, `stdarg.h`, `string.h`, `stdlib.h`

## API pública

```c
bool     util_fs_ensure_dir(const char *path);
uint64_t util_now_ns(void);
double   util_ns_to_ms(uint64_t ns);
bool     util_csv_append(const char *path, const char *header,
                         const char *fmt, ...);
```

## Funciones

### `bool util_fs_ensure_dir(const char *path)`

**Qué hace:** Garantiza que exista el directorio `path`. Si no existe, lo crea recursivamente (comportamiento tipo `mkdir -p`).

**Detalles:**

- Permisos `0755` al crear.
- Soporta separador `/` y tolera barra final.
- Devuelve `true` si el directorio ya existía o se creó; `false` en error real.

**Notas de portabilidad:** Asume POSIX (`mkdir` y permisos Unix). No trata `\\` de Windows.

### `uint64_t util_now_ns(void)`

**Qué hace:** Devuelve tiempo **monótono** en nanosegundos desde un origen no especificado.

**Uso recomendado:** medir intervalos (no es fecha/hora de calendario).

**Fuente:** `clock_gettime(CLOCK_MONOTONIC, ...)`.

### `double util_ns_to_ms(uint64_t ns)`

**Qué hace:** Conversión simple de nanosegundos a milisegundos (`ns / 1e6`).

### `bool util_csv_append(const char *path, const char *header, const char *fmt, ...)`

**Qué hace:** Abre/crea `path` en modo append y escribe una fila formateada (estilo `printf`).
Si el archivo **no existe**, primero escribe `header` (si no es vacío) y le añade `\n` si falta.

**Comportamiento:**

- Asegura el **directorio contenedor** (crea si no existe).
- No realiza *locking* entre procesos/hilos.
- Devuelve `true` en éxito, `false` si falla asegurar dir, abrir o escribir.

**Recomendación:** incluye `\n` en el `fmt` de la fila.

## Funciones internas (no exportadas)

- `static bool mkpath(const char *path)`: implementación recursiva de creación de directorios (usa `mkdir`, ignora `EEXIST`).
- `static void dirname_from_path(const char *path, char *out, size_t out_sz)`: extrae directorio de una ruta (`"a/b/c.csv" -> "a/b"`).

## Ejemplos de uso

### Medir tiempo de una operación

```c
uint64_t t0 = util_now_ns();
/* ... trabajo ... */
uint64_t t1 = util_now_ns();
printf("took %.3f ms\n", util_ns_to_ms(t1 - t0));
```

### Registrar métricas en CSV con cabecera automática

```c
util_csv_append("images/output/seq/metrics.csv",
                "iter,ms,step,gamma,npoints",
                "%d,%.3f,%d,%.3f,%d\n",
                it, ms, step, gamma, npoints);
```
