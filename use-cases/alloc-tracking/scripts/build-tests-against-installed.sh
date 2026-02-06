#!/usr/bin/env bash
set -euo pipefail

INSTALL_PREFIX="${1:-/tmp/alloctracking-install}"

# Define configurations as triples: "<build_dir>:<slicing>:<logging>:<build_type>"
CONFIGS=(
    "build_debug_slicing_off_logging_on:OFF:ON:DEBUG"
    "build_debug_slicing_on_logging_on:ON:ON:DEBUG"
    "build_release_slicing_off_logging_on:OFF:ON:RELEASE"
    "build_release_slicing_on_logging_on:ON:ON:RELEASE"
)

for cfg in "${CONFIGS[@]}"; do
  IFS=":" read -r BUILD_DIR ENABLE_SLICING ENABLE_LOGGING BUILD_TYPE <<< "$cfg"

  echo "=== Building test suite: ${BUILD_DIR} ==="
  
  mkdir -p "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  cmake ../use_cases/test_suite \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER=mpicc \
    -DCMAKE_CXX_COMPILER=mpicxx \
    -DAllocTracking_DIR="${INSTALL_PREFIX}/lib/cmake/AllocTracking" \
    -DENABLE_SLICING="${ENABLE_SLICING}" \
    -DENABLE_LOGGING="${ENABLE_LOGGING}"

  make #-j$(nproc)

  cd ..
done
