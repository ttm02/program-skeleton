#!/usr/bin/env bash

# location of this script
# this is the location where tha path file to introduce a datarace is
APP_PATCH_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER=""

APP_NAME="TEALEAF"
APP_CXX_FLAGS="-flto -fwhole-program-vtables -fuse-ld=lld -O3"
APP_CMAKE_PARAMETER="-DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DMODEL=omp"

# $1 : directory to download into
download() {
  echo "download"
  git clone https://github.com/UoB-HPC/TeaLeaf.git "$1"
  # set the specific commit we used
  # probably not necessary
  (cd "$1" && git checkout e70261c0be40537da75b258108ed2898f84f3c58)
  # patch input file to have smaller problem size for testing
  patch "$1/tea.in" "${APP_PATCH_DIR}/problem_size.patch"
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace() {
  echo "patch to inject datarace"
  # re-introduce the datarace present in original code
  patch "$1/src/omp/cg.cpp" "${APP_PATCH_DIR}/introduce_datarace.patch"
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace() {
  echo "reverse data race injection"
  patch -R "$1/src/omp/cg.cpp" "${APP_PATCH_DIR}/introduce_datarace.patch"
}

# build
# $1 : directory with src (same argument as given to download dir)
# $2 : build mode: original or modified by pass
build_app() {
  source "${APP_PATCH_DIR}/../generic_app.sh"
  setup_build_generic_app "$1" "$2" "$3"

  rm -fr "$BUILD_DIR"
  mkdir "$BUILD_DIR"

  (
    cd "$BUILD_DIR" &&
      USE_COMPILER_PASS=false cmake $APP_CMAKE_PARAMETER -DCMAKE_CXX_FLAGS="$APP_CXX_FLAGS" .. &&
      make &&
      cp omp-tealeaf "$TARGET_BIN"
  )
  cp "$1/tea.in" "$1/tea.problems" "$(dirname "$TARGET_BIN")/"
}
