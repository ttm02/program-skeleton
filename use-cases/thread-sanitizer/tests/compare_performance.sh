#!/usr/bin/env bash

usage() {
  echo "$0 <build dir>"
  exit 1
}

MY_TESTCASE_TIMEOUT='42'
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

MY_CMAKE_BUILD_DIR="${PRECOMPUTE_DIR}/build"

if [ -n "$1" ]; then
  if ! [ -d "$1" ]; then
    usage
  else
    MY_CMAKE_BUILD_DIR=$(realpath "$1")
    shift
  fi
fi

BINARY_DIR="${MY_CMAKE_BUILD_DIR}/use-cases/thread-sanitizer"

# TODO fortran testcases
# TODO pthread testcases
TEST_CASE_DIR="${MY_CMAKE_BUILD_DIR}/_deps/drb-src/micro-benchmarks"
TEST_CASES=$(ls "$TEST_CASE_DIR")

source "${BINARY_DIR}/setup_env.sh"

GREP_STRING="WARNING: ThreadSanitizer: data race"

MY_TMP_DIR=$(mktemp -d --suffix=".JustTheRaces-timing")
MY_CUR_DIR=$(pwd)

cd "$MY_TMP_DIR" || exit 10

echo "id,threads,testcase,mode,found,time" | tee "${MY_CUR_DIR}/timing.csv"

save_time_to_file() {
  TEST_CASE="$1"
  MODE="$2"
  (
    echo -n "${SLURM_ARRAY_JOB_ID:-0}"
    echo -n ","
    echo -n "${OMP_NUM_THREADS:-0}"
    echo -n ","
    echo -n "$(basename "$TEST_CASE")"
    echo -n ","
    echo -n "${MODE}"
    echo -n ","
    cat "${MY_TMP_DIR}/found_${MODE}.log" | tr -d "\n"
    echo -n ","
    cat "${MY_TMP_DIR}/time_${MODE}.log" | tr -d "\n"
    echo ""
  ) | tee -a "${MY_CUR_DIR}/timing.csv"
}

time_testcase() {
  /usr/bin/env time -f "%e" -o "${MY_TMP_DIR}/time_${1}.log" \
    --quiet timeout "$MY_TESTCASE_TIMEOUT" "$2" 2>&1
}

run_binary() {
  NAME="$1"

  if [[ -x "./a.out_${NAME}" ]]; then
    if time_testcase "${NAME}" "./a.out_${NAME}" | grep -qF "$GREP_STRING"; then
      echo "yes" >"${MY_TMP_DIR}/found_${NAME}.log"
    else
      echo "no" >"${MY_TMP_DIR}/found_${NAME}.log"
    fi
  else
    echo "build" >"${MY_TMP_DIR}/found_${NAME}.log"
    echo "-1" >"${MY_TMP_DIR}/time_${NAME}.log"
  fi
}

run_testcase_on_cluster() {
  if [ "$MY_STAN_PASS_MODE" = "orig" ]; then
    "${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" false >/dev/null 2>&1
    run_binary 'orig'
    save_time_to_file "$TEST_CASE" 'orig'
  else
    OUTPUT_SUFFIX="$MY_STAN_PASS_MODE"
    export OUTPUT_SUFFIX
    "${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" true >/dev/null 2>&1
    run_binary "$MY_STAN_PASS_MODE"
    save_time_to_file "$TEST_CASE" "$MY_STAN_PASS_MODE"
  fi

}

run_testcase() {
  TEST_CASE="$1"

  if [[ "$TEST_CASE" == *.c ]] || [[ "$TEST_CASE" == *.cpp ]] || [[ "$TEST_CASE" == *.f ]]; then
    if [ -n "$MY_STAN_PASS_MODE" ]; then
      run_testcase_on_cluster
      return
    fi

    "${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" false >/dev/null 2>&1 &
    pid_compile_orig=$!

    "${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" true >/dev/null 2>&1 &
    pid_compile_pass=$!

    export OUTPUT_SUFFIX='stan'
    export USE_STATIC_ANALYSIS=true
    "${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" true >/dev/null 2>&1 &
    pid_compile_stan=$!
    unset OUTPUT_SUFFIX
    unset USE_STATIC_ANALYSIS

    wait $pid_compile_orig
    wait $pid_compile_pass
    wait $pid_compile_stan

    run_binary 'orig'
    run_binary 'pass'
    run_binary 'stan'

    save_time_to_file "$TEST_CASE" 'orig'
    save_time_to_file "$TEST_CASE" 'pass'
    save_time_to_file "$TEST_CASE" 'stan'
  else
    echo "Not a known Source Code file: $TEST_CASE"
    return
  fi
}

clean_exit() {
  rm -fr "$MY_TMP_DIR"
  exit 0
}

if [ -n "$1" ]; then
  run_testcase "${TEST_CASE_DIR}/"*"${1}"*
  clean_exit
fi

for tc in $TEST_CASES; do
  run_testcase "${TEST_CASE_DIR}/${tc}"
done
clean_exit
