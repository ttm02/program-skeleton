#!/usr/bin/env bash

usage() {
  echo "$0 <build dir>"
  exit 1
}

MY_CUR_DIR=$(pwd)
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
MY_CMAKE_BUILD_DIR="${PRECOMPUTE_DIR}/build"

if [ -n "$1" ]; then
  if ! [ -d "$1" ]; then
    usage
  else
    MY_CMAKE_BUILD_DIR=$(realpath "$1")
    shift
  fi
fi

BINARY_DIR="${MY_CMAKE_BUILD_DIR}/use-cases/thread-sanitizer"
source "${BINARY_DIR}/setup_env.sh"

[ -z "$SANITIZER_PASS" ] && exit 21
[ -f "$SANITIZER_PASS" ] || exit 22
if ! echo "$SANITIZER_PASS" | grep -q "$BUILD_DIR"; then
  exit 23
fi

WORK_DIR="${BINARY_DIR}/sample_apps"
cd "$WORK_DIR" || exit 24

[ -z "${APPNAME_LOWER}" ] && exit 31
unset APP_NAME
source "${SCRIPT_DIR}/../${APPNAME_LOWER}/setup.sh"
[ -z "$APP_NAME" ] && exit 32

if [ ! -d "$APP_NAME" ]; then
  echo "download $APP_NAME"
  download "$APP_NAME" >/dev/null 2>&1
fi

[ -z "$MY_STAN_PASS_MODE" ] && exit 41
echo "building $APP_NAME with config: ${MY_STAN_PASS_MODE}"

if [ "$MY_STAN_PASS_MODE" = "vanilla" ] || [ "$MY_STAN_PASS_MODE" = "orig" ]; then
  build_app "$APP_NAME" "$MY_STAN_PASS_MODE" 'false' >/dev/null 2>&1
else
  build_app "$APP_NAME" "$MY_STAN_PASS_MODE" 'true' >/dev/null 2>&1
fi

if [ -f "${WORK_DIR}/${APP_NAME}_${MY_STAN_PASS_MODE}.exe" ]; then
  touch "${MY_CUR_DIR}/timing.csv"
  exit 0
else
  exit 255
fi
