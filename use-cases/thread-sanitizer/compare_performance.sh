#!/usr/bin/env bash

usage() {
  echo "$0 <build dir>"
  exit 1
}

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
TEST_CASE_DIR="${MY_CMAKE_BUILD_DIR}/_deps/drb-src/micro-benchmarks"
TEST_CASES=$(ls "$TEST_CASE_DIR")

source "${BINARY_DIR}/setup_env.sh"

GREP_STRING="WARNING: ThreadSanitizer: data race"

MY_TMP_DIR=$(mktemp -d --suffix="JustTheRaces-timing")
MY_CUR_DIR=$(pwd)

cd "$MY_TMP_DIR" || exit 10

echo "testcase,time_original,time_precompute,found_by" | tee "${MY_CUR_DIR}/timing.csv"

save_time_to_file() {
  (
    echo -n "$(basename "$1")"
    echo -n ","
    cat "${MY_TMP_DIR}/time_orig.log" | tr -d "\n"
    echo -n ","
    cat "${MY_TMP_DIR}/time_pass.log" | tr -d "\n"
    echo -n ","
    echo "$2"
  ) | tee -a "${MY_CUR_DIR}/timing.csv"
}

time_testcase() {
  /usr/bin/env time -f "%e" -o "${MY_TMP_DIR}/time_${1}.log" \
    --quiet timeout 300 "$2" 2>&1
}

run_testcase() {
  TEST_CASE="$1"

  if [[ "$TEST_CASE" == *.c ]]; then
    "${SCRIPT_DIR}/tests/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" false >/dev/null 2>&1 &
    pid_compile_orig=$!

    "${SCRIPT_DIR}/tests/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" true >/dev/null 2>&1 &
    pid_compile_pass=$!

    wait $pid_compile_orig
    wait $pid_compile_pass

    if [[ -x "./a.out" ]]; then
      if time_testcase "orig" ./a.out_original | grep -qF "$GREP_STRING"; then
        if time_testcase "pass" ./a.out | grep -qF "$GREP_STRING"; then
          save_time_to_file "$TEST_CASE" "both"
        else
          save_time_to_file "$TEST_CASE" "orig"
        fi
      else
        if time_testcase "pass" ./a.out | grep -qF "$GREP_STRING"; then
          save_time_to_file "$TEST_CASE" "precomputed"
        else
          save_time_to_file "$TEST_CASE" "neither"
        fi
      fi
    else
      echo "0" >"${MY_TMP_DIR}/time_orig.log"
      echo "0" >"${MY_TMP_DIR}/time_precompute.log"
      save_time_to_file "$TEST_CASE" "compilation failed"
    fi
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
