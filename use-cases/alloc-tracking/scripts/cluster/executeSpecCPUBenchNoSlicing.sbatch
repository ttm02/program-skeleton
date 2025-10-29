#!/bin/bash

#SBATCH -J testingAllocTrackingSpecCPUBenchExecutionSliceOffLoggingOn
#SBATCH --mail-user=sebastian.laux@stud.tu-darmstadt.de
#SBATCH --mail-type=ALL
#SBATCH -A project02560
#SBATCH -e /home/sl82bepo/job_output/job.err.%j
#SBATCH -o /home/sl82bepo/job_output/job.out.%j
#SBATCH -n 1
#SBATCH --mem-per-cpu=1800
#SBATCH -t 00:15:00
#SBATCH -D /home/sl82bepo/alloc-tracking/use_cases/spec_cpu_mcf


echo "Starting spec cpu bench execution slice off logging on"

module purge
module load gcc/13.1.0 clang/16.0.6
module load gdb

which gcc
which clang

./mcf inp.in

echo "Finish spec cpu bench execution slice off logging on"
