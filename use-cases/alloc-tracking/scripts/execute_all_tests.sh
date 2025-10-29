#!/bin/bash

# build dirs
BUILD_DIRS=(
    "build_debug_slicing_off_logging_on"
    "build_debug_slicing_on_logging_on"
    "build_release_slicing_off_logging_on"
    "build_release_slicing_on_logging_on"
)

TEST_CASES_PATH="tests"

for BUILD_DIR in "${BUILD_DIRS[@]}"; do
    echo "========== Processing build dir: $BUILD_DIR =========="
    TEST_CASES_DIR="$BUILD_DIR/$TEST_CASES_PATH"
    
    # check if build dir and test case dir exist
    if [ ! -d "$BUILD_DIR" ]; then
        continue
    fi
    if [ ! -d "$TEST_CASES_DIR" ]; then
        continue
    fi

    for TEST_FOLDER in "$TEST_CASES_DIR"/*/; do
        # Ensure the path is a directory before processing
        if [ ! -d "$TEST_FOLDER" ]; then
            continue
        fi

        TEST_NAME=$(basename "$TEST_FOLDER")
        TEST_EXECUTABLE="$TEST_FOLDER/$TEST_NAME"

        # only proceed if executable was built
        if [ ! -f "$TEST_EXECUTABLE" ]; then
            continue
        fi

        # --- MODIFICATION START ---
        # Delete previous log files matching the specified pattern.
        # This command finds all files (-type f) in the current test folder
        # whose names contain "_allocation_log_" and end with ".txt", then deletes them.
        echo "====> Cleaning up old logs in: $TEST_FOLDER"
        find "$TEST_FOLDER" -type f -name "*_allocation_log_*.txt" -delete
        # --- MODIFICATION END ---

        # here we execute the executables, however we need to 
        # pass command line args to one test case and make sure 
        # the mpi test cases are run with mpiexec command
        if [ "$TEST_NAME" = "009_from_cmd_args" ]; then
            echo "====> Executing test with argument: $TEST_NAME 5"
            (cd "$TEST_FOLDER" && ./"$TEST_NAME" 5)

        elif [[ "$TEST_NAME" == *"mpi"* ]]; then

            # the mpi heat equation solver needs at least 8 processes, or 27 ...
            if [[ "$TEST_NAME" == 048_mpi_heat_eq_solver* ]]; then
                echo "====> Executing MPI test with 8 processes: $TEST_NAME"
                (cd "$TEST_FOLDER" && mpiexec -np 8 ./"$TEST_NAME")
            else
                # the default for mpi programs is simply 4 processes for testing
                echo "====> Executing MPI test with 4 processes: $TEST_NAME"
                (cd "$TEST_FOLDER" && mpiexec -np 4 ./"$TEST_NAME")
            fi
        else
            echo "====> Executing test: $TEST_NAME"
            (cd "$TEST_FOLDER" && ./"$TEST_NAME") # here we just run the executable
        fi
        
        if [ $? -ne 0 ]; then
            echo "ERROR: Test '$TEST_NAME' failed (exit code: $?)."
        fi
    done
done

echo "========== All tests executed =========="
