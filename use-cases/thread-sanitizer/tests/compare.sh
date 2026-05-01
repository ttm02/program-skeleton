#!/usr/bin/env bash

# where the wrappers are found
BINARY_DIR=$1
source "${BINARY_DIR}/setup_env.sh"

# the testcase to use
TEST_CASE=$2

SCRIPT_DIR=$(dirname "$(realpath "$0")")

"${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" false &
pid_compile_orig=$!

export USE_STATIC_ANALYSIS=true
"${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE" true &
pid_compile_pass=$!
unset USE_STATIC_ANALYSIS

wait $pid_compile_orig
wait $pid_compile_pass

# compares standard thread sanitizer with the sliced one
GREP_STRING="WARNING: ThreadSanitizer: data race"

# execution
if [[ -x "./a.out_pass" ]]; then
    if ./a.out_orig 2>&1 | grep -qF "$GREP_STRING"; then
        if ./a.out_pass 2>&1 | grep -qF "$GREP_STRING"; then
            # success
            echo "both versions found the datarace"
            exit 0
        else
            echo "Original sanitizer found the data race but sliced not"
            exit 1
        fi
    else
        if ./a.out_pass 2>&1 | grep -qF "$GREP_STRING"; then
            echo "Original sanitizer found no race but sliced did"
            exit 1
        else
            # success
            echo "both versions found no race"
            exit 0
        fi
    fi
else
    echo "Compilation fail"
    exit 2
fi
