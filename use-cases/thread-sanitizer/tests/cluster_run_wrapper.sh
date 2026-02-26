#!/usr/bin/env bash

EXEC_SCRIPT=$(realpath "$1")

if ! [ -e "$EXEC_SCRIPT" ]; then
    exit 1
fi

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

TARGET_DIR=$(mktemp -d --suffix=".precompute")
cd "${TARGET_DIR}/" || exit 23

set -e
rsync -aHAX "${PRECOMPUTE_DIR}/" "${TARGET_DIR}/"
rm -fr ./build-cluster
cmake -B 'build-cluster' -DCMAKE_EXPORT_COMPILE_COMMANDS='ON' -S . -G 'Ninja' \
    -DMPI_USE_CASE='OFF' -DALLOC_TRACKING_USE_CASE='OFF'
cmake --build 'build-cluster'
set +e

source "${SCRIPT_DIR}/static_analysis_mode.sh"

MY_EXEC_SCRIPT=$(echo $EXEC_SCRIPT | sed -e 's|'"${PRECOMPUTE_DIR}/"'|'"${TARGET_DIR}/"'|')
/usr/bin/env time -f "%e" -o "${TARGET_DIR}/drb_time.log" \
    "$MY_EXEC_SCRIPT" 'build-cluster'

APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
LOG_DIR="${HPC_SCRATCH}/precompute/results/DRB"
mkdir -p "$LOG_DIR"
cp "${TARGET_DIR}/timing.csv" "${LOG_DIR}/${APP_LOG_NAME}.csv"

mkdir -p "${LOG_DIR}_time"
echo "id,threads,mode,time" | tee "${LOG_DIR}_time/${APP_LOG_NAME}.csv"
(
    echo -n "${SLURM_ARRAY_JOB_ID:-0}"
    echo -n ","
    echo -n "${OMP_NUM_THREADS:-0}"
    echo -n ","
    echo -n "${MY_STAN_PASS_MODE}"
    echo -n ","
    cat "${TARGET_DIR}/drb_time.log" | tr -d "\n"
    echo ""
) | tee -a "${LOG_DIR}_time/${APP_LOG_NAME}.csv"

rm -fr "${TARGET_DIR}"
exit 0
