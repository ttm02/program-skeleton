#!/usr/bin/env bash

source ../setup_env.sh
source "$1" # the script for the tested mini app

if [ ! -d "$APP_NAME" ]; then
  # download if necessary
  download "$APP_NAME"
fi
GREP_STRING="WARNING: ThreadSanitizer: data race"

export USE_STATIC_ANALYSIS=true

build_app "$APP_NAME" 'orig' false &
pid_compile_orig=$!
build_app "$APP_NAME" 'pass' true &
pid_compile_pass=$!

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

build_app "$APP_NAME" 'orig' false &
pid_compile_orig=$!
build_app "$APP_NAME" 'pass' true &
pid_compile_pass=$!

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
    exit 1
  else
    # unexpected, but if unmodified TSAN already could not find it
    echo "both versions did not find the injected data race"
    exit 0
  fi
fi
