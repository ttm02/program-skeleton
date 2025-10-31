#!/bin/bash

#SBATCH -J testingAllocTrackingTestSuite
#SBATCH --mail-user=sebastian.laux@stud.tu-darmstadt.de
#SBATCH --mail-type=ALL
#SBATCH -A project02560
#SBATCH -e /home/sl82bepo/job_output/job.err.%j
#SBATCH -o /home/sl82bepo/job_output/job.out.%j
#SBATCH -n 1
#SBATCH --mem-per-cpu=750
#SBATCH -t 00:15:00
#SBATCH -D /home/sl82bepo/alloc-tracking

echo "This is Job $SLURM_JOB_ID"

module purge
module load gcc/13.1.0 clang/16.0.6 cmake/3.26.1

# The rest is done acording to my local shell script
export PATH="$HOME/build/mpich-clang/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/build/mpich-clang/lib:$LD_LIBRARY_PATH"

SCRATCH_USER="/work/scratch/sl82bepo"
SRC_ROOT="$HOME/alloc-tracking"
INSTALL_PREFIX="${1:-$HOME/test_install}"
SRC_ROOT="$HOME/alloc-tracking"
TEST_SUITE_SRC="$SRC_ROOT/use_cases/test_suite"

#export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH:-}"

# dir on scratch
WORK_ROOT="${SLURM_TMPDIR:-$SCRATCH_USER/alloc-tracking-build}"
mkdir -p "$WORK_ROOT" || true
if [ ! -d "$WORK_ROOT" ]; then
  WORK_ROOT="$SRC_ROOT/.tmp-$SLURM_JOB_ID"
  mkdir -p "$WORK_ROOT"
fi
cd "$WORK_ROOT"

echo "Using INSTALL_PREFIX=$INSTALL_PREFIX"
echo "Using WORK_ROOT=$WORK_ROOT"
which mpicc || true

CONFIGS=(
  #"build_debug_slicing_off_logging_on:OFF:ON:Debug"
  #"build_debug_slicing_on_logging_on:ON:ON:Debug"
  "build_release_slicing_off_logging_on:OFF:ON:Release"
  "build_release_slicing_on_logging_on:ON:ON:Release"
)

which cmake
cmake --version

for cfg in "${CONFIGS[@]}"; do
  IFS=":" read -r BUILD_DIR ENABLE_SLICING ENABLE_LOGGING BUILD_TYPE <<< "$cfg"
  echo "=== Building test suite: ${BUILD_DIR} (${BUILD_TYPE}, slicing=${ENABLE_SLICING}, logging=${ENABLE_LOGGING}) ==="
  mkdir -p "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  cmake "${TEST_SUITE_SRC}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER=mpicc \
    -DCMAKE_CXX_COMPILER=mpicxx \
    -DAllocTracking_DIR="${INSTALL_PREFIX}/lib/cmake/AllocTracking" \
    -DENABLE_SLICING="${ENABLE_SLICING}" \
    -DENABLE_LOGGING="${ENABLE_LOGGING}"
  make
  cd ..
done

echo "Job script done!"

