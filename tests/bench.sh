#!/usr/bin/env bash
set -euo pipefail

# -----------------------------
# Parámetros por default
# -----------------------------
N_START=2000
N_END=2000
N_STEP=2000
ITERS=200
IMG="images/input/twitch.png"
OUTDIR="tests/bench"

SEQ_BIN="./build/bin/stippling_seq"
PAR_BIN="./build/bin/stippling_par"

mkdir -p "${OUTDIR}/seq" "${OUTDIR}/par"

usage() {
  cat <<EOF
Uso: $(basename "$0") [--n-start X --n-end Y --n-step Z --iters K --img path.png]

Default:
  N_START=${N_START}, N_END=${N_END}, N_STEP=${N_STEP}
  ITERS=${ITERS}, IMG=${IMG}
EOF
}

# -----------------------------
# Parseo flags
# -----------------------------
while (( "$#" )); do
  case "$1" in
    --n-start) N_START="$2"; shift 2;;
    --n-end)   N_END="$2"; shift 2;;
    --n-step)  N_STEP="$2"; shift 2;;
    --iters)   ITERS="$2"; shift 2;;
    --img)     IMG="$2"; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo "Flag desconocida: $1"; usage; exit 1;;
  esac
done

# -----------------------------
# Colores (ANSI)
# -----------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# -----------------------------
# Función para correr un binario
# -----------------------------
run_bin() {
  local bin="$1" tag="$2" n="$3" outcsv="$4"

  echo -e "   ${CYAN}→ Ejecutando ${tag}${NC} (${bin}) con N=${n}, iters=${ITERS}, img=${IMG}"

  export STIPPLE_AUTORUN=1
  export STIPPLE_MAX_ITERS="${ITERS}"
  export STIPPLE_METRICS="${outcsv}"
  export STIPPLE_GAMMA_START=1.0
  export STIPPLE_GAMMA_END=1.0
  export STIPPLE_GAMMA_STEP=0.0
  export STIPPLE_GAMMA_EVERY=1
  export STIPPLE_COLOR=1
  export STIPPLE_MINR=0.8
  export STIPPLE_MAXR=3.0
  export STIPPLE_SEED=42

  "${bin}" -n "${n}" "${IMG}" >/dev/null
}

# -----------------------------
# Loop sobre N
# -----------------------------
for (( n=${N_START}; n<=${N_END}; n+=${N_STEP} )); do
  echo -e "\n${YELLOW}==============================${NC}"
  echo -e "${YELLOW} Bench N=${n} (iters=${ITERS}) ${NC}"
  echo -e "${YELLOW}==============================${NC}"

  ts=$(date +%Y%m%d_%H%M%S)
  seq_csv="${OUTDIR}/seq/run_N${n}_${ts}.csv"
  par_csv="${OUTDIR}/par/run_N${n}_${ts}.csv"

  run_bin "${SEQ_BIN}" "SEQ" "${n}" "${seq_csv}"
  run_bin "${PAR_BIN}" "PAR" "${n}" "${par_csv}"

  echo -e "   ${GREEN}✓ Guardado:${NC} ${seq_csv}, ${par_csv}"

  # Indentar salida de compare.sh
  echo "   ─ Resultados comparación ─"
  ./tests/compare.sh "${seq_csv}" "${par_csv}" | sed 's/^/   | /'
done

echo -e "\n${GREEN}✔ OK.${NC} Resultados en ${OUTDIR}"
