#!/bin/bash

#SBATCH -J testingAllocTrackingTestSuiteExecutionPart1
#SBATCH --mail-user=sebastian.laux@stud.tu-darmstadt.de
#SBATCH --mail-type=ALL
#SBATCH -A project02560
#SBATCH -e /home/sl82bepo/job_output/job.err.%j
#SBATCH -o /home/sl82bepo/job_output/job.out.%j
#SBATCH -n 1
#SBATCH --mem-per-cpu=500
#SBATCH -t 00:10:00
#SBATCH -D /work/scratch/sl82bepo/alloc-tracking-build


echo "Starting tests execution Part 1"

module purge
module load gcc/13.1.0 clang/16.0.6 cmake/3.26.1


# Build dirs
BUILD_DIRS=(
    #"build_debug_slicing_off_logging_on"
    #"build_debug_slicing_on_logging_on"
    "build_release_slicing_off_logging_on"
    "build_release_slicing_on_logging_on"
)

TEST_CASES_PATH="tests"

for BUILD_DIR in "${BUILD_DIRS[@]}"; do
    echo "========== Processing build dir: $BUILD_DIR =========="
    TEST_CASES_DIR="$BUILD_DIR/$TEST_CASES_PATH"
    
    # Check if build dir and test case dir exist
    if [ ! -d "$BUILD_DIR" ]; then
        echo "Warning: Build directory $BUILD_DIR does not exist, skipping..."
        continue
    fi
    if [ ! -d "$TEST_CASES_DIR" ]; then
        echo "Warning: Test cases directory $TEST_CASES_DIR does not exist, skipping..."
        continue
    fi
    
    for TEST_FOLDER in "$TEST_CASES_DIR"/*/; do
        # Ensure the path is a directory before processing
        if [ ! -d "$TEST_FOLDER" ]; then
            continue
        fi
        
        TEST_NAME=$(basename "$TEST_FOLDER")
        TEST_EXECUTABLE="$TEST_FOLDER/$TEST_NAME"
        
        # Only proceed if executable was built
        if [ ! -f "$TEST_EXECUTABLE" ]; then
            echo "Warning: Executable $TEST_EXECUTABLE not found, skipping..."
            continue
        fi

	if [ "$TEST_NAME" = "008_from_file" ]; then
	    echo "Skipping test: $TEST_NAME"
	    continue
	fi
        
        # Skip MPI tests - they will be handled by other scripts
        if [[ "$TEST_NAME" == *"mpi"* ]]; then
            echo "Skipping MPI test: $TEST_NAME (handled by MPI scripts)"
            continue
        fi
        
        # Clean up old logs
        echo "====> Cleaning up old logs in: $TEST_FOLDER"
        find "$TEST_FOLDER" -type f -name "*_allocation_log_*.txt" -delete
        
        # Execute serial tests
        if [ "$TEST_NAME" = "009_from_cmd_args" ]; then
            echo "====> Executing test with argument: $TEST_NAME 5"
            (cd "$TEST_FOLDER" && ./"$TEST_NAME" 5)
        else
            echo "====> Executing test: $TEST_NAME"
            (cd "$TEST_FOLDER" && ./"$TEST_NAME")
        fi
        
        # Check exit status
        EXIT_CODE=$?
        if [ $EXIT_CODE -ne 0 ]; then
            echo "ERROR: Test '$TEST_NAME' in '$BUILD_DIR' failed (exit code: $EXIT_CODE)."
        else
            echo "SUCCESS: Test '$TEST_NAME' completed successfully."
        fi
    done
done

echo "========== Serial tests completed =========="
echo "Job completed at: $(date)"
