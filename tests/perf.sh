#!/usr/bin/env bash
set -euo pipefail

BIN="./build/bin/stippling_demo"
OUTRAW="images/output/seq/perf_raw"
SUMMARY="tests/data/summary_seq.csv"

usage() {
  cat <<EOF
Uso: $(basename "$0") -i IMG -n N -s STEP -k ITERS -r REPS [--gamma X]

  -i IMG       Imagen (ruta)
  -n N         Número de puntos
  -s STEP      pixelStride
  -k ITERS     Iteraciones auto-run
  -r REPS      Repeticiones
  --gamma X    Fijar gamma inicial (opcional; si usas sweep, ignóralo)

Ejemplo:
  $(basename "$0") -i images/input/twitch.png -n 3000 -s 3 -k 300 -r 5 --gamma 1.2
EOF
}

IMG=""
N=""
STEP=""
ITERS=""
REPS=""
GAMMA=""

while (( "$#" )); do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -i) IMG="$2"; shift 2 ;;
    -n) N="$2"; shift 2 ;;
    -s) STEP="$2"; shift 2 ;;
    -k) ITERS="$2"; shift 2 ;;
    -r) REPS="$2"; shift 2 ;;
    --gamma) GAMMA="$2"; shift 2 ;;
    *) echo "Flag desconocida: $1"; usage; exit 1 ;;
  esac
done

[[ -z "${IMG}" || -z "${N}" || -z "${STEP}" || -z "${ITERS}" || -z "${REPS}" ]] && { usage; exit 1; }

mkdir -p "${OUTRAW}" "$(dirname "${SUMMARY}")"

base="$(basename "${IMG}")"
tag="${base%.png}"
tag="${tag%.jpg}"
tag="${tag%.jpeg}"
prefix="${OUTRAW}/${tag}_N${N}_S${STEP}_K${ITERS}"

# Encabezado del summary
if [ ! -f "${SUMMARY}" ]; then
  echo "image,npoints,step,gamma,iters,reps,mean_ms,std_ms,samples" > "${SUMMARY}"
fi

# Ejecutar repeticiones
raw_concat="$(mktemp)"
trap 'rm -f "${raw_concat}"' EXIT

for r in $(seq 1 "${REPS}"); do
  raw="${prefix}_R${r}.csv"
  echo "-> Repetición ${r}/${REPS}: ${raw}"
  export STIPPLE_AUTORUN=1
  export STIPPLE_MAX_ITERS="${ITERS}"
  export STIPPLE_METRICS="${raw}"

  # Opcional: setear gamma inicial forzando START=END
  if [[ -n "${GAMMA}" ]]; then
    export STIPPLE_GAMMA_START="${GAMMA}"
    export STIPPLE_GAMMA_END="${GAMMA}"
    export STIPPLE_GAMMA_STEP="0"
    export STIPPLE_GAMMA_EVERY="1"
  else
    unset STIPPLE_GAMMA_START STIPPLE_GAMMA_END STIPPLE_GAMMA_STEP STIPPLE_GAMMA_EVERY || true
  fi

  "${BIN}" -n "${N}" "${IMG}" >/dev/null

  # Concatenar (saltando encabezado)
  awk 'NR>1' "${raw}" >> "${raw_concat}"
done

# Calcular media y desviación de la columna ms
read -r MEAN STD COUNT <<< "$(awk -F',' '
  BEGIN{sum=0; sumsq=0; n=0}
  NR>=1 { ms=$2; sum+=ms; sumsq+=ms*ms; n++ }
  END{
    if(n>0){
      mean=sum/n;
      var=(sumsq/n - mean*mean);
      if (var<0) var=0;
      std=sqrt(var);
      printf("%.6f %.6f %d", mean, std, n);
    } else {
      printf("0 0 0");
    }
  }' "${raw_concat}")"

gval="${GAMMA:-NA}"
echo "${base},${N},${STEP},${gval},${ITERS},${REPS},${MEAN},${STD},${COUNT}" >> "${SUMMARY}"

echo "OK. Summary -> ${SUMMARY}"
