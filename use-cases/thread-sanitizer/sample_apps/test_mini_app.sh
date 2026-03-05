#!/usr/bin/env bash

SCRIPT_DIR=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
PRECOMPUTE_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

if [ -f "../setup_env.sh" ]; then
  SETUP_SCRIPT="../setup_env.sh"
elif [ -f "${PRECOMPUTE_DIR}/build/use-cases/thread-sanitizer/setup_env.sh" ]; then
  SETUP_SCRIPT="${PRECOMPUTE_DIR}/build/use-cases/thread-sanitizer/setup_env.sh"
else
  exit 31
fi

MINIAPP_SCRIPT="${SCRIPT_DIR}/${1}/${1}.sh"
[ -f "$MINIAPP_SCRIPT" ] || exit 32

source "$SETUP_SCRIPT"
source "$MINIAPP_SCRIPT" # the script for the tested mini app

if [ ! -d "$APP_NAME" ]; then
  # download if necessary
  download "$APP_NAME"
fi
GREP_STRING="WARNING: ThreadSanitizer: data race"

export MY_STAN_PASS_MODE=""
export MY_STAN_PASS_MODE_ARGS=""

test_compile() {
  MY_STAN_PASS_MODE="orig"
  MY_STAN_PASS_MODE_ARGS=""
  build_app "$APP_NAME" 'orig' false &
  pid_compile_orig=$!

  MY_STAN_PASS_MODE="pass"
  MY_STAN_PASS_MODE_ARGS="--static-analysis-mode=single,merge,loop" # --disable-slicing
  build_app "$APP_NAME" 'pass' true &
  pid_compile_pass=$!
}

test_compile

wait $pid_compile_pass
if [[ ! -x "./${APP_NAME}_pass.exe" ]]; then
  echo "build error"
  exit 1
fi

wait $pid_compile_orig
if [[ ! -x "./${APP_NAME}_orig.exe" ]]; then
  echo "build error"
  exit 1
fi

if "./${APP_NAME}_orig.exe" $TEST_INVOCATION_PARAMETER 2>&1 | grep -qF "$GREP_STRING"; then
  echo "Original TSAN detected race in original application"
  exit 1
fi

if "./${APP_NAME}_pass.exe" $TEST_INVOCATION_PARAMETER 2>&1 | grep -qF "$GREP_STRING"; then
  echo "Modified TSAN detected race in original application"
  exit 1
fi

patch_datarace "$APP_NAME"

test_compile

unpatch_datarace "$APP_NAME"

wait $pid_compile_pass
if [[ ! -x "./${APP_NAME}_pass.exe" ]]; then
  echo "build error"
  exit 1
fi

wait $pid_compile_orig
if [[ ! -x "./${APP_NAME}_orig.exe" ]]; then
  echo "build error"
  exit 1
fi

if "./${APP_NAME}_orig.exe" $TEST_INVOCATION_PARAMETER 2>&1 | grep -qF "$GREP_STRING"; then
  if "./${APP_NAME}_pass.exe" $TEST_INVOCATION_PARAMETER 2>&1 | grep -qF "$GREP_STRING"; then
    # success
    echo "both versions found the injected datarace"
    exit 0
  else
    echo "Original sanitizer found the injected data race but sliced not"
    exit 1
  fi
else
  if "./${APP_NAME}_pass.exe" $TEST_INVOCATION_PARAMETER 2>&1 | grep -qF "$GREP_STRING"; then
    echo "Original sanitizer did not find the injected data race but sliced did"
    exit 2
  else
    # unexpected, but if unmodified TSAN already could not find it
    echo "both versions did not find the injected data race"
    exit 3
  fi
fi
