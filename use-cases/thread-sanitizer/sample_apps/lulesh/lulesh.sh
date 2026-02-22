#!/usr/bin/env bash

# location of this script
# this is the location where tha path file to introduce a datarace is
LULESH_PATCH_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER="-s 10 -i 3"

APP_NAME="LULESH"
APP_CXX_FLAGS="-O2 -flto -fwhole-program-vtables -fuse-ld=lld"
APP_CMAKE_PARAMETER="-DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

# $1 : directory to download into
download() {
  echo "download"
  git clone https://github.com/LLNL/LULESH.git "$1"
  # set the specific commit we used
  # probably not necessary
  (cd "$1" && git checkout 3e01c40b3281aadb7f996525cdd4a3354f6d3801)
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace() {
  echo "patch to inject datarace"
  patch "$1/lulesh.cc" "$LULESH_PATCH_DIR/introduce_datarace.patch"
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace() {
  echo "reverse data race injection"
  patch -R "$1/lulesh.cc" "$LULESH_PATCH_DIR/introduce_datarace.patch"
}

# build
# $1 : directory with src (same argument as given to download dir)
# $2 : build mode: original or modified by pass
build_app() {
  APP_DIR="$1"
  BUILD_MODE="$2"
  USE_COMPILER_PASS=$3
  export USE_COMPILER_PASS

  echo "build ${APP_DIR} with ${BUILD_MODE}"

  BUILD_DIR="${APP_DIR}/build_${BUILD_MODE}"
  TARGET_BIN=$(realpath "${PWD}/${APP_NAME}_${BUILD_MODE}.exe")

  if [ "$BUILD_MODE" != 'vanilla' ]; then
    # for testing we need this
    APP_CXX_FLAGS="$APP_CXX_FLAGS -fsanitize=thread"

    if [ -n "$MY_STAN_PASS_MODE" ]; then
      APP_CXX_FLAGS="$APP_CXX_FLAGS $MY_STAN_PASS_MODE_ARGS"
    fi
  fi

  # clean up any previous build
  rm -f "$TARGET_BIN"
  rm -fr "$BUILD_DIR"

  mkdir "$BUILD_DIR"
  (
    cd "$BUILD_DIR" &&
      USE_COMPILER_PASS=false cmake $APP_CMAKE_PARAMETER -DCMAKE_CXX_FLAGS="$APP_CXX_FLAGS" .. &&
      make &&
      cp lulesh2.0 "$TARGET_BIN"
  )
}
