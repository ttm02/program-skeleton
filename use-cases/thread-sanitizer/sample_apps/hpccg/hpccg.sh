#!/usr/bin/env bash

# location of this script
# this is the location where tha path file to introduce a datarace is
APP_PATCH_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER="3 3 3"

APP_NAME="HPCCG"

# $1 : directory to download into
download() {
  echo "download"
  git clone https://github.com/Mantevo/HPCCG.git $1
  # set the specific commit we used
  # probably not necessary
  (cd "$1" && git checkout 80dd2f12a4e8aa70c330a5686cdda3fd187c2545)
  # patch makefile
  patch "$1/Makefile" "${APP_PATCH_DIR}/Makefile.patch"
  # patch application to remove datarace
  patch "$1/main.cpp" "${APP_PATCH_DIR}/remove_datarace.patch"
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace() {
  echo "patch to inject datarace"
  # re-introduce the datarace present in original code
  patch -R "$1/main.cpp" "${APP_PATCH_DIR}/remove_datarace.patch"
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace() {
  echo "reverse data race injection"
  patch "$1/main.cpp" "${APP_PATCH_DIR}/remove_datarace.patch"
}

# build
# $1 : directory with src (same argument as given to download dir)
# $2 : build mode: original or modified by pass
build_app() {
  source "${APP_PATCH_DIR}/../generic_app.sh"
  setup_build_generic_app "$1" "$2" "$3"

  export SANITIZE_FLAG="$APP_CXX_FLAGS"
  rm -f "./hpccg-*.yaml"

  (
    # clean up any previous build
    rsync -aHAX --delete "${APP_DIR}/" "${APP_DIR}_${BUILD_MODE}/" --exclude='.git' || return
    cd "${APP_DIR}_${BUILD_MODE}" || return

    make && cp test_HPCCG "$TARGET_BIN"
  )
}
