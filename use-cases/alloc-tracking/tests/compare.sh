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

CC=$CLANG_WRAP_CC

if [[ "$MPI_USAGE" == "USE_MPI" ]]; then
  CC=mpicc
  export OMPI_CC=$CLANG_WRAP_CC
  if [[ "$MPI_MODE" == "IGNORE_MPI_COMMUNICATION" ]]; then
    export "AT_PASS_IGNORE_MPI_COMMUNICATION=true"
  elif [[ "$MPI_MODE" == "ADD_ALL_MPI_COMMUNICATION" ]]; then
    export "AT_PASS_ADD_ALL_MPI_COMMUNICATION=true"
  fi
fi

# all testcases are c
echo "$CC $CFLAGS -o ./original/a.out $LINK_ALLOC_TRACKING_LIB_ARG $2/*.c"
export "AT_PASS_ENABLE_LOGGING=true"
$CC $CFLAGS -o ./original/a.out $LINK_ALLOC_TRACKING_LIB_ARG $2/*.c
export "AT_PASS_ENABLE_SLICING=true"
$CC $CFLAGS -o ./skeleton/a.out $LINK_ALLOC_TRACKING_LIB_ARG $2/*.c


# execution
if [[ -x "./skeleton/a.out" ]]; then
    # skeletonization successfull
    if [[ "$MPI_USAGE" == "USE_MPI" ]]; then
          cd original
          mpirun -n $NUM_PROCS --map-by :OVERSUBSCRIBE ./a.out
          cd ../skeleton
          mpirun -n $NUM_PROCS --map-by :OVERSUBSCRIBE ./a.out
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






