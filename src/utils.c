#include "utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>


/*
 * mkpath
 * ------
 * Crea recursivamente el árbol de directorios indicado por 'path' (estilo POSIX),
 * p.ej. "a/b/c" crea "a", luego "a/b" y finalmente "a/b/c".
 *
 * Parámetros:
 *   path -> ruta de directorio a asegurar (no vacía).
 *
 * Retorno:
 *   true  si el directorio existe al final (porque ya existía o se creó).
 *   false en error: path vacío, demasiado largo (> 1023), o fallo de mkdir distinto de EEXIST.
 *
 * Detalles:
 *   - Permisos 0755 al crear.
 *   - Tolera barra final; la recorta.
 *   - No es reentrante frente a procesos concurrentes que creen/eliminan las mismas rutas.
 *   - Solo usa separador '/', no interpreta Windows '\\'.
 */
static bool mkpath(const char *path)
{
  if (!path || !*path)
    return false;

  char tmp[1024];
  size_t len = strlen(path);
  if (len >= sizeof(tmp))
    return false;

  strcpy(tmp, path);
  if (tmp[len - 1] == '/')
    tmp[len - 1] = '\0';

  // Recorre y crea cada prefijo terminado en '/'
  for (char *p = tmp + 1; *p; ++p)
  {
    if (*p == '/')
    {
      *p = '\0';
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return false;
      *p = '/';
    }
  }

  // Crear la hoja
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return false;
  return true;
}

/*
 * util_fs_ensure_dir
 * ------------------
 * Garantiza que exista el directorio 'path'. Si no existe, intenta crearlo
 * (incluyendo directorios intermedios).
 *
 * Parámetros:
 *   path -> ruta de directorio (no vacía).
 *
 * Retorno:
 *   true  si 'path' existe y es un directorio, o si pudo crearse.
 *   false en error real (p.ej., ruta inválida o un archivo regular con ese nombre).
 */
bool util_fs_ensure_dir(const char *path)
{
  if (!path || !*path)
    return false;

  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    return true;

  return mkpath(path);
}

/*
 * util_now_ns
 * -----------
 * Reloj monótono de alta resolución en nanosegundos.
 *
 * Retorno:
 *   Conteo de nanosegundos desde un origen no especificado (CLOCK_MONOTONIC).
 *
 * Notas:
 *   - Apto para medición de intervalos; no para time-stamping de calendario.
 *   - Inmune a ajustes del reloj del sistema.
 */
uint64_t util_now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/*
 * util_ns_to_ms
 * -------------
 * Conversión de nanosegundos a milisegundos (double).
 */
double util_ns_to_ms(uint64_t ns) { return (double)ns / 1.0e6; }

/*
 * dirname_from_path
 * -----------------
 * Extrae el directorio contenedor de una ruta de archivo.
 *
 * Ejemplos:
 *   "a/b/c.csv" -> "a/b"
 *   "file.csv"  -> "" (cadena vacía)
 *
 * Parámetros:
 *   path   -> ruta de entrada.
 *   out    -> buffer de salida.
 *   out_sz -> tamaño del buffer de salida.
 *
 * Notas:
 *   - Si no hay '/', deja out vacío.
 *   - Asegura terminación NUL.
 */
static void dirname_from_path(const char *path, char *out, size_t out_sz)
{
  size_t n = strlen(path);
  size_t i = n;
  while (i > 0 && path[i - 1] != '/')
    --i;

  if (i == 0)
  { // no hay directorio
    if (out_sz)
      out[0] = '\0';
    return;
  }

  size_t dlen = (i > out_sz - 1) ? (out_sz - 1) : i;
  memcpy(out, path, dlen);
  out[dlen] = '\0';
}

/*
 * util_csv_append
 * ---------------
 * Agrega una fila formateada (printf-like) a un CSV. Si el archivo no existe,
 * primero escribe 'header' y luego la fila. Crea el directorio contenedor si hace falta.
 *
 * Parámetros:
 *   path   -> ruta del CSV (p.ej., "images/output/seq/metrics.csv").
 *   header -> cabecera a escribir si el archivo no existía (puede ser NULL o "").
 *   fmt, ... -> formato printf y argumentos de la fila a agregar (debe terminar en '\n').
 *
 * Retorno:
 *   true  en éxito.
 *   false en error (no pudo asegurar dir, abrir archivo, etc.).
 *
 * Detalles:
 *   - Modo "append" ('a'). No hace locking entre procesos concurrentes.
 *   - Si 'header' no termina en '\n', se le añade uno.
 *   - Usa vfprintf para el varargs.
 */
bool util_csv_append(const char *path, const char *header,
                     const char *fmt, ...)
{
  if (!path || !fmt)
    return false;

  // Asegurar directorio contenedor
  char dir[1024];
  dirname_from_path(path, dir, sizeof(dir));
  if (*dir)
  {
    if (!util_fs_ensure_dir(dir))
      return false;
  }

  // ¿Existe ya el archivo?
  bool exists = false;
  struct stat st;
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    exists = true;

  // Abrir en append
  FILE *fp = fopen(path, "a");
  if (!fp)
    return false;

  // Escribir cabecera si es un archivo nuevo
  if (!exists && header && *header)
  {
    fputs(header, fp);
    if (header[strlen(header) - 1] != '\n')
      fputc('\n', fp);
  }

  // Fila formateada
  va_list ap;
  va_start(ap, fmt);
  vfprintf(fp, fmt, ap);
  va_end(ap);

  fclose(fp);
  return true;
}