PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
BUILD_DIR="${PRECOMPUTE_DIR}/build-perf-tests"
EXEC_DIR="${BUILD_DIR}/use-cases/thread-sanitizer/sample_apps"

source "${BUILD_DIR}/use-cases/thread-sanitizer/setup_env.sh"
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
export OMP_PLACES=cores

OUTPUT_DIR="/work/scratch/${USER}/precompute/${APPNAME}/${SLURM_ARRAY_JOB_ID}"

# get parameter
PARAM_LINE=$SLURM_ARRAY_TASK_ID
APP_PARAMS=$(sed -n "${PARAM_LINE}p" "$PARAMETER_FILE")

setup_resources() {
    # TODO now using LLVM/Clang 21.1
    ml gcc/8.5.0 clang/16.0.6
}

exec_internal() {
    MODE="$1"
    /usr/bin/env time -f "%e" -o "${OUTPUT_DIR}/time/${APPNAME}_${PARAM_LINE}_${MODE}.log" \
        "${EXEC_DIR}/${APPNAME}_${MODE}.exe" $APP_PARAMS
}

exec_test() {
    mkdir -p "${OUTPUT_DIR}/time"
    exec_internal 'orig'
    exec_internal 'pass'
    exec_internal 'stan'
}

write_result() {
    mkdir -p "${OUTPUT_DIR}/timings"

    echo 'testcase,parameter,time_orig,time_pass,time_stan' | tee "${OUTPUT_DIR}/timings/${APPNAME}_${PARAM_LINE}.log"
    (
        echo -n "${APP_NAME}"
        echo -n ","
        echo -n "${APP_PARAMS}"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APPNAME}_${PARAM_LINE}_orig.log" | tr -d "\n"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APPNAME}_${PARAM_LINE}_pass.log" | tr -d "\n"
        echo -n ","
        cat "${OUTPUT_DIR}/time/${APPNAME}_${PARAM_LINE}_stan.log" | tr -d "\n"
        echo ""
    ) | tee -a "${OUTPUT_DIR}/timings/${APPNAME}_${PARAM_LINE}.log"
}
