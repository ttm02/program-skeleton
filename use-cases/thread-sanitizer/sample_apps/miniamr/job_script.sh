#!/usr/bin/env bash

#SBATCH --mem-per-cpu=3072

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

export OMP_PROC_BIND='true'

setup_resources
exec_test
write_result
