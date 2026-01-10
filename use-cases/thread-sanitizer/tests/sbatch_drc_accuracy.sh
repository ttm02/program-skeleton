#!/usr/bin/env bash

#SBATCH --ntasks 1
#SBATCH --exclusive
#SBATCH --array 1-10
#SBATCH --mem-per-cpu=128
#SBATCH -o /dev/null
#SBATCH -e /dev/null
#SBATCH --time 00:30:00

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
else
  REAL_HOME=$(realpath "$HOME")
  CONTAINER_IMAGE_PATH="${HOME}/myCont/precompute-devshell"

  export TMPDIR="/tmp"
  export APPTAINER_TMPDIR="${HPC_SCRATCH}/tmp"

  export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
  export OMP_PLACES=cores

  cd "$TMPDIR" || exit 23
  apptainer run \
    --mount "type=bind,source=${REAL_HOME},destination=${REAL_HOME}" \
    --mount "type=bind,source=${HPC_SCRATCH},destination=${HPC_SCRATCH}" \
    --env-file "${CONTAINER_IMAGE_PATH}.env" \
    "${CONTAINER_IMAGE_PATH}.sif" \
    "${SCRIPT_DIR}/cluster_run_wrapper.sh" "${SCRIPT_DIR}/compare_performance.sh"
fi
