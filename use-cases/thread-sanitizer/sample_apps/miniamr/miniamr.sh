#!/usr/bin/env bash

# location of this script
# this is the location where tha path file to introduce a datarace is
APP_PATCH_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER=""

APP_NAME="MINIAMR"

# $1 : directory to download into
download() {
    echo "download"
    git clone https://github.com/Mantevo/miniAMR "$1"
    # select the latest release version
    (cd "$1" &&
        git switch -c 'precompute-testing' &&
        git reset --hard 'v1.7.1')
    patch "$1/openmp/Makefile" "${APP_PATCH_DIR}/Makefile.patch"
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace() {
    echo "patch to inject datarace"
    # re-introduce the datarace present in original code
    # TODO
    #patch -R "$1/main.cpp" "${APP_PATCH_DIR}/remove_datarace.patch"
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace() {
    echo "reverse data race injection"
    # TODO
    #patch "$1/main.cpp" "${APP_PATCH_DIR}/remove_datarace.patch"
}

# build
# $1 : directory with src (same argument as given to download dir)
# $2 : build mode: original or modified by pass
build_app() {
    source "${APP_PATCH_DIR}/../generic_app.sh"
    setup_build_generic_app "$1" "$2" "$3"

    export MY_CUSTOM_CPP_FLAGS="$APP_CXX_FLAGS"
    export MY_CUSTOM_LD_FLAGS="$APP_CXX_FLAGS"

    (
        # clean up any previous build
        rsync -aHAX --delete "${APP_DIR}/" "${APP_DIR}_${BUILD_MODE}/" --exclude='.git' || return
        cd "${APP_DIR}_${BUILD_MODE}" || return

        cd 'openmp' && make && cp 'miniAMR.x' "$TARGET_BIN"
    )
}
