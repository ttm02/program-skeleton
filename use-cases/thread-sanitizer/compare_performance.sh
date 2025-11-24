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
    MY_CMAKE_BUILD_DIR="$1"
  fi
fi

BINARY_DIR="${MY_CMAKE_BUILD_DIR}/use-cases/thread-sanitizer"

# TODO fortran testcases
TEST_CASE_DIR="${MY_CMAKE_BUILD_DIR}/_deps/drb-src/micro-benchmarks"
TEST_CASES=$(ls "$TEST_CASE_DIR")

source "${BINARY_DIR}/setup_env.sh"
RUN_SCRIPT="${BINARY_DIR}/run.sh"

GREP_STRING="WARNING: ThreadSanitizer: data race"

echo "testcase,time_original,time_precompute,found_by" >timing.csv

save_time_to_file() {
  (
    echo -n "$1"
    echo -n ","
    cat "${MY_TMP_DIR}/time_orig.log" | tr -d "\n"
    echo -n ","
    cat "${MY_TMP_DIR}/time_precompute.log" | tr -d "\n"
    echo -n ","
    echo "$2"
  ) | tee -a timing.csv
}

MY_TMP_DIR=$(mktemp -d --suffix="JustTheRaces-timing")

for TEST_CASE in $TEST_CASES; do
  if [[ "$TEST_CASE" == *.c ]]; then
    rm -f ./a.out ./a.out_original

    # compile
    $RUN_SCRIPT "${TEST_CASE_DIR}/${TEST_CASE}" &>/dev/null

    if [[ -x "./a.out" ]]; then
      # compilation successful
      /usr/bin/env time -f "%e" -o "${MY_TMP_DIR}/time_orig.log" \
        --quiet timeout 300 ./a.out_original &>"${MY_TMP_DIR}/orig.log"
      /usr/bin/env time -f "%e" -o "${MY_TMP_DIR}/time_precompute.log" \
        --quiet timeout 300 ./a.out &>"${MY_TMP_DIR}/precompute.log"

      if grep -qF "$GREP_STRING" "${MY_TMP_DIR}/orig.log"; then
        if grep -qF "$GREP_STRING" "${MY_TMP_DIR}/precompute.log"; then
          save_time_to_file "$TEST_CASE" "both"
        else
          save_time_to_file "$TEST_CASE" "orig"
        fi
      else
        if grep -qF "$GREP_STRING" "${MY_TMP_DIR}/precompute.log"; then
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
done

rm -fr "$MY_TMP_DIR"
exit 0
