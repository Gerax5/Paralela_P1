#!/usr/bin/env bash
set -euo pipefail

# -----------------------------
# Parámetros (con defaults)
# -----------------------------
IMAGES=("images/input/twitch.png")
N_LIST=(3000)
STEPS=(1 2 3 4)
ITERS=200
OUTDIR="tests/data"

# Sweep de gamma opcional (si GEND y GSTEP están definidos, se activa)
GSTART="10"
GEND="500"
GSTEP="20"
GEVERY=10

BIN="./build/bin/stippling_demo"

usage() {
  cat <<EOF
Uso: $(basename "$0") [opciones]

Opciones:
  -i img1[,img2,...]   Lista de imágenes (por coma). Default: ${IMAGES[*]}
  -n "N1 N2 ..."       Lista de N puntos. Default: ${N_LIST[*]}
  -s "S1 S2 ..."       Lista de steps. Default: ${STEPS[*]}
  -k ITERS             Iteraciones auto-run. Default: ${ITERS}
  -o OUTDIR            Directorio salida. Default: ${OUTDIR}
  --gstart X           Gamma start (activa sweep si se usa con --gend y --gstep)
  --gend X             Gamma end   (requerido para activar sweep)
  --gstep X            Paso de gamma (requerido)
  --gevery N           Cambiar gamma cada N iteraciones (default ${GEVERY})

Ejemplo:
  $(basename "$0") -i images/input/twitch.png -n "3000 6000" -s "2 4" -k 300 \
    --gstart 1.0 --gend 1.8 --gstep 0.05 --gevery 10
EOF
}

# Parseo simple de flags
while (( "$#" )); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -i) IFS=',' read -r -a IMAGES <<< "$2"; shift 2 ;;
    -n) read -r -a N_LIST <<< "$2"; shift 2 ;;
    -s) read -r -a STEPS  <<< "$2"; shift 2 ;;
    -k) ITERS="$2"; shift 2 ;;
    -o) OUTDIR="$2"; shift 2 ;;
    --gstart) GSTART="$2"; shift 2 ;;
    --gend)   GEND="$2"; shift 2 ;;
    --gstep)  GSTEP="$2"; shift 2 ;;
    --gevery) GEVERY="$2"; GEVERY="${2}"; shift 2 ;;
    *) echo "Flag desconocida: $1"; usage; exit 1 ;;
  esac
done

mkdir -p "${OUTDIR}/raw"

GRID_CSV="${OUTDIR}/grid_seq.csv"
if [ ! -f "${GRID_CSV}" ]; then
  echo "image,npoints,step,g_start,g_end,g_step,g_every,iter,ms,gamma,npoints_row" > "${GRID_CSV}"
fi

run_one() {
  local img="$1" n="$2" step="$3"
  local base imgbase tag raw_csv

  imgbase="$(basename "${img}")"
  base="${imgbase%.png}"
  base="${base%.jpg}"
  base="${base%.jpeg}"

  tag="${base}_N${n}_S${step}_K${ITERS}"
  raw_csv="${OUTDIR}/raw/${tag}.csv"

  echo "-> Ejecutando: img=${imgbase} N=${n} step=${step} iters=${ITERS}"

  # Variables de entorno para el binario
  export STIPPLE_AUTORUN=1
  export STIPPLE_MAX_ITERS="${ITERS}"
  export STIPPLE_METRICS="${raw_csv}"

  # Sweep gamma (opcional)
  if [[ -n "${GEND}" && -n "${GSTEP}" ]]; then
    export STIPPLE_GAMMA_END="${GEND}"
    export STIPPLE_GAMMA_STEP="${GSTEP}"
    export STIPPLE_GAMMA_EVERY="${GEVERY}"
    if [[ -n "${GSTART}" ]]; then
      export STIPPLE_GAMMA_START="${GSTART}"
    fi
  else
    unset STIPPLE_GAMMA_END STIPPLE_GAMMA_STEP STIPPLE_GAMMA_EVERY STIPPLE_GAMMA_START || true
  fi

  # Ejecutar
  "${BIN}" -n "${n}" "${img}" >/dev/null

  # Meter metadata al grid combinado
  # raw_csv tiene: iter,ms,step,gamma,npoints
  # añadimos: image,npoints,step,gstart,gend,gstep,gevery al principio
  awk -F',' -v OFS=',' \
      -v IMG="${imgbase}" -v N="${n}" -v STEP="${step}" \
      -v GS="${GSTART}" -v GE="${GEND}" -v GP="${GSTEP}" -v GV="${GEVERY}" \
      'NR>1 { print IMG, N, STEP, GS, GE, GP, GV, $1, $2, $4, $5 }' "${raw_csv}" >> "${GRID_CSV}"
}

# Barrido
for img in "${IMAGES[@]}"; do
  for n in "${N_LIST[@]}"; do
    for step in "${STEPS[@]}"; do
      run_one "${img}" "${n}" "${step}"
    done
  done
done

echo "OK. CSV combinado: ${GRID_CSV}"
