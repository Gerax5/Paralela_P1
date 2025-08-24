#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
Uso: $(basename "$0") seq.csv omp.csv

Ambos archivos deben tener formato:
  iter,ms,step,gamma,npoints
(con encabezado en la primera línea)
EOF
}

[[ $# -ne 2 ]] && { usage; exit 1; }

SEQ="$1"
OMP="$2"

[[ ! -f "${SEQ}" || ! -f "${OMP}" ]] && { echo "Archivo no encontrado."; exit 1; }

avg_ms() {
  local f="$1"
  awk -F',' 'NR>1 {sum+=$2; n++} END{ if(n>0) printf("%.6f", sum/n); else print "0" }' "${f}"
}

SEQ_MEAN="$(avg_ms "${SEQ}")"
OMP_MEAN="$(avg_ms "${OMP}")"

# Evitar división por cero
if awk "BEGIN{exit !(${OMP_MEAN}==0)}"; then
  echo "OMP mean ms = 0; no se puede calcular speedup."
  exit 1
fi

SPEEDUP="$(awk -v a="${SEQ_MEAN}" -v b="${OMP_MEAN}" 'BEGIN{printf("%.3f", a/b)}')"

echo "Promedio ms (SEQ): ${SEQ_MEAN}"
echo "Promedio ms (OMP): ${OMP_MEAN}"
echo "Speedup SEQ/OMP  : ${SPEEDUP}x"
