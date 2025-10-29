#!/bin/bash

#SBATCH -J testingAllocTrackingSpecCPUBench
#SBATCH --mail-user=sebastian.laux@stud.tu-darmstadt.de
#SBATCH --mail-type=ALL
#SBATCH -A project02560
#SBATCH -e /home/sl82bepo/job_output/job.err.%j
#SBATCH -o /home/sl82bepo/job_output/job.out.%j
#SBATCH -n 1
#SBATCH --mem-per-cpu=750
#SBATCH -t 00:15:00
#SBATCH -D /home/sl82bepo/alloc-tracking/use_cases/spec_cpu_mcf


echo "Starting spec cpu bench build"

module purge
module load gcc/13.1.0 clang/16.0.6

which gcc
which clang

make clean
make

echo "Finish spec cpu bench build"
