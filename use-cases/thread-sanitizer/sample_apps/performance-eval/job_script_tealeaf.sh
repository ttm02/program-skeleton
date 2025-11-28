#!/usr/bin/env bash

#SBATCH --ntasks 1

#SBATCH --cpus-per-task 8
#SBATCH --mem-per-cpu=3800

#SBATCH --time 00:30:00
#SBATCH --exclusive

# TODO actually 15 lines
#SBATCH --array 1-6

# change for debugging the environment
#SBATCH -o /dev/null
#SBATCH -e /dev/null

SCRIPT_DIR=$(dirname "$(realpath "$0")")

APPNAME='TeaLeaf'
PARAMETER_FILE="${SCRIPT_DIR}/parameters_tealeaf.txt"

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

RUN_DIR="$OUTPUT_DIR/tmp/${SLURM_ARRAY_TASK_ID}"
mkdir -p "$RUN_DIR"
cd "$RUN_DIR" || exit 1

# copy inputs
IFS=',' read -r RESOLUTION STEPS <<<"$RUN_PARAMETER"
cp "${EXEC_DIR}/tea.in" "${EXEC_DIR}/tea.problems" "${EXEC_DIR}/${APPNAME}_${MODE}.exe" ./
EXEC_DIR="$RUN_DIR"

# update input file with correct parameters
sed -i \
    -e "s/^x_cells=.*/x_cells=$RESOLUTION/" \
    -e "s/^y_cells=.*/y_cells=$RESOLUTION/" \
    -e "s/^end_step=.*/end_step=$STEPS/" \
    tea.in

setup_resources
exec_test
write_result
