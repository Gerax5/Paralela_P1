#!/usr/bin/env bash
set -euo pipefail

# Colores ANSI
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
MAGENTA="\033[35m"
CYAN="\033[36m"
BOLD="\033[1m"
RESET="\033[0m"

usage() {
  cat <<EOF
${BOLD}Uso:${RESET} $(basename "$0") seq.csv omp.csv

Ambos archivos deben tener formato:
  iter,ms,step,gamma,npoints
(con encabezado en la primera línea)
EOF
}

[[ $# -ne 2 ]] && { usage; exit 1; }

SEQ="$1"
OMP="$2"

[[ ! -f "${SEQ}" || ! -f "${OMP}" ]] && { echo -e "${RED}Archivo no encontrado.${RESET}"; exit 1; }

avg_ms() {
  local f="$1"
  awk -F',' 'NR>1 {sum+=$2; n++} END{ if(n>0) printf("%.6f", sum/n); else print "0" }' "${f}"
}

SEQ_MEAN="$(avg_ms "${SEQ}")"
OMP_MEAN="$(avg_ms "${OMP}")"

# Evitar división por cero
if [ "$(printf "%.0f" "${OMP_MEAN}")" -eq 0 ]; then
  echo -e "${RED}OMP mean ms = 0; no se puede calcular speedup.${RESET}"
  exit 1
fi

SPEEDUP="$(awk -v a="${SEQ_MEAN}" -v b="${OMP_MEAN}" 'BEGIN{printf("%.3f", a/b)}')"

echo -e "${CYAN}------------------------------------------${RESET}"
echo -e "${BOLD}${BLUE}Resultados de comparación:${RESET}"
echo -e "  ${GREEN}Promedio ms (SEQ):${RESET} ${SEQ_MEAN}"
echo -e "  ${GREEN}Promedio ms (OMP):${RESET} ${OMP_MEAN}"
echo -e "  ${YELLOW}Speedup SEQ/OMP  :${RESET} ${SPEEDUP}x"
echo -e "${CYAN}------------------------------------------${RESET}"
