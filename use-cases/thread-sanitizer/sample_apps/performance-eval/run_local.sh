#!/usr/bin/env bash

usage() {
    echo "$0 [APPNAME] [PARAM FILE] <PARAM LINE>"
    exit 1
}

SCRIPT_DIR=$(dirname "$(realpath "$0")")

if [ "$#" -lt 2 ]; then
    usage
fi

APPNAME="$1"
PARAMETER_FILE="$2"

SLURM_CPUS_PER_TASK=$(nproc)
SLURM_ARRAY_TASK_ID="${3:-1}"

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

OUTPUT_DIR="$(realpath .)/output"

exec_test
write_result
