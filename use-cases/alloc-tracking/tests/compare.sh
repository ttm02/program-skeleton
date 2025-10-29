#!/bin/bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

# the python script used to compare teh output
SCRIPT_DIR=$(dirname "$0")
COMPARE_SCRIPT=$SCRIPT_DIR/validate_logging_and_test_output.py

source ${BINARY_DIR}/setup_env.sh

CFLAGS="-O2 -g -fuse-ld=lld -flto -fwhole-program-vtables -fno-inline"

RUNDIR=$(mktemp -d)
trap "rm -rf $RUNDIR" EXIT
# make sure the tempdir is removed on script exit

#rm ./a.out ./a.out_original
cd $RUNDIR

mkdir original
mkdir skeleton

export USE_COMPILER_PASS=true
# compile


$CLANG_WRAP_CC $CFLAGS -o ./original/a.out $ENABLE_ALLOC_TRACKING_ARG $2
$CLANG_WRAP_CC $CFLAGS -o ./skeleton/a.out $ENABLE_ALLOC_TRACKING_ARG $SLICE_ALLOC_ARG $2


# execution
if [[ -x "./skeleton/a.out" ]]; then
    # skeletonization successfull
    cd original
    ./a.out
    cd ../skeleton
    ./a.out
    cd ..
else
    echo "Compilation fail"
    exit -2
fi


python3 $COMPARE_SCRIPT ./original ./skeleton






