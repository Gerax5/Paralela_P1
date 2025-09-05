#!/usr/bin/env bash
set -euo pipefail

# -----------------------------
# Colores
# -----------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# -----------------------------
# Parámetros por default
# -----------------------------
N=2000
ITERS=200
IMG="images/input/twitch.png"
OUTDIR="tests/bench_once"

SEQ_BIN="./build/bin/stippling_seq"
PAR_BIN="./build/bin/stippling_par"

mkdir -p "${OUTDIR}/seq" "${OUTDIR}/par"

usage() {
  cat <<EOF
Uso: $(basename "$0") [--n N --iters K --img path.png]

Default:
  N=${N}, ITERS=${ITERS}, IMG=${IMG}
EOF
}

# -----------------------------
# Parseo flags
# -----------------------------
while (( "$#" )); do
  case "$1" in
    --n)     N="$2"; shift 2;;
    --iters) ITERS="$2"; shift 2;;
    --img)   IMG="$2"; shift 2;;
    -h|--help) usage; exit 0;;
    *) echo -e "${RED}Flag desconocida: $1${RESET}"; usage; exit 1;;
  esac
done

# -----------------------------
# Timestamp para archivos únicos
# -----------------------------
ts="$(date +%Y%m%d_%H%M%S)"
seq_csv="${OUTDIR}/seq/run_N${N}_${ts}.csv"
par_csv="${OUTDIR}/par/run_N${N}_${ts}.csv"

# -----------------------------
# Función para correr un binario
# -----------------------------
run_bin() {
  local bin="$1" outcsv="$2" label="$3"

  echo -e "${CYAN}-> Ejecutando ${label} con N=${N}, iters=${ITERS}, img=${IMG}${RESET}"

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

  "${bin}" -n "${N}" "${IMG}" >/dev/null
}

# -----------------------------
# Ejecución única
# -----------------------------
run_bin "${SEQ_BIN}" "${seq_csv}" "SEQ"
run_bin "${PAR_BIN}" "${par_csv}" "OMP"

echo -e "${YELLOW}Guardado:${RESET} ${seq_csv}, ${par_csv}"

# Comparación
echo -e "${BOLD}${GREEN}>>> Comparando resultados...${RESET}"
./tests/compare.sh "${seq_csv}" "${par_csv}"
