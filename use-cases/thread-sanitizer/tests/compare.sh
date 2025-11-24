#!/usr/bin/env bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

SCRIPT_DIR=$(dirname "$(realpath "$0")")
"${SCRIPT_DIR}/drb_compile.sh" "$BINARY_DIR" "$TEST_CASE"

# compares standard thread sanitizer with the precomputed one
GREP_STRING="WARNING: ThreadSanitizer: data race"

# execution
if [[ -x "./a.out" ]]; then
    # a.out exists
    if ./a.out_original 2>&1 | grep -qF "$GREP_STRING"; then
        # original sanitizer found data race

        if ./a.out 2>&1 | grep -qF "$GREP_STRING"; then
            # success
            echo "both versions found the datarace"
            exit 0
        else
            echo "Original sanitizer found the data race but precomputed not"
            exit 1
        fi
    else
        #echo "Original sanitizer found no race"
        if ./a.out 2>&1 | grep -qF "$GREP_STRING"; then
            echo "Original sanitizer found no race but precomputed did"
            exit 1
        else
            #success
            echo "both versions found no race"
            exit 0
        fi
    fi
else
    echo "Compilation fail"
    exit 2
fi
