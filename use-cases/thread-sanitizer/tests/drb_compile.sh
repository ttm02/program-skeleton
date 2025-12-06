#!/usr/bin/env bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2
[ -n "$TEST_CASE" ] || exit 42
DRB_DIR=$(dirname "$TEST_CASE")

source "${BINARY_DIR}/setup_env.sh"

USE_COMPILER_PASS=$3
export USE_COMPILER_PASS

CFLAGS="-O2 -g -fopenmp -fsanitize=thread"
PASS_FLAGS="-fuse-ld=lld -flto -fwhole-program-vtables -fno-inline"

poly_flags() {
    # needs additional compiler flags
    POLYFLAG="-I$DRB_DIR -I$DRB_DIR/utilities -DPOLYBENCH_NO_FLUSH_CACHE -DPOLYBENCH_TIME -D_POSIX_C_SOURCE=200112L"
    CFLAGS="$CFLAGS $POLYFLAG"
}

compile() {
    TEST_CASE="$1"
    SUFFIX_NAME="$2"

    case "$TEST_CASE" in
    *.c) WRAPPER="$CLANG_WRAP_CC" ;;
    *.cpp) WRAPPER="$CLANG_WRAP_CXX" ;;
    *.f) WRAPPER="$CLANG_WRAP_FC" ;;
    *) WRAPPER="$CLANG_WRAP_CC" ;;
    esac

    if grep -q 'PolyBench' "$TEST_CASE"; then
        poly_flags
        $WRAPPER $CFLAGS $PASS_FLAGS -c -o "polybench_${SUFFIX_NAME}.o" "${DRB_DIR}/utilities/polybench.c"
        $WRAPPER $CFLAGS $PASS_FLAGS -c -o "main_${SUFFIX_NAME}.o" "$TEST_CASE"
        $WRAPPER $CFLAGS $PASS_FLAGS -o "./a.out_${SUFFIX_NAME}" "main_${SUFFIX_NAME}.o" "polybench_${SUFFIX_NAME}.o"
    else
        $WRAPPER $CFLAGS $PASS_FLAGS -o "./a.out_${SUFFIX_NAME}" "$TEST_CASE"
    fi
}

if [ -z "$OUTPUT_SUFFIX" ]; then
    if [ "$USE_COMPILER_PASS" = 'true' ]; then
        OUTPUT_SUFFIX='pass'
    else
        OUTPUT_SUFFIX='orig'
    fi
fi

if [ "$USE_STATIC_ANALYSIS" = 'true' ]; then
    CFLAGS="$CFLAGS --enable-static-analysis"
fi

rm -f ./a.out_"${OUTPUT_SUFFIX}"
compile "$TEST_CASE" "$OUTPUT_SUFFIX"

exit 0
