# `utils.c` — Documentación técnica

## Rol del módulo

Utilidades de bajo nivel para: **asegurar directorios** (tipo `mkdir -p`), **medir tiempo monótono** (ns) y **apendear** filas a **CSV** con cabecera automática. API mínima y portable sobre POSIX/C estándar.

## Flujo (alto nivel)

1. La app solicita un **timestamp** antes/después de una sección crítica -> `util_now_ns()` -> diferencia -> `util_ns_to_ms()` para loguear ms.
2. Para **métricas**, se llama `util_csv_append(path, header, fmt, ...)`; si el archivo no existía, se **asegura el directorio** y se escribe el **header** una vez.

## Convenciones

- **Thread-safety**: seguro si cada hilo escribe **archivos distintos**; para el **mismo CSV** usa sincronización externa (no hay locking interno).
- **Errores**: funciones retornan `bool` (éxito/fallo) y no dependen de `errno` del llamador.
- **Portabilidad**: asume separador `/` y API POSIX (`mkdir`, permisos `0755`). No interpreta `\\` de Windows.

## `bool util_fs_ensure_dir(const char *path);`

- **Entradas:**
  - `path` (`const char*`): ruta de **directorio** a garantizar (no vacía).
- **Salidas:**
  - `true` si el directorio existe (ya existía o fue creado); `false` en error real.
- **Descripción**
Comprueba si `path` es un directorio; si no, intenta crearlo (recursivo) vía `mkpath`. Soporta barra final y usa permisos `0755`.

## `uint64_t util_now_ns(void);`

- **Entradas:**
- **Salidas:**
  - `uint64_t`: **nanosegundos** desde un reloj **monótono** (`CLOCK_MONOTONIC`).
- **Descripción**
Reloj de alta resolución para **medir intervalos** (no calendario). Implementado con `clock_gettime(CLOCK_MONOTONIC, ...)`.

## `double util_ns_to_ms(uint64_t ns);`

- **Entradas:**
  - `ns` (`uint64_t`): intervalo en nanosegundos.
- **Salidas:**
  - `double`: **milisegundos** (`ns / 1e6`).
- **Descripción**
Conversión directa de ns->ms para reportes de tiempo.

## `bool util_csv_append(const char *path, const char *header, const char *fmt, ...);`

- **Entradas:**
  - `path` (`const char*`): ruta del CSV.
  - `header` (`const char*`): cabecera opcional a escribir si el archivo **no existía**.
  - `fmt, ...`: formato estilo `printf` de la **fila** a apendear (recomendado terminar con `\n`).

- **Salidas:**
  - `true` en éxito; `false` si falla asegurar dir/abrir/escribir.
- **Descripción**
Abre el archivo en **append**; si el archivo no existía, escribe primero `header` (añade `\n` si falta). Antes, **extrae el directorio** (`dirname_from_path`) y llama a `util_fs_ensure_dir` para asegurarlo. **No** realiza locking concurrente.  

## Funciones internas (no exportadas)

### `static bool mkpath(const char *path);`

Crea **recursivamente** cada prefijo de `path` (estilo `mkdir -p`), ignorando `EEXIST`. Recorta barra final, usa permisos `0755`.

### `static void dirname_from_path(const char *path, char *out, size_t out_sz);`

Extrae el **directorio contenedor** de una ruta (`"a/b/c.csv"->"a/b"`, `"file.csv"->""`), asegurando terminación NUL.

## Ejemplos de uso

**Medición de tiempo:**

```c
uint64_t t0 = util_now_ns();
/* ... trabajo ... */
uint64_t t1 = util_now_ns();
double ms = util_ns_to_ms(t1 - t0);
```

**Registro de métricas:**

```c
util_csv_append("images/output/seq/metrics.csv",
                "iter,ms,step,gamma,npoints",
                "%d,%.3f,%d,%.3f,%d\n",
                it, ms, step, gamma, npoints);
```
