#!/usr/bin/env bash

# 20 lines (config arguments) * 3 modes
#SBATCH --array 0-59

#SBATCH --mem-per-cpu=3072

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

setup_resources
exec_test
write_result
