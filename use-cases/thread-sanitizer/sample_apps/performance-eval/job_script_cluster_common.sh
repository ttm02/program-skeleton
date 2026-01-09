PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
BUILD_DIR="${PRECOMPUTE_DIR}/build-perf-tests"
EXEC_DIR="${BUILD_DIR}/use-cases/thread-sanitizer/sample_apps"

SETUP_ENV_FILE="${BUILD_DIR}/use-cases/thread-sanitizer/setup_env.sh"
# TODO build on cluster (and remove the following three lines)
if ! grep -q "$BUILD_DIR" "$SETUP_ENV_FILE"; then
    sed -i 's|=.*/build-perf-tests/|='"${BUILD_DIR}"'/|g' "${SETUP_ENV_FILE}"
fi

source "${SETUP_ENV_FILE}"
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
export OMP_PLACES=cores

MODE_COUNT='4'
MODE_NUM=$((SLURM_ARRAY_TASK_ID % MODE_COUNT))
PARAM_LINE=$((SLURM_ARRAY_TASK_ID / MODE_COUNT + 1))

# get config parameter
PARAMETER_FILE="${SCRIPT_DIR}/parameters_${APPNAME_LOWER}.txt"
APP_PARAMS=$(sed -n "${PARAM_LINE}p" "$PARAMETER_FILE")

case "$MODE_NUM" in
0) MODE='orig' ;; # TSAN
1) MODE='pass' ;; # TSAN + slicing
2) MODE='stan' ;; # TSAN + slicing + static analysis
3) MODE='norm' ;; # without
*) exit 1 ;;
esac

APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
APP_PARAMS_ESCAPED=$(echo "$APP_PARAMS" | tr ' ' '_' | tr '-' '_' | tr ',' '_')
OUTPUT_DIR="${HPC_SCRATCH}/precompute/timings/${APPNAME_UPPER}/${OMP_NUM_THREADS}/${APP_PARAMS_ESCAPED}"

REAL_HOME=$(realpath "$HOME")
CONTAINER_IMAGE_PATH="${HOME}/myCont/precompute-devshell"

setup_resources() {
    export TMPDIR="${HPC_SCRATCH}/tmp"
    export APPTAINER_TMPDIR=${TMPDIR}
    mkdir -p "$TMPDIR"
    if ! [ -f "${CONTAINER_IMAGE_PATH}.sif" ] || ! [ -f "${CONTAINER_IMAGE_PATH}.env" ]; then
        echo "Missing container setup!"
        exit 42
    fi
    MY_LD_PATH=$(echo "$LD_LIBRARY_PATH" | cut -d':' -f1)
    if ! grep -q "LD_LIBRARY_PATH=$MY_LD_PATH" "${CONTAINER_IMAGE_PATH}.env"; then
        sed -i '/LD_LIBRARY_PATH=/d' "${CONTAINER_IMAGE_PATH}.env"
        echo "LD_LIBRARY_PATH=$MY_LD_PATH" >>"${CONTAINER_IMAGE_PATH}.env"
    fi
}

exec_test() {
    mkdir -p "${OUTPUT_DIR}"
    LOG_FILE="${OUTPUT_DIR}/${APP_LOG_NAME}_${MODE}.log"

    if [ -n "$RUN_DIR" ]; then
        # TeaLeaf workaround
        cd "$RUN_DIR" || exit 1
        cp "${EXEC_DIR}/${APPNAME_UPPER}_${MODE}.exe" ./
    else
        RUN_DIR=${EXEC_DIR}
    fi

    cd "$TMPDIR" || exit 23
    apptainer run \
        --mount "type=bind,source=${REAL_HOME},destination=${REAL_HOME}" \
        --mount "type=bind,source=${HPC_SCRATCH},destination=${HPC_SCRATCH}" \
        --env-file "${CONTAINER_IMAGE_PATH}.env" \
        "${CONTAINER_IMAGE_PATH}.sif" \
        /usr/bin/env time -f '%e' \
        -o "$LOG_FILE" \
        "${RUN_DIR}/${APPNAME_UPPER}_${MODE}.exe" $APP_PARAMS
}

write_result() {
    mkdir -p "${OUTPUT_DIR}/timings"

    CSV_FILE="${HPC_SCRATCH}/precompute/results/${APPNAME_LOWER}/${APP_LOG_NAME}.csv"
    mkdir -p "$(dirname "$CSV_FILE")"
    echo 'id,name,threads,config,mode,time' >"$CSV_FILE"
    (
        echo -n "${SLURM_ARRAY_JOB_ID}"
        echo -n ","
        echo -n "${APPNAME_LOWER}"
        echo -n ","
        echo -n "${OMP_NUM_THREADS}"
        echo -n ","
        echo -n "${APP_PARAMS_ESCAPED}"
        echo -n ","
        echo -n "${MODE}"
        echo -n ","
        cat "$LOG_FILE" | tr -d "\n"
        echo ""
    ) | tee -a "$CSV_FILE"
}
