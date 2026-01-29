#!/bin/bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

# the python script used to compare teh output
SCRIPT_DIR=$(dirname "$0")
COMPARE_SCRIPT=$SCRIPT_DIR/../validate_logging_and_test_output.py

source ${BINARY_DIR}/setup_env.sh

CFLAGS="-O2 -g -fuse-ld=lld -flto"

RUNDIR=$(mktemp -d)
trap "rm -rf $RUNDIR" EXIT
# make sure the tempdir is removed on script exit

#rm ./a.out ./a.out_original
cd $RUNDIR

mkdir original
mkdir skeleton

export USE_COMPILER_PASS=true
# compile

FC=$FLANG_WRAP

export "AT_PASS_ENABLE_LOGGING=true"
$FC $CFLAGS -o ./original/a.out $LINK_ALLOC_TRACKING_LIB_ARG $2/*.f90
export "AT_PASS_ENABLE_SLICING=true"
$FC $CFLAGS -o ./skeleton/a.out $LINK_ALLOC_TRACKING_LIB_ARG $2/*.f90


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






