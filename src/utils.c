#include "utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Crea árbol de directorios: "a/b/c"
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
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return false;
  return true;
}

bool util_fs_ensure_dir(const char *path)
{
  if (!path || !*path)
    return false;
  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    return true;
  return mkpath(path);
}

uint64_t util_now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
double util_ns_to_ms(uint64_t ns) { return (double)ns / 1.0e6; }

static void dirname_from_path(const char *path, char *out, size_t out_sz)
{
  // Extrae directorio de "a/b/c.csv" -> "a/b"
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

bool util_csv_append(const char *path, const char *header,
                     const char *fmt, ...)
{
  if (!path || !fmt)
    return false;

  // Asegurar directorio
  char dir[1024];
  dirname_from_path(path, dir, sizeof(dir));
  if (*dir)
  {
    if (!util_fs_ensure_dir(dir))
      return false;
  }

  // Saber si existe para decidir si escribir header
  bool exists = false;
  struct stat st;
  if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
    exists = true;

  FILE *fp = fopen(path, "a");
  if (!fp)
    return false;

  if (!exists && header && *header)
  {
    fputs(header, fp);
    if (header[strlen(header) - 1] != '\n')
      fputc('\n', fp);
  }

  va_list ap;
  va_start(ap, fmt);
  vfprintf(fp, fmt, ap);
  va_end(ap);

  fclose(fp);
  return true;
}
