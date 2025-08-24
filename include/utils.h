#pragma once
#include <stdbool.h>
#include <stdint.h>

/*
 * util_fs_ensure_dir
 * ------------------
 * Crea el árbol de directorios indicado por 'path' si no existe.
 * Retorna true si existe o se pudo crear; false en error real.
 */
bool util_fs_ensure_dir(const char *path);

/*
 * util_now_ns / util_ns_to_ms
 * ---------------------------
 * Reloj monótono de alta resolución en nanosegundos y conversión a ms.
 */
uint64_t util_now_ns(void);
double util_ns_to_ms(uint64_t ns);

/*
 * util_csv_append
 * ---------------
 * Agrega una fila (printf-like) a un CSV. Si el archivo no existe, escribe
 * primero 'header' y luego la fila. Crea el directorio si hace falta.
 * Ejemplo:
 *   util_csv_append("images/output/seq/metrics.csv",
 *                   "iter,ms,step,gamma,n",
 *                   "%d,%.3f,%d,%.3f,%d\n", it, ms, step, gamma, n);
 */
bool util_csv_append(const char *path,
                     const char *header,
                     const char *fmt, ...);
