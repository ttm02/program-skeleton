#!/usr/bin/env bash
set -euo pipefail

# Adjust this install prefix to your desired location
INSTALL_PREFIX="${1:-/tmp/alloctracking-install}"
BUILD_TYPE="${2:-Debug}"  # Second parameter for build type, defaults to Debug
BUILD_DIR="build-libs"

# Validate build type
case "${BUILD_TYPE}" in
    Debug|Release|RelWithDebInfo|MinSizeRel)
        echo "Building with CMAKE_BUILD_TYPE=${BUILD_TYPE}"
        ;;
    *)
        echo "Error: Invalid build type '${BUILD_TYPE}'"
        echo "Valid options: Debug, Release, RelWithDebInfo, MinSizeRel"
        exit 1
        ;;
esac


mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. \
  -DBUILD_TEST_SUITE=OFF \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DCMAKE_C_COMPILER=mpicc \
  -DCMAKE_CXX_COMPILER=mpicxx

# build and install
make -j$(nproc)
cmake --install . --prefix "${INSTALL_PREFIX}"

echo "Installed AllocTracking to ${INSTALL_PREFIX} (${BUILD_TYPE})"
