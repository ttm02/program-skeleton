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
        --array '0-17' \
        --time '00:30:00' \
        --cpus-per-task "$TC" \
        --job-name="${APPNAME_UPPER}_${TC}_${LINE}" \
        --export=APPNAME_LOWER="$APPNAME_LOWER",APPNAME_UPPER="$APPNAME_UPPER",SCRIPT_DIR="$SCRIPT_DIR",APP_PARAM_LINE="$LINE" \
        "${SCRIPT_DIR}/../${APPNAME_LOWER}/job_script.sh"
}

enqueue_app() {
    TC="$1"
    PARAMETER_FILE="${SCRIPT_DIR}/../${APPNAME_LOWER}/parameters.txt"
    APPNAME_PARAM_LINES_COUNT=$(wc -l "$PARAMETER_FILE" | cut -d' ' -f1) # --total=only
    if [ -z "$APPNAME_PARAM_LINES_COUNT" ]; then
        exit 2
    fi
    for i in $(seq 1 "$APPNAME_PARAM_LINES_COUNT"); do
        sbatch_wait_time=1
        while ! enqueue_sbatch "$TC" "$i"; do
            # avoid "AssocMaxSubmitJobLimit"
            # "Batch job submission failed: Job violates accounting/QOS policy"
            sleep ${sbatch_wait_time}s
            [ 3600 -lt "$sbatch_wait_time" ] && sbatch_wait_time=1
            milli_wait_time=$(date +%N | tail -c 2)
            sbatch_wait_time=$((sbatch_wait_time + sbatch_wait_time + milli_wait_time))
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
