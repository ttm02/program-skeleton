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

  num_modes=16
  build_array=()

  build_app "$APP_NAME" 'vanilla' false >/dev/null 2>&1 &
  pid_compile_vanilla=$!

  build_app "$APP_NAME" 'orig' false >/dev/null 2>&1 &
  build_pid=$!
  build_array+=("$build_pid")

  for i in $(seq 1 $num_modes); do
    SLURM_ARRAY_TASK_ID=$i
    source "${SCRIPT_DIR}/../../tests/static_analysis_mode.sh"
    build_app "$APP_NAME" "${MY_STAN_PASS_MODE}" true >/dev/null 2>&1 &
    build_pid=$!
    build_array+=("$build_pid")
  done

  wait $pid_compile_vanilla
  for i in $(seq 0 $num_modes); do
    wait "${build_array["$i"]}"
  done

  build_successful="true"

  if [[ ! -x "./${APP_NAME}_vanilla.exe" ]]; then
    echo "build error in ${APP_NAME}_vanilla.exe"
    build_successful="false"
  fi

  for i in $(seq 0 $num_modes); do
    SLURM_ARRAY_TASK_ID=$i
    source "${SCRIPT_DIR}/../../tests/static_analysis_mode.sh"
    if [[ ! -x "./${APP_NAME}_${MY_STAN_PASS_MODE}.exe" ]]; then
      echo "build error in ${APP_NAME}_${MY_STAN_PASS_MODE}.exe"
      build_successful="false"
    fi
  done

  if [ "$build_successful" = "true" ]; then
    echo "SUCCESSFULLY build $APP_NAME"
  else
    echo "FAILURE during building $APP_NAME"
  fi
}

if [ -n "$1" ]; then
  init_app "$1"
else
  init_app 'lulesh'
  init_app 'hpccg'
  init_app 'tealeaf'
  init_app 'miniamr'
fi

echo "setup for all sample apps completed"
echo "$WORK_DIR"
exit 0
