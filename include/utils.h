#pragma once
#include <stdbool.h>
#include <stdint.h>

/*
 * utils.h
 * -------
 * Utilidades de sistema y cronometraje:
 *   - Asegurar directorios (creacion recursiva).
 *   - Reloj monotono de alta resolucion (ns) y conversiones.
 *   - Append a CSV con cabecera automatica (printf-like).
 *
 * Notas:
 *   - Todas las funciones son bloqueantes (I/O cuando aplica).
 *   - Thread-safety: seguro si cada hilo opera sobre rutas distintas.
 *     Para escribir el MISMO CSV desde varios hilos/procesos, sincronizar afuera.
 */

/**
 * util_fs_ensure_dir
 * ------------------
 * Crea el arbol de directorios indicado por `path` si no existe (mkdir -p).
 *
 * Params:
 *   path -> ruta de directorio (puede incluir separadores intermedios).
 *
 * Return:
 *   true  si ya existe o se creo correctamente.
 *   false en error (p.ej. permisos). No modifica errno externamente.
 */
bool util_fs_ensure_dir(const char *path);

/**
 * util_now_ns / util_ns_to_ms
 * ---------------------------
 * Reloj monotono en nanosegundos desde un punto arbitrario y conversion a ms.
 *
 * Return:
 *   util_now_ns() -> nanosegundos (uint64_t) monotonicos.
 *   util_ns_to_ms(ns) -> milisegundos (double).
 *
 * Notas:
 *   - Apto para medir duraciones (no para tiempo de pared).
 */
uint64_t util_now_ns(void);
double util_ns_to_ms(uint64_t ns);

/**
 * util_csv_append
 * ---------------
 * Agrega una fila a un CSV en `path`. Si no existe, escribe primero `header`
 * y luego la fila. Crea el directorio contenedor si hace falta.
 *
 * Params:
 *   path   -> ruta del CSV (se crea si no existe).
 *   header -> linea de cabecera (sin salto final agregado automaticamente si NULL).
 *   fmt    -> formato printf de la fila. Se recomienda terminar con '\n'.
 *   ...    -> argumentos variadicos segun `fmt`.
 *
 * Return:
 *   true en exito; false si falla abrir/escribir/flush.
 *
 * Ejemplo:
 *   util_csv_append("images/output/seq/metrics.csv",
 *                   "iter,ms,step,gamma,n",
 *                   "%d,%.3f,%d,%.3f,%d\n", it, ms, step, gamma, n);
 *
 * Concurrencia:
 *   - No atomico para escrituras concurrentes al mismo archivo.
 *     Coordinar con mutex/bloqueo externo si varios hilos escriben el mismo CSV.
 */
bool util_csv_append(const char *path,
                     const char *header,
                     const char *fmt, ...);
