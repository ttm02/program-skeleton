setup_build_generic_app() {
    APP_DIR="$1"
    BUILD_MODE="$2"
    USE_COMPILER_PASS=$3
    export USE_COMPILER_PASS

    echo "build ${APP_DIR} with ${BUILD_MODE}"

    BUILD_DIR=$(realpath "${APP_DIR}/build_${BUILD_MODE}")
    TARGET_BIN=$(realpath "${PWD}/${APP_NAME}_${BUILD_MODE}.exe")

    if [ "$BUILD_MODE" != 'vanilla' ]; then
        # for testing we need this
        APP_CXX_FLAGS="$APP_CXX_FLAGS -fsanitize=thread"

        if [ -n "$MY_STAN_PASS_MODE" ]; then
            APP_CXX_FLAGS="$APP_CXX_FLAGS $MY_STAN_PASS_MODE_ARGS"
        fi
    else
        APP_CXX_FLAGS="$APP_CXX_FLAGS --disable-precompute-pass"
    fi

    # clean up any previous build
    rm -f "$TARGET_BIN"
}
