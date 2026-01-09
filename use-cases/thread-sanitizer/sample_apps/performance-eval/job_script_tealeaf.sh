#!/usr/bin/env bash

# 15 lines (config arguments) * 3 modes
#SBATCH --array 0-59

#SBATCH --mem-per-cpu=3072

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

RUN_DIR="${HPC_SCRATCH}/tmp/${SLURM_ARRAY_JOB_ID}/${SLURM_ARRAY_TASK_ID}"
mkdir -p "$RUN_DIR"
cd "$RUN_DIR" || exit 1

# copy inputs
IFS=',' read -r RESOLUTION STEPS <<<"$RUN_PARAMETER"
cp "${EXEC_DIR}/tea.in" "${EXEC_DIR}/tea.problems" ./

# update input file with correct parameters
sed -i \
    -e "s/^x_cells=.*/x_cells=$RESOLUTION/" \
    -e "s/^y_cells=.*/y_cells=$RESOLUTION/" \
    -e "s/^end_step=.*/end_step=$STEPS/" \
    tea.in

setup_resources
exec_test
write_result
