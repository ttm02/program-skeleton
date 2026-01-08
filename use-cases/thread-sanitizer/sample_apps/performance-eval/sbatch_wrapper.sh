#!/usr/bin/env bash

usage() {
    echo "$0 [SAMPLE APP NAME] <THREAD COUNT>"
    exit 1
}

if [ "$#" -lt 1 ]; then
    echo "missing parameter"
    usage
fi

APPNAME="$1"
THREAD_COUNT="$2"

APPNAME_LOWER=$(echo "$APPNAME" | tr '[:upper:]' '[:lower:]')
APPNAME_UPPER=$(echo "$APPNAME" | tr '[:lower:]' '[:upper:]')
SCRIPT_DIR="$(dirname "$(realpath "$0")")"

enqueue_sbatch() {
    TC="$1"
    echo "Queue Slurm tasks for $APPNAME_UPPER with $TC threads:"
    sbatch \
        --ntasks 1 \
        --exclusive \
        -o /dev/null \
        -e /dev/null \
        --time '00:30:00' \
        --cpus-per-task "$TC" \
        --job-name="${APPNAME_UPPER}_${TC}" \
        --export=APPNAME_LOWER="$APPNAME_LOWER",APPNAME_UPPER="$APPNAME_UPPER",SCRIPT_DIR="$SCRIPT_DIR" \
        "${SCRIPT_DIR}/job_script_${APPNAME_LOWER}.sh"
    echo ""
}

if [[ "$THREAD_COUNT" =~ ^[0-9]+$ ]]; then
    enqueue_sbatch "$THREAD_COUNT"
else
    for i in $(seq 1 96); do
        # avoid "AssocMaxSubmitJobLimit"
        # "Batch job submission failed: Job violates accounting/QOS policy"
        while [[ 32 -lt "$(squeue | wc -l)" ]]; do
            echo "on hold until the job queue stabilizes"
            echo "currently queued jobs: $(squeue | wc -l)"
            sleep 10s
        done
        enqueue_sbatch "$i"
    done
fi
