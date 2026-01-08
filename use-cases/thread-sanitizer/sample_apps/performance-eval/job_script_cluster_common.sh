PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
BUILD_DIR="${PRECOMPUTE_DIR}/build-perf-tests"
EXEC_DIR="${BUILD_DIR}/use-cases/thread-sanitizer/sample_apps"

source "${BUILD_DIR}/use-cases/thread-sanitizer/setup_env.sh"
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
export OMP_PLACES=cores

# get parameter
PARAM_LINE=$SLURM_ARRAY_TASK_ID
PARAMETER_FILE="${SCRIPT_DIR}/parameters_${APPNAME_LOWER}.txt"
APP_PARAMS=$(sed -n "${PARAM_LINE}p" "$PARAMETER_FILE")

APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}"
APP_PARAMS_ESCAPED=$(echo "$APP_PARAMS" | tr ' ' '_' | tr '-' '_' | tr ',' '_')
OUTPUT_DIR="${HPC_SCRATCH}/precompute/${APPNAME_UPPER}/${OMP_NUM_THREADS}/${APP_PARAMS_ESCAPED}"

setup_resources() {
    # TODO now using LLVM/Clang 21.1
    ml gcc/8.5.0 clang/16.0.6
}

exec_internal() {
    MODE="$1"
    if [ -n "$RUN_DIR" ]; then
        # TeaLeaf workaround
        cd "$RUN_DIR" || exit 1
        cp "${EXEC_DIR}/${APPNAME_UPPER}_${MODE}.exe" ./
    else
        RUN_DIR=${EXEC_DIR}
    fi
    /usr/bin/env time -f "%e" -o "${OUTPUT_DIR}/time/${APP_LOG_NAME}_${MODE}.log" \
        "${RUN_DIR}/${APPNAME_UPPER}_${MODE}.exe" $APP_PARAMS
}

exec_test() {
    mkdir -p "${OUTPUT_DIR}/time"
    exec_internal 'orig'
    exec_internal 'pass'
    exec_internal 'stan'
}

write_result() {
    mkdir -p "${OUTPUT_DIR}/timings"

    echo 'testcase,parameter,time_orig,time_pass,time_stan' | tee "${OUTPUT_DIR}/timings/${APP_LOG_NAME}.log"
    (
        echo -n "${APPNAME_LOWER}"
        echo -n ","
        echo -n "${APP_PARAMS_ESCAPED}"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APP_LOG_NAME}_orig.log" | tr -d "\n"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APP_LOG_NAME}_pass.log" | tr -d "\n"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APP_LOG_NAME}_stan.log" | tr -d "\n"
        echo ""
    ) | tee -a "${OUTPUT_DIR}/timings/${APP_LOG_NAME}.log"
}
