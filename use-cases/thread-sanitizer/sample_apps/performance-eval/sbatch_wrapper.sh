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
}

if [[ "$THREAD_COUNT" =~ ^[0-9]+$ ]]; then
    enqueue_sbatch "$THREAD_COUNT"
else
    for i in 1 2 3 4 6 8 10 12 14 16 20 24 28 32 40 48 56 64 80 96; do
        echo ""
        date
        echo "Trying to queue slurm job for $APPNAME_UPPER with $TC threads."
        echo ""
        while ! enqueue_sbatch "$i"; do
            # avoid "AssocMaxSubmitJobLimit"
            # "Batch job submission failed: Job violates accounting/QOS policy"
            sleep 42s
        done
        echo ""
        date
        echo "Queued slurm job for $APPNAME_UPPER with $TC threads."
        echo ""
    done
fi
