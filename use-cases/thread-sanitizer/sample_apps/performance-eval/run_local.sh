#!/usr/bin/env bash

usage() {
	echo "$0 [APPNAME] --line <number> --mode <number> --threads <number>"
	echo "$0 --help"
	exit 1
}

SCRIPT_DIR=$(dirname "$(realpath "$0")")

if [ "$#" -lt 1 ]; then
	usage
fi

cli_args=$(getopt -o '' --long "help,line:,mode:,threads:" --name "$0" -- "$@")
RES=$?
[[ "$RES" -ne 0 ]] && usage
eval set -- "$cli_args"

# defaults
SLURM_CPUS_PER_TASK=$(nproc)
SLURM_ARRAY_TASK_ID="17"
APP_PARAM_LINE="1"

while true; do
	case "$1" in
	--help)
		usage
		;;
	--line)
		APP_PARAM_LINE="$2"
		shift 2
		;;
	--mode)
		if [ "$2" -lt 0 ] || [ 17 -lt "$2" ]; then
			echo "--mode is out of range [0, 17]"
			exit 1
		fi
		SLURM_ARRAY_TASK_ID="$2"
		shift 2
		;;
	--threads)
		SLURM_CPUS_PER_TASK="$2"
		shift 2
		;;
	--)
		shift
		break
		;;
	esac
done

export APPNAME="$1"
APPNAME_LOWER=$(echo "$APPNAME" | tr '[:upper:]' '[:lower:]')
APPNAME_UPPER=$(echo "$APPNAME" | tr '[:lower:]' '[:upper:]')

PARAMETER_FILE="${SCRIPT_DIR}/parameters_${APPNAME_LOWER}.txt"
APPNAME_PARAM_LINES_COUNT=$(wc -l "${SCRIPT_DIR}/parameters_${APPNAME_LOWER}.txt" | cut -d' ' -f1)
if [ "$APP_PARAM_LINE" -lt 1 ] || [ "$APPNAME_PARAM_LINES_COUNT" -lt "$APP_PARAM_LINE" ]; then
	echo "--line is out of range [1, $APPNAME_PARAM_LINES_COUNT]"
	exit 1
fi

HPC_SCRATCH="$(realpath .)/scratch"
export HPC_SCRATCH
export APPNAME_LOWER
export APPNAME_UPPER
export PRECOMPUTE_RUN_JOB_LOCALLY="true"

source "${SCRIPT_DIR}/job_script_cluster_common.sh"

exec_test
write_result
