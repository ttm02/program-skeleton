#!/usr/bin/env bash

# location of this script
# this is the location where tha path file to introduce a datarace is
APP_PATCH_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER="--zones 16,16,16"

APP_NAME="KRIPKE"
APP_CXX_FLAGS="-O2 -flto -fwhole-program-vtables -fuse-ld=lld"
APP_CMAKE_PARAMETER="-DCMAKE_CXX_COMPILER=${CLANG_WRAP_CXX} -DENABLE_OPENMP=ON -DENABLE_MPI=OFF"

# $1 : directory to download into
download() {
  echo "download"
  git clone 'https://github.com/LLNL/Kripke.git' "$1"
  # set the specific commit we used
  # probably not necessary
  (cd "$1" && git switch -c 'precompute-testing' &&
    git reset --hard '01f6f85c02ceffcd2bc06e42cee997867dd142c5' &&
    git submodule update --init --depth 1 -- 'blt' &&
    git submodule update --init --depth 1 -- 'tpl/raja' &&
    git -C 'tpl/raja' submodule update --init --depth 1 -- 'tpl/camp')
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace() {
  echo "patch to inject datarace"
  # TODO
  patch "$1/my_file.txt" "${APP_PATCH_DIR}/introduce_datarace.patch"
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace() {
  echo "reverse data race injection"
  # TODO
  patch -R "$1/my_file.txt" "${APP_PATCH_DIR}/introduce_datarace.patch"
}

# build
# $1 : directory with src (same argument as given to download dir)
# $2 : build mode: original or modified by pass
build_app() {
  source "${APP_PATCH_DIR}/../generic_app.sh"
  setup_build_generic_app "$1" "$2" "$3"

  rm -fr "$BUILD_DIR"
  (
    cd "$APP_DIR"
    USE_COMPILER_PASS=false cmake -B "$BUILD_DIR" $APP_CMAKE_PARAMETER -DCMAKE_CXX_FLAGS="$APP_CXX_FLAGS" &&
      cmake --build "$BUILD_DIR"
  )
  cp "${BUILD_DIR}/kripke.exe" "$TARGET_BIN"
}
