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
    LINE="$2"
    # constraints limit job scheduling the cluster stage 1
    sbatch \
        --constraint='i01' \
        --ntasks 1 \
        --exclusive \
        -o /dev/null \
        -e /dev/null \
        --array '0-15' \
        --time '00:30:00' \
        --cpus-per-task "$TC" \
        --job-name="${APPNAME_UPPER}_${TC}_${LINE}" \
        --export=APPNAME_LOWER="$APPNAME_LOWER",APPNAME_UPPER="$APPNAME_UPPER",SCRIPT_DIR="$SCRIPT_DIR",APP_PARAM_LINE="$LINE" \
        "${SCRIPT_DIR}/job_script_${APPNAME_LOWER}.sh"
}

enqueue_app() {
    APPNAME_PARAM_LINES_COUNT=$(wc -l "${SCRIPT_DIR}/parameters_${APPNAME_LOWER}.txt" | cut -d' ' -f1) # --total=only
    if [ -z "$APPNAME_PARAM_LINES_COUNT" ]; then
        exit 2
    fi
    for i in $(seq 1 "$APPNAME_PARAM_LINES_COUNT"); do
        while ! enqueue_sbatch "$1" "$i"; do
            # avoid "AssocMaxSubmitJobLimit"
            # "Batch job submission failed: Job violates accounting/QOS policy"
            sleep 42s
        done
    done
}

if [[ "$THREAD_COUNT" =~ ^[0-9]+$ ]]; then
    enqueue_app "$THREAD_COUNT"
else
    for i in 1 2 3 4 6 8 10 12 14 16 20 24 28 32 40 48 56 64 80 96; do
        echo ""
        date
        echo "Trying to queue slurm job for $APPNAME_UPPER with $TC threads."
        echo ""
        enqueue_app "$i"
        echo ""
        date
        echo "Queued slurm job for $APPNAME_UPPER with $TC threads."
        echo ""
    done
fi
