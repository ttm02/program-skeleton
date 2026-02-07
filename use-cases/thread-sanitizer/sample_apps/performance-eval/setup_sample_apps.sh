#!/usr/bin/env bash

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
cd "${PRECOMPUTE_DIR}" || exit 11

BUILD_NAME='build-perf-tests'
echo "create new build directory"
cmake -B $BUILD_NAME -S . -G 'Ninja' -DMPI_USE_CASE='OFF' >/dev/null 2>&1
echo "building slicing pass"
cmake --build $BUILD_NAME >/dev/null 2>&1

BUILD_DIR="${PRECOMPUTE_DIR}/${BUILD_NAME}"
WORK_DIR="${BUILD_DIR}/use-cases/thread-sanitizer/sample_apps"
cd "$WORK_DIR" || exit 12
source ../setup_env.sh

[ -z "$SANITIZER_PASS" ] && exit 21
if ! [ -f "$SANITIZER_PASS" ]; then
  exit 22
fi
if ! echo "$SANITIZER_PASS" | grep -q "$BUILD_DIR"; then
  exit 23
fi

init_app() {
  unset APP_NAME
  source "${SCRIPT_DIR}/../$1/$1.sh"
  [ -z "$APP_NAME" ] && exit 31

  if [ ! -d "$APP_NAME" ]; then
    echo "download $APP_NAME"
    download "$APP_NAME" >/dev/null 2>&1
  fi

  echo "building $APP_NAME"

  build_app "$APP_NAME" 'norm' false &>/dev/null
  pid_compile_norm=$!
  build_app "$APP_NAME" 'orig' false &>/dev/null
  pid_compile_orig=$!
  build_app "$APP_NAME" 'pass' true &>/dev/null
  pid_compile_pass=$!
  export USE_STATIC_ANALYSIS=true
  build_app "$APP_NAME" 'stan' true &>/dev/null
  pid_compile_stan=$!
  unset USE_STATIC_ANALYSIS

  wait $pid_compile_norm
  wait $pid_compile_orig
  wait $pid_compile_pass
  wait $pid_compile_stan

  if [[ ! -x "./${APP_NAME}_orig.exe" ]] || [[ ! -x "./${APP_NAME}_pass.exe" ]] || [[ ! -x "./${APP_NAME}_stan.exe" ]]; then
    echo "build error in $APP_NAME"
    exit 32
  fi

  echo "successfully build $APP_NAME"
}

init_app 'lulesh'
init_app 'hpccg'
init_app 'tealeaf'

echo "setup for all sample apps completed"
echo "$WORK_DIR"
exit 0
