#!/usr/bin/env bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2
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

if [ "$USE_COMPILER_PASS" = "true" ]; then
    rm -f ./a.out
    # with pass
    if grep -q 'PolyBench' "$TEST_CASE"; then
        poly_flags
        $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -c -o polybench_pass.o $DRB_DIR/utilities/polybench.c
        $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -c -o main_pass.o "$TEST_CASE"
        $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -o ./a.out main_pass.o polybench_pass.o
    else
        $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -o ./a.out "$TEST_CASE"
    fi
else
    rm -f ./a.out_original
    # normal compilation
    if grep -q 'PolyBench' "$TEST_CASE"; then
        poly_flags
        $CLANG_WRAP_CC $CFLAGS -c -o polybench_orig.o $DRB_DIR/utilities/polybench.c
        $CLANG_WRAP_CC $CFLAGS -c -o main_orig.o "$TEST_CASE"
        $CLANG_WRAP_CC $CFLAGS -o ./a.out_original main_orig.o polybench_orig.o
    else
        # normal compilation
        $CLANG_WRAP_CC $CFLAGS -o ./a.out_original "$TEST_CASE"
    fi
fi

exit 0
