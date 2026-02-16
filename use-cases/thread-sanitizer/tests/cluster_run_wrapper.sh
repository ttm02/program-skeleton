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

case "$SLURM_ARRAY_TASK_ID" in
0)
    MY_STAN_PASS_MODE="orig"
    MY_STAN_PASS_MODE_ARGS=""
    ;;
1)
    MY_STAN_PASS_MODE="slicing"
    MY_STAN_PASS_MODE_ARGS=""
    ;;
2)
    MY_STAN_PASS_MODE="slicing+single"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single"
    ;;
3)
    MY_STAN_PASS_MODE="slicing+single+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge"
    ;;
4)
    MY_STAN_PASS_MODE="slicing+single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge,loop"
    ;;
5)
    MY_STAN_PASS_MODE="slicing+merge"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge"
    ;;
6)
    MY_STAN_PASS_MODE="slicing+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=merge,loop"
    ;;
7)
    MY_STAN_PASS_MODE="slicing+loop"
    MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=loop"
    ;;
8)
    MY_STAN_PASS_MODE="passthrough"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing"
    ;;
9)
    MY_STAN_PASS_MODE="single"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single"
    ;;
10)
    MY_STAN_PASS_MODE="single+merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge"
    ;;
11)
    MY_STAN_PASS_MODE="single+merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=single,merge,loop"
    ;;
12)
    MY_STAN_PASS_MODE="merge"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge"
    ;;
13)
    MY_STAN_PASS_MODE="merge+loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=merge,loop"
    ;;
14)
    MY_STAN_PASS_MODE="loop"
    MY_STAN_PASS_MODE_ARGS="--disable-slicing --static-analysis-mode=loop"
    ;;
*) exit 1 ;;
esac

export MY_STAN_PASS_MODE
export MY_STAN_PASS_MODE_ARGS

MY_EXEC_SCRIPT=$(echo $EXEC_SCRIPT | sed -e 's|'"${PRECOMPUTE_DIR}/"'|'"${TARGET_DIR}/"'|')
"$MY_EXEC_SCRIPT" 'build-cluster'

APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
LOG_DIR="${HPC_SCRATCH}/precompute/results/DRB"
mkdir -p "$LOG_DIR"
cp "${TARGET_DIR}/timing.csv" "${LOG_DIR}/${APP_LOG_NAME}.csv"

rm -fr "${TARGET_DIR}"
exit 0
