#!/bin/bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

# the python script used to compare the output
SCRIPT_DIR=$(dirname "$0")
COMPARE_SCRIPT=$SCRIPT_DIR/../validate_logging_and_test_output.py

source ${BINARY_DIR}/setup_env.sh

RUNDIR=$(mktemp -d)
trap "rm -rf $RUNDIR" EXIT
# make sure the tempdir is removed on script exit

#rm ./a.out ./a.out_original
cd $TEST_CASE

mkdir original
mkdir skeleton

export USE_COMPILER_PASS=true
# compile

CC=mpicc
export OMPI_CC=$CLANG_WRAP_CC
export "AT_PASS_ADD_ALL_MPI_COMMUNICATION=true"

# all testcases are c
export "AT_PASS_ENABLE_LOGGING=true"

make
cp ./104.milc original/104.milc
make clean

export "AT_PASS_ENABLE_SLICING=true"

make
cp ./104.milc skeleton/104.milc
make clean


# execution
if [[ -x "./skeleton/104.milc" ]]; then
    # skeletonization successfull
          cd original
          mpirun -n 4 --map-by :OVERSUBSCRIBE ./104.milc < ../data/mtest_su3imp.in
          cd ../skeleton
          mpirun -n 4 --map-by :OVERSUBSCRIBE ./104.milc < ../data/mtest_su3imp.in
          cd ..

else
    echo "Compilation fail"
    exit -2
fi


python3 $COMPARE_SCRIPT ./original ./skeleton






