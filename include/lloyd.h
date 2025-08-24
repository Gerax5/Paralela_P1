#pragma once
#include <stdbool.h>
#include "image.h"
#include "stippling.h"

// Una iteración de Lloyd (coarse) sobre el canvas de w×h.
// pixelStride: muestreo cada k píxeles (3–4 rápido, 1 exacto).
// gamma: (1.0 lineal; >1 enfatiza zonas oscuras).
bool lloydStep(const Image *img, Stippling *s, int w, int h, int pixelStride, float gamma);
