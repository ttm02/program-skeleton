#!/bin/bash

# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

# flags about MPI usage
MPI_USAGE=$3
MPI_MODE=$4
NUM_PROCS=$5

# the python script used to compare teh output
SCRIPT_DIR=$(dirname "$0")
COMPARE_SCRIPT=$SCRIPT_DIR/validate_logging_and_test_output.py

source ${BINARY_DIR}/setup_env.sh

CFLAGS="-O2 -g -fuse-ld=lld -flto -fwhole-program-vtables -fno-inline -lm"

RUNDIR=$(mktemp -d)
trap "rm -rf $RUNDIR" EXIT
# make sure the tempdir is removed on script exit

#rm ./a.out ./a.out_original
cd $RUNDIR

mkdir original
mkdir skeleton

export USE_COMPILER_PASS=true
# compile

CC=clang

if [[ "$MPI_USAGE" == "USE_MPI" ]]; then
  CC=mpicc
  export OMPI_CC=clang
  if [[ "$MPI_MODE" == "IGNORE_MPI_COMMUNICATION" ]]; then
    CFLAGS="$CFLAGS -Wl,-mllvm=-at-pass-ignore-mpi-communication=true"
  elif [[ "$MPI_MODE" == "ADD_ALL_MPI_COMMUNICATION" ]]; then
    CFLAGS="$CFLAGS -Wl,-mllvm=-at-pass-add-all-mpi-communication=true"
  fi
fi

# all testcases are c
echo "$CC -Wl,--load-pass-plugin=$COMPILER_PASS -Wl,-mllvm=-load=$COMPILER_PASS $CFLAGS -o ./original/a.out $ENABLE_ALLOC_TRACKING_ARG $2/*.c"
$CC -Wl,--load-pass-plugin=$COMPILER_PASS -Wl,-mllvm=-load=$COMPILER_PASS $CFLAGS -o ./original/a.out $ENABLE_ALLOC_TRACKING_ARG $2/*.c
$CC -Wl,--load-pass-plugin=$COMPILER_PASS -Wl,-mllvm=-load=$COMPILER_PASS $CFLAGS -o ./skeleton/a.out $ENABLE_ALLOC_TRACKING_ARG $SLICE_ALLOC_ARG $2/*.c


# execution
if [[ -x "./skeleton/a.out" ]]; then
    # skeletonization successfull
    if [[ "$MPI_USAGE" == "USE_MPI" ]]; then
          cd original
          mpirun -n $NUM_PROCS ./a.out
          cd ../skeleton
          mpirun -n $NUM_PROCS ./a.out
          cd ..
    else
      cd original
      ./a.out
      cd ../skeleton
      ./a.out
      cd ..
    fi
else
    echo "Compilation fail"
    exit -2
fi


python3 $COMPARE_SCRIPT ./original ./skeleton






