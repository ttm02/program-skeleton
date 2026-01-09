#!/usr/bin/env bash

# 9 lines (config arguments) * 3 modes
#SBATCH --array 0-35

#SBATCH --mem-per-cpu=3072

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

setup_resources
exec_test
write_result
