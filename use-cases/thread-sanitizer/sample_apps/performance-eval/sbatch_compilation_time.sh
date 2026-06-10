#!/usr/bin/env bash

#SBATCH --ntasks 1
#SBATCH --exclusive
#SBATCH -o /dev/null
#SBATCH -e /dev/null
#SBATCH --array 0-17
#SBATCH --mem-per-cpu=1G
#SBATCH --cpus-per-task 96
#SBATCH --time 00:15:00

usage() {
    echo "$0 [SAMPLE APP NAME]"
    exit 1
}

if [ -z "$SLURM_ARRAY_JOB_ID" ]; then
    SCRIPT_DIR="$(dirname "$(realpath "$0")")"

    if [ "$#" -lt 1 ]; then
        echo "missing parameter"
        usage
    fi

    APPNAME="$1"
    APPNAME_LOWER=$(echo "$APPNAME" | tr '[:upper:]' '[:lower:]')
    APPNAME_UPPER=$(echo "$APPNAME" | tr '[:lower:]' '[:upper:]')
    SCRIPT_DIR="$(dirname "$(realpath "$0")")"

    if ! [ -d "${SCRIPT_DIR}/../${APPNAME_LOWER}" ]; then
        echo "app dir does not exist"
        usage
    fi

    sbatch \
        --job-name="COMPILE_${APPNAME_UPPER}" \
        --export=APPNAME_LOWER="$APPNAME_LOWER",APPNAME_UPPER="$APPNAME_UPPER",SCRIPT_DIR="$SCRIPT_DIR" \
        "$0"
else
    REAL_HOME=$(realpath "$HOME")
    CONTAINER_IMAGE_PATH="${HOME}/myCont/precompute-devshell"

    export TMPDIR="/dev/shm"
    TMPDIR=$(mktemp -d --suffix='.sample-app-compilation')
    export APPTAINER_TMPDIR="$TMPDIR"
    [ -z "$TMPDIR" ] && exit 22

    APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"

    cd "$TMPDIR" || exit 23
    apptainer run \
        --mount "type=bind,source=${TMPDIR},destination=${HPC_SCRATCH}/tmp" \
        --mount "type=bind,source=${REAL_HOME},destination=${REAL_HOME}" \
        --env-file "${CONTAINER_IMAGE_PATH}.env" \
        --env APP_LOG_NAME="$APP_LOG_NAME" \
        "${CONTAINER_IMAGE_PATH}.sif" \
        "${SCRIPT_DIR}/../../tests/cluster_run_wrapper.sh" "${SCRIPT_DIR}/compare_compilation_time.sh"

    LOG_DIR="${HPC_SCRATCH}/precompute/results/compile_time"
    TIME_CSV="${TMPDIR}/results/${APP_LOG_NAME}.csv-time"
    [ -f "$TIME_CSV" ] || exit 42

    mkdir -p "$LOG_DIR"
    (
        head -n 1 "$TIME_CSV" | tr -d "\n"
        echo -n ","
        echo "app"
        tail -n 1 "$TIME_CSV" | tr -d "\n"
        echo -n ","
        echo "$APPNAME_LOWER"
    ) | tee -a "${LOG_DIR}/${APP_LOG_NAME}.csv"

    rm -fr "${TMPDIR}"
fi

exit 0
