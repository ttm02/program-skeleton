#!/usr/bin/env bash

#SBATCH --ntasks 1
#SBATCH --array 0-16
#SBATCH --mem-per-cpu=2G
#SBATCH -o /dev/null
#SBATCH -e /dev/null
#SBATCH --time 00:15:00

usage() {
  echo "$0 <THREAD COUNT>"
  exit 1
}

if [ 0 -lt "$#" ]; then
  THREAD_COUNT="$1"
  if ! [[ "$THREAD_COUNT" =~ ^[0-9]+$ ]]; then
    echo "Thread count is not a number!"
    usage
  fi
fi

enqueue_sbatch() {
  TC="$1"
  sbatch \
    --cpus-per-task "$TC" \
    --job-name="DRB_${TC}" \
    --export=SCRIPT_DIR="$SCRIPT_DIR" \
    "$0"
}

if [ -z "$SLURM_ARRAY_JOB_ID" ]; then
  SCRIPT_DIR="$(dirname "$(realpath "$0")")"

  if [[ "$THREAD_COUNT" =~ ^[0-9]+$ ]]; then
    enqueue_sbatch "$THREAD_COUNT"
  else
    for i in $(seq 2 96); do
      echo ""
      date
      echo "Trying to queue slurm job for $APPNAME_UPPER with $TC threads."
      echo ""
      sbatch_wait_time=1
      while ! enqueue_sbatch "$i"; do
        # avoid "AssocMaxSubmitJobLimit"
        # "Batch job submission failed: Job violates accounting/QOS policy"
        sleep ${sbatch_wait_time}s
        [ 3600 -lt "$sbatch_wait_time" ] && sbatch_wait_time=1
        milli_wait_time=$(date +%N | tail -c 2)
        sbatch_wait_time=$((sbatch_wait_time + sbatch_wait_time + milli_wait_time))
      done
      echo ""
      date
      echo "Queued slurm job for $APPNAME_UPPER with $TC threads."
      echo ""
    done
  fi
else
  REAL_HOME=$(realpath "$HOME")
  CONTAINER_IMAGE_PATH="${HOME}/myCont/precompute-devshell"

  export TMPDIR="/dev/shm"
  TMPDIR=$(mktemp -d --suffix='.drb-testing')
  export APPTAINER_TMPDIR="$TMPDIR"

  export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
  export OMP_PLACES=cores

  APP_LOG_NAME="${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"

  cd "$TMPDIR" || exit 23
  apptainer run \
    --mount "type=bind,source=${TMPDIR},destination=${HPC_SCRATCH}/tmp" \
    --mount "type=bind,source=${REAL_HOME},destination=${REAL_HOME}" \
    --env-file "${CONTAINER_IMAGE_PATH}.env" \
    --env APP_LOG_NAME="$APP_LOG_NAME" \
    "${CONTAINER_IMAGE_PATH}.sif" \
    "${SCRIPT_DIR}/cluster_run_wrapper.sh" "${SCRIPT_DIR}/compare_performance.sh"

  LOG_DIR="${HPC_SCRATCH}/precompute/results/DRB"
  mkdir -p "$LOG_DIR"
  mv "${TMPDIR}/results/${APP_LOG_NAME}.csv-acc" "${LOG_DIR}/${APP_LOG_NAME}.csv"
  mkdir -p "${LOG_DIR}_time"
  mv "${TMPDIR}/results/${APP_LOG_NAME}.csv-time" "${LOG_DIR}_time/${APP_LOG_NAME}.csv"

  rm -fr "${TMPDIR}"
  find /dev/shm -name "__KMP_REGISTERED_LIB_*" -user "$USER" -amin +1 -mmin +1 -delete
fi

exit 0
