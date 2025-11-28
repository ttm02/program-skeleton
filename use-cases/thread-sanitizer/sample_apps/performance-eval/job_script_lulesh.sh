#!/usr/bin/env bash

#SBATCH --ntasks 1

#SBATCH --cpus-per-task 8
#SBATCH --mem-per-cpu=3800

#SBATCH --time 00:30:00
#SBATCH --exclusive

#SBATCH --array 1-20

# change for debugging the environment
#SBATCH -o /dev/null
#SBATCH -e /dev/null

SCRIPT_DIR=$(dirname "$(realpath "$0")")

APPNAME='LULESH'
PARAMETER_FILE="${SCRIPT_DIR}/parameters_lulesh.txt"

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

setup_resources
exec_test
write_result
